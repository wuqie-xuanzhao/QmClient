// 请抬头享受阳光｜日子很好 我很我---------致咩子
/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_UIDOGFOOD_H
#define GAME_CLIENT_QMUI_UIDOGFOOD_H

#include "UiContext.h"

class CUIRect;

// Renders a debug page showing all 11 feat-003 widgets in their hover / focus
// / disabled / selected states. Called from RenderSettingsQmClient when the
// dbg_qm_ui_dogfood config is non-zero. This is the first visual verification
// of feat-002 (spring + easing curves) + feat-003 (design tokens + widgets).
void RenderQmUiDogfood(const IUiContext &Ctx, const CUIRect &Rect);

#endif
