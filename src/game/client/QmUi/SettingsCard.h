/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_SETTINGSCARD_H
#define GAME_CLIENT_QMUI_SETTINGSCARD_H

#include "SettingsCardGeometry.h"

#include <cmath>
#include <functional>

struct IUiContext;

struct SSettingsCardDeckVisualOptions
{
	bool m_RainbowTitles = false;
	bool m_AlwaysShowBorders = true;
	ColorRGBA m_BorderColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.10f);
};

struct SSettingsCardVisualState
{
	bool m_Hovered = false;
	bool m_PointerInside = false;
	bool m_SubtitleVisibleDuringMotion = false;
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

inline bool SettingsCardSubtitleVisible(const bool PointerInside, const bool SubtitleVisibleDuringMotion, const bool Focused)
{
	return PointerInside || SubtitleVisibleDuringMotion || Focused;
}

inline bool ResolveSettingsCardSubtitleMotionLatch(const bool PointerInsideCurrentFrame, const bool MotionActive, const bool MotionWasActive, const bool VisibleDuringMotion)
{
	if(!MotionActive)
		return false;
	if(!MotionWasActive)
		return PointerInsideCurrentFrame;
	return VisibleDuringMotion;
}

inline bool SettingsCardDeckGeometryMoved(const bool Initialized, const float LastY, const float LastHeight, const float CurrentY, const float CurrentHeight)
{
	return Initialized && (std::abs(LastY - CurrentY) > 0.001f || std::abs(LastHeight - CurrentHeight) > 0.001f);
}

inline bool SettingsCardInteractionBorderVisible(const SSettingsCardVisualState &State)
{
	return State.m_Hovered || State.m_Focused || State.m_DropFeedback;
}

inline bool SettingsCardShouldDrawChrome(const bool RenderOnly)
{
	return !RenderOnly;
}

template<typename TDrawSurface, typename TDrawBorderedSurface>
inline void ExecuteSettingsCardChromeDraw(const bool DrawChrome, const bool DrawBorder, TDrawSurface &&DrawSurface, TDrawBorderedSurface &&DrawBorderedSurface)
{
	if(!DrawChrome)
		return;
	if(DrawBorder)
		DrawBorderedSurface();
	else
		DrawSurface();
}

inline float ResolveSettingsCardBorderWidth(const float UiScale)
{
	return std::max(2.0f, 2.0f * std::max(0.0f, UiScale));
}

inline CUIRect ResolveSettingsCardInteractionBorderRect(const CUIRect &SurfaceRect, const float BorderWidth)
{
	const float MaxInset = std::max(0.0f, std::min(SurfaceRect.w, SurfaceRect.h) * 0.5f);
	const float Inset = std::clamp(std::max(0.0f, BorderWidth) * 0.5f, 0.0f, MaxInset);
	return {SurfaceRect.x + Inset, SurfaceRect.y + Inset, std::max(0.0f, SurfaceRect.w - 2.0f * Inset), std::max(0.0f, SurfaceRect.h - 2.0f * Inset)};
}

inline SSettingsCardFrame ResolveSettingsCardDrawFrame(SSettingsCardFrame Frame, const float OffsetX, const float OffsetY)
{
	for(CUIRect *pRect : {&Frame.m_Rect, &Frame.m_HeaderRect, &Frame.m_TitleRect, &Frame.m_SubtitleRect, &Frame.m_HandleRect, &Frame.m_ContentRect})
	{
		pRect->x += OffsetX;
		pRect->y += OffsetY;
	}
	return Frame;
}

inline ColorRGBA ResolveSettingsCardSurfaceColor(ColorRGBA Surface, const SSettingsCardVisualState &State)
{
	// Hover、焦点与拖放反馈只属于边框；卡片内部仅跟随整个 Deck 的绘制透明度。
	Surface.a *= State.m_DrawAlpha;
	return Surface;
}

using FSettingsCardMeasure = std::function<float(float ContentWidth)>;
using FSettingsCardRender = std::function<void(CUIRect ContentRect)>;
using FSettingsCardRenderMeasured = std::function<void(CUIRect &ContentRect)>;
using FSettingsCardPreLayoutInput = std::function<bool(CUIRect ContentRect)>;
using FSettingsCardPreLayoutHeaderInput = std::function<bool(const SSettingsCardFrame &Frame, bool Collapsed)>;
using FSettingsCardHeaderAction = std::function<void(const SSettingsCardFrame &Frame, bool Collapsed)>;

SSettingsCardFrame SettingsCard(const IUiContext &Ctx, const CUIRect &Slot, const SSettingsCardSpec &Spec, const SSettingsCardVisualState &State, const SSettingsCardDeckVisualOptions &VisualOptions, const FSettingsCardMeasure &Measure, const FSettingsCardRender &Render, const FSettingsCardHeaderAction &HeaderAction = {}, const FSettingsCardRenderMeasured &RenderMeasured = {}, bool *pPointerInside = nullptr);
SSettingsCardFrame SettingsCard(const IUiContext &Ctx, const SSettingsCardFrame &Frame, const SSettingsCardSpec &Spec, const SSettingsCardVisualState &State, const SSettingsCardDeckVisualOptions &VisualOptions, const FSettingsCardRender &Render, const FSettingsCardHeaderAction &HeaderAction = {}, const FSettingsCardRenderMeasured &RenderMeasured = {}, bool *pPointerInside = nullptr);

#endif
