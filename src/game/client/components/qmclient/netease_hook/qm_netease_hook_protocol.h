// 网易云音乐 Hook 与 QmClient 之间的固定布局协议。
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_NETEASE_HOOK_QM_NETEASE_HOOK_PROTOCOL_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_NETEASE_HOOK_QM_NETEASE_HOOK_PROTOCOL_H

#include <cstddef>
#include <cstdint>

namespace QmNeteaseHook
{
	// v4 仅供迁移期 DesktopLyrics 兼容路径使用。新桥接必须使用 v5。
	constexpr uint16_t PROTOCOL_SCHEMA_VERSION_V4 = 2;
	constexpr uint16_t PROTOCOL_SCHEMA_VERSION_V5 = 5;
	constexpr char PROTOCOL_MAPPING_NAME_V5[] = "Local\\QmClient.NeteaseHook.v5";
	constexpr wchar_t PROTOCOL_MAPPING_NAME_V5_W[] = L"Local\\QmClient.NeteaseHook.v5";
	constexpr wchar_t PROTOCOL_WRITER_MUTEX_NAME_V5_W[] = L"Local\\QmClient.NeteaseHook.v5.Writer";
	constexpr uint32_t V5_MAX_LYRIC_BYTES = 1024;
	constexpr uint32_t V5_KNOWN_FLAGS = 0x1FU;

	constexpr uint32_t PROTOCOL_MAGIC = 0x514D4E48; // "Q MNH"
	constexpr uint16_t PROTOCOL_SCHEMA_VERSION = 2;
	constexpr uint32_t TARGET_BUILD = 205322;
	constexpr uint32_t TARGET_PATCH = 0x2E74C96;
	constexpr uint32_t MAX_TEXT_BYTES = 256;
	constexpr uint32_t MAX_ID_BYTES = 128;
	constexpr uint32_t MAX_COVER_PATH_BYTES = 1024;
	constexpr uint32_t MAX_COVER_URL_BYTES = 2048;
	constexpr uint32_t STATUS_HAS_MEDIA = 1 << 0;
	constexpr uint32_t STATUS_PLAYING = 1 << 1;
	constexpr uint32_t STATUS_HAS_CURRENT_LINE = 1 << 2;
	constexpr uint32_t STATUS_SOURCE_DIRECT = 1 << 3;
	constexpr uint32_t STATUS_HAS_COVER = 1 << 4;
	// 共享控制字段：客户端请求停止后，DLL 完成 Hook 清理再确认。
	constexpr uint32_t CONTROL_STOP_REQUESTED = 1 << 0;
	constexpr uint32_t CONTROL_STOP_ACKNOWLEDGED = 1 << 1;

	// 该结构只含固定宽度字段，禁止加入指针、std::string 或平台句柄。
	struct SSnapshot
	{
		uint32_t m_Magic = PROTOCOL_MAGIC;
		uint16_t m_SchemaVersion = PROTOCOL_SCHEMA_VERSION;
		uint16_t m_SnapshotSize = 0;
		uint64_t m_Sequence = 0;
		uint32_t m_ProducerPid = 0;
		uint32_t m_PlayerBuild = TARGET_BUILD;
		uint32_t m_PlayerPatch = TARGET_PATCH;
		uint32_t m_Status = 0;
		uint64_t m_SongId = 0;
		char m_aSongId[MAX_ID_BYTES] = {};
		char m_aTitle[MAX_TEXT_BYTES] = {};
		char m_aArtist[MAX_TEXT_BYTES] = {};
		char m_aAlbum[MAX_TEXT_BYTES] = {};
		char m_aCoverPath[MAX_COVER_PATH_BYTES] = {};
		char m_aCoverUrl[MAX_COVER_URL_BYTES] = {};
		int64_t m_DurationMs = 0;
		int64_t m_PositionMs = 0;
		double m_PlaybackRate = 1.0;
		char m_aCurrentLine[MAX_TEXT_BYTES] = {};
		int64_t m_CurrentLineStartMs = -1;
		int64_t m_CurrentLineEndMs = -1;
		uint64_t m_ObservedQpc = 0;
		uint64_t m_TimelineGeneration = 0;
		uint32_t m_Checksum = 0;
		uint32_t m_Reserved = 0;
	};

	static_assert(sizeof(SSnapshot) % 8 == 0, "snapshot must stay naturally aligned");

	// 共享内存采用双槽位：写入非活动槽位后，再用一个 64 位序列号切换活动槽位。
	// 该结构只用于 Windows 进程间共享，不能加入 C++ 原子或带析构的成员。
	struct SSharedBlock
	{
		volatile int64_t m_ActiveSequence = 0;
		volatile uint32_t m_ControlFlags = 0;
		uint32_t m_Reserved = 0;
		SSnapshot m_aSnapshots[2]{};
	};

	static_assert(offsetof(SSharedBlock, m_aSnapshots) % 8 == 0, "shared snapshots must stay naturally aligned");

	// 网易云私有 Bridge 的 v5 快照。标准媒体状态（播放状态、标题、艺术家、专辑、封面
	// 以及普通时间轴）仍由 Windows SMTC 提供，不能从这些字段反推。
	enum class ENeteaseLyricSource : uint32_t
	{
		None = 0,
		Frontend = 1,
		InternalApi = 2,
		DesktopLyricsFallback = 3,
	};

	constexpr uint32_t V5_FLAG_HAS_SONG = 1U << 0;
	constexpr uint32_t V5_FLAG_LYRIC_VALID = 1U << 1;
	constexpr uint32_t V5_FLAG_POSITION_VALID = 1U << 2;
	constexpr uint32_t V5_FLAG_PLAYING_HINT = 1U << 3;
	constexpr uint32_t V5_FLAG_POSITION_ANCHORED = 1U << 4;

// v5 使用自然 8 字节对齐，x86/x64 的布局一致；共享内存中不放指针、句柄或 STL 对象。
#pragma pack(push, 8)
	struct SSnapshotV5
	{
		uint32_t m_Magic = PROTOCOL_MAGIC;
		uint16_t m_SchemaVersion = PROTOCOL_SCHEMA_VERSION_V5;
		uint16_t m_SnapshotSize = 0;
		uint64_t m_Sequence = 0;
		uint32_t m_CloudMusicPid = 0;
		uint32_t m_Flags = 0;
		uint64_t m_SongId = 0;
		uint64_t m_Generation = 0;
		uint32_t m_LyricSource = (uint32_t)ENeteaseLyricSource::None;
		uint32_t m_Reserved = 0;
		int64_t m_PositionMs = 0;
		int64_t m_LineStartMs = -1;
		int64_t m_LineEndMs = -1;
		uint64_t m_UpdatedAtTick = 0;
		char m_aCurrentLyric[V5_MAX_LYRIC_BYTES] = {};
		uint32_t m_Checksum = 0;
		uint32_t m_ReservedTail = 0;
	};

	struct SSharedBlockV5
	{
		// Writer seqlock: odd while payload is being written, even when stable.
		volatile uint64_t m_Sequence = 0;
		SSnapshotV5 m_Snapshot{};
	};
#pragma pack(pop)

	static_assert(offsetof(SSnapshotV5, m_Sequence) == 8, "v5 sequence offset changed");
	static_assert(offsetof(SSnapshotV5, m_SongId) % 8 == 0, "v5 song id must be aligned");
	static_assert(offsetof(SSharedBlockV5, m_Snapshot) % 8 == 0, "v5 payload must be aligned");
	static_assert(sizeof(SSnapshotV5) % 8 == 0, "v5 snapshot must stay naturally aligned");
	static_assert(sizeof(SSnapshotV5) == 1112, "v5 snapshot ABI changed");
	static_assert(sizeof(SSharedBlockV5) == 1120, "v5 shared block ABI changed");

	uint32_t CalculateChecksumV5(const SSnapshotV5 &Snapshot);
	void FinalizeSnapshotV5(SSnapshotV5 *pSnapshot);
	bool ValidateSnapshotV5(const SSnapshotV5 &Snapshot);
	bool IsStableSequenceV5(uint64_t BeginSequence, uint64_t EndSequence);
	bool IsStaleV5(const SSnapshotV5 &Snapshot, uint64_t NowTick, uint64_t TimeoutMs);
	bool IsValidLyricSource(uint32_t Source);
	// Helper 暂时没有高优先级结果时保留同进程、同歌曲且仍新鲜的 GDI fallback。
	bool ShouldPreserveDesktopFallbackV5(const SSnapshotV5 &Existing, const SSnapshotV5 &Candidate, uint64_t NowTick, uint64_t TimeoutMs);
	// GDI fallback 不能覆盖 Helper 已确认的其它歌曲身份。
	bool CanPublishDesktopFallbackV5(const SSnapshotV5 &Existing, const SSnapshotV5 &Fallback, uint64_t NowTick, uint64_t TimeoutMs);
	// UTF-8 字符串复制：只在完整 codepoint 边界截断，并保证 NUL 结尾。
	size_t CopyUtf8Truncated(char *pDestination, size_t DestinationSize, const char *pSource, size_t SourceSize);

	uint32_t Crc32(const void *pData, size_t Size);
	uint32_t CalculateChecksum(const SSnapshot &Snapshot);
	void FinalizeSnapshot(SSnapshot *pSnapshot);
	bool ValidateSnapshot(const SSnapshot &Snapshot);
	bool HasMedia(const SSnapshot &Snapshot);
	bool HasCurrentLine(const SSnapshot &Snapshot);
	bool HasCover(const SSnapshot &Snapshot);

	// 读取共享内存时，先后两次序列号必须相同且为偶数。
	bool IsStableSequence(uint64_t BeginSequence, uint64_t EndSequence);

} // namespace QmNeteaseHook

#endif
