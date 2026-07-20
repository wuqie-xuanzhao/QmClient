/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "SettingsCard.h"

#include "SettingsPageLayout.h"
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

}

SSettingsCardFrame SettingsCard(const IUiContext &Ctx, const CUIRect &Slot, const SSettingsCardSpec &Spec, const SSettingsCardVisualState &State, const SSettingsCardDeckVisualOptions &VisualOptions, const FSettingsCardMeasure &Measure, const FSettingsCardRender &Render, const FSettingsCardHeaderAction &HeaderAction, const FSettingsCardRenderMeasured &RenderMeasured, bool *pPointerInside)
{
	const float UiScale = Ctx.m_UiScale > 0.0f ? Ctx.m_UiScale : 1.0f;
	const float ContentWidth = std::max(0.0f, Slot.w - 2.0f * ui_token::settings::CARD_PADDING * UiScale);
	const float ContentHeight = Measure ? std::max(0.0f, Measure(ContentWidth)) : 0.0f;
	const SSettingsCardFrame Frame = BuildSettingsCardFrame(Slot, Spec, ContentHeight, UiScale);
	return SettingsCard(Ctx, Frame, Spec, State, VisualOptions, Render, HeaderAction, RenderMeasured, pPointerInside);
}

SSettingsCardFrame SettingsCard(const IUiContext &Ctx, const SSettingsCardFrame &Frame, const SSettingsCardSpec &Spec, const SSettingsCardVisualState &State, const SSettingsCardDeckVisualOptions &VisualOptions, const FSettingsCardRender &Render, const FSettingsCardHeaderAction &HeaderAction, const FSettingsCardRenderMeasured &RenderMeasured, bool *pPointerInside)
{
	const float UiScale = Ctx.m_UiScale > 0.0f ? Ctx.m_UiScale : 1.0f;
	SSettingsCardVisualState DrawState = State;
	const SSettingsCardFrame DrawFrame = ResolveSettingsCardDrawFrame(Frame, State.m_DrawOffsetX, State.m_DrawOffsetY);
	DrawState.m_PointerInside = Ctx.m_pUi != nullptr && Ctx.m_pUi->MouseHovered(&DrawFrame.m_Rect);
	if(pPointerInside != nullptr)
		*pPointerInside = DrawState.m_PointerInside;
	DrawState.m_Hovered = State.m_HoverFeedbackEnabled && DrawState.m_PointerInside;

	SUiTheme Fallback;
	const SUiTheme &Theme = SettingsCardTheme(Ctx, Fallback);
	const bool DrawCardChrome = SettingsCardShouldDrawChrome(Ctx.m_pUi != nullptr && Ctx.m_pUi->RenderOnly());
	const ColorRGBA Surface = ResolveSettingsCardSurfaceColor(Theme.m_Surface, DrawState);
	// 重排与拖放反馈只改变边框，卡片背景透明度保持稳定，避免滚动或切页时闪烁。
	// 完成反馈只属于显式拖放。普通高度/布局变化不得改变卡片 chrome，
	// 否则半透明卡片在展开、折叠或首次布局时会表现为一次亮闪。
	const bool InteractionComplete = DrawState.m_DropFeedback;
	const bool DrawInteractionBorder = VisualOptions.m_AlwaysShowBorders;
	ColorRGBA Border = DrawState.m_Focused || InteractionComplete ? Theme.m_BorderFocused : DrawState.m_Hovered ? Theme.m_BorderHovered :
														      VisualOptions.m_BorderColor;
	Border.a *= DrawState.m_DrawAlpha;
	const float CardRadius = ui_token::settings::CARD_RADIUS * UiScale;
	// Focus/hover 只改变颜色，不能改变 Surface 的几何，否则边框获得焦点时
	// 会产生一次内缩跳变并重新触发用户看到的卡片闪动。
	const float BorderWidth = ResolveSettingsCardBorderWidth(UiScale);
	ExecuteSettingsCardChromeDraw(
		DrawCardChrome,
		DrawInteractionBorder,
		[&] { DrawFrame.m_Rect.Draw(Surface, IGraphics::CORNER_ALL, CardRadius); },
		[&] {
			// 先绘制完整边框，再绘制内缩的 Surface，避免 Surface 抗锯齿边缘与
			// 自定义描边重叠，产生截图中可见的双层边框。
			DrawFrame.m_Rect.Draw(Border, IGraphics::CORNER_ALL, CardRadius);
			CUIRect InnerSurface = DrawFrame.m_Rect;
			InnerSurface.Margin(BorderWidth, &InnerSurface);
			InnerSurface.Draw(Surface, IGraphics::CORNER_ALL, std::max(0.0f, CardRadius - BorderWidth));
		});

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
		const char *pSubtitle = Spec.m_pSubtitle;
		if(pSubtitle != nullptr && SettingsCardSubtitleVisible(DrawState.m_PointerInside, DrawState.m_SubtitleVisibleDuringMotion, DrawState.m_Focused))
		{
			ColorRGBA SubtitleColor = Theme.m_TextSmall;
			SubtitleColor.a *= DrawState.m_DrawAlpha;
			Ctx.m_pTextRender->TextColor(SubtitleColor);
			SLabelProperties SubtitleProps;
			SubtitleProps.m_MaxWidth = DrawFrame.m_SubtitleRect.w;
			SubtitleProps.m_EllipsisAtEnd = true;
			const float SubtitleSize = ResolveSettingsSmallFontSize(UiScale);
			Ctx.m_pUi->DoLabel(&DrawFrame.m_SubtitleRect, pSubtitle, SubtitleSize, TEXTALIGN_ML, SubtitleProps);
		}
		Ctx.m_pTextRender->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	}

	if(HeaderAction && DrawCardChrome)
		HeaderAction(DrawFrame, DrawState.m_Collapsed);
	const bool ClipContent = DrawState.m_ClipContent && Ctx.m_pUi != nullptr && DrawFrame.m_ContentRect.w > 0.0f && DrawFrame.m_ContentRect.h > 0.0f;
	if(ClipContent)
		Ctx.m_pUi->ClipEnable(&DrawFrame.m_ContentRect);
	if(RenderMeasured)
	{
		CUIRect ContentRect = DrawFrame.m_ContentRect;
		RenderMeasured(ContentRect);
	}
	else if(Render)
		Render(DrawFrame.m_ContentRect);
	if(ClipContent)
		Ctx.m_pUi->ClipDisable();
	return Frame;
}
