/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_SETTINGSCARD_H
#define GAME_CLIENT_QMUI_SETTINGSCARD_H

#include "SettingsCardGeometry.h"

#include <functional>

struct IUiContext;

struct SSettingsCardDeckVisualOptions
{
	bool m_RainbowTitles = false;
};

struct SSettingsCardVisualState
{
	bool m_Hovered = false;
	bool m_HoverFeedbackEnabled = true;
	bool m_Focused = false;
	bool m_Dragged = false;
	bool m_Collapsed = false;
	bool m_DropFeedback = false;
	bool m_ReflowCompleteFeedback = false;
	bool m_ClipContent = false;
	float m_DrawOffsetX = 0.0f;
	float m_DrawOffsetY = 0.0f;
	float m_DrawAlpha = 1.0f;
};

using FSettingsCardMeasure = std::function<float(float ContentWidth)>;
using FSettingsCardRender = std::function<void(CUIRect ContentRect)>;
using FSettingsCardRenderMeasured = std::function<void(CUIRect &ContentRect)>;
using FSettingsCardPreLayoutInput = std::function<bool(CUIRect ContentRect)>;
using FSettingsCardHeaderAction = std::function<void(const SSettingsCardFrame &Frame, bool Collapsed)>;

SSettingsCardFrame SettingsCard(const IUiContext &Ctx, const CUIRect &Slot, const SSettingsCardSpec &Spec, const SSettingsCardVisualState &State, const SSettingsCardDeckVisualOptions &VisualOptions, const FSettingsCardMeasure &Measure, const FSettingsCardRender &Render, const FSettingsCardHeaderAction &HeaderAction = {}, const FSettingsCardRenderMeasured &RenderMeasured = {});
SSettingsCardFrame SettingsCard(const IUiContext &Ctx, const SSettingsCardFrame &Frame, const SSettingsCardSpec &Spec, const SSettingsCardVisualState &State, const SSettingsCardDeckVisualOptions &VisualOptions, const FSettingsCardRender &Render, const FSettingsCardHeaderAction &HeaderAction = {}, const FSettingsCardRenderMeasured &RenderMeasured = {});

#endif
