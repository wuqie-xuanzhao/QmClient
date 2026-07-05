#ifndef GAME_CLIENT_LIVE_LIVE_MATCH_REPLAY_H
#define GAME_CLIENT_LIVE_LIVE_MATCH_REPLAY_H

#include "live_replay_sidecar.h"

#include <engine/shared/protocol.h>

#include <array>

class CGameClient;
class CLiveFinishEvent;

class CLiveMatchReplay
{
public:
	void Reset();

	bool Start(CGameClient *pGameClient);
	bool Stop(CGameClient *pGameClient, bool WriteSidecar = true);
	void OnSnapshot(CGameClient *pGameClient);
	void OnMessage(CGameClient *pGameClient, int MsgId, void *pRawMsg);
	void OnFinishEvent(const CLiveFinishEvent &Event);
	void OnStateChange(CGameClient *pGameClient, int NewState, int OldState);
	void OnShutdown(CGameClient *pGameClient);

	bool Recording(const CGameClient *pGameClient) const;
	bool OwnsManualRecorder() const { return m_OwnsManualRecorder; }
	const char *DemoFilename() const { return m_aDemoFilename; }
	const char *SidecarFilename() const { return m_aSidecarFilename; }
	const char *StatusMessage() const { return m_aStatusMessage; }
	int StartTick() const { return m_StartTick; }
	int LastTick() const { return m_LastTick; }
	int LengthTicks(const CGameClient *pGameClient) const;

private:
	void BeginSidecar(CGameClient *pGameClient);
	void FinishAfterExternalStop(CGameClient *pGameClient);
	void WriteSidecar(CGameClient *pGameClient);
	void SetStatus(const char *pMessage);

	std::array<int, MAX_CLIENTS> m_aLastTeams{};
	CLiveReplaySidecar m_Sidecar;
	char m_aDemoFilename[IO_MAX_PATH_LENGTH] = "";
	char m_aSidecarFilename[IO_MAX_PATH_LENGTH] = "";
	char m_aStatusMessage[128] = "";
	int m_StartTick = -1;
	int m_LastTick = -1;
	bool m_OwnsManualRecorder = false;
	bool m_HadTeamSnapshot = false;
};

#endif // GAME_CLIENT_LIVE_LIVE_MATCH_REPLAY_H
