/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_SETTINGSCARDGEOMETRY_H
#define GAME_CLIENT_QMUI_SETTINGSCARDGEOMETRY_H

#include <game/client/ui_rect.h>

struct SSettingsCardSpec
{
	const char *m_pStableId = nullptr;
	const char *m_pTitle = nullptr;
	const char *m_pSubtitle = nullptr;
};

struct SSettingsCardFrame
{
	CUIRect m_Rect;
	CUIRect m_HeaderRect;
	CUIRect m_TitleRect;
	CUIRect m_SubtitleRect;
	CUIRect m_HandleRect;
	CUIRect m_ContentRect;

	const CUIRect &DisplayRect() const { return m_Rect; }
	const CUIRect &HitRect() const { return m_Rect; }
	const CUIRect &DragRect() const { return m_Rect; }
	const CUIRect &ProxySourceRect() const { return m_Rect; }
};

struct SCardMotionSpec
{
	float m_EntryDistance = 0.0f;
	float m_EntryDuration = 0.0f;
	float m_ReflowDuration = 0.0f;
	float m_DropFeedbackDuration = 0.08f;
	float m_ReflowCompleteFeedbackDuration = 0.08f;
	bool m_DecorativeMotion = false;
	bool m_KeepDragProxy = true;
	bool m_KeepDropFeedback = true;
	bool m_KeepReflowCompleteFeedback = true;
};

SSettingsCardFrame BuildSettingsCardFrame(const CUIRect &Slot, const SSettingsCardSpec &Spec, float ContentHeight, float UiScale);
SCardMotionSpec ResolveCardMotionSpec(int MotionLevel, bool ExtraAnimations);

#endif
