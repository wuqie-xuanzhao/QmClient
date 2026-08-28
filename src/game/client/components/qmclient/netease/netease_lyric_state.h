#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_NETEASE_NETEASE_LYRIC_STATE_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_NETEASE_NETEASE_LYRIC_STATE_H

#include "netease_lyric_parser.h"
#include "netease_lyric_timeline.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace NeteaseLyrics
{
	enum class ESource : uint32_t
	{
		None = 0,
		Frontend = 1,
		DesktopLyricsFallback = 2,
	};

	int SourcePriority(ESource Source);
	bool CanReplaceSource(ESource Existing, ESource Candidate, bool ExistingValid);

	enum class ELyricSongDecision
	{
		Reject,
		ApplyCurrent,
		SwitchSong,
		Defer,
	};

	// progress 在短时间内是歌曲身份的权威来源。异歌歌词先延迟，避免迟到的
	// onProcessLyrics 把当前 songId 回拨；progress 长时间缺失时歌词仍可接管。
	ELyricSongDecision DecideLyricSongReport(uint64_t CurrentSongId, bool HasCurrentSong, uint64_t ReportSongId, uint64_t LastProgressTick, uint64_t NowTick, uint64_t ProgressFreshMs);
	bool IsMeaningfulMediaIdentityChange(std::string_view PreviousTitle, std::string_view PreviousArtist, std::string_view CurrentTitle, std::string_view CurrentArtist);
	bool IsBridgeIdentityStillBlocked(uint64_t BlockedSongId, uint64_t BlockedGeneration, uint64_t CandidateSongId, uint64_t CandidateGeneration);
	// Bridge 暂时不可读时，暂停中的当前句只保留一个有限窗口；超过窗口
	// 必须清理，避免 Helper 崩溃后无限显示旧歌词。
	bool ShouldPreservePausedLyric(bool Paused, bool HasSong, bool LyricValid, uint64_t LastBridgeTick, uint64_t NowTick, uint64_t GraceMs);
	// 暂停时只有进度锚点真正改变才允许新快照替换当前句；普通 heartbeat
	// 不应把暂停时已经选中的句子重新清掉或推进。
	bool HasPausedLyricSeek(bool Paused, bool PositionValid, bool PositionAnchored, int64_t PositionMs, bool LastPositionValid, int64_t LastPositionMs);
	// Helper 无法从前端报告确认播放状态时，不把缺少字段误判为 Playing。
	// 连接仍然有效且有同步锚点时保留结果，断连/明确播放超时才清理。
	bool ShouldPreserveConnectedLyric(bool WorkerConnected, bool PlayingHintKnown, bool PlayingHint, bool PositionValid, bool HasLyricData);

	struct SCurrentState
	{
		uint64_t m_SongId = 0;
		uint64_t m_Generation = 0;
		ESource m_Source = ESource::None;
		int64_t m_PositionMs = 0;
		int64_t m_LineStartMs = -1;
		int64_t m_LineEndMs = -1;
		bool m_HasSong = false;
		bool m_LyricValid = false;
		std::string m_CurrentLyric;
	};

	// 网易云歌词状态机：切歌先清空，再接受新来源；低优先级来源不能覆盖高优先级结果。
	class CLyricState
	{
	public:
		void Reset();
		bool UpdateSong(uint64_t SongId);
		// 将共享 ABI 的 generation 镜像到客户端状态；0 表示未知，保留本地值。
		void AdoptGeneration(uint64_t Generation);
		void ClearLyrics();
		// 接收 helper 已经根据完整时间轴选出的当前句。客户端不解析 raw
		// LRC/YRC，只保存展示所需的单行和真实边界。
		bool ApplyCurrentLine(ESource Source, std::string_view Text, int64_t LineStartMs, int64_t LineEndMs, int64_t PositionMs);
		bool ApplyTimeline(ESource Source, const STimeline &Timeline, int64_t PositionMs);
		void UpdatePosition(int64_t PositionMs);
		void MarkStopped();
		const SCurrentState &Snapshot() const { return m_State; }

	private:
		SCurrentState m_State{};
		STimeline m_Timeline;
		ESource m_TimelineSource = ESource::None;
	};

} // namespace NeteaseLyrics

namespace QmNetease
{
	using NeteaseLyrics::CanReplaceSource;
	using NeteaseLyrics::CLyricState;
	using NeteaseLyrics::DecideLyricSongReport;
	using NeteaseLyrics::ELyricSongDecision;
	using NeteaseLyrics::ESource;
	using NeteaseLyrics::HasPausedLyricSeek;
	using NeteaseLyrics::IsBridgeIdentityStillBlocked;
	using NeteaseLyrics::IsMeaningfulMediaIdentityChange;
	using NeteaseLyrics::SCurrentState;
	using NeteaseLyrics::ShouldPreserveConnectedLyric;
	using NeteaseLyrics::ShouldPreservePausedLyric;
	using NeteaseLyrics::SourcePriority;
}

#endif
