#include "axiom_auto_login.h"

#include <base/str.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/client/components/chat.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <initializer_list>

static constexpr int QMCLIENT_AXIOM_AUTO_LOGIN_MAX_ATTEMPTS = 3;
static constexpr int QMCLIENT_AXIOM_AUTO_LOGIN_RETRY_DELAY_SECONDS = 2;
static constexpr int QMCLIENT_AXIOM_AUTO_LOGIN_REPLY_TIMEOUT_SECONDS = 8;
static constexpr int QMCLIENT_AXIOM_AUTO_LOGIN_SLOW_RETRY_SECONDS = 30;

static bool QmTextContainsAny(const char *pText, const std::initializer_list<const char *> &Tokens)
{
	if(!pText || pText[0] == '\0')
		return false;

	for(const char *pToken : Tokens)
	{
		if(pToken && pToken[0] != '\0' && str_find_nocase(pText, pToken))
			return true;
	}
	return false;
}

static bool IsLoginContextMessage(const char *pText)
{
	return QmTextContainsAny(pText, {"login", "logged in", "password", "登入", "登录", "密码"});
}

static bool IsHardLoginFailure(const char *pText)
{
	return IsLoginContextMessage(pText) &&
	       QmTextContainsAny(pText, {"incorrect", "invalid", "wrong", "password incorrect", "wrong password", "密码错误", "密码不正确"});
}

const char *CQmAxiomAutoLogin::CurrentCommunityId() const
{
	IServerBrowser *pServerBrowser = ServerBrowser();
	if(!pServerBrowser)
		return nullptr;

	const char *pCommunityId = nullptr;
	const NETADDR *pServerAddr = Client()->ServerAddress();
	const IServerBrowser::CServerEntry *pEntry = pServerAddr ? pServerBrowser->Find(*pServerAddr) : nullptr;
	if(pEntry)
		pCommunityId = pEntry->m_Info.m_aCommunityId;
	else if(GameClient()->m_ConnectServerInfo)
		pCommunityId = GameClient()->m_ConnectServerInfo->m_aCommunityId;

	if(!pCommunityId || pCommunityId[0] == '\0')
		return nullptr;
	return pCommunityId;
}

bool CQmAxiomAutoLogin::IsAxiomCommunity() const
{
	const char *pCommunityId = CurrentCommunityId();
	if(!pCommunityId)
		return false;

	if(str_find_nocase(pCommunityId, "axiom"))
		return true;

	IServerBrowser *pServerBrowser = ServerBrowser();
	if(!pServerBrowser)
		return false;

	const CCommunity *pCommunity = pServerBrowser->Community(pCommunityId);
	return pCommunity && pCommunity->Name() && str_find_nocase(pCommunity->Name(), "axiom");
}

void CQmAxiomAutoLogin::EnableDummyReconnectForServer()
{
	m_DummyLoginAllowedThisServer = true;
}

void CQmAxiomAutoLogin::DisableDummyReconnectForServer()
{
	m_DummyLoginAllowedThisServer = false;
	m_DummyAutoLoginSent = false;
	m_aDummyAutoLoginServer[0] = '\0';
}

void CQmAxiomAutoLogin::ResetState()
{
	m_AutoLoginAnnounced = false;
	m_AutoLoginSucceeded = false;
	m_AutoLoginWaitingReply = false;
	m_AutoLoginSlowRetryMode = false;
	m_AutoLoginHardFailed = false;
	m_AutoLoginAttempts = 0;
	m_AutoLoginNextTryTick = 0;
	m_AutoLoginEnabledLastFrame = false;
	m_aAutoLoginServer[0] = '\0';
	m_DummyAutoLoginSent = false;
	m_DummyWasConnected = false;
	m_DummyLoginAllowedThisServer = false;
	m_aDummyAutoLoginServer[0] = '\0';
}

void CQmAxiomAutoLogin::ScheduleSlowRetry()
{
	m_AutoLoginSlowRetryMode = true;
	m_AutoLoginNextTryTick = time_get() + (int64_t)QMCLIENT_AXIOM_AUTO_LOGIN_SLOW_RETRY_SECONDS * time_freq();
}

void CQmAxiomAutoLogin::ScheduleSoftRetry()
{
	if(m_AutoLoginAttempts < QMCLIENT_AXIOM_AUTO_LOGIN_MAX_ATTEMPTS)
	{
		m_AutoLoginNextTryTick = time_get() + (int64_t)QMCLIENT_AXIOM_AUTO_LOGIN_RETRY_DELAY_SECONDS * time_freq();
	}
	else
	{
		ScheduleSlowRetry();
	}
}

void CQmAxiomAutoLogin::TrySendLogin()
{
	if(g_Config.m_QmAxiomAutoLogin == 0 || g_Config.m_QmAxiomLoginPassword[0] == '\0')
		return;
	if(Client()->State() != IClient::STATE_ONLINE || !IsAxiomCommunity())
		return;
	if(m_AutoLoginSucceeded || m_AutoLoginHardFailed)
		return;

	char aServerAddress[NETADDR_MAXSTRSIZE] = "";
	const NETADDR *pServerAddr = Client()->ServerAddress();
	if(pServerAddr)
		net_addr_str(pServerAddr, aServerAddress, sizeof(aServerAddress), true);
	if(aServerAddress[0] != '\0')
		str_copy(m_aAutoLoginServer, aServerAddress, sizeof(m_aAutoLoginServer));

	char aLoginCommand[192];
	str_format(aLoginCommand, sizeof(aLoginCommand), "/login %s", g_Config.m_QmAxiomLoginPassword);
	GameClient()->m_Chat.SendChat(0, aLoginCommand);

	m_AutoLoginAttempts++;
	m_AutoLoginWaitingReply = true;
	m_AutoLoginNextTryTick = time_get() + (int64_t)QMCLIENT_AXIOM_AUTO_LOGIN_REPLY_TIMEOUT_SECONDS * time_freq();

	if(!m_AutoLoginAnnounced)
	{
		GameClient()->Echo(Localize("Trying Axiom auto login"));
		m_AutoLoginAnnounced = true;
	}
}

void CQmAxiomAutoLogin::TrySendDummyLogin()
{
	if(g_Config.m_QmAxiomAutoLogin == 0 || g_Config.m_QmAxiomDummyLoginPassword[0] == '\0')
		return;
	if(Client()->State() != IClient::STATE_ONLINE || !Client()->DummyConnected() || !IsAxiomCommunity())
		return;
	if(!m_DummyLoginAllowedThisServer)
		return;
	if(m_DummyAutoLoginSent)
		return;

	char aServerAddress[NETADDR_MAXSTRSIZE] = "";
	const NETADDR *pServerAddr = Client()->ServerAddress();
	if(pServerAddr)
		net_addr_str(pServerAddr, aServerAddress, sizeof(aServerAddress), true);
	if(aServerAddress[0] != '\0')
		str_copy(m_aDummyAutoLoginServer, aServerAddress, sizeof(m_aDummyAutoLoginServer));

	char aLoginCommand[192];
	str_format(aLoginCommand, sizeof(aLoginCommand), "/login %s", g_Config.m_QmAxiomDummyLoginPassword);
	GameClient()->m_Chat.SendChatOnConn(IClient::CONN_DUMMY, 0, aLoginCommand);
	m_DummyAutoLoginSent = true;
	GameClient()->Echo(Localize("Trying Axiom dummy auto login"));
}

void CQmAxiomAutoLogin::OnMessage(int MsgType, void *pRawMsg)
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK || MsgType != NETMSGTYPE_SV_CHAT)
		return;

	CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
	if(pMsg->m_ClientId >= 0 || !pMsg->m_pMessage)
		return;

	const char *pText = pMsg->m_pMessage;
	if(!m_AutoLoginWaitingReply || !IsAxiomCommunity() || !pText || pText[0] == '\0')
		return;

	if(IsHardLoginFailure(pText))
	{
		m_AutoLoginWaitingReply = false;
		m_AutoLoginHardFailed = true;
		m_AutoLoginSlowRetryMode = false;
		m_AutoLoginNextTryTick = 0;
		GameClient()->Echo(Localize("Axiom auto login failed"));
		return;
	}

	const bool IsLoginMessage = QmTextContainsAny(pText, {"login", "logged in", "登入", "登录"});
	if(!IsLoginMessage)
		return;

	const bool Success = QmTextContainsAny(pText, {"success", "successful", "logged in", "welcome", "succeeded", "登入成功", "登录成功", "欢迎"});
	const bool Failure = QmTextContainsAny(pText, {"fail", "failed", "incorrect", "invalid", "wrong", "denied", "error", "password incorrect", "登入失败", "登录失败", "密码错误"});

	if(Success)
	{
		m_AutoLoginSucceeded = true;
		m_AutoLoginWaitingReply = false;
		m_AutoLoginSlowRetryMode = false;
		m_AutoLoginHardFailed = false;
		m_AutoLoginNextTryTick = 0;
		GameClient()->Echo(Localize("Axiom auto login succeeded"));
	}
	else if(Failure)
	{
		m_AutoLoginWaitingReply = false;
		ScheduleSoftRetry();
		GameClient()->Echo(Localize("Axiom auto login failed, retrying"));
	}
}

void CQmAxiomAutoLogin::OnUpdate()
{
	if(Client()->State() != IClient::STATE_ONLINE)
	{
		return;
	}
	if(g_Config.m_QmAxiomAutoLogin == 0)
	{
		if(m_AutoLoginEnabledLastFrame)
			ResetState();
		m_AutoLoginEnabledLastFrame = false;
		return;
	}
	if(!m_AutoLoginEnabledLastFrame)
	{
		ResetState();
		m_AutoLoginEnabledLastFrame = true;
	}
	if(!IsAxiomCommunity())
	{
		ResetState();
		return;
	}

	char aServerAddress[NETADDR_MAXSTRSIZE] = "";
	const NETADDR *pServerAddr = Client()->ServerAddress();
	if(pServerAddr)
		net_addr_str(pServerAddr, aServerAddress, sizeof(aServerAddress), true);
	if(m_aAutoLoginServer[0] != '\0' && aServerAddress[0] != '\0' && str_comp(m_aAutoLoginServer, aServerAddress) != 0)
		ResetState();

	const bool DummyConnected = Client()->DummyConnected();
	if(!DummyConnected || g_Config.m_QmAxiomDummyLoginPassword[0] == '\0')
	{
		m_DummyAutoLoginSent = false;
		m_aDummyAutoLoginServer[0] = '\0';
		if(!DummyConnected && m_DummyLoginAllowedThisServer && g_Config.m_QmAxiomDummyLoginPassword[0] != '\0' && !Client()->DummyConnecting() && !Client()->DummyConnectingDelayed() && Client()->DummyAllowed())
			Client()->DummyConnect();
	}
	else
	{
		if(m_aDummyAutoLoginServer[0] != '\0' && aServerAddress[0] != '\0' && str_comp(m_aDummyAutoLoginServer, aServerAddress) != 0)
		{
			m_DummyAutoLoginSent = false;
			m_DummyLoginAllowedThisServer = false;
			m_aDummyAutoLoginServer[0] = '\0';
		}

		if(!m_DummyAutoLoginSent)
			TrySendDummyLogin();
	}
	m_DummyWasConnected = DummyConnected;

	if(g_Config.m_QmAxiomLoginPassword[0] == '\0')
	{
		m_AutoLoginAnnounced = false;
		m_AutoLoginSucceeded = false;
		m_AutoLoginWaitingReply = false;
		m_AutoLoginSlowRetryMode = false;
		m_AutoLoginHardFailed = false;
		m_AutoLoginAttempts = 0;
		m_AutoLoginNextTryTick = 0;
		m_aAutoLoginServer[0] = '\0';
	}
	else if(!m_AutoLoginSucceeded && !m_AutoLoginWaitingReply)
	{
		const int64_t Now = time_get();
		if(!m_AutoLoginHardFailed && (m_AutoLoginAttempts == 0 || (m_AutoLoginNextTryTick > 0 && Now >= m_AutoLoginNextTryTick)))
			TrySendLogin();
		if(m_AutoLoginSlowRetryMode && m_AutoLoginNextTryTick == 0 && !m_AutoLoginHardFailed)
			ScheduleSlowRetry();
	}
	else if(m_AutoLoginWaitingReply)
	{
		const int64_t Now = time_get();
		if(m_AutoLoginNextTryTick > 0 && Now >= m_AutoLoginNextTryTick)
		{
			m_AutoLoginWaitingReply = false;
			ScheduleSoftRetry();
		}
	}
}

void CQmAxiomAutoLogin::OnStateChange(int NewState, int OldState)
{
	CComponent::OnStateChange(NewState, OldState);
	// 地图切换通常是 ONLINE -> LOADING -> ONLINE，同一连接的认证状态应保留。
	if(NewState == IClient::STATE_OFFLINE || NewState == IClient::STATE_DEMOPLAYBACK || OldState == IClient::STATE_DEMOPLAYBACK)
		ResetState();
}
