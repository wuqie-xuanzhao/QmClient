#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_AXIOM_AUTO_LOGIN_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_AXIOM_AUTO_LOGIN_H

#include <game/client/component.h>

class CQmAxiomAutoLogin : public CComponent
{
	bool m_AutoLoginAnnounced = false;
	bool m_AutoLoginSucceeded = false;
	bool m_AutoLoginWaitingReply = false;
	bool m_AutoLoginSlowRetryMode = false;
	bool m_AutoLoginHardFailed = false;
	int m_AutoLoginAttempts = 0;
	int64_t m_AutoLoginNextTryTick = 0;
	char m_aAutoLoginServer[NETADDR_MAXSTRSIZE] = "";
	bool m_DummyAutoLoginSent = false;
	bool m_DummyWasConnected = false;
	bool m_DummyLoginAllowedThisServer = false;
	char m_aDummyAutoLoginServer[NETADDR_MAXSTRSIZE] = "";

	const char *CurrentCommunityId() const;
	void TrySendLogin();
	void TrySendDummyLogin();
	void ScheduleSoftRetry();
	void ScheduleSlowRetry();

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnUpdate() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnMessage(int MsgType, void *pRawMsg) override;

	bool IsAxiomCommunity() const;
	void ResetState();
	void EnableDummyReconnectForServer();
	void DisableDummyReconnectForServer();
};

#endif
