#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_CLOCK_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_CLOCK_H

#include <cstdint>

namespace QmLyrics
{

	bool ShouldUpdateTimelineAnchor(int64_t LastPositionMs, double LastPlaybackRate, int64_t PositionMs, double PlaybackRate, bool IdentityChanged);

	// 时钟插值器：根据 SMTC 上报的 (AnchorPositionMs, AnchorWallTick) 与本地时钟
	// 推算当前播放毫秒。
	//
	// 抗漂移策略：本地时间正常推进；SMTC 重新锚定时如果与当前推算位置小幅偏离，
	// 用 correction 逐步收敛；差值 > DriftCorrectMs 时硬切。
	//
	// 单元测试用 fake clock 注入 NowTick + TickFreq，避免依赖真实 time_get()。
	class CClockInterpolator
	{
	public:
		CClockInterpolator();

		// SMTC 报告一次新 position：AnchorPositionMs 是 SMTC 给的播放秒×1000。
		// AnchorWallTick 是 SMTC 报告时的本地时钟 tick（由调用方采样）。
		// Rate 是播放速率（1.0 = 正常，0.0 = 暂停）。
		void Anchor(int64_t AnchorPositionMs, int64_t AnchorWallTick, double Rate);

		// 播放/暂停状态切换。暂停时 Now 不推进。
		void SetPlaying(bool Playing, int64_t NowWallTick);

		// 按当前 WallTick 推算当前播放毫秒并应用平滑漂移。
		// TickFreq 是 time_freq() 返回值。
		// 多次调用同一 (NowWallTick, TickFreq) 必须返回同样结果。
		int64_t Now(int64_t NowWallTick, int64_t TickFreq);

		// 偏移设置（来自 qm_lyrics_offset_ms 用户调节）。
		void SetOffsetMs(int64_t OffsetMs) { m_OffsetMs = OffsetMs; }

		// 漂移硬切阈值（actual 与 target 差 > 此值时瞬切）。
		void SetDriftCorrectMs(int64_t DriftCorrectMs) { m_DriftCorrectMs = DriftCorrectMs; }

		// 重置。
		void Reset();

		// 测试钩子。
		int64_t LastTargetMsForTests() const { return m_LastTargetMs; }
		int64_t LastActualMsForTests() const { return m_LastActualMs; }

	private:
		int64_t m_AnchorPositionMs = 0;
		int64_t m_AnchorWallTick = 0;
		double m_Rate = 1.0;
		bool m_Playing = false;
		int64_t m_OffsetMs = 0;
		int64_t m_DriftCorrectMs = 1000;

		// 平滑状态
		int64_t m_LastActualMs = 0;
		int64_t m_LastTargetMs = 0;
		int64_t m_CorrectionMs = 0;
		int64_t m_LastNowWallTick = 0;
		int64_t m_LastTickFreq = 0;
		bool m_HasState = false;
		bool m_HasLastNow = false;
	};

} // namespace QmLyrics

#endif
