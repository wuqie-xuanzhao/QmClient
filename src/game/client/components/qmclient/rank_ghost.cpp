#include "rank_ghost.h"

#include "rank_demo_manifest.h"

#include <base/log.h>
#include <base/str.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/client/ghost.h>
#include <engine/shared/compression.h>
#include <engine/shared/json.h>
#include <engine/shared/network.h>
#include <engine/shared/snapshot.h>
#include <engine/storage.h>

#include <generated/protocol.h>
#include <generated/protocol7.h>

#include <game/client/components/ghost.h>
#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/gamecore.h>
#include <game/localization.h>

#include <zlib.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
	constexpr int64_t PARSE_TIME_BUDGET_MS = 5;
	constexpr int MIN_PATH_TICKS = 25;
	constexpr size_t MAX_MANIFEST_BYTES = 16 * 1024 * 1024;
	constexpr size_t MAX_DEMO_BYTES = 128 * 1024 * 1024;
	constexpr size_t MAX_UNPACKED_DEMO_BYTES = 256 * 1024 * 1024;
	constexpr int64_t MANIFEST_CACHE_TTL_SECONDS = 300;
	constexpr const char *USER_AGENT = "QmClient (https://github.com/wxj881027/QmClient)";

	// rust::Box 没有默认构造，必须在构造函数里创建
	rust::Box<CSnapshotDelta> CreateSnapshotDelta();
	rust::Box<CSnapshotDelta> CreateSnapshotDeltaSixup();

	// demo 解析期间的状态，独立于主客户端，避免打断正常游戏
	struct SParseContext
	{
		rust::Box<CSnapshotDelta> m_pDelta;
		rust::Box<CSnapshotDelta> m_pDeltaSixup;
		std::unique_ptr<CDemoPlayer> m_pPlayer;
		CNetObjHandler m_NetObjHandler;

		std::vector<CGhostCharacter> m_vPath;
		CGhostSkin m_Skin{};
		char m_aOwner[MAX_NAME_LENGTH] = "";
		char m_aMapName[64] = "";
		SHA256_DIGEST m_MapSha256{};

		int m_RunStart = 0;
		int m_RunEnd = 0;
		int m_Cid = 0;
		bool m_FoundClientInfo = false;
		bool m_FoundCharacter = false;

		SParseContext();
	};

	// 生成用于缓存文件的名称：去掉服务端的 .gz 后缀并清理非法字符。
	// 服务端文件名本身是 uuid-hash.demo.gz 形式，这里只做防御性处理。
	void MakeSafeCacheName(const char *pRawName, char *pOut, int OutSize)
	{
		std::string Name = pRawName != nullptr ? pRawName : "";
		if(Name.size() > 3 && Name.compare(Name.size() - 3, 3, ".gz") == 0)
			Name.resize(Name.size() - 3);
		// 去掉已带的 .demo 后缀，后续统一追加，避免出现 .demo.demo
		if(Name.size() > 5 && Name.compare(Name.size() - 5, 5, ".demo") == 0)
			Name.resize(Name.size() - 5);
		str_copy(pOut, Name.c_str(), OutSize);
		str_sanitize_filename(pOut);
	}

	// 按 CGhostRecorder 的格式写 ghost 文件。
	// 不使用 recorder 类是因为它的 Kernel() 依赖接口注册时才注入的指针，
	// 组件内直接构造的实例无法取得 storage。
	class CGhostFileWriter
	{
		IOHANDLE m_File = nullptr;
		alignas(uint32_t) char m_aBuffer[MAX_CHUNK_SIZE] = {};
		alignas(uint32_t) char m_aBufferTemp[MAX_CHUNK_SIZE] = {};
		char *m_pBufferPos = m_aBuffer;
		int m_BufferNumItems = 0;
		std::optional<CGhostItem> m_LastItem;
		bool m_Failed = false;

		void ResetBuffer()
		{
			m_pBufferPos = m_aBuffer;
			m_BufferNumItems = 0;
		}

		bool FlushChunk()
		{
			const int Size = (int)(m_pBufferPos - m_aBuffer);
			if(Size == 0 || m_BufferNumItems == 0 || !m_LastItem.has_value())
			{
				ResetBuffer();
				return !m_Failed;
			}

			int CompressedSize = CVariableInt::Compress(m_aBuffer, Size, m_aBufferTemp, sizeof(m_aBufferTemp));
			if(CompressedSize < 0)
			{
				m_Failed = true;
				ResetBuffer();
				m_LastItem = std::nullopt;
				return false;
			}
			CompressedSize = CNetBase::Compress(m_aBufferTemp, CompressedSize, m_aBuffer, sizeof(m_aBuffer));
			if(CompressedSize < 0)
			{
				m_Failed = true;
				ResetBuffer();
				m_LastItem = std::nullopt;
				return false;
			}

			unsigned char aChunkHeader[4];
			aChunkHeader[0] = m_LastItem.value().m_Type & 0xff;
			aChunkHeader[1] = m_BufferNumItems & 0xff;
			aChunkHeader[2] = (CompressedSize >> 8) & 0xff;
			aChunkHeader[3] = CompressedSize & 0xff;
			if(io_write(m_File, aChunkHeader, sizeof(aChunkHeader)) != sizeof(aChunkHeader) || io_write(m_File, m_aBuffer, CompressedSize) != CompressedSize)
			{
				m_Failed = true;
				ResetBuffer();
				m_LastItem = std::nullopt;
				return false;
			}

			m_LastItem = std::nullopt;
			ResetBuffer();
			return true;
		}

	public:
		bool Open(IStorage *pStorage, const char *pPath, const char *pMap, const SHA256_DIGEST &MapSha256, const char *pOwner, int NumTicks, int TimeMs)
		{
			m_File = pStorage->OpenFile(pPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
			if(m_File == nullptr)
				return false;

			CGhostHeader Header;
			mem_zero(&Header, sizeof(Header));
			mem_copy(Header.m_aMarker, "TWGHOST", 7);
			Header.m_Version = 6;
			str_copy(Header.m_aOwner, pOwner);
			str_copy(Header.m_aMap, pMap);
			Header.m_MapSha256 = MapSha256;
			uint_to_bytes_be(Header.m_aNumTicks, NumTicks);
			uint_to_bytes_be(Header.m_aTime, TimeMs);
			if(io_write(m_File, &Header, sizeof(Header)) != sizeof(Header))
			{
				io_close(m_File);
				m_File = nullptr;
				return false;
			}

			m_LastItem = std::nullopt;
			m_Failed = false;
			ResetBuffer();
			return true;
		}

		bool WriteData(int Type, const void *pData, size_t Size)
		{
			if(m_File == nullptr || m_Failed || Size == 0 || Size > MAX_ITEM_SIZE || Size % sizeof(uint32_t) != 0)
				return false;
			if((size_t)(sizeof(m_aBuffer) - (m_pBufferPos - m_aBuffer)) < Size)
			{
				if(!FlushChunk())
					return false;
			}

			CGhostItem Item;
			mem_copy(Item.m_aData, pData, Size);
			Item.m_Size = Size;
			Item.m_Type = Type;
			if(m_LastItem.has_value() && m_LastItem.value().m_Type == Item.m_Type)
			{
				// 同类型连续项写差分
				const uint32_t *pPast = (const uint32_t *)m_LastItem.value().m_aData;
				const uint32_t *pCurrent = (const uint32_t *)Item.m_aData;
				uint32_t *pOut = (uint32_t *)m_pBufferPos;
				for(size_t i = 0; i < Size / sizeof(uint32_t); i++)
					pOut[i] = pCurrent[i] - pPast[i];
			}
			else
			{
				if(!FlushChunk())
					return false;
				mem_copy(m_pBufferPos, Item.m_aData, Size);
			}

			m_LastItem = Item;
			m_pBufferPos += Size;
			m_BufferNumItems++;
			if(m_BufferNumItems >= NUM_ITEMS_PER_CHUNK)
				FlushChunk();
			return !m_Failed;
		}

		bool Close()
		{
			if(m_File == nullptr)
				return !m_Failed;
			const bool Flushed = FlushChunk();
			io_close(m_File);
			m_File = nullptr;
			return Flushed && !m_Failed;
		}
	};

	bool IsGzipData(const unsigned char *pData, size_t Size)
	{
		return Size >= 2 && pData[0] == 0x1f && pData[1] == 0x8b;
	}

	bool HasDemoMagic(const unsigned char *pData, size_t Size)
	{
		return Size >= 7 && mem_comp(pData, "TWDEMO\0", 7) == 0;
	}

	// 读取文件头，判断是否为有效的 demo（服务端通常已解压，个别缓存路径可能返回原始 gzip 字节）
	bool ValidateOrUnpackDemo(class IStorage *pStorage, const char *pStoragePath)
	{
		void *pData = nullptr;
		unsigned DataSize = 0;
		if(!pStorage->ReadFile(pStoragePath, IStorage::TYPE_SAVE, &pData, &DataSize) || pData == nullptr || DataSize == 0)
		{
			free(pData);
			return false;
		}

		if(!IsGzipData((const unsigned char *)pData, DataSize))
		{
			const bool Valid = HasDemoMagic((const unsigned char *)pData, DataSize);
			free(pData);
			return Valid;
		}

		// gzip：解压到内存后写回
		z_stream Stream = {};
		Stream.next_in = (Bytef *)pData;
		Stream.avail_in = DataSize;
		if(inflateInit2(&Stream, 15 + 32) != Z_OK)
		{
			free(pData);
			return false;
		}

		std::vector<unsigned char> Output;
		Output.resize(std::min<size_t>(std::max<size_t>(DataSize * 4, 1024 * 1024), MAX_UNPACKED_DEMO_BYTES));
		size_t OutputLength = 0;
		int Result = Z_OK;
		while(Result == Z_OK)
		{
			if(OutputLength == Output.size())
			{
				if(Output.size() >= MAX_UNPACKED_DEMO_BYTES)
				{
					inflateEnd(&Stream);
					free(pData);
					return false;
				}
				Output.resize(std::min(Output.size() * 2, MAX_UNPACKED_DEMO_BYTES));
			}
			Stream.next_out = Output.data() + OutputLength;
			Stream.avail_out = (uInt)std::min<size_t>(Output.size() - OutputLength, 0x40000000);
			Result = inflate(&Stream, Z_NO_FLUSH);
			OutputLength = Output.size() - Stream.avail_out;
		}
		inflateEnd(&Stream);
		free(pData);

		if(Result != Z_STREAM_END || OutputLength > MAX_UNPACKED_DEMO_BYTES || !HasDemoMagic(Output.data(), OutputLength))
			return false;

		IOHANDLE File = pStorage->OpenFile(pStoragePath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
		if(!File)
			return false;
		const bool WriteOk = io_write(File, Output.data(), OutputLength) == OutputLength;
		io_close(File);
		return WriteOk;
	}

	int FindObjItemIndex(const CSnapshot *pSnapshot, int Type, int Id)
	{
		const int NumItems = pSnapshot->NumItems();
		for(int i = 0; i < NumItems; i++)
		{
			if(pSnapshot->GetItemType(i) == Type && pSnapshot->GetItem(i)->Id() == Id)
				return i;
		}
		return -1;
	}

	// 演示影子只需要位置等渲染信息，提取方式与 CGhost::GetGhostCharacter 保持一致
	void CopyGhostCharacter(CGhostCharacter &Out, const CNetObj_Character &Char, const CNetObj_DDNetCharacter *pDDNetChar, int GhostTick)
	{
		Out.m_X = Char.m_X;
		Out.m_Y = Char.m_Y;
		Out.m_VelX = Char.m_VelX;
		Out.m_VelY = 0;
		Out.m_Angle = Char.m_Angle;
		Out.m_Direction = Char.m_Direction;
		int Weapon = Char.m_Weapon;
		if(pDDNetChar != nullptr && pDDNetChar->m_FreezeEnd != 0)
			Weapon = WEAPON_NINJA;
		Out.m_Weapon = Weapon;
		Out.m_HookState = Char.m_HookState;
		Out.m_HookX = Char.m_HookX;
		Out.m_HookY = Char.m_HookY;
		Out.m_AttackTick = Char.m_AttackTick;
		Out.m_Tick = GhostTick;
	}

	// 尝试抓取目标玩家的皮肤与名字（ClientInfo 更新频率低，遇到就记下）
	void ExtractClientInfoOnce(SParseContext &Parse, const CSnapshot *pSnapshot)
	{
		if(Parse.m_FoundClientInfo)
			return;
		const int Index = FindObjItemIndex(pSnapshot, NETOBJTYPE_CLIENTINFO, Parse.m_Cid);
		if(Index < 0)
			return;

		CUnpacker Unpacker;
		Unpacker.Reset(pSnapshot->GetItem(Index)->Data(), pSnapshot->GetItemSize(Index));
		const void *pRaw = Parse.m_NetObjHandler.SecureUnpackObj(NETOBJTYPE_CLIENTINFO, &Unpacker);
		if(!pRaw)
			return;

		const CNetObj_ClientInfo *pInfo = (const CNetObj_ClientInfo *)pRaw;
		if(!IntsToStr(pInfo->m_aName, std::size(pInfo->m_aName), Parse.m_aOwner, sizeof(Parse.m_aOwner)))
			Parse.m_aOwner[0] = '\0';
		mem_copy(Parse.m_Skin.m_aSkin, pInfo->m_aSkin, sizeof(Parse.m_Skin.m_aSkin));
		Parse.m_Skin.m_UseCustomColor = pInfo->m_UseCustomColor;
		Parse.m_Skin.m_ColorBody = pInfo->m_ColorBody;
		Parse.m_Skin.m_ColorFeet = pInfo->m_ColorFeet;
		Parse.m_FoundClientInfo = true;
	}

	void ExtractCharacter(SParseContext &Parse, const CSnapshot *pSnapshot, int Tick)
	{
		const int Index = FindObjItemIndex(pSnapshot, NETOBJTYPE_CHARACTER, Parse.m_Cid);
		if(Index < 0)
			return;

		CUnpacker Unpacker;
		Unpacker.Reset(pSnapshot->GetItem(Index)->Data(), pSnapshot->GetItemSize(Index));
		const void *pRawChar = Parse.m_NetObjHandler.SecureUnpackObj(NETOBJTYPE_CHARACTER, &Unpacker);
		if(!pRawChar)
			return;

		const CNetObj_DDNetCharacter *pDDNetChar = nullptr;
		const int ExtIndex = FindObjItemIndex(pSnapshot, NETOBJTYPE_DDNETCHARACTER, Parse.m_Cid);
		if(ExtIndex >= 0)
		{
			CUnpacker ExtUnpacker;
			ExtUnpacker.Reset(pSnapshot->GetItem(ExtIndex)->Data(), pSnapshot->GetItemSize(ExtIndex));
			pDDNetChar = (const CNetObj_DDNetCharacter *)Parse.m_NetObjHandler.SecureUnpackObj(NETOBJTYPE_DDNETCHARACTER, &ExtUnpacker);
		}

		CGhostCharacter GhostChar;
		CopyGhostCharacter(GhostChar, *(const CNetObj_Character *)pRawChar, pDDNetChar, Tick - Parse.m_RunStart);
		Parse.m_vPath.push_back(GhostChar);
		Parse.m_FoundCharacter = true;
	}

	// 与上游 demo_extract_chat 工具一致：snapshot delta 需要按对象类型设置静态大小
	rust::Box<CSnapshotDelta> CreateSnapshotDelta()
	{
		rust::Box<CSnapshotDelta> pResult = CSnapshotDelta::New();
		CNetObjHandler NetObjHandler;
		for(int i = 0; i < NUM_NETOBJTYPES; i++)
			pResult->SetStaticsize(i, NetObjHandler.GetObjSize(i));
		return pResult;
	}

	rust::Box<CSnapshotDelta> CreateSnapshotDeltaSixup()
	{
		rust::Box<CSnapshotDelta> pResult = CSnapshotDelta::New();
		protocol7::CNetObjHandler NetObjHandler7;
		// HACK: 只设置 0.7 首个版本存在的对象，避免新对象破坏 snapshot delta
		static const int OLD_NUM_NETOBJTYPES = 23;
		for(int i = 0; i < OLD_NUM_NETOBJTYPES; i++)
			pResult->SetStaticsize(i, NetObjHandler7.GetObjSize(i));
		return pResult;
	}

	SParseContext::SParseContext() :
		m_pDelta(CreateSnapshotDelta()),
		m_pDeltaSixup(CreateSnapshotDeltaSixup())
	{
	}
} // namespace

// 头文件中前向声明的解析状态
struct CRankGhost::SParseState
{
	SParseContext m_Context;
};

CRankGhost::CRankGhost() = default;
CRankGhost::~CRankGhost() = default;

void CRankGhost::RequestCurrentMapGhost(int Rank)
{
	StartLookup(nullptr, Rank);
}

void CRankGhost::OnConsoleInit()
{
	Console()->Register("qm_rank_ghost", "?s[map] ?i[rank]", CFGFLAG_CLIENT, ConRankGhost, this,
		"Load the official rank replay of a map as a ghost (default: current map, rank 1)");
	Console()->Register("qm_rank_ghost_off", "", CFGFLAG_CLIENT, ConRankGhostOff, this, "Unload the rank ghost");
}

void CRankGhost::ConRankGhost(IConsole::IResult *pResult, void *pUserData)
{
	CRankGhost *pSelf = (CRankGhost *)pUserData;
	const char *pMap = pResult->NumArguments() > 0 ? pResult->GetString(0) : nullptr;
	const int Rank = pResult->NumArguments() > 1 ? pResult->GetInteger(1) : 1;
	pSelf->StartLookup(pMap, Rank);
}

void CRankGhost::ConRankGhostOff(IConsole::IResult *pResult, void *pUserData)
{
	CRankGhost *pSelf = (CRankGhost *)pUserData;
	pSelf->m_UnloadPending = true;
}

void CRankGhost::Echo(const char *pMessage) const
{
	if(pMessage == nullptr || pMessage[0] == '\0')
		return;
	GameClient()->m_Chat.Echo(pMessage);
}

void CRankGhost::Fail(const char *pMessage)
{
	log_error("rank_ghost", "%s", pMessage != nullptr ? pMessage : "unknown error");
	Echo(pMessage);
	AbortTask();
}

void CRankGhost::AbortTask()
{
	const bool DownloadInProgress = m_Stage == EStage::FETCH_DEMO;
	if(m_pManifestRequest)
	{
		m_pManifestRequest->Abort();
		m_pManifestRequest = nullptr;
	}
	if(m_pDemoRequest)
	{
		m_pDemoRequest->Abort();
		m_pDemoRequest = nullptr;
	}
	if(m_pParse && m_pParse->m_Context.m_pPlayer)
		m_pParse->m_Context.m_pPlayer->Stop();
	m_pParse.reset();
	m_Stage = EStage::IDLE;
	if(DownloadInProgress && m_aDemoStoragePath[0] != '\0')
		Storage()->RemoveFile(m_aDemoStoragePath, IStorage::TYPE_SAVE);
}

void CRankGhost::OnUpdate()
{
	if(!m_PendingNotify.empty())
	{
		Echo(m_PendingNotify.c_str());
		m_PendingNotify.clear();
	}

	if(m_UnloadPending)
	{
		m_UnloadPending = false;
		m_StartPending = false;
		m_RetryLoadPending = false;
		m_RetryLoadDeadline = 0;
		m_RetryLoadNextAttempt = 0;
		if(m_Stage != EStage::IDLE)
			AbortTask();
		UnloadGhost();
		Echo(Localize("Rank ghost unloaded"));
	}

	if(m_StartPending)
	{
		m_StartPending = false;
		if(m_ManifestLoaded && time_get() - m_ManifestLoadedAt < time_freq() * MANIFEST_CACHE_TTL_SECONDS)
			LookupInManifest();
		else
			StartManifestFetch();
	}

	if(m_RetryLoadPending)
	{
		const int64_t Now = time_get();
		if(Client()->GetCurrentMap()[0] != '\0' && str_comp_nocase(Client()->GetCurrentMap(), m_PendingMap.c_str()) == 0 && Now >= m_RetryLoadNextAttempt)
		{
			if(LoadGhostFile(m_aGhostStoragePath))
			{
				m_RetryLoadPending = false;
				NotifyLoaded(m_ActiveEntry.m_Names.c_str(), m_ActiveEntry.m_Time.c_str());
			}
			else
			{
				// 地图已加载仍无法读取，说明缓存影子损坏或与地图不匹配；
				// 删除后从缓存回放重建，避免在同一坏文件上无限重试。
				Storage()->RemoveFile(m_aGhostStoragePath, IStorage::TYPE_SAVE);
				m_RetryLoadPending = false;
				m_RetryLoadDeadline = 0;
				m_RetryLoadNextAttempt = 0;
				if(Storage()->FileExists(m_aDemoStoragePath, IStorage::TYPE_SAVE))
					StartParse();
				else
					StartDemoFetch();
			}
		}
		if(m_RetryLoadPending && Now >= m_RetryLoadDeadline)
		{
			m_RetryLoadPending = false;
			m_PendingNotify = Localize("Rank ghost: timed out waiting for the map to load");
		}
	}

	AlignToCurrentRun();

	switch(m_Stage)
	{
	case EStage::IDLE:
		break;
	case EStage::FETCH_MANIFEST:
		UpdateManifestStage();
		break;
	case EStage::FETCH_DEMO:
		UpdateDemoStage();
		break;
	case EStage::PARSE:
		UpdateParseStage();
		break;
	}
}

void CRankGhost::OnMapLoad()
{
	// CGhost::OnMapLoad 已先清空全部槽位并重建列表，旧 rank 槽位不再有效。
	OnGhostsUnloaded();
}

void CRankGhost::OnReset()
{
	const bool KeepRetryLoad = m_RetryLoadPending && m_aGhostStoragePath[0] != '\0';
	if(m_Stage != EStage::IDLE)
		AbortTask();
	if(m_LoadedSlot >= 0)
		GameClient()->m_Ghost.Unload(m_LoadedSlot);
	m_StartPending = false;
	m_RetryLoadPending = KeepRetryLoad;
	m_RetryLoadDeadline = KeepRetryLoad ? time_get() + time_freq() * 30 : 0;
	m_RetryLoadNextAttempt = 0;
	// 地图切换/断线会清空 CGhost 的全部槽位，这里同步失效本地记录，
	// 避免之后误卸载别的影子。
	m_LoadedSlot = -1;
	m_LastAlignedRaceTick = -1;
}

void CRankGhost::StartLookup(const char *pMap, int Rank)
{
	if(m_Stage != EStage::IDLE || m_StartPending)
	{
		m_PendingNotify = Localize("Rank ghost: another task is already running");
		return;
	}

	char aMap[64] = "";
	if(pMap != nullptr && pMap[0] != '\0')
		str_copy(aMap, pMap);
	else
		str_copy(aMap, Client()->GetCurrentMap());

	if(aMap[0] == '\0')
	{
		m_PendingNotify = Localize("Rank ghost: specify a map name when not in a game");
		return;
	}

	// 新请求不能继承上一次等待地图加载的状态，否则地图切换后可能尝试加载旧影子。
	m_RetryLoadPending = false;
	m_RetryLoadDeadline = 0;
	m_RetryLoadNextAttempt = 0;
	m_PendingMap = aMap;
	m_PendingRank = std::clamp(Rank, 1, 9999);
	m_StartPending = true;
	log_info("rank_ghost", "lookup queued: map='%s' rank=%d", aMap, m_PendingRank);
}

void CRankGhost::StartManifestFetch()
{
	if(Http() == nullptr)
	{
		Fail(Localize("Rank ghost: HTTP is not available"));
		return;
	}

	m_pManifestRequest = HttpGet(MANIFEST_URL);
	m_pManifestRequest->MaxResponseSize(MAX_MANIFEST_BYTES);
	m_pManifestRequest->Timeout(CTimeout{5000, 30000, 100, 5});
	m_pManifestRequest->LogProgress(HTTPLOG::FAILURE);
	m_pManifestRequest->FailOnErrorStatus(false);
	m_pManifestRequest->HeaderString("User-Agent", USER_AGENT);
	Http()->Run(m_pManifestRequest);

	m_Stage = EStage::FETCH_MANIFEST;
	log_info("rank_ghost", "fetching manifest from '%s'", MANIFEST_URL);
	Echo(Localize("Rank ghost: fetching the replay list ..."));
}

void CRankGhost::UpdateManifestStage()
{
	if(!m_pManifestRequest || !m_pManifestRequest->Done())
		return;

	const bool Ok = m_pManifestRequest->State() == EHttpState::DONE && m_pManifestRequest->StatusCode() == 200;
	unsigned char *pResult = nullptr;
	size_t ResultLength = 0;
	if(Ok)
		m_pManifestRequest->Result(&pResult, &ResultLength);

	if(!Ok || pResult == nullptr || ResultLength == 0)
	{
		m_pManifestRequest = nullptr;
		Fail(Localize("Rank ghost: failed to fetch the replay list"));
		return;
	}

	// 响应缓冲区由请求对象持有，必须等解析完成后再释放请求
	const bool Parsed = ParseManifest(pResult, ResultLength);
	m_pManifestRequest = nullptr;

	if(!Parsed)
	{
		Fail(Localize("Rank ghost: the replay list is empty or malformed"));
		return;
	}

	m_ManifestLoaded = true;
	m_ManifestLoadedAt = time_get();
	m_Stage = EStage::IDLE;
	log_info("rank_ghost", "manifest loaded: %d entries", (int)m_vEntries.size());
	LookupInManifest();
}

bool CRankGhost::ParseManifest(const unsigned char *pData, size_t DataSize)
{
	std::vector<qmclient::rank_demo::SEntry> ParsedEntries;
	if(!qmclient::rank_demo::ParseManifest(pData, DataSize, ParsedEntries))
	{
		m_vEntries.clear();
		return false;
	}

	m_vEntries.clear();
	m_vEntries.reserve(ParsedEntries.size());
	for(const qmclient::rank_demo::SEntry &Parsed : ParsedEntries)
	{
		SEntry Entry;
		Entry.m_Map = Parsed.m_Map;
		Entry.m_Rank = Parsed.m_Rank;
		Entry.m_Time = Parsed.m_Time;
		Entry.m_Demo = Parsed.m_Demo;
		Entry.m_Names = Parsed.m_Names;
		Entry.m_Cid = Parsed.m_Cid;
		Entry.m_Ts = Parsed.m_Ts == std::numeric_limits<int64_t>::min() ? 0 : Parsed.m_Ts;
		m_vEntries.push_back(std::move(Entry));
	}
	return true;
}

const CRankGhost::SEntry *CRankGhost::FindEntry(const char *pMap, int Rank) const
{
	const SEntry *pBest = nullptr;
	for(const SEntry &Entry : m_vEntries)
	{
		if(Entry.m_Rank != Rank || str_comp_nocase(Entry.m_Map.c_str(), pMap) != 0)
			continue;
		// 同一名次可能有历史记录，取最新完成的一条
		if(pBest == nullptr || Entry.m_Ts > pBest->m_Ts)
			pBest = &Entry;
	}
	return pBest;
}

void CRankGhost::LookupInManifest()
{
	const SEntry *pEntry = FindEntry(m_PendingMap.c_str(), m_PendingRank);
	if(pEntry == nullptr)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), Localize("Rank ghost: no official replay for '%s' rank %d"),
			m_PendingMap.c_str(), m_PendingRank);
		Fail(aBuf);
		return;
	}

	m_ActiveEntry = *pEntry;
	log_info("rank_ghost", "entry found: demo='%s' time='%s' cid=%d", m_ActiveEntry.m_Demo.c_str(), m_ActiveEntry.m_Time.c_str(), m_ActiveEntry.m_Cid);

	char aSafeName[IO_MAX_PATH_LENGTH];
	MakeSafeCacheName(m_ActiveEntry.m_Demo.c_str(), aSafeName, sizeof(aSafeName));

	// 影子文件放在 ghosts/rank_ghost 子目录，与玩家自己的影子分开存放；
	// 文件名带地图名前缀，便于文件管理器里辨认。Ghost 页会单独扫描该子目录。
	char aSafeMap[64];
	char aSafeDemo[256];
	str_copy(aSafeMap, m_PendingMap.c_str(), sizeof(aSafeMap));
	str_sanitize_filename(aSafeMap);
	MakeSafeCacheName(m_ActiveEntry.m_Demo.c_str(), aSafeDemo, sizeof(aSafeDemo));
	str_format(m_aDemoStoragePath, sizeof(m_aDemoStoragePath), "%s/%s_%lld.demo", DEMO_CACHE_DIR, aSafeDemo, (long long)m_ActiveEntry.m_Ts);
	str_format(m_aGhostStoragePath, sizeof(m_aGhostStoragePath), "%s/%s/%s_rank%d_%lld_%s.gho", GHOST_ROOT, GHOST_SUBDIR, aSafeMap, m_PendingRank, (long long)m_ActiveEntry.m_Ts, aSafeDemo);

	Storage()->CreateFolder(DEMO_CACHE_DIR, IStorage::TYPE_SAVE);
	char aRankGhostDir[IO_MAX_PATH_LENGTH];
	str_format(aRankGhostDir, sizeof(aRankGhostDir), "%s/%s", GHOST_ROOT, GHOST_SUBDIR);
	Storage()->CreateFolder(aRankGhostDir, IStorage::TYPE_SAVE);

	// 已生成过 ghost：直接加载
	if(Storage()->FileExists(m_aGhostStoragePath, IStorage::TYPE_SAVE))
	{
		if(LoadGhostFile(m_aGhostStoragePath))
		{
			NotifyLoaded(m_ActiveEntry.m_Names.c_str(), m_ActiveEntry.m_Time.c_str());
			return;
		}
		if(Client()->GetCurrentMap()[0] != '\0' && str_comp_nocase(Client()->GetCurrentMap(), m_PendingMap.c_str()) == 0)
		{
			Storage()->RemoveFile(m_aGhostStoragePath, IStorage::TYPE_SAVE);
			if(Storage()->FileExists(m_aDemoStoragePath, IStorage::TYPE_SAVE))
				StartParse();
			else
				StartDemoFetch();
		}
		else
		{
			m_RetryLoadPending = true;
			m_RetryLoadDeadline = time_get() + time_freq() * 30;
			m_RetryLoadNextAttempt = 0;
			m_PendingNotify = Localize("Rank ghost: replay converted, waiting for the map to load ...");
		}
		return;
	}

	// 已下载过回放：直接解析
	if(Storage()->FileExists(m_aDemoStoragePath, IStorage::TYPE_SAVE))
	{
		StartParse();
		return;
	}

	StartDemoFetch();
}

void CRankGhost::OnGhostsUnloaded()
{
	m_LoadedSlot = -1;
	m_LastAlignedRaceTick = -1;
}

void CRankGhost::OnGhostLoaded(const char *pStoragePath, int Slot)
{
	if(Slot < 0 || pStoragePath == nullptr || !str_startswith(pStoragePath, "ghosts/rank_ghost/") || (m_LoadedSlot >= 0 && str_comp(pStoragePath, m_aGhostStoragePath) != 0))
		return;
	m_LoadedSlot = Slot;
	m_LastAlignedRaceTick = -1;
}

void CRankGhost::OnGhostUnloaded(int Slot)
{
	if(m_LoadedSlot == Slot)
	{
		m_LoadedSlot = -1;
		m_LastAlignedRaceTick = -1;
	}
}

void CRankGhost::StartDemoFetch()
{
	if(Http() == nullptr)
	{
		Fail(Localize("Rank ghost: HTTP is not available"));
		return;
	}

	char aUrl[1024];
	str_format(aUrl, sizeof(aUrl), "%s/%s", DEMO_URL_PREFIX, m_ActiveEntry.m_Demo.c_str());

	m_pDemoRequest = HttpGetFile(aUrl, Storage(), m_aDemoStoragePath, IStorage::TYPE_SAVE);
	m_pDemoRequest->MaxResponseSize(MAX_DEMO_BYTES);
	m_pDemoRequest->Timeout(CTimeout{5000, 120000, 200, 10});
	m_pDemoRequest->LogProgress(HTTPLOG::FAILURE);
	m_pDemoRequest->FailOnErrorStatus(false);
	m_pDemoRequest->HeaderString("User-Agent", USER_AGENT);
	Http()->Run(m_pDemoRequest);

	m_Stage = EStage::FETCH_DEMO;
	log_info("rank_ghost", "downloading demo from '%s'", aUrl);
	Echo(Localize("Rank ghost: downloading the replay ..."));
}

void CRankGhost::UpdateDemoStage()
{
	if(!m_pDemoRequest || !m_pDemoRequest->Done())
		return;

	const bool Ok = m_pDemoRequest->State() == EHttpState::DONE && m_pDemoRequest->StatusCode() == 200;
	m_pDemoRequest = nullptr;

	if(!Ok)
	{
		Fail(Localize("Rank ghost: failed to download the replay"));
		return;
	}

	if(!ValidateOrUnpackDemo(Storage(), m_aDemoStoragePath))
	{
		Storage()->RemoveFile(m_aDemoStoragePath, IStorage::TYPE_SAVE);
		Fail(Localize("Rank ghost: the downloaded replay is not a valid demo"));
		return;
	}

	StartParse();
}

void CRankGhost::StartParse()
{
	m_Stage = EStage::IDLE;

	m_pParse = std::make_unique<SParseState>();
	SParseContext &Parse = m_pParse->m_Context;
	Parse.m_Cid = m_ActiveEntry.m_Cid;
	StrToInts(Parse.m_Skin.m_aSkin, std::size(Parse.m_Skin.m_aSkin), "default");
	Parse.m_Skin.m_UseCustomColor = 0;
	Parse.m_Skin.m_ColorBody = 0;
	Parse.m_Skin.m_ColorFeet = 0;

	Parse.m_pPlayer = std::make_unique<CDemoPlayer>(&*Parse.m_pDelta, &*Parse.m_pDeltaSixup, false);

	if(Parse.m_pPlayer->Load(Storage(), nullptr, m_aDemoStoragePath, IStorage::TYPE_SAVE) == -1)
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), Localize("Rank ghost: failed to load the replay: %s"), Parse.m_pPlayer->ErrorMessage());
		m_pParse.reset();
		Storage()->RemoveFile(m_aDemoStoragePath, IStorage::TYPE_SAVE);
		Fail(aBuf);
		return;
	}
	Parse.m_pPlayer->SetListener(this);

	const CDemoPlayer::CPlaybackInfo *pInfo = Parse.m_pPlayer->Info();
	const int NumMarkers = bytes_be_to_uint(pInfo->m_TimelineMarkers.m_aNumTimelineMarkers);
	if(NumMarkers < 2)
	{
		m_pParse.reset();
		Storage()->RemoveFile(m_aDemoStoragePath, IStorage::TYPE_SAVE);
		Fail(Localize("Rank ghost: the replay has no run start/end markers"));
		return;
	}
	Parse.m_RunStart = (int)bytes_be_to_uint(pInfo->m_TimelineMarkers.m_aTimelineMarkers[0]);
	Parse.m_RunEnd = (int)bytes_be_to_uint(pInfo->m_TimelineMarkers.m_aTimelineMarkers[1]);
	if(Parse.m_RunEnd <= Parse.m_RunStart)
	{
		m_pParse.reset();
		Storage()->RemoveFile(m_aDemoStoragePath, IStorage::TYPE_SAVE);
		Fail(Localize("Rank ghost: the replay has an invalid run range"));
		return;
	}

	const CMapInfo *pMapInfo = Parse.m_pPlayer->GetMapInfo();
	str_copy(Parse.m_aMapName, pMapInfo->m_aName);
	if(pMapInfo->m_Sha256.has_value())
		Parse.m_MapSha256 = pMapInfo->m_Sha256.value();
	else
		mem_zero(&Parse.m_MapSha256, sizeof(Parse.m_MapSha256));

	Parse.m_vPath.reserve((size_t)std::clamp(Parse.m_RunEnd - Parse.m_RunStart, 0, 60 * 60 * 50));

	Parse.m_pPlayer->Play();

	m_Stage = EStage::PARSE;
	log_info("rank_ghost", "parsing demo: run ticks [%d, %d], cid=%d", Parse.m_RunStart, Parse.m_RunEnd, Parse.m_Cid);
	Echo(Localize("Rank ghost: converting the replay ..."));
}

void CRankGhost::UpdateParseStage()
{
	if(!m_pParse)
	{
		m_Stage = EStage::IDLE;
		return;
	}

	CDemoPlayer *pPlayer = m_pParse->m_Context.m_pPlayer.get();
	if(!pPlayer)
	{
		m_pParse.reset();
		m_Stage = EStage::IDLE;
		return;
	}

	// 逐 tick 推进（SeekTick(TICK_NEXT) 走轻量路径），并按帧预算限制耗时
	const int64_t Deadline = time_get() + time_freq() * PARSE_TIME_BUDGET_MS / 1000;
	int Processed = 0;
	bool Failed = false;
	while(pPlayer->IsPlaying() && !pPlayer->BaseInfo()->m_Paused)
	{
		if(!pPlayer->SeekTick(IDemoPlayer::TICK_NEXT))
		{
			Failed = true;
			break;
		}
		Processed++;
		if(Processed >= PARSE_TICKS_PER_FRAME)
			break;
		if((Processed & 0xFF) == 0 && time_get() >= Deadline)
			break;
	}

	if(!Failed && pPlayer->IsPlaying() && !pPlayer->BaseInfo()->m_Paused)
		return;

	FinishParse();
}

void CRankGhost::FinishParse()
{
	if(!m_pParse)
	{
		m_Stage = EStage::IDLE;
		return;
	}

	std::unique_ptr<SParseState> pState = std::move(m_pParse);
	SParseContext &Parse = pState->m_Context;
	m_Stage = EStage::IDLE;

	if(Parse.m_pPlayer)
		Parse.m_pPlayer->Stop();

	const int NumTicks = (int)Parse.m_vPath.size();
	const int RunTicks = Parse.m_RunEnd - Parse.m_RunStart;
	const int TimeMs = RunTicks * 1000 / SERVER_TICK_SPEED;
	if(!Parse.m_FoundCharacter || NumTicks < MIN_PATH_TICKS || TimeMs <= 0)
	{
		Storage()->RemoveFile(m_aDemoStoragePath, IStorage::TYPE_SAVE);
		Fail(Localize("Rank ghost: could not extract the runner's path from the replay"));
		return;
	}

	// 按 ghost 文件格式写出，与本地录制 ghost 一致
	const int StartTick = 0;
	char aTempGhostPath[IO_MAX_PATH_LENGTH];
	str_format(aTempGhostPath, sizeof(aTempGhostPath), "%s.tmp", m_aGhostStoragePath);
	Storage()->RemoveFile(aTempGhostPath, IStorage::TYPE_SAVE);
	CGhostFileWriter TempWriter;
	if(!TempWriter.Open(Storage(), aTempGhostPath, Parse.m_aMapName, Parse.m_MapSha256, Parse.m_aOwner, NumTicks, TimeMs))
	{
		Fail(Localize("Rank ghost: failed to write the ghost file"));
		return;
	}
	bool WriteOk = TempWriter.WriteData(GHOSTDATA_TYPE_START_TICK, &StartTick, sizeof(int));
	WriteOk = WriteOk && TempWriter.WriteData(GHOSTDATA_TYPE_SKIN, &Parse.m_Skin, sizeof(CGhostSkin));
	for(const CGhostCharacter &Char : Parse.m_vPath)
		WriteOk = WriteOk && TempWriter.WriteData(GHOSTDATA_TYPE_CHARACTER, &Char, sizeof(CGhostCharacter));
	WriteOk = TempWriter.Close() && WriteOk;
	if(!WriteOk || !Storage()->RenameFile(aTempGhostPath, m_aGhostStoragePath, IStorage::TYPE_SAVE))
	{
		Storage()->RemoveFile(aTempGhostPath, IStorage::TYPE_SAVE);
		Fail(Localize("Rank ghost: failed to write the ghost file"));
		return;
	}

	if(!LoadGhostFile(m_aGhostStoragePath))
	{
		log_error("rank_ghost", "ghost written to '%s' (%d ticks) but loading failed (no map loaded?)", m_aGhostStoragePath, NumTicks);
		Fail(Localize("Rank ghost: failed to load the generated ghost (join the map first)"));
		return;
	}
	log_info("rank_ghost", "ghost loaded: %d ticks, %d ms", NumTicks, TimeMs);

	char aName[MAX_NAME_LENGTH];
	str_copy(aName, Parse.m_aOwner);
	if(aName[0] == '\0')
		str_copy(aName, m_ActiveEntry.m_Names.c_str());
	char aTime[32];
	str_format(aTime, sizeof(aTime), "%.2f", TimeMs / 1000.0f);
	NotifyLoaded(aName, aTime);
}

bool CRankGhost::LoadGhostFile(const char *pStoragePath)
{
	UnloadGhost();
	const int Slot = GameClient()->m_Ghost.Load(pStoragePath);
	if(Slot < 0)
		return false;
	m_LoadedSlot = Slot;
	// 新加载的影子需要在之后的帧里按玩家当前进度重新对齐
	m_LastAlignedRaceTick = -1;
	RefreshGhostList();
	return true;
}

void CRankGhost::UnloadGhost()
{
	m_LastAlignedRaceTick = -1;
	if(m_LoadedSlot < 0)
	{
		return;
	}
	GameClient()->m_Ghost.Unload(m_LoadedSlot);
	m_LoadedSlot = -1;
}

// 刷新 Ghost 页列表，并把我们占用的槽位写回列表项，
// 否则玩家打开 Ghost 页会看到影子文件显示为“未激活”。
void CRankGhost::RefreshGhostList()
{
	GameClient()->m_Menus.GhostlistPopulate();
	for(CMenus::CGhostItem &Item : GameClient()->m_Menus.m_vGhosts)
	{
		if(str_comp(Item.m_aFilename, m_aGhostStoragePath) == 0)
		{
			Item.m_Slot = m_LoadedSlot;
			break;
		}
	}
}

// CGhost 只在玩家从起点线前跨到线后的瞬间开始播放影子。
// 如果影子加载时玩家已经在跑图（或开始新一轮），这里主动把影子的播放起点
// 对齐到当前 run 的开始 tick，让影子立刻按当前进度出现在画面里。
void CRankGhost::AlignToCurrentRun()
{
	if(m_LoadedSlot < 0 || Client()->State() != IClient::STATE_ONLINE)
	{
		m_LastAlignedRaceTick = -1;
		return;
	}
	if(!GameClient()->m_GameInfo.m_Race || !GameClient()->m_Snap.m_pGameInfoObj || GameClient()->m_Snap.m_SpecInfo.m_Active)
	{
		m_LastAlignedRaceTick = -1;
		return;
	}

	const int RaceTick = GameClient()->LastRaceTick();
	if(RaceTick < 0)
	{
		// 还没出发：等玩家跨过起点线时由 CGhost 自行开始
		m_LastAlignedRaceTick = -1;
		return;
	}
	if(RaceTick == m_LastAlignedRaceTick)
		return;

	GameClient()->m_Ghost.StartRender(RaceTick);
	m_LastAlignedRaceTick = RaceTick;
}

void CRankGhost::NotifyLoaded(const char *pOwner, const char *pTimeText)
{
	char aBuf[320];
	if(Client()->State() == IClient::STATE_ONLINE && GameClient()->LastRaceTick() >= 0)
	{
		// 玩家已在跑图中：影子会立即按当前进度对齐播放
		str_format(aBuf, sizeof(aBuf), Localize("Rank ghost loaded: %s, %s s. It follows your current run."),
			pOwner, pTimeText);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), Localize("Rank ghost loaded: %s, %s s. It plays when you cross the start line."),
			pOwner, pTimeText);
	}
	Echo(aBuf);
}

void CRankGhost::OnDemoPlayerSnapshot(void *pData, int Size)
{
	(void)Size;
	if(!m_pParse || !m_pParse->m_Context.m_pPlayer)
		return;

	SParseContext &Parse = m_pParse->m_Context;
	const CSnapshot *pSnapshot = (const CSnapshot *)pData;
	const int Tick = Parse.m_pPlayer->Info()->m_Info.m_CurrentTick;

	// 皮肤/名字可能在 run 开始前就已出现，尽早抓取
	ExtractClientInfoOnce(Parse, pSnapshot);

	if(Tick < Parse.m_RunStart)
		return;
	if(Tick > Parse.m_RunEnd)
	{
		Parse.m_pPlayer->Pause();
		return;
	}

	ExtractCharacter(Parse, pSnapshot, Tick);
}

void CRankGhost::OnDemoPlayerMessage(void *pData, int Size)
{
	// 轨迹全部来自 snapshot，消息流无需处理
	(void)pData;
	(void)Size;
}
