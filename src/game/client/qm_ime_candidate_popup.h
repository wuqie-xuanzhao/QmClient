/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QM_IME_CANDIDATE_POPUP_H
#define GAME_CLIENT_QM_IME_CANDIDATE_POPUP_H

#include <base/vmath.h>

#include <cstdint>
#include <string>
#include <vector>

class CGameClient;

struct SQmImePopupState
{
	bool m_Visible = false;
	bool m_Disabled = false;
	std::string m_Composition;
	std::vector<std::string> m_vCandidates;
	int m_SelectedIndex = -1;
	int m_PageIndex = -1;
	int m_PageCount = 0;
	vec2 m_AnchorScreen = vec2(0.0f, 0.0f);
	float m_LineHeightScreen = 0.0f;
};

class CQmImeCandidatePopup
{
public:
	void Reset();
	void Render(CGameClient *pGameClient, const SQmImePopupState &State);

private:
	SQmImePopupState m_LastState;
	bool m_WasVisible = false;
	uint64_t m_PresenceGeneration = 1;
};

#endif
