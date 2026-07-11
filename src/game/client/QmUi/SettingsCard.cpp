/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "SettingsCard.h"

#include "UiContext.h"
#include "UiTheme.h"
#include "UiTokens.h"

#include <base/system.h>

#include <engine/graphics.h>
#include <engine/textrender.h>

#include <game/client/ui.h>

#include <algorithm>
#include <cmath>

namespace
{
	const SUiTheme &SettingsCardTheme(const IUiContext &Ctx, SUiTheme &Fallback)
	{
		Fallback = ResolveUiTheme(ColorHSLA(0.0f, 0.0f, 0.29f, 1.0f), 1.0f);
		return Ctx.m_pTheme != nullptr ? *Ctx.m_pTheme : Fallback;
	}

	void OffsetSettingsCardFrame(SSettingsCardFrame &Frame, const float OffsetX, const float OffsetY)
	{
		for(CUIRect *pRect : {&Frame.m_Rect, &Frame.m_HeaderRect, &Frame.m_TitleRect, &Frame.m_SubtitleRect, &Frame.m_HandleRect, &Frame.m_ContentRect})
		{
			pRect->x += OffsetX;
			pRect->y += OffsetY;
		}
	}

	void RenderCanonicalSettingsCardHandle(const IUiContext &Ctx, const CUIRect &HandleRect, const bool Active, const float DrawAlpha)
	{
		SUiTheme Fallback;
		const SUiTheme &Theme = SettingsCardTheme(Ctx, Fallback);
		ColorRGBA Color = Active ? Theme.m_Accent : Theme.m_TextSmall;
		Color.a *= DrawAlpha;
		const float Stroke = std::max(2.0f, 2.0f * Ctx.m_UiScale);
		const float Width = std::max(0.0f, HandleRect.w * 0.5f);
		for(int LineIndex = -1; LineIndex <= 1; ++LineIndex)
		{
			const CUIRect Line{HandleRect.x + (HandleRect.w - Width) * 0.5f, HandleRect.y + HandleRect.h * 0.5f + LineIndex * Stroke * 2.0f - Stroke * 0.5f, Width, Stroke};
			Line.Draw(Color, IGraphics::CORNER_ALL, Stroke * 0.5f);
		}
	}
}

SSettingsCardFrame SettingsCard(const IUiContext &Ctx, const CUIRect &Slot, const SSettingsCardSpec &Spec, const SSettingsCardVisualState &State, const SSettingsCardDeckVisualOptions &VisualOptions, const FSettingsCardMeasure &Measure, const FSettingsCardRender &Render)
{
	const float UiScale = Ctx.m_UiScale > 0.0f ? Ctx.m_UiScale : 1.0f;
	const float ContentWidth = std::max(0.0f, Slot.w - 2.0f * ui_token::settings::CARD_PADDING * UiScale);
	const float ContentHeight = Measure ? std::max(0.0f, Measure(ContentWidth)) : 0.0f;
	const SSettingsCardFrame Frame = BuildSettingsCardFrame(Slot, Spec, ContentHeight, UiScale);

	SSettingsCardVisualState DrawState = State;
	DrawState.m_Hovered = Ctx.m_pUi != nullptr && Ctx.m_pUi->MouseHovered(&Frame.m_Rect);
	SSettingsCardFrame DrawFrame = Frame;
	OffsetSettingsCardFrame(DrawFrame, State.m_DrawOffsetX, State.m_DrawOffsetY);

	SUiTheme Fallback;
	const SUiTheme &Theme = SettingsCardTheme(Ctx, Fallback);
	const float FeedbackAlpha = DrawState.m_DropFeedback ? 0.94f : DrawState.m_ReflowCompleteFeedback ? 0.97f :
													    1.0f;
	ColorRGBA Surface = DrawState.m_Hovered ? Theme.m_SurfaceHovered : Theme.m_Surface;
	Surface.a *= DrawState.m_DrawAlpha * FeedbackAlpha;
	const bool InteractionComplete = DrawState.m_DropFeedback || DrawState.m_ReflowCompleteFeedback;
	ColorRGBA Border = DrawState.m_Focused || InteractionComplete ? Theme.m_BorderFocused : DrawState.m_Hovered ? Theme.m_BorderHovered :
														      Theme.m_Border;
	Border.a *= DrawState.m_DrawAlpha;
	DrawFrame.m_Rect.Draw(Surface, IGraphics::CORNER_ALL, ui_token::settings::CARD_RADIUS * UiScale);
	DrawFrame.m_Rect.DrawOutline(Border);
	if(DrawState.m_Focused)
	{
		CUIRect FocusRect = DrawFrame.m_Rect;
		FocusRect.Margin(-Theme.m_FocusRingWidth * UiScale, &FocusRect);
		ColorRGBA FocusRing = Theme.m_FocusRing;
		FocusRing.a *= DrawState.m_DrawAlpha;
		FocusRect.DrawOutline(FocusRing);
	}

	if(Ctx.m_pUi != nullptr && Ctx.m_pTextRender != nullptr)
	{
		ColorRGBA TitleColor = Theme.m_TextTitle;
		if(VisualOptions.m_RainbowTitles)
		{
			const float TimePhase = (float)time_get() / (float)time_freq() * 0.08f;
			const float IdPhase = Spec.m_pStableId != nullptr ? (float)(str_quickhash(Spec.m_pStableId) & 0xffff) / 65535.0f : 0.0f;
			TitleColor = color_cast<ColorRGBA>(ColorHSLA(std::fmod(TimePhase + IdPhase, 1.0f), 0.75f, 0.65f, 1.0f));
		}
		TitleColor.a *= DrawState.m_DrawAlpha;
		Ctx.m_pTextRender->TextColor(TitleColor);
		Ctx.m_pUi->DoLabel(&DrawFrame.m_TitleRect, Spec.m_pTitle != nullptr ? Spec.m_pTitle : "", ui_token::font::TITLE * UiScale, TEXTALIGN_ML);
		if(Spec.m_pSubtitle != nullptr && (DrawState.m_Hovered || DrawState.m_Focused))
		{
			ColorRGBA SubtitleColor = Theme.m_TextSmall;
			SubtitleColor.a *= DrawState.m_DrawAlpha;
			Ctx.m_pTextRender->TextColor(SubtitleColor);
			Ctx.m_pUi->DoLabel(&DrawFrame.m_SubtitleRect, Spec.m_pSubtitle, ui_token::font::SMALL * UiScale, TEXTALIGN_ML);
		}
		Ctx.m_pTextRender->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	}

	RenderCanonicalSettingsCardHandle(Ctx, DrawFrame.m_HandleRect, DrawState.m_Hovered || DrawState.m_Focused || DrawState.m_Dragged, DrawState.m_DrawAlpha);
	if(Render)
		Render(DrawFrame.m_ContentRect);
	return Frame;
}
