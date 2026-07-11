#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_CLOCK_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_CLOCK_H

#include <cstdint>

namespace QmLyrics
{
	struct SPlaybackSnapshot
	{
		int64_t m_PositionMs = 0;
		int64_t m_PositionUpdatedTick = 0;
		uint64_t m_TimelineGeneration = 0;
		double m_PlaybackRate = 1.0;
		bool m_Playing = false;
		bool m_IdentityChanged = false;
	};

	// 时钟插值器：用完整 SMTC 快照校准，再按本地单调时钟推算当前播放毫秒。
	//
	// 抗漂移策略：本地时间正常推进；SMTC 重新锚定时如果与当前推算位置小幅偏离，
	// 用 correction 逐步收敛；差值 > DriftCorrectMs 时硬切。
	//
	// timeline generation 与 playback state 分开处理，避免 stale timeline 在恢复时
	// 计入暂停区间。单元测试用 fake clock 注入 tick，避免依赖真实 time_get()。
	class CClockInterpolator
	{
	public:
		CClockInterpolator();

		void Update(const SPlaybackSnapshot &Snapshot, int64_t ObservedWallTick, int64_t TickFreq);

		// 按当前 WallTick 推算当前播放毫秒并应用平滑漂移。
		// TickFreq 是 time_freq() 返回值。
		// 多次调用同一 (NowWallTick, TickFreq) 必须返回同样结果。
		int64_t Now(int64_t NowWallTick, int64_t TickFreq) const;

		// 偏移设置（来自 qm_lyrics_offset_ms 用户调节）。
		void SetOffsetMs(int64_t OffsetMs) { m_OffsetMs = OffsetMs; }

		// 漂移硬切阈值（actual 与 target 差 > 此值时瞬切）。
		void SetDriftCorrectMs(int64_t DriftCorrectMs) { m_DriftCorrectMs = DriftCorrectMs; }

		// 重置。
		void Reset();

	private:
		void Anchor(int64_t AnchorPositionMs, int64_t AnchorWallTick, double Rate, bool ForceSnap);
		void SetPlaying(bool Playing, int64_t NowWallTick, bool PreserveAnchorTick = false);
		int64_t PositionAt(int64_t NowWallTick, int64_t TickFreq) const;

		int64_t m_AnchorPositionMs = 0;
		int64_t m_AnchorWallTick = 0;
		double m_Rate = 1.0;
		bool m_Playing = false;
		int64_t m_OffsetMs = 0;
		int64_t m_DriftCorrectMs = 1000;

		int64_t m_CorrectionMs = 0;
		int64_t m_CorrectionStartTick = 0;
		int64_t m_LastTickFreq = 0;
		uint64_t m_LastTimelineGeneration = 0;
		double m_LastSnapshotPlaybackRate = 1.0;
		bool m_HasState = false;
		bool m_HasPlaybackSnapshot = false;
		bool m_LastSnapshotPlaying = false;
	};

} // namespace QmLyrics

#endif
