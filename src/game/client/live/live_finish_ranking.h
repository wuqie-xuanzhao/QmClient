// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_LIVE_LIVE_FINISH_RANKING_H
#define GAME_CLIENT_LIVE_LIVE_FINISH_RANKING_H

#include <engine/shared/protocol.h>

#include <deque>
#include <string>
#include <vector>

class CLiveFinishEvent
{
public:
	int m_Team = -1;
	int m_ClientId = -1;
	int m_TimeMs = 0;
	int m_FinishTick = -1;
};

class CLiveFinishCard
{
public:
	CLiveFinishEvent m_Event;
	int m_Rank = 0;
	int m_QueuedTick = -1;
	int m_DisplayStartTick = -1;
};

class CLiveFinishRanking
{
public:
	enum class EFinishStatus
	{
		IGNORED,
		PENDING,
		DUPLICATE,
		ACCEPTED,
	};

	class CResult
	{
	public:
		EFinishStatus m_Status = EFinishStatus::IGNORED;
		CLiveFinishEvent m_Event;
		int m_Rank = 0;
	};

	class CResolveResult
	{
	public:
		std::vector<CResult> m_vAccepted;
		int m_DroppedPending = 0;
	};

	static constexpr int DEFAULT_TEAM_MIN = 1;
	static constexpr int DEFAULT_TEAM_MAX = 30;
	static constexpr int PENDING_FINISH_MAX_TICKS = SERVER_TICK_SPEED * 2;
	static constexpr int DEDUP_TICK_WINDOW = SERVER_TICK_SPEED;

	void Reset();
	bool OnTimelineTick(int Tick);

	void SetTeamRange(int TeamMin, int TeamMax);
	int TeamMin() const { return m_TeamMin; }
	int TeamMax() const { return m_TeamMax; }
	int LastObservedTick() const { return m_LastObservedTick; }

	CResult OnFinishMessage(int ClientId, int TimeMs, int FinishTick, bool TeamKnown, int Team);
	CResolveResult ResolvePending(const int *pTeams, int NumTeams, int CurrentTick);
	int DropExpiredPending(int CurrentTick);

	bool RebuildFromEvents(const std::vector<CLiveFinishEvent> &vEvents, int CurrentTick);

	void EnqueueCard(const CLiveFinishEvent &Event, int Rank, int CurrentTick);
	const CLiveFinishCard *VisibleCard(int CurrentTick, int DurationTicks);
	void ClearCards();

	const std::vector<CLiveFinishEvent> &Events() const { return m_vEvents; }
	int RankForTeam(int Team, bool IncludeOutOfRange) const;
	bool IsTeamFinished(int Team) const;
	bool IsTeamInConfiguredRange(int Team) const;

	static bool IsValidDDRaceTeam(int Team);
	static bool ParseSidecarJson(const char *pJson, size_t JsonSize, std::vector<CLiveFinishEvent> &vEvents);
	static std::string EventsToSidecarJson(const std::vector<CLiveFinishEvent> &vEvents);

private:
	class CPendingFinish
	{
	public:
		int m_ClientId = -1;
		int m_TimeMs = 0;
		int m_FinishTick = -1;
	};

	std::vector<CLiveFinishEvent> m_vEvents;
	std::vector<CPendingFinish> m_vPending;
	std::deque<CLiveFinishCard> m_vQueuedCards;
	CLiveFinishCard m_ActiveCard;
	bool m_HasActiveCard = false;
	int m_LastObservedTick = -1;
	int m_TeamMin = DEFAULT_TEAM_MIN;
	int m_TeamMax = DEFAULT_TEAM_MAX;

	CResult AddConfirmedEvent(const CLiveFinishEvent &Event);
	bool IsDuplicateEvent(const CLiveFinishEvent &Event) const;
	bool IsDuplicatePending(int ClientId, int TimeMs, int FinishTick) const;
	static bool EventLess(const CLiveFinishEvent &Left, const CLiveFinishEvent &Right);
};

#endif // GAME_CLIENT_LIVE_LIVE_FINISH_RANKING_H
