/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_UIOVERLAYS_H
#define GAME_CLIENT_QMUI_UIOVERLAYS_H

#include "QmAnimResolve.h"
#include "UiContainers.h"
#include "UiContext.h"
#include "UiTokens.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/tooltips.h>
#include <game/client/ui.h>
#include <game/client/ui_rect.h>

namespace ui_widget
{

	// Thin shim over CTooltips::DoToolTip. Centralised here so dogfood/widget
	// callers can address tooltips without pulling tooltips.h directly.
	inline void Tooltip(const IUiContext &Ctx, const void *pId, const CUIRect &Anchor, const char *pText, float WidthHint = -1.0f)
	{
		if(Ctx.m_pTooltips != nullptr && pText != nullptr)
			Ctx.m_pTooltips->DoToolTip(pId, &Anchor, pText, WidthHint);
	}

	struct SModalProps
	{
		float m_Width = 480.0f;
		float m_Height = 280.0f;
		const char *m_pTitle = nullptr;
		bool m_EscToClose = true;
	};

	struct SToastProps
	{
		float m_Width = 280.0f;
		float m_Height = 38.0f;
		float m_Margin = ui_token::spacing::LG;
		const char *m_pText = nullptr;
	};

	struct SToastState
	{
		bool m_WasVisible = false;
	};

	inline bool Toast(const IUiContext &Ctx, const void *pId, SToastState *pState, bool Visible, const CUIRect &ScreenRect, const SToastProps &Props)
	{
		if(Ctx.m_pUi == nullptr || Props.m_pText == nullptr || Props.m_pText[0] == '\0')
			return false;

		CUIRect Target;
		Target.w = Props.m_Width;
		Target.h = Props.m_Height;
		Target.x = ScreenRect.x + (ScreenRect.w - Target.w) * 0.5f;
		Target.y = ScreenRect.y + ScreenRect.h - Props.m_Margin - Target.h;

		const float HiddenY = Target.y + Target.h + Props.m_Margin;
		float CurrentY = Visible ? Target.y : HiddenY;
		float Alpha = Visible ? 1.0f : 0.0f;

		if(Ctx.m_pAnim != nullptr)
		{
			const uint64_t NodeKey = BuildUiAnimNodeKey(Ctx.m_ScopeHash, reinterpret_cast<uint64_t>(pId));
			if(Visible && pState != nullptr && !pState->m_WasVisible)
			{
				Ctx.m_pAnim->SetValue(NodeKey, EUiAnimProperty::POS_Y, HiddenY);
				Ctx.m_pAnim->SetValue(NodeKey, EUiAnimProperty::ALPHA, 0.0f);
			}
			CurrentY = ResolveUiAnimValue(*Ctx.m_pAnim, NodeKey, EUiAnimProperty::POS_Y, Visible ? Target.y : HiddenY, ui_token::motion::TOAST_SLIDE.m_DurationSec, ui_token::motion::TOAST_SLIDE.m_Easing);
			Alpha = ResolveUiAnimValue(*Ctx.m_pAnim, NodeKey, EUiAnimProperty::ALPHA, Visible ? 1.0f : 0.0f, ui_token::motion::TOAST_SLIDE.m_DurationSec, ui_token::motion::TOAST_SLIDE.m_Easing);
			if(!Visible && Alpha <= 0.01f && !Ctx.m_pAnim->HasActiveAnimation(NodeKey, EUiAnimProperty::ALPHA))
			{
				if(pState != nullptr)
					pState->m_WasVisible = false;
				return false;
			}
		}
		else if(!Visible)
		{
			if(pState != nullptr)
				pState->m_WasVisible = false;
			return false;
		}

		if(pState != nullptr)
			pState->m_WasVisible = Visible;

		CUIRect ToastRect = Target;
		ToastRect.y = CurrentY;

		ColorRGBA Shadow = ui_token::color::SURFACE_SHADOW;
		Shadow.a *= Alpha;
		CUIRect ShadowRect = ToastRect;
		ShadowRect.x += ui_token::elevation::SHADOW_X_HIGH;
		ShadowRect.y += ui_token::elevation::SHADOW_Y_HIGH;
		ShadowRect.Draw(Shadow, IGraphics::CORNER_ALL, ui_token::radius::BASE);

		ColorRGBA Bg = ui_token::color::SURFACE_ELEVATED;
		Bg.a *= Alpha;
		ToastRect.Draw(Bg, IGraphics::CORNER_ALL, ui_token::radius::BASE);

		ColorRGBA Text = ui_token::color::TEXT_PRIMARY;
		Text.a *= Alpha;
		if(Ctx.m_pTextRender != nullptr)
			Ctx.m_pTextRender->TextColor(Text);
		CUIRect Label;
		ToastRect.Margin(ui_token::spacing::MD, &Label);
		Ctx.m_pUi->DoLabel(&Label, Props.m_pText, ui_token::font::BODY, TEXTALIGN_ML);
		if(Ctx.m_pTextRender != nullptr)
			Ctx.m_pTextRender->TextColor(Ctx.m_pTextRender->DefaultTextColor());

		return true;
	}

	// Centered modal with overlay backdrop, scale-in animation and optional ESC
	// dismissal. Renders inline when *pOpen is true; Body is invoked with the
	// inner content rect (already inset). Caller owns the *pOpen flag.
	template<typename BodyFn>
	bool Modal(const IUiContext &Ctx, const void *pId, bool *pOpen, const CUIRect &ScreenRect, const SModalProps &Props, BodyFn &&Body)
	{
		if(Ctx.m_pUi == nullptr || pOpen == nullptr || !*pOpen)
			return false;

		// Backdrop
		ScreenRect.Draw(ui_token::color::SURFACE_OVERLAY, IGraphics::CORNER_NONE, 0.0f);

		// Compute centered rect at target size
		CUIRect Centered;
		Centered.w = Props.m_Width;
		Centered.h = Props.m_Height;
		Centered.x = ScreenRect.x + (ScreenRect.w - Centered.w) * 0.5f;
		Centered.y = ScreenRect.y + (ScreenRect.h - Centered.h) * 0.5f;

		// Scale-in animation. Drive SCALE from 0.92 → 1.0 on open via SPRING for a
		// soft pop; collapse back when closed (handled by GetValue going to 0 once
		// *pOpen=false on next call — though we only render while open).
		float Scale = 1.0f;
		if(Ctx.m_pAnim != nullptr)
		{
			const uint64_t NodeKey = BuildUiAnimNodeKey(Ctx.m_ScopeHash, reinterpret_cast<uint64_t>(pId));
			Scale = ResolveUiAnimValue(*Ctx.m_pAnim, NodeKey, EUiAnimProperty::SCALE, 1.0f, ui_token::motion::MODAL_IN.m_DurationSec, ui_token::motion::MODAL_IN.m_Easing);
			// First frame after open we need to seed Scale at 0.92 so the spring
			// has somewhere to travel from. Done by snapping if very close to 1
			// without prior history.
			if(g_Config.m_QmUiMotionLevel != 0 && Scale > 0.99f && !Ctx.m_pAnim->HasActiveAnimation(NodeKey, EUiAnimProperty::SCALE))
			{
				Ctx.m_pAnim->SetValue(NodeKey, EUiAnimProperty::SCALE, 0.92f);
				Scale = 0.92f;
			}
		}

		CUIRect Scaled = Centered;
		const float DeltaW = Centered.w * (1.0f - Scale);
		const float DeltaH = Centered.h * (1.0f - Scale);
		Scaled.x += DeltaW * 0.5f;
		Scaled.y += DeltaH * 0.5f;
		Scaled.w -= DeltaW;
		Scaled.h -= DeltaH;

		SCardProps CardProps;
		CardProps.m_pTitle = Props.m_pTitle;
		CardProps.m_Elevation = 2;
		DrawCard(Ctx, Scaled, CardProps, [&](CUIRect &Content) {
			Body(Content);
		});

		if(Props.m_EscToClose && Ctx.m_pUi->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
			*pOpen = false;

		return *pOpen;
	}

} // namespace ui_widget

#endif
