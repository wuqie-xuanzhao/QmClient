// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef ENGINE_SHARED_QM_LIVE_OBSERVER_SESSION_H
#define ENGINE_SHARED_QM_LIVE_OBSERVER_SESSION_H

#include "qm_live_protocol.h"

#include <base/vmath.h>

class CLiveObserverSession
{
public:
	enum class EHandshakeState
	{
		INACTIVE = 0,
		REQUESTED,
		ACCEPTED,
		DENIED,
	};

	enum class EDirectorMode
	{
		FOLLOW_TEAM = 0,
		FOLLOW_PLAYER,
		FREEVIEW,
		TRANSITION,
	};

	void Reset();
	void StartRequest();
	void Accept(int Capabilities);
	void Deny(EQmLiveDenyReason Reason, const char *pReasonText);
	void StartCompatDirector(EQmLiveDenyReason Reason, const char *pReasonText);

	EHandshakeState HandshakeState() const { return m_HandshakeState; }
	bool RequestPending() const { return m_HandshakeState == EHandshakeState::REQUESTED; }
	bool Accepted() const { return m_HandshakeState == EHandshakeState::ACCEPTED; }
	bool Denied() const { return m_HandshakeState == EHandshakeState::DENIED; }
	bool CompatDirectorActive() const { return m_CompatDirectorActive; }
	bool DirectorActive() const { return Accepted() || CompatDirectorActive(); }

	int Capabilities() const { return m_Capabilities; }
	EQmLiveDenyReason DenyReason() const { return m_DenyReason; }
	const char *DenyReasonText() const { return m_aDenyReasonText; }

	void SetReadyPending(bool ReadyPending) { m_ReadyPending = ReadyPending; }
	bool ReadyPending() const { return m_ReadyPending; }

	void SetCurrentTeam(int Team) { m_CurrentTeam = Team; }
	int CurrentTeam() const { return m_CurrentTeam; }

	void SetFollowClientId(int ClientId) { m_FollowClientId = ClientId; }
	int FollowClientId() const { return m_FollowClientId; }

	void SetDirectorMode(EDirectorMode Mode) { m_DirectorMode = Mode; }
	EDirectorMode DirectorMode() const { return m_DirectorMode; }

	void SetFreeview(bool Freeview) { m_Freeview = Freeview; }
	bool Freeview() const { return m_Freeview; }

	void SetFreeviewPosition(vec2 Position) { m_FreeviewPosition = Position; }
	vec2 FreeviewPosition() const { return m_FreeviewPosition; }

private:
	EHandshakeState m_HandshakeState = EHandshakeState::INACTIVE;
	EDirectorMode m_DirectorMode = EDirectorMode::FREEVIEW;
	EQmLiveDenyReason m_DenyReason = EQmLiveDenyReason::UNSUPPORTED;
	int m_Capabilities = 0;
	int m_CurrentTeam = -1;
	int m_FollowClientId = -1;
	bool m_Freeview = true;
	bool m_ReadyPending = false;
	bool m_CompatDirectorActive = false;
	vec2 m_FreeviewPosition = vec2(0.0f, 0.0f);
	char m_aDenyReasonText[32] = "";
};

#endif // ENGINE_SHARED_QM_LIVE_OBSERVER_SESSION_H
