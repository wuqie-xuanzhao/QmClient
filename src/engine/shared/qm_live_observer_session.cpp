// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "qm_live_observer_session.h"

#include <base/system.h>

void CLiveObserverSession::Reset()
{
	m_HandshakeState = EHandshakeState::INACTIVE;
	m_DirectorMode = EDirectorMode::FREEVIEW;
	m_DenyReason = EQmLiveDenyReason::UNSUPPORTED;
	m_Capabilities = 0;
	m_CurrentTeam = -1;
	m_FollowClientId = -1;
	m_Freeview = true;
	m_ReadyPending = false;
	m_CompatDirectorActive = false;
	m_FreeviewPosition = vec2(0.0f, 0.0f);
	m_aDenyReasonText[0] = '\0';
}

void CLiveObserverSession::StartRequest()
{
	Reset();
	m_HandshakeState = EHandshakeState::REQUESTED;
}

void CLiveObserverSession::Accept(int Capabilities)
{
	m_HandshakeState = EHandshakeState::ACCEPTED;
	m_CompatDirectorActive = false;
	m_Capabilities = Capabilities;
	m_DenyReason = EQmLiveDenyReason::UNSUPPORTED;
	m_aDenyReasonText[0] = '\0';
}

void CLiveObserverSession::Deny(EQmLiveDenyReason Reason, const char *pReasonText)
{
	m_HandshakeState = EHandshakeState::DENIED;
	m_CompatDirectorActive = false;
	m_Capabilities = 0;
	m_DenyReason = Reason;
	str_copy(m_aDenyReasonText, pReasonText && pReasonText[0] ? pReasonText : QmLiveDenyReasonString(Reason));
}

void CLiveObserverSession::StartCompatDirector(EQmLiveDenyReason Reason, const char *pReasonText)
{
	Deny(Reason, pReasonText);
	m_CompatDirectorActive = true;
}
