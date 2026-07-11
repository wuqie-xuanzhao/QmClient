#ifndef GAME_CLIENT_COMPONENTS_SYSTEM_MEDIA_CONTROLS_TIMELINE_H
#define GAME_CLIENT_COMPONENTS_SYSTEM_MEDIA_CONTROLS_TIMELINE_H

#include <algorithm>
#include <cstdint>

namespace SystemMediaControls
{
	constexpr int64_t HUNDRED_NS_PER_SECOND = 10000000;
	constexpr int64_t MAX_TIMELINE_AGE_100NS = 24LL * 60 * 60 * HUNDRED_NS_PER_SECOND;

	struct STimelineProperties
	{
		int64_t m_Start100ns = 0;
		int64_t m_End100ns = 0;
		int64_t m_Position100ns = 0;
		int64_t m_LastUpdatedUtc100ns = 0;

		bool operator==(const STimelineProperties &Other) const = default;
	};

	struct STimelineSnapshot
	{
		int64_t m_PositionMs = 0;
		int64_t m_DurationMs = 0;
		int64_t m_PositionUpdatedTick = 0;
	};

	class CTimelineGenerationTracker
	{
	public:
		uint64_t Update(const STimelineProperties &Properties)
		{
			if(!m_HasProperties || !(Properties == m_LastProperties))
			{
				m_LastProperties = Properties;
				m_HasProperties = true;
				++m_Generation;
			}
			return m_Generation;
		}

	private:
		STimelineProperties m_LastProperties;
		uint64_t m_Generation = 0;
		bool m_HasProperties = false;
	};

	inline int64_t TimelineAgeToTicks(int64_t Age100ns, int64_t TickFreq)
	{
		if(Age100ns <= 0 || TickFreq <= 0)
			return 0;
		const int64_t WholeSeconds = Age100ns / HUNDRED_NS_PER_SECOND;
		const int64_t Remainder100ns = Age100ns % HUNDRED_NS_PER_SECOND;
		return WholeSeconds * TickFreq + Remainder100ns * TickFreq / HUNDRED_NS_PER_SECOND;
	}

	inline STimelineSnapshot NormalizeTimelineProperties(const STimelineProperties &Properties, int64_t ObservedUtc100ns, int64_t ObservedSteadyTick, int64_t TickFreq)
	{
		STimelineSnapshot Snapshot;
		const int64_t Duration100ns = std::max<int64_t>(0, Properties.m_End100ns - Properties.m_Start100ns);
		const int64_t Position100ns = std::max<int64_t>(0, Properties.m_Position100ns - Properties.m_Start100ns);
		Snapshot.m_DurationMs = Duration100ns / 10000;
		Snapshot.m_PositionMs = (Duration100ns > 0 ? std::min(Position100ns, Duration100ns) : Position100ns) / 10000;
		Snapshot.m_PositionUpdatedTick = ObservedSteadyTick;

		const int64_t TimelineAge100ns = ObservedUtc100ns - Properties.m_LastUpdatedUtc100ns;
		if(Properties.m_LastUpdatedUtc100ns > 0 && TimelineAge100ns >= 0 && TimelineAge100ns <= MAX_TIMELINE_AGE_100NS)
		{
			Snapshot.m_PositionUpdatedTick = ObservedSteadyTick - TimelineAgeToTicks(TimelineAge100ns, TickFreq);
		}
		return Snapshot;
	}

} // namespace SystemMediaControls

#endif
