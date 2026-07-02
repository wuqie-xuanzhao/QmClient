/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_UICONTAINERS_H
#define GAME_CLIENT_QMUI_UICONTAINERS_H

#include "UiContext.h"
#include "UiTokens.h"

#include <engine/graphics.h>

#include <game/client/ui.h>
#include <game/client/ui_rect.h>

namespace ui_widget
{

	struct SCardProps
	{
		float m_Padding = ui_token::spacing::MD;
		float m_Radius = ui_token::radius::CARD;
		int m_Elevation = 1; // 0 = flat, 1 = medium shadow, 2 = high shadow
		const char *m_pTitle = nullptr;
		float m_TitleFontSize = ui_token::font::HEADLINE;
		bool m_DrawBorder = false;
	};

	// Renders a glass-surface card (drop shadow + main fill + 1px top highlight,
	// optional title and border). After drawing the chrome, invokes Body(Content)
	// with the inner content rect already inset by m_Padding.
	template<typename BodyFn>
	void DrawCard(const IUiContext &Ctx, const CUIRect &Rect, const SCardProps &Props, BodyFn &&Body)
	{
		// 1) Shadow
		if(Props.m_Elevation > 0)
		{
			const float ShadowX = Props.m_Elevation >= 2 ? ui_token::elevation::SHADOW_X_HIGH : ui_token::elevation::SHADOW_X_MED;
			const float ShadowY = Props.m_Elevation >= 2 ? ui_token::elevation::SHADOW_Y_HIGH : ui_token::elevation::SHADOW_Y_MED;
			CUIRect Shadow = Rect;
			Shadow.x += ShadowX;
			Shadow.y += ShadowY;
			Shadow.Draw(ui_token::color::SURFACE_SHADOW, IGraphics::CORNER_ALL, Props.m_Radius);
		}

		// 2) Card fill (glass)
		Rect.Draw(ui_token::color::SURFACE_GLASS, IGraphics::CORNER_ALL, Props.m_Radius);

		// 3) Top 1px highlight (sub-pixel sliver near upper edge for the lift cue)
		CUIRect Highlight = Rect;
		Highlight.h = 1.0f;
		Highlight.x += Props.m_Radius * 0.5f;
		Highlight.w -= Props.m_Radius;
		Highlight.Draw(ui_token::color::SURFACE_HIGHLIGHT, IGraphics::CORNER_T, 0.0f);

		// 4) Optional border
		if(Props.m_DrawBorder)
		{
			// Simulate a 1px border by drawing a slightly inset transparent fill on
			// top of the card using BORDER_SUBTLE alpha — DDNet has no stroke
			// primitive so this is an inexpensive approximation.
			CUIRect Border = Rect;
			Border.Margin(0.5f, &Border);
			Border.Draw(ui_token::color::BORDER_SUBTLE, IGraphics::CORNER_ALL, Props.m_Radius - 0.5f);
		}

		// 5) Content rect
		CUIRect Content = Rect;
		Content.Margin(Props.m_Padding, &Content);

		if(Props.m_pTitle != nullptr && Ctx.m_pUi != nullptr)
		{
			CUIRect Title;
			Content.HSplitTop(Props.m_TitleFontSize + ui_token::spacing::XS, &Title, &Content);
			Content.HSplitTop(ui_token::spacing::SM, nullptr, &Content);
			Ctx.m_pUi->DoLabel(&Title, Props.m_pTitle, Props.m_TitleFontSize, TEXTALIGN_ML);
		}

		Body(Content);
	}

} // namespace ui_widget

#endif
