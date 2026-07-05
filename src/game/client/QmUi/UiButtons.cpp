/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "UiButtons.h"

#include "QmAnimResolve.h"
#include "UiTokens.h"

#include <engine/graphics.h>

#include <game/client/components/menus.h>
#include <game/client/qm_icon_manager.h>
#include <game/client/ui.h>
#include <game/client/ui_rect.h>

namespace ui_widget
{

	namespace
	{
		bool DoStyledButton(const IUiContext &Ctx, CButtonContainer *pBtn, const char *pText, const CUIRect &Rect, bool Disabled, const ColorRGBA &Idle, const ColorRGBA &Hover, bool DrawBorder)
		{
			if(Ctx.m_pUi == nullptr || Ctx.m_pMenus == nullptr || pBtn == nullptr)
				return false;

			if(Disabled)
			{
				Rect.Draw(ui_token::color::BORDER_SUBTLE, IGraphics::CORNER_ALL, ui_token::radius::BASE);
				SLabelProperties LabelProps;
				Ctx.m_pUi->DoLabel(&Rect, pText, ui_token::font::BODY, TEXTALIGN_MC, LabelProps);
				return false;
			}

			// Compute current frame color via the v2 animation runtime so hover/leave
			// transitions ease through Steam-ish hover blue.
			const bool HoverPrev = Ctx.m_pUi->HotItem() == static_cast<const void *>(pBtn);
			const ColorRGBA Target = HoverPrev ? Hover : Idle;
			ColorRGBA Resolved = Target;
			if(Ctx.m_pAnim != nullptr)
			{
				const uint64_t NodeKey = BuildUiAnimNodeKey(Ctx.m_ScopeHash, reinterpret_cast<uint64_t>(pBtn));
				Resolved = ResolveUiAnimValueColor(*Ctx.m_pAnim, NodeKey, Target, ui_token::motion::BTN_HOVER.m_DurationSec, ui_token::motion::BTN_HOVER.m_Easing);
			}

			if(DrawBorder)
			{
				// Border emulated by an underlay one pixel larger; cheap and good
				// enough at our scales.
				CUIRect Underlay = Rect;
				Underlay.Margin(-0.5f, &Underlay);
				Underlay.Draw(ui_token::color::BORDER_SUBTLE, IGraphics::CORNER_ALL, ui_token::radius::BASE + 0.5f);
			}

			const int Result = Ctx.m_pMenus->DoButton_Menu(pBtn, pText, 0, &Rect, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, ui_token::radius::BASE, 0.0f, Resolved);
			return Result != 0;
		}
	} // namespace

	bool PrimaryButton(const IUiContext &Ctx, CButtonContainer *pBtn, const char *pText, const CUIRect &Rect, bool Disabled)
	{
		return DoStyledButton(Ctx, pBtn, pText, Rect, Disabled, ui_token::color::ACCENT_PRIMARY_DIM, ui_token::color::ACCENT_PRIMARY_HOVER, false);
	}

	bool SecondaryButton(const IUiContext &Ctx, CButtonContainer *pBtn, const char *pText, const CUIRect &Rect, bool Disabled)
	{
		// Idle is fully transparent so only the border shows; on hover, tint
		// gently toward ACCENT_PRIMARY_DIM.
		const ColorRGBA Idle{0.0f, 0.0f, 0.0f, 0.0f};
		return DoStyledButton(Ctx, pBtn, pText, Rect, Disabled, Idle, ui_token::color::ACCENT_PRIMARY_DIM, true);
	}

	bool IconButton(const IUiContext &Ctx, CButtonContainer *pBtn, const char *pIcon, const CUIRect &Rect, bool Disabled)
	{
		if(Ctx.m_pUi == nullptr || pBtn == nullptr)
			return false;

		const bool HoverPrev = Ctx.m_pUi->HotItem() == static_cast<const void *>(pBtn);
		const ColorRGBA Target = HoverPrev ? ui_token::color::ACCENT_PRIMARY_DIM : ColorRGBA{0.0f, 0.0f, 0.0f, 0.0f};
		ColorRGBA BgColor = Target;
		if(Ctx.m_pAnim != nullptr)
		{
			const uint64_t NodeKey = BuildUiAnimNodeKey(Ctx.m_ScopeHash, reinterpret_cast<uint64_t>(pBtn));
			BgColor = ResolveUiAnimValueColor(*Ctx.m_pAnim, NodeKey, Target, ui_token::motion::BTN_HOVER.m_DurationSec, ui_token::motion::BTN_HOVER.m_Easing);
		}

		const int Result = Ctx.m_pUi->DoButton_FontIcon(pBtn, pIcon, 0, &Rect, BUTTONFLAG_LEFT, IGraphics::CORNER_ALL, !Disabled, BgColor);
		return Result != 0;
	}

	bool IconButton(const IUiContext &Ctx, CButtonContainer *pBtn, EQmIcon Icon, const char *pFallbackIcon, const CUIRect &Rect, bool Disabled)
	{
		if(Ctx.m_pUi == nullptr || pBtn == nullptr)
			return false;

		const bool HoverPrev = Ctx.m_pUi->HotItem() == static_cast<const void *>(pBtn);
		const ColorRGBA Target = HoverPrev ? ui_token::color::ACCENT_PRIMARY_DIM : ColorRGBA{0.0f, 0.0f, 0.0f, 0.0f};
		ColorRGBA BgColor = Target;
		if(Ctx.m_pAnim != nullptr)
		{
			const uint64_t NodeKey = BuildUiAnimNodeKey(Ctx.m_ScopeHash, reinterpret_cast<uint64_t>(pBtn));
			BgColor = ResolveUiAnimValueColor(*Ctx.m_pAnim, NodeKey, Target, ui_token::motion::BTN_HOVER.m_DurationSec, ui_token::motion::BTN_HOVER.m_Easing);
		}

		Rect.Draw(BgColor, IGraphics::CORNER_ALL, ui_token::radius::BASE);
		const int Result = Disabled ? 0 : Ctx.m_pUi->DoButtonLogic(pBtn, 0, &Rect, BUTTONFLAG_LEFT);

		const float IconSide = minimum(Rect.w, Rect.h) * 0.58f;
		CUIRect IconRect;
		IconRect.w = IconSide;
		IconRect.h = IconSide;
		IconRect.x = Rect.x + (Rect.w - IconSide) * 0.5f;
		IconRect.y = Rect.y + (Rect.h - IconSide) * 0.5f;
		const EQmIconState IconState = Disabled ? EQmIconState::DISABLED : (HoverPrev ? EQmIconState::HOVER : EQmIconState::NORMAL);
		if(Ctx.m_pIconManager == nullptr || !Ctx.m_pIconManager->RenderIcon(Icon, IconRect, IconState))
		{
			Ctx.m_pUi->DoLabel(&IconRect, pFallbackIcon, IconRect.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);
		}

		return Result != 0;
	}

} // namespace ui_widget
