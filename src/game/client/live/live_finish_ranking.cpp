#include "live_finish_ranking.h"

#include <base/math.h>
#include <base/system.h>

#include <engine/shared/json.h>

#include <game/teamscore.h>

#include <algorithm>
#include <cstdlib>

namespace
{
	bool JsonIntField(const json_value *pObject, const char *pName, int &Out)
	{
		if(pObject == nullptr || pObject->type != json_object)
			return false;
		const json_value *pValue = json_object_get(pObject, pName);
		if(pValue == &json_value_none || pValue->type != json_integer)
			return false;
		Out = (int)pValue->u.integer;
		return true;
	}

	const json_value *JsonEventArray(const json_value *pRoot)
	{
		if(pRoot == nullptr || pRoot->type != json_object)
			return nullptr;
		const json_value *pEvents = json_object_get(pRoot, "finish_events");
		if(pEvents != &json_value_none)
			return pEvents;
		pEvents = json_object_get(pRoot, "events");
		return pEvents != &json_value_none ? pEvents : nullptr;
	}
} // namespace

void CLiveFinishRanking::Reset()
{
	m_vEvents.clear();
	m_vPending.clear();
	ClearCards();
	m_LastObservedTick = -1;
}

bool CLiveFinishRanking::OnTimelineTick(int Tick)
{
	if(Tick < 0)
		return false;
	if(m_LastObservedTick >= 0 && Tick < m_LastObservedTick)
	{
		Reset();
		m_LastObservedTick = Tick;
		return true;
	}
	m_LastObservedTick = Tick;
	return false;
}

void CLiveFinishRanking::SetTeamRange(int TeamMin, int TeamMax)
{
	TeamMin = std::clamp(TeamMin, TEAM_FLOCK + 1, TEAM_SUPER - 1);
	TeamMax = std::clamp(TeamMax, TEAM_FLOCK + 1, TEAM_SUPER - 1);
	m_TeamMin = minimum(TeamMin, TeamMax);
	m_TeamMax = maximum(TeamMin, TeamMax);
}

bool CLiveFinishRanking::IsValidDDRaceTeam(int Team)
{
	return Team > TEAM_FLOCK && Team < TEAM_SUPER;
}

bool CLiveFinishRanking::IsTeamInConfiguredRange(int Team) const
{
	return IsValidDDRaceTeam(Team) && Team >= m_TeamMin && Team <= m_TeamMax;
}

bool CLiveFinishRanking::EventLess(const CLiveFinishEvent &Left, const CLiveFinishEvent &Right)
{
	if(Left.m_TimeMs != Right.m_TimeMs)
		return Left.m_TimeMs < Right.m_TimeMs;
	if(Left.m_FinishTick != Right.m_FinishTick)
		return Left.m_FinishTick < Right.m_FinishTick;
	return Left.m_Team < Right.m_Team;
}

bool CLiveFinishRanking::IsTeamFinished(int Team) const
{
	for(const CLiveFinishEvent &Event : m_vEvents)
	{
		if(Event.m_Team == Team)
			return true;
	}
	return false;
}

bool CLiveFinishRanking::IsDuplicateEvent(const CLiveFinishEvent &Event) const
{
	for(const CLiveFinishEvent &Existing : m_vEvents)
	{
		if(Existing.m_Team == Event.m_Team)
			return true;
	}
	return false;
}

bool CLiveFinishRanking::IsDuplicatePending(int ClientId, int TimeMs, int FinishTick) const
{
	for(const CPendingFinish &Pending : m_vPending)
	{
		if(Pending.m_ClientId == ClientId &&
			Pending.m_TimeMs == TimeMs &&
			absolute(Pending.m_FinishTick - FinishTick) <= DEDUP_TICK_WINDOW)
			return true;
	}
	return false;
}

CLiveFinishRanking::CResult CLiveFinishRanking::AddConfirmedEvent(const CLiveFinishEvent &Event)
{
	CResult Result;
	Result.m_Event = Event;
	if(!IsValidDDRaceTeam(Event.m_Team) || Event.m_ClientId < 0 || Event.m_ClientId >= MAX_CLIENTS || Event.m_TimeMs < 0 || Event.m_FinishTick < 0)
		return Result;
	if(IsDuplicateEvent(Event))
	{
		Result.m_Status = EFinishStatus::DUPLICATE;
		return Result;
	}

	m_vEvents.push_back(Event);
	Result.m_Status = EFinishStatus::ACCEPTED;
	Result.m_Rank = RankForTeam(Event.m_Team, false);
	return Result;
}

CLiveFinishRanking::CResult CLiveFinishRanking::OnFinishMessage(int ClientId, int TimeMs, int FinishTick, bool TeamKnown, int Team)
{
	OnTimelineTick(FinishTick);

	CResult Result;
	Result.m_Event = {Team, ClientId, TimeMs, FinishTick};
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || TimeMs < 0 || FinishTick < 0)
		return Result;

	if(!TeamKnown)
	{
		if(!IsDuplicatePending(ClientId, TimeMs, FinishTick))
			m_vPending.push_back({ClientId, TimeMs, FinishTick});
		Result.m_Status = EFinishStatus::PENDING;
		return Result;
	}

	if(!IsValidDDRaceTeam(Team))
		return Result;

	return AddConfirmedEvent({Team, ClientId, TimeMs, FinishTick});
}

CLiveFinishRanking::CResolveResult CLiveFinishRanking::ResolvePending(const int *pTeams, int NumTeams, int CurrentTick)
{
	OnTimelineTick(CurrentTick);

	CResolveResult ResolveResult;
	std::vector<CPendingFinish> vRemaining;
	vRemaining.reserve(m_vPending.size());

	for(const CPendingFinish &Pending : m_vPending)
	{
		if(CurrentTick >= 0 && CurrentTick - Pending.m_FinishTick > PENDING_FINISH_MAX_TICKS)
		{
			++ResolveResult.m_DroppedPending;
			continue;
		}
		if(Pending.m_ClientId < 0 || Pending.m_ClientId >= NumTeams || pTeams == nullptr)
		{
			++ResolveResult.m_DroppedPending;
			continue;
		}

		const int Team = pTeams[Pending.m_ClientId];
		if(!IsValidDDRaceTeam(Team))
		{
			vRemaining.push_back(Pending);
			continue;
		}

		CResult Result = AddConfirmedEvent({Team, Pending.m_ClientId, Pending.m_TimeMs, Pending.m_FinishTick});
		if(Result.m_Status == EFinishStatus::ACCEPTED)
			ResolveResult.m_vAccepted.push_back(Result);
	}

	m_vPending.swap(vRemaining);
	return ResolveResult;
}

int CLiveFinishRanking::DropExpiredPending(int CurrentTick)
{
	if(CurrentTick < 0)
		return 0;
	OnTimelineTick(CurrentTick);

	int Dropped = 0;
	std::vector<CPendingFinish> vRemaining;
	vRemaining.reserve(m_vPending.size());
	for(const CPendingFinish &Pending : m_vPending)
	{
		if(CurrentTick - Pending.m_FinishTick > PENDING_FINISH_MAX_TICKS)
		{
			++Dropped;
			continue;
		}
		vRemaining.push_back(Pending);
	}
	m_vPending.swap(vRemaining);
	return Dropped;
}

int CLiveFinishRanking::RankForTeam(int Team, bool IncludeOutOfRange) const
{
	const CLiveFinishEvent *pTarget = nullptr;
	for(const CLiveFinishEvent &Event : m_vEvents)
	{
		if(Event.m_Team == Team)
		{
			pTarget = &Event;
			break;
		}
	}
	if(pTarget == nullptr)
		return 0;
	if(!IncludeOutOfRange && !IsTeamInConfiguredRange(Team))
		return 0;

	int Rank = 1;
	for(const CLiveFinishEvent &Event : m_vEvents)
	{
		if(Event.m_Team != Team && EventLess(Event, *pTarget))
			++Rank;
	}
	return Rank;
}

bool CLiveFinishRanking::RebuildFromEvents(const std::vector<CLiveFinishEvent> &vEvents, int CurrentTick)
{
	Reset();
	m_LastObservedTick = CurrentTick;

	std::vector<CLiveFinishEvent> vSorted;
	vSorted.reserve(vEvents.size());
	for(const CLiveFinishEvent &Event : vEvents)
	{
		if(Event.m_FinishTick <= CurrentTick)
			vSorted.push_back(Event);
	}
	std::stable_sort(vSorted.begin(), vSorted.end(), EventLess);

	bool AcceptedAny = false;
	for(const CLiveFinishEvent &Event : vSorted)
	{
		if(AddConfirmedEvent(Event).m_Status == EFinishStatus::ACCEPTED)
			AcceptedAny = true;
	}
	ClearCards();
	return AcceptedAny || vEvents.empty();
}

void CLiveFinishRanking::EnqueueCard(const CLiveFinishEvent &Event, int Rank, int CurrentTick)
{
	CLiveFinishCard Card;
	Card.m_Event = Event;
	Card.m_Rank = Rank;
	Card.m_QueuedTick = CurrentTick;
	m_vQueuedCards.push_back(Card);
}

const CLiveFinishCard *CLiveFinishRanking::VisibleCard(int CurrentTick, int DurationTicks)
{
	if(CurrentTick < 0 || DurationTicks <= 0)
		return nullptr;

	if(m_HasActiveCard && m_ActiveCard.m_DisplayStartTick > CurrentTick)
	{
		ClearCards();
		return nullptr;
	}

	while(m_HasActiveCard && CurrentTick - m_ActiveCard.m_DisplayStartTick >= DurationTicks)
		m_HasActiveCard = false;

	if(!m_HasActiveCard && !m_vQueuedCards.empty())
	{
		m_ActiveCard = m_vQueuedCards.front();
		m_vQueuedCards.pop_front();
		m_ActiveCard.m_DisplayStartTick = CurrentTick;
		m_HasActiveCard = true;
	}

	return m_HasActiveCard ? &m_ActiveCard : nullptr;
}

void CLiveFinishRanking::ClearCards()
{
	m_vQueuedCards.clear();
	m_ActiveCard = CLiveFinishCard{};
	m_HasActiveCard = false;
}

bool CLiveFinishRanking::ParseSidecarJson(const char *pJson, size_t JsonSize, std::vector<CLiveFinishEvent> &vEvents)
{
	vEvents.clear();
	if(pJson == nullptr || JsonSize == 0)
		return false;

	char aError[256] = "";
	json_settings Settings{};
	json_value *pRoot = JsonParseEx(&Settings, reinterpret_cast<const json_char *>(pJson), JsonSize, aError);
	if(pRoot == nullptr)
		return false;

	bool Success = false;
	do
	{
		const json_value *pEvents = JsonEventArray(pRoot);
		if(pEvents == nullptr || pEvents->type != json_array)
			break;

		for(unsigned int i = 0; i < pEvents->u.array.length; ++i)
		{
			const json_value *pEntry = pEvents->u.array.values[i];
			if(pEntry == nullptr || pEntry->type != json_object)
				break;

			CLiveFinishEvent Event;
			if(!JsonIntField(pEntry, "team", Event.m_Team) ||
				!JsonIntField(pEntry, "client_id", Event.m_ClientId) ||
				!JsonIntField(pEntry, "tick", Event.m_FinishTick))
			{
				break;
			}
			if(!JsonIntField(pEntry, "time", Event.m_TimeMs) &&
				!JsonIntField(pEntry, "time_ms", Event.m_TimeMs))
			{
				break;
			}
			if(!IsValidDDRaceTeam(Event.m_Team) ||
				Event.m_ClientId < 0 || Event.m_ClientId >= MAX_CLIENTS ||
				Event.m_TimeMs < 0 || Event.m_FinishTick < 0)
			{
				break;
			}
			vEvents.push_back(Event);
		}

		Success = vEvents.size() == pEvents->u.array.length;
	} while(false);

	json_value_free(pRoot);
	if(!Success)
		vEvents.clear();
	return Success;
}

std::string CLiveFinishRanking::EventsToSidecarJson(const std::vector<CLiveFinishEvent> &vEvents)
{
	std::string Json;
	Json.reserve(vEvents.size() * 80 + 32);
	Json.append("{\"version\":1,\"finish_events\":[");
	for(size_t i = 0; i < vEvents.size(); ++i)
	{
		const CLiveFinishEvent &Event = vEvents[i];
		if(i > 0)
			Json.push_back(',');
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "{\"team\":%d,\"client_id\":%d,\"time\":%d,\"tick\":%d}",
			Event.m_Team, Event.m_ClientId, Event.m_TimeMs, Event.m_FinishTick);
		Json.append(aBuf);
	}
	Json.append("]}");
	return Json;
}
