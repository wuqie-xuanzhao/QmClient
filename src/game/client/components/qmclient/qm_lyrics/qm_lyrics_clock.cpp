#include "qm_lyrics_clock.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace QmLyrics
{
	namespace
	{

		int64_t ApproachCorrection(int64_t Correction)
		{
			if(std::abs(Correction) <= 1)
				return 0;
			return Correction - Correction / 5;
		}

	} // anonymous namespace

	CClockInterpolator::CClockInterpolator() = default;

	bool ShouldUpdateTimelineAnchor(int64_t LastPositionMs, double LastPlaybackRate, int64_t PositionMs, double PlaybackRate, bool IdentityChanged)
	{
		if(IdentityChanged || LastPositionMs < 0)
			return true;
		if(PositionMs != LastPositionMs)
			return true;
		return std::abs(PlaybackRate - LastPlaybackRate) > 0.001;
	}

	void CClockInterpolator::Reset()
	{
		m_AnchorPositionMs = 0;
		m_AnchorWallTick = 0;
		m_Rate = 1.0;
		m_Playing = false;
		m_LastActualMs = 0;
		m_LastTargetMs = 0;
		m_CorrectionMs = 0;
		m_LastNowWallTick = 0;
		m_LastTickFreq = 0;
		m_HasState = false;
		m_HasLastNow = false;
	}

	void CClockInterpolator::Anchor(int64_t AnchorPositionMs, int64_t AnchorWallTick, double Rate)
	{
		int64_t OldDisplayedMs = m_LastActualMs;
		if(m_HasState && m_LastTickFreq > 0)
		{
			OldDisplayedMs = m_AnchorPositionMs + m_OffsetMs;
			if(m_Playing)
			{
				const int64_t DeltaTicks = AnchorWallTick - m_AnchorWallTick;
				const int64_t DeltaMs = (int64_t)((double)DeltaTicks * 1000.0 * m_Rate / (double)m_LastTickFreq);
				OldDisplayedMs += DeltaMs;
			}
			OldDisplayedMs += m_CorrectionMs;
		}

		m_AnchorPositionMs = AnchorPositionMs;
		m_AnchorWallTick = AnchorWallTick;
		m_Rate = Rate;
		m_CorrectionMs = 0;
		m_HasLastNow = false;

		if(m_HasState)
		{
			const int64_t NewTargetMs = m_AnchorPositionMs + m_OffsetMs;
			const int64_t Diff = std::abs(NewTargetMs - OldDisplayedMs);
			if(Diff > m_DriftCorrectMs)
			{
				m_LastActualMs = NewTargetMs;
			}
			else
			{
				m_CorrectionMs = OldDisplayedMs - NewTargetMs;
			}
			m_LastTargetMs = NewTargetMs;
		}
	}

	void CClockInterpolator::SetPlaying(bool Playing, int64_t NowWallTick)
	{
		if(Playing == m_Playing)
			return;
		if(Playing)
		{
			// 从暂停恢复：刷新 anchor 到当前时刻，position 不变。
			m_AnchorWallTick = NowWallTick;
		}
		else
		{
			if(m_LastTickFreq > 0)
			{
				int64_t CurrentMs = m_AnchorPositionMs + m_OffsetMs;
				if(m_Playing)
				{
					const int64_t DeltaTicks = NowWallTick - m_AnchorWallTick;
					const int64_t DeltaMs = (int64_t)((double)DeltaTicks * 1000.0 * m_Rate / (double)m_LastTickFreq);
					CurrentMs += DeltaMs;
				}
				CurrentMs += m_CorrectionMs;
				m_AnchorPositionMs = CurrentMs - m_OffsetMs;
			}
			m_AnchorWallTick = NowWallTick;
			m_CorrectionMs = 0;
		}
		m_Playing = Playing;
		m_HasLastNow = false;
	}

	int64_t CClockInterpolator::Now(int64_t NowWallTick, int64_t TickFreq)
	{
		if(m_HasLastNow && NowWallTick == m_LastNowWallTick && TickFreq == m_LastTickFreq)
			return m_LastActualMs;

		int64_t TargetMs = m_AnchorPositionMs;
		if(m_Playing && TickFreq > 0)
		{
			const int64_t DeltaTicks = NowWallTick - m_AnchorWallTick;
			const int64_t DeltaMs = (int64_t)((double)DeltaTicks * 1000.0 * m_Rate / (double)TickFreq);
			TargetMs += DeltaMs;
		}
		TargetMs += m_OffsetMs;

		if(!m_HasState)
		{
			m_LastActualMs = TargetMs;
			m_LastTargetMs = TargetMs;
			m_LastNowWallTick = NowWallTick;
			m_LastTickFreq = TickFreq;
			m_HasState = true;
			m_HasLastNow = true;
			return m_LastActualMs;
		}

		m_CorrectionMs = ApproachCorrection(m_CorrectionMs);
		m_LastActualMs = TargetMs + m_CorrectionMs;
		m_LastTargetMs = TargetMs;
		m_LastNowWallTick = NowWallTick;
		m_LastTickFreq = TickFreq;
		m_HasLastNow = true;
		return m_LastActualMs;
	}

} // namespace QmLyrics
