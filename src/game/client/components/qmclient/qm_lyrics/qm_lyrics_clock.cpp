#include "qm_lyrics_clock.h"

#include <algorithm>
#include <cmath>

namespace QmLyrics
{
	namespace
	{

		constexpr double CORRECTION_TIME_CONSTANT_MS = 300.0;

		int64_t CorrectionAt(int64_t Correction, int64_t CorrectionStartTick, int64_t NowWallTick, int64_t TickFreq)
		{
			if(Correction == 0 || TickFreq <= 0 || NowWallTick <= CorrectionStartTick)
				return Correction;
			const double ElapsedMs = (double)(NowWallTick - CorrectionStartTick) * 1000.0 / (double)TickFreq;
			const double Decayed = (double)Correction * std::exp(-ElapsedMs / CORRECTION_TIME_CONSTANT_MS);
			return std::abs(Decayed) < 0.5 ? 0 : (int64_t)std::llround(Decayed);
		}

	} // anonymous namespace

	CClockInterpolator::CClockInterpolator() = default;

	void CClockInterpolator::Reset()
	{
		m_AnchorPositionMs = 0;
		m_AnchorWallTick = 0;
		m_Rate = 1.0;
		m_Playing = false;
		m_CorrectionMs = 0;
		m_CorrectionStartTick = 0;
		m_LastTickFreq = 0;
		m_LastTimelineGeneration = 0;
		m_LastSnapshotPlaybackRate = 1.0;
		m_HasState = false;
		m_HasPlaybackSnapshot = false;
		m_LastSnapshotPlaying = false;
	}

	void CClockInterpolator::Anchor(int64_t AnchorPositionMs, int64_t AnchorWallTick, double Rate, bool ForceSnap)
	{
		const bool HadState = m_HasState;
		const int64_t OldDisplayedMs = HadState ? PositionAt(AnchorWallTick, m_LastTickFreq) : AnchorPositionMs + m_OffsetMs;

		m_AnchorPositionMs = AnchorPositionMs;
		m_AnchorWallTick = AnchorWallTick;
		m_Rate = Rate;
		m_CorrectionMs = 0;
		m_CorrectionStartTick = AnchorWallTick;
		m_HasState = true;

		if(HadState && !ForceSnap)
		{
			const int64_t NewTargetMs = m_AnchorPositionMs + m_OffsetMs;
			const int64_t Diff = std::abs(NewTargetMs - OldDisplayedMs);
			if(Diff <= m_DriftCorrectMs)
			{
				m_CorrectionMs = OldDisplayedMs - NewTargetMs;
				m_CorrectionStartTick = AnchorWallTick;
			}
		}
	}

	void CClockInterpolator::Update(const SPlaybackSnapshot &Snapshot, int64_t ObservedWallTick, int64_t TickFreq)
	{
		m_LastTickFreq = TickFreq;
		const bool TimelineChanged = !m_HasPlaybackSnapshot || Snapshot.m_TimelineGeneration != m_LastTimelineGeneration;
		const bool PlayingChanged = m_HasPlaybackSnapshot && Snapshot.m_Playing != m_LastSnapshotPlaying;
		const double PlaybackRate = std::isfinite(Snapshot.m_PlaybackRate) ? std::max(0.0, Snapshot.m_PlaybackRate) : 1.0;
		const bool PlaybackRateChanged = m_HasPlaybackSnapshot && std::abs(PlaybackRate - m_LastSnapshotPlaybackRate) > 0.001;
		const bool HasTimelineSample = Snapshot.m_TimelineGeneration != 0 && Snapshot.m_PositionUpdatedTick <= ObservedWallTick;
		const int64_t PositionUpdatedTick = HasTimelineSample ? Snapshot.m_PositionUpdatedTick : ObservedWallTick;

		if(Snapshot.m_IdentityChanged || TimelineChanged)
		{
			Anchor(Snapshot.m_PositionMs, PositionUpdatedTick, PlaybackRate, Snapshot.m_IdentityChanged || PlayingChanged || !Snapshot.m_Playing);
			SetPlaying(Snapshot.m_Playing, ObservedWallTick, true);
		}
		else if(PlayingChanged)
		{
			SetPlaying(Snapshot.m_Playing, ObservedWallTick);
			m_Rate = PlaybackRate;
		}
		else if(PlaybackRateChanged)
		{
			const int64_t CurrentPositionMs = Now(ObservedWallTick, TickFreq) - m_OffsetMs;
			Anchor(CurrentPositionMs, ObservedWallTick, PlaybackRate, true);
		}

		m_LastTimelineGeneration = Snapshot.m_TimelineGeneration;
		m_LastSnapshotPlaybackRate = PlaybackRate;
		m_LastSnapshotPlaying = Snapshot.m_Playing;
		m_HasPlaybackSnapshot = true;
	}

	void CClockInterpolator::SetPlaying(bool Playing, int64_t NowWallTick, bool PreserveAnchorTick)
	{
		if(Playing == m_Playing)
			return;
		if(Playing)
		{
			if(!PreserveAnchorTick)
			{
				m_AnchorPositionMs = PositionAt(NowWallTick, m_LastTickFreq) - m_OffsetMs;
				m_AnchorWallTick = NowWallTick;
				m_CorrectionMs = 0;
				m_CorrectionStartTick = NowWallTick;
			}
		}
		else
		{
			const int64_t CurrentMs = PreserveAnchorTick ?
							  m_AnchorPositionMs + m_OffsetMs + CorrectionAt(m_CorrectionMs, m_CorrectionStartTick, NowWallTick, m_LastTickFreq) :
							  PositionAt(NowWallTick, m_LastTickFreq);
			m_AnchorPositionMs = CurrentMs - m_OffsetMs;
			m_AnchorWallTick = NowWallTick;
			m_CorrectionMs = 0;
			m_CorrectionStartTick = NowWallTick;
		}
		m_Playing = Playing;
	}

	int64_t CClockInterpolator::PositionAt(int64_t NowWallTick, int64_t TickFreq) const
	{
		int64_t TargetMs = m_AnchorPositionMs;
		if(m_Playing && TickFreq > 0)
		{
			const int64_t DeltaTicks = std::max<int64_t>(0, NowWallTick - m_AnchorWallTick);
			const int64_t DeltaMs = (int64_t)((double)DeltaTicks * 1000.0 * m_Rate / (double)TickFreq);
			TargetMs += DeltaMs;
		}
		TargetMs += m_OffsetMs;
		return TargetMs + CorrectionAt(m_CorrectionMs, m_CorrectionStartTick, NowWallTick, TickFreq);
	}

	int64_t CClockInterpolator::Now(int64_t NowWallTick, int64_t TickFreq) const
	{
		return PositionAt(NowWallTick, TickFreq);
	}

} // namespace QmLyrics
