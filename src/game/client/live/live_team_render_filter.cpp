#include "live_team_render_filter.h"

#include <game/teamscore.h>

bool CLiveTeamRenderFilter::IsValidDDRaceTeam(int Team)
{
	return Team > TEAM_FLOCK && Team < TEAM_SUPER;
}

void CLiveTeamRenderFilter::Reset()
{
	m_aTeams.fill(TEAM_FLOCK);
	m_Team = -1;
	m_LastPlaybackTick = -1;
	m_ResetSerial = 0;
	m_Active = false;
	m_PreviewEnabled = true;
	m_AudioEnabled = true;
	m_HideExternalFinish = true;
	m_StrictUnknownEvents = true;
}

bool CLiveTeamRenderFilter::SetTeam(int Team)
{
	if(!IsValidDDRaceTeam(Team))
		return false;

	if(!m_Active || m_Team != Team)
	{
		m_Team = Team;
		m_Active = true;
		m_LastPlaybackTick = -1;
		MarkTransientReset();
	}
	return true;
}

void CLiveTeamRenderFilter::Disable()
{
	if(m_Active)
	{
		m_Active = false;
		m_Team = -1;
		m_LastPlaybackTick = -1;
		MarkTransientReset();
	}
}

void CLiveTeamRenderFilter::UpdateTeams(const std::array<int, MAX_CLIENTS> &aTeams)
{
	m_aTeams = aTeams;
}

bool CLiveTeamRenderFilter::AllowsTeam(int Team) const
{
	if(!m_Active)
		return true;
	return IsValidDDRaceTeam(Team) && Team == m_Team;
}

bool CLiveTeamRenderFilter::AllowsClient(int ClientId) const
{
	if(!m_Active)
		return true;
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return false;
	return AllowsTeam(m_aTeams[ClientId]);
}

bool CLiveTeamRenderFilter::AllowsKnownOwner(int ClientId) const
{
	if(!m_Active)
		return true;
	return AllowsClient(ClientId);
}

bool CLiveTeamRenderFilter::AllowsUnknownPlayerEvent() const
{
	if(!m_Active)
		return true;
	return !m_StrictUnknownEvents;
}

bool CLiveTeamRenderFilter::AllowsFinishForTeam(int Team) const
{
	if(!m_Active || !m_HideExternalFinish)
		return true;
	return AllowsTeam(Team);
}

bool CLiveTeamRenderFilter::ObservePlaybackTick(int Tick)
{
	if(!m_Active)
	{
		m_LastPlaybackTick = Tick;
		return false;
	}

	const bool SeekedOrRestarted = m_LastPlaybackTick != -1 && Tick <= m_LastPlaybackTick;
	m_LastPlaybackTick = Tick;
	if(SeekedOrRestarted)
		MarkTransientReset();
	return SeekedOrRestarted;
}
