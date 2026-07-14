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

}

SSettingsCardFrame SettingsCard(const IUiContext &Ctx, const CUIRect &Slot, const SSettingsCardSpec &Spec, const SSettingsCardVisualState &State, const SSettingsCardDeckVisualOptions &VisualOptions, const FSettingsCardMeasure &Measure, const FSettingsCardRender &Render, const FSettingsCardHeaderAction &HeaderAction, const FSettingsCardRenderMeasured &RenderMeasured)
{
	const float UiScale = Ctx.m_UiScale > 0.0f ? Ctx.m_UiScale : 1.0f;
	const float ContentWidth = std::max(0.0f, Slot.w - 2.0f * ui_token::settings::CARD_PADDING * UiScale);
	const float ContentHeight = Measure ? std::max(0.0f, Measure(ContentWidth)) : 0.0f;
	const SSettingsCardFrame Frame = BuildSettingsCardFrame(Slot, Spec, ContentHeight, UiScale);
	return SettingsCard(Ctx, Frame, Spec, State, VisualOptions, Render, HeaderAction, RenderMeasured);
}

SSettingsCardFrame SettingsCard(const IUiContext &Ctx, const SSettingsCardFrame &Frame, const SSettingsCardSpec &Spec, const SSettingsCardVisualState &State, const SSettingsCardDeckVisualOptions &VisualOptions, const FSettingsCardRender &Render, const FSettingsCardHeaderAction &HeaderAction, const FSettingsCardRenderMeasured &RenderMeasured)
{
	const float UiScale = Ctx.m_UiScale > 0.0f ? Ctx.m_UiScale : 1.0f;
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
	const float CardRadius = ui_token::settings::CARD_RADIUS * UiScale;
	const float BorderBaseWidth = DrawState.m_Focused ? 3.0f : 2.0f;
	const float BorderWidth = std::max(BorderBaseWidth, BorderBaseWidth * UiScale);
	const CUIRect BorderRect = DrawFrame.m_Rect;
	BorderRect.Draw(Border, IGraphics::CORNER_ALL, CardRadius);
	CUIRect SurfaceRect;
	BorderRect.Margin(BorderWidth, &SurfaceRect);
	const float InnerRadius = std::max(0.0f, CardRadius - BorderWidth);
	SurfaceRect.Draw(Surface, IGraphics::CORNER_ALL, InnerRadius);

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
		SLabelProperties TitleProps;
		TitleProps.m_MaxWidth = DrawFrame.m_TitleRect.w;
		TitleProps.m_EllipsisAtEnd = true;
		Ctx.m_pUi->DoLabel(&DrawFrame.m_TitleRect, Spec.m_pTitle != nullptr ? Spec.m_pTitle : "", ui_token::font::TITLE * UiScale, TEXTALIGN_ML, TitleProps);
		if(Spec.m_pSubtitle != nullptr && (DrawState.m_Hovered || DrawState.m_Focused))
		{
			ColorRGBA SubtitleColor = Theme.m_TextSmall;
			SubtitleColor.a *= DrawState.m_DrawAlpha;
			Ctx.m_pTextRender->TextColor(SubtitleColor);
			SLabelProperties SubtitleProps;
			SubtitleProps.m_MaxWidth = DrawFrame.m_SubtitleRect.w;
			SubtitleProps.m_EllipsisAtEnd = true;
			Ctx.m_pUi->DoLabel(&DrawFrame.m_SubtitleRect, Spec.m_pSubtitle, ui_token::font::SMALL * UiScale, TEXTALIGN_ML, SubtitleProps);
		}
		Ctx.m_pTextRender->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	}

	if(HeaderAction)
		HeaderAction(DrawFrame, DrawState.m_Collapsed);
	if(RenderMeasured)
	{
		CUIRect ContentRect = DrawFrame.m_ContentRect;
		RenderMeasured(ContentRect);
	}
	else if(Render)
		Render(DrawFrame.m_ContentRect);
	return Frame;
}
