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
#include <array>
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

	void DrawSettingsCardBorderRing(IGraphics *pGraphics, const CUIRect &Rect, const ColorRGBA &Color, const float Width, const float Radius)
	{
		if(pGraphics == nullptr || Rect.w <= 0.0f || Rect.h <= 0.0f)
			return;
		const float BorderWidth = std::clamp(Width, 0.0f, std::min(Rect.w, Rect.h) * 0.5f);
		if(BorderWidth <= 0.0f)
			return;

		const float OuterRadius = std::clamp(Radius, 0.0f, std::min(Rect.w, Rect.h) * 0.5f);
		const float InnerRadius = std::max(0.0f, OuterRadius - BorderWidth);
		std::array<IGraphics::CQuadItem, 4> aEdges;
		int NumEdges = 0;
		if(Rect.w > 2.0f * OuterRadius)
		{
			aEdges[NumEdges++] = IGraphics::CQuadItem(Rect.x + OuterRadius, Rect.y, Rect.w - 2.0f * OuterRadius, BorderWidth);
			aEdges[NumEdges++] = IGraphics::CQuadItem(Rect.x + OuterRadius, Rect.y + Rect.h - BorderWidth, Rect.w - 2.0f * OuterRadius, BorderWidth);
		}
		if(Rect.h > 2.0f * OuterRadius)
		{
			aEdges[NumEdges++] = IGraphics::CQuadItem(Rect.x, Rect.y + OuterRadius, BorderWidth, Rect.h - 2.0f * OuterRadius);
			aEdges[NumEdges++] = IGraphics::CQuadItem(Rect.x + Rect.w - BorderWidth, Rect.y + OuterRadius, BorderWidth, Rect.h - 2.0f * OuterRadius);
		}

		constexpr int CornerSegments = 3;
		constexpr float Pi = 3.14159265358979323846f;
		std::array<IGraphics::CFreeformItem, CornerSegments * 4> aCorners;
		int NumCorners = 0;
		const auto AddCornerSegment = [&](const float CenterX, const float CenterY, const float XDirection, const float YDirection, const float AngleStart, const float AngleEnd) {
			const vec2 InnerStart(CenterX + XDirection * std::cos(AngleStart) * InnerRadius, CenterY + YDirection * std::sin(AngleStart) * InnerRadius);
			const vec2 OuterStart(CenterX + XDirection * std::cos(AngleStart) * OuterRadius, CenterY + YDirection * std::sin(AngleStart) * OuterRadius);
			const vec2 OuterEnd(CenterX + XDirection * std::cos(AngleEnd) * OuterRadius, CenterY + YDirection * std::sin(AngleEnd) * OuterRadius);
			const vec2 InnerEnd(CenterX + XDirection * std::cos(AngleEnd) * InnerRadius, CenterY + YDirection * std::sin(AngleEnd) * InnerRadius);
			aCorners[NumCorners++] = IGraphics::CFreeformItem(InnerStart, OuterStart, OuterEnd, InnerEnd);
		};
		for(int Segment = 0; Segment < CornerSegments; ++Segment)
		{
			const float AngleStart = Segment * Pi * 0.5f / CornerSegments;
			const float AngleEnd = (Segment + 1) * Pi * 0.5f / CornerSegments;
			AddCornerSegment(Rect.x + OuterRadius, Rect.y + OuterRadius, -1.0f, -1.0f, AngleStart, AngleEnd);
			AddCornerSegment(Rect.x + Rect.w - OuterRadius, Rect.y + OuterRadius, 1.0f, -1.0f, AngleStart, AngleEnd);
			AddCornerSegment(Rect.x + OuterRadius, Rect.y + Rect.h - OuterRadius, -1.0f, 1.0f, AngleStart, AngleEnd);
			AddCornerSegment(Rect.x + Rect.w - OuterRadius, Rect.y + Rect.h - OuterRadius, 1.0f, 1.0f, AngleStart, AngleEnd);
		}

		pGraphics->TextureClear();
		pGraphics->QuadsBegin();
		pGraphics->SetColor(Color);
		if(NumEdges > 0)
			pGraphics->QuadsDrawTL(aEdges.data(), NumEdges);
		if(NumCorners > 0)
			pGraphics->QuadsDrawFreeform(aCorners.data(), NumCorners);
		pGraphics->QuadsEnd();
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
	SSettingsCardFrame DrawFrame = Frame;
	OffsetSettingsCardFrame(DrawFrame, State.m_DrawOffsetX, State.m_DrawOffsetY);
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
	const bool DrawInteractionBorder = SettingsCardInteractionBorderVisible(DrawState);
	ColorRGBA Border = DrawState.m_Focused || InteractionComplete ? Theme.m_BorderFocused : DrawState.m_Hovered ? Theme.m_BorderHovered :
														      Theme.m_Border;
	Border.a *= DrawState.m_DrawAlpha;
	const float CardRadius = ui_token::settings::CARD_RADIUS * UiScale;
	const float BorderBaseWidth = DrawState.m_Focused ? 3.0f : 2.0f;
	const float BorderWidth = std::max(BorderBaseWidth, BorderBaseWidth * UiScale);
	const CUIRect BorderRect = ResolveSettingsCardInteractionBorderRect(DrawFrame.m_Rect, BorderWidth);
	const float BorderRadius = std::max(0.0f, CardRadius - BorderWidth * 0.5f);
	if(DrawCardChrome)
	{
		DrawFrame.m_Rect.Draw(Surface, IGraphics::CORNER_ALL, CardRadius);
		// 静止态只绘制一次带抗锯齿的圆角 Surface。若再叠加默认圆角描边，
		// Surface 的外沿抗锯齿会与描边混合成双层阴影。描边仅用于明确交互状态。
		if(DrawInteractionBorder)
			DrawSettingsCardBorderRing(Ctx.m_pUi != nullptr ? Ctx.m_pUi->Graphics() : nullptr, BorderRect, Border, BorderWidth, BorderRadius);
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
