#include "live_match_replay.h"

#include "live_finish_ranking.h"
#include "live_team_render_filter.h"

#include <base/math.h>
#include <base/str.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/console.h>
#include <engine/demo.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/teamscore.h>

namespace
{
	constexpr const char *LIVE_MATCH_REPLAY_CONSOLE = "qmlive/match";
	constexpr const char *LIVE_MATCH_REPLAY_DEMO_DIR = "demos/qm_live/matches";

	bool ValidClientId(int ClientId)
	{
		return ClientId >= 0 && ClientId < MAX_CLIENTS;
	}

	void FormatMatchDemoName(CGameClient *pGameClient, char *pBuffer, int BufferSize)
	{
		char aMap[64];
		str_copy(aMap, pGameClient->Client()->GetCurrentMap(), sizeof(aMap));
		str_sanitize_filename(aMap);
		if(aMap[0] == '\0')
			str_copy(aMap, "unknown", sizeof(aMap));

		char aTimestamp[20];
		str_timestamp(aTimestamp, sizeof(aTimestamp));
		str_format(pBuffer, BufferSize, "qm_live/matches/%s_match_%s", aMap, aTimestamp);
	}

	bool EnsureSaveFolder(IStorage *pStorage, const char *pFolder)
	{
		return pStorage->CreateFolder(pFolder, IStorage::TYPE_SAVE) || pStorage->FolderExists(pFolder, IStorage::TYPE_SAVE);
	}

	bool EnsureMatchDemoFolder(IStorage *pStorage)
	{
		return EnsureSaveFolder(pStorage, "demos") &&
		       EnsureSaveFolder(pStorage, "demos/qm_live") &&
		       EnsureSaveFolder(pStorage, LIVE_MATCH_REPLAY_DEMO_DIR);
	}
} // namespace

void CLiveMatchReplay::Reset()
{
	m_aLastTeams.fill(TEAM_FLOCK);
	m_Sidecar.Reset();
	m_aDemoFilename[0] = '\0';
	m_aSidecarFilename[0] = '\0';
	m_aStatusMessage[0] = '\0';
	m_StartTick = -1;
	m_LastTick = -1;
	m_OwnsManualRecorder = false;
	m_HadTeamSnapshot = false;
}

bool CLiveMatchReplay::Recording(const CGameClient *pGameClient) const
{
	return m_OwnsManualRecorder && pGameClient != nullptr && pGameClient->Client()->DemoRecorder(RECORDER_MANUAL)->IsRecording();
}

int CLiveMatchReplay::LengthTicks(const CGameClient *pGameClient) const
{
	if(pGameClient == nullptr || m_StartTick < 0)
		return 0;
	const int Tick = m_LastTick >= m_StartTick ? m_LastTick : pGameClient->Client()->GameTick(g_Config.m_ClDummy);
	return maximum(0, Tick - m_StartTick);
}

void CLiveMatchReplay::SetStatus(const char *pMessage)
{
	str_copy(m_aStatusMessage, pMessage == nullptr ? "" : pMessage, sizeof(m_aStatusMessage));
}

bool CLiveMatchReplay::Start(CGameClient *pGameClient)
{
	if(pGameClient == nullptr)
		return false;
	if(pGameClient->Client()->State() != IClient::STATE_ONLINE)
	{
		SetStatus("client is not online");
		return false;
	}
	if(pGameClient->Client()->DemoRecorder(RECORDER_MANUAL)->IsRecording())
	{
		SetStatus("manual demo recorder is already recording");
		return false;
	}
	if(!EnsureMatchDemoFolder(pGameClient->Storage()))
	{
		SetStatus("failed to create demos/qm_live/matches");
		return false;
	}

	char aDemoName[IO_MAX_PATH_LENGTH];
	FormatMatchDemoName(pGameClient, aDemoName, sizeof(aDemoName));
	pGameClient->Client()->DemoRecorder_Start(aDemoName, false, RECORDER_MANUAL, true);
	if(!pGameClient->Client()->DemoRecorder(RECORDER_MANUAL)->IsRecording())
	{
		SetStatus("failed to start demo recorder");
		return false;
	}

	str_copy(m_aDemoFilename, pGameClient->Client()->DemoRecorder(RECORDER_MANUAL)->CurrentFilename(), sizeof(m_aDemoFilename));
	CLiveReplaySidecar::SidecarPathForDemo(m_aDemoFilename, m_aSidecarFilename, sizeof(m_aSidecarFilename));
	m_StartTick = pGameClient->Client()->GameTick(g_Config.m_ClDummy);
	m_LastTick = m_StartTick;
	m_OwnsManualRecorder = true;
	m_HadTeamSnapshot = false;
	m_aLastTeams.fill(TEAM_FLOCK);
	BeginSidecar(pGameClient);
	SetStatus("recording");

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "started full match recording: %s", m_aDemoFilename);
	pGameClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, LIVE_MATCH_REPLAY_CONSOLE, aBuf);
	return true;
}

bool CLiveMatchReplay::Stop(CGameClient *pGameClient, bool WriteSidecarFile)
{
	if(pGameClient == nullptr || !m_OwnsManualRecorder)
		return false;

	const bool WasRecording = pGameClient->Client()->DemoRecorder(RECORDER_MANUAL)->IsRecording();
	if(WasRecording)
		pGameClient->Client()->DemoRecorder(RECORDER_MANUAL)->Stop(IDemoRecorder::EStopMode::KEEP_FILE);
	m_LastTick = pGameClient->Client()->GameTick(g_Config.m_ClDummy);
	m_Sidecar.SetEndTick(m_LastTick);
	if(WriteSidecarFile)
		WriteSidecar(pGameClient);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "stopped full match recording: %s", m_aDemoFilename);
	pGameClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, LIVE_MATCH_REPLAY_CONSOLE, aBuf);

	m_OwnsManualRecorder = false;
	g_Config.m_QmLiveMatchRecord = 0;
	SetStatus(WasRecording ? "stopped" : "recorder was already stopped");
	return true;
}

void CLiveMatchReplay::BeginSidecar(CGameClient *pGameClient)
{
	m_Sidecar.Start(
		m_aDemoFilename,
		pGameClient->Client()->GetCurrentMap(),
		pGameClient->Client()->GetCurrentMapSha256(),
		pGameClient->Client()->GetCurrentMapCrc(),
		m_StartTick);
}

void CLiveMatchReplay::WriteSidecar(CGameClient *pGameClient)
{
	if(m_aSidecarFilename[0] == '\0')
		return;

	if(m_Sidecar.WriteAtomic(pGameClient->Storage(), m_aSidecarFilename))
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "wrote sidecar: %s", m_aSidecarFilename);
		pGameClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, LIVE_MATCH_REPLAY_CONSOLE, aBuf);
	}
	else
	{
		SetStatus("failed to write sidecar");
		pGameClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, LIVE_MATCH_REPLAY_CONSOLE, "failed to write sidecar");
	}
}

void CLiveMatchReplay::FinishAfterExternalStop(CGameClient *pGameClient)
{
	if(!m_OwnsManualRecorder)
		return;
	m_LastTick = pGameClient->Client()->GameTick(g_Config.m_ClDummy);
	m_Sidecar.SetEndTick(m_LastTick);
	WriteSidecar(pGameClient);
	m_OwnsManualRecorder = false;
	g_Config.m_QmLiveMatchRecord = 0;
	SetStatus("recorder stopped externally");
}

void CLiveMatchReplay::OnSnapshot(CGameClient *pGameClient)
{
	if(pGameClient == nullptr || !m_OwnsManualRecorder)
		return;
	if(!pGameClient->Client()->DemoRecorder(RECORDER_MANUAL)->IsRecording())
	{
		FinishAfterExternalStop(pGameClient);
		return;
	}

	const int Tick = pGameClient->Client()->GameTick(g_Config.m_ClDummy);
	m_LastTick = Tick;
	m_Sidecar.SetEndTick(Tick);

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		const int Team = pGameClient->m_Teams.Team(ClientId);
		if(!m_HadTeamSnapshot)
		{
			m_aLastTeams[ClientId] = Team;
			if(Team != TEAM_FLOCK)
				m_Sidecar.AddTeamEvent(Tick, ClientId, TEAM_FLOCK, Team);
			continue;
		}

		if(m_aLastTeams[ClientId] != Team)
		{
			m_Sidecar.AddTeamEvent(Tick, ClientId, m_aLastTeams[ClientId], Team);
			m_aLastTeams[ClientId] = Team;
		}
	}
	m_HadTeamSnapshot = true;
}

void CLiveMatchReplay::OnMessage(CGameClient *pGameClient, int MsgId, void *pRawMsg)
{
	(void)pGameClient;
	(void)MsgId;
	(void)pRawMsg;
}

void CLiveMatchReplay::OnFinishEvent(const CLiveFinishEvent &Event)
{
	if(!m_OwnsManualRecorder || !m_Sidecar.Active())
		return;

	if(!ValidClientId(Event.m_ClientId) || !CLiveTeamRenderFilter::IsValidDDRaceTeam(Event.m_Team) || Event.m_TimeMs < 0 || Event.m_FinishTick < 0)
		return;

	m_Sidecar.AddFinishEvent(Event.m_FinishTick, Event.m_Team, Event.m_TimeMs, Event.m_ClientId);
}

void CLiveMatchReplay::OnStateChange(CGameClient *pGameClient, int NewState, int OldState)
{
	(void)OldState;
	if(pGameClient == nullptr || !m_OwnsManualRecorder)
		return;
	if(NewState < IClient::STATE_ONLINE || NewState == IClient::STATE_DEMOPLAYBACK)
		Stop(pGameClient);
}

void CLiveMatchReplay::OnShutdown(CGameClient *pGameClient)
{
	if(m_OwnsManualRecorder)
		Stop(pGameClient);
}
