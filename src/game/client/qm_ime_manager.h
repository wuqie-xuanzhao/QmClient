// 请抬头享受阳光｜日子很好 我很我---------致咩子
/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QM_IME_MANAGER_H
#define GAME_CLIENT_QM_IME_MANAGER_H

#include "qm_ime_candidate_popup.h"

class CGameClient;

enum class EQmImeCandidateRenderAction
{
	VALIDATE_ONLY = 0,
	LEGACY,
	POPUP,
};

inline EQmImeCandidateRenderAction QmImeComputeCandidateRenderAction(bool SupportsCustomCandidateUi, int NewImeMode)
{
	if(!SupportsCustomCandidateUi)
		return EQmImeCandidateRenderAction::VALIDATE_ONLY;

	return NewImeMode == 0 ? EQmImeCandidateRenderAction::LEGACY : EQmImeCandidateRenderAction::POPUP;
}

class CQmImeBlocker
{
public:
	bool WantsTextInput(const CGameClient *pGameClient) const;

private:
	bool HasTextFocus(const CGameClient *pGameClient) const;
	bool IsGameplayOverlayActive(const CGameClient *pGameClient) const;
};

class CQmImeManager
{
public:
	void Init(CGameClient *pGameClient);
	void OnFrame();
	void RenderCandidatePopup();
	void Reset();

private:
	SQmImePopupState BuildPopupState() const;

	CGameClient *m_pGameClient = nullptr;
	CQmImeBlocker m_Blocker;
	CQmImeCandidatePopup m_CandidatePopup;
	bool m_TextInputWanted = false;
};

#endif
