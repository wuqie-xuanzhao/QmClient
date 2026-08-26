// 网易云音乐 Hook 与 QmClient 之间的固定布局协议。
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_NETEASE_HOOK_QM_NETEASE_HOOK_PROTOCOL_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_NETEASE_HOOK_QM_NETEASE_HOOK_PROTOCOL_H

#include <cstddef>
#include <cstdint>

namespace QmNeteaseHook
{
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
