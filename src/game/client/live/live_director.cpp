// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "live_director.h"

#include <game/teamscore.h>

void CLiveDirector::Reset()
{
	m_vEntries.clear();
	m_FallbackPlayer = -1;
	m_HasDDRaceTeams = false;
	m_Mode = CLiveObserverSession::EDirectorMode::FREEVIEW;
}

void CLiveDirector::UpdateEntries(const std::array<int, MAX_CLIENTS> &aTeams, const std::array<bool, MAX_CLIENTS> &aActivePlayers)
{
	m_vEntries.clear();
	m_FallbackPlayer = -1;
	m_HasDDRaceTeams = false;

	std::array<int, TEAM_SUPER> aTeamCounts{};
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(!aActivePlayers[ClientId])
			continue;

		if(m_FallbackPlayer < 0)
			m_FallbackPlayer = ClientId;

		const int Team = aTeams[ClientId];
		if(Team > TEAM_FLOCK && Team < TEAM_SUPER)
			aTeamCounts[Team]++;
	}

	for(int Team = TEAM_FLOCK + 1; Team < TEAM_SUPER; ++Team)
	{
		if(aTeamCounts[Team] > 0)
		{
			m_vEntries.push_back({EEntryType::DDRACE_TEAM, Team, -1, aTeamCounts[Team]});
			m_HasDDRaceTeams = true;
		}
	}

	if(m_HasDDRaceTeams)
		return;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(aActivePlayers[ClientId])
			m_vEntries.push_back({EEntryType::PLAYER, -1, ClientId, 1});
	}
}

int CLiveDirector::SelectRandomTeam(unsigned Seed) const
{
	int NumTeams = 0;
	for(const CEntry &Entry : m_vEntries)
	{
		if(Entry.m_Type == EEntryType::DDRACE_TEAM)
			++NumTeams;
	}
	if(NumTeams == 0)
		return -1;

	int Target = Seed % NumTeams;
	for(const CEntry &Entry : m_vEntries)
	{
		if(Entry.m_Type != EEntryType::DDRACE_TEAM)
			continue;
		if(Target == 0)
			return Entry.m_Team;
		--Target;
	}
	return -1;
}

int CLiveDirector::SelectRandomPlayer(unsigned Seed) const
{
	int NumPlayers = 0;
	for(const CEntry &Entry : m_vEntries)
	{
		if(Entry.m_Type == EEntryType::PLAYER)
			++NumPlayers;
	}
	if(NumPlayers == 0)
		return -1;

	int Target = Seed % NumPlayers;
	for(const CEntry &Entry : m_vEntries)
	{
		if(Entry.m_Type != EEntryType::PLAYER)
			continue;
		if(Target == 0)
			return Entry.m_ClientId;
		--Target;
	}
	return -1;
}
