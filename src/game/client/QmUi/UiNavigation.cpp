// 请抬头享受阳光｜日子很好 我很我---------致咩子
/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "UiNavigation.h"

#include "QmAnimResolve.h"
#include "UiMotion.h"
#include "UiTokens.h"

#include <engine/graphics.h>

#include <game/client/components/menus.h>
#include <game/client/ui.h>
#include <game/client/ui_rect.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ui_widget
{

	int TabBar(const IUiContext &Ctx, const char *const *ppLabels, int Count, int *pActive, const CUIRect &Rect)
	{
		if(Ctx.m_pUi == nullptr || Ctx.m_pMenus == nullptr || pActive == nullptr || Count <= 0)
			return pActive != nullptr ? *pActive : 0;

		// Static identity pool — caller-side need not allocate CButtonContainer
		// per tab. We index by (TabBar address, tab index) which is stable as long
		// as the caller's TabBar invocation site is stable.
		static std::unordered_map<std::uintptr_t, std::vector<CButtonContainer>> s_vButtonPools;
		std::vector<CButtonContainer> &ButtonPool = s_vButtonPools[reinterpret_cast<std::uintptr_t>(pActive)];
		const std::size_t Needed = static_cast<std::size_t>(Count);
		if(ButtonPool.size() < Needed)
			ButtonPool.resize(Needed);

		const float TabWidth = Rect.w / static_cast<float>(Count);
		CUIRect Tabs = Rect;
		CUIRect TabsRow, Underline;
		Tabs.HSplitBottom(2.0f, &TabsRow, &Underline);

		for(int i = 0; i < Count; ++i)
		{
			CUIRect TabRect;
			TabRect.x = Rect.x + TabWidth * static_cast<float>(i);
			TabRect.y = TabsRow.y;
			TabRect.w = TabWidth;
			TabRect.h = TabsRow.h;
			const int Checked = (*pActive == i) ? 1 : 0;
			const int Result = Ctx.m_pMenus->DoButton_MenuTab(&ButtonPool[i], ppLabels[i], Checked, &TabRect, IGraphics::CORNER_T);
			if(Result != 0)
				*pActive = i;
		}

		// Underline — animate POS_X toward the active tab. Uses the v2 runtime so
		// the slide eases without manual lerp.
		float SlideX = Rect.x + TabWidth * static_cast<float>(*pActive);
		if(Ctx.m_pAnim != nullptr)
		{
			const uint64_t NodeKey = BuildUiAnimNodeKey(Ctx.m_ScopeHash, reinterpret_cast<uint64_t>(pActive));
			SlideX = ResolveUiAnimValue(*Ctx.m_pAnim, NodeKey, EUiAnimProperty::POS_X, SlideX, ui_curve::EMPHASIZED.m_DurationSec, ui_curve::EMPHASIZED.m_Easing);
		}

		CUIRect Indicator;
		Indicator.x = SlideX + TabWidth * 0.15f;
		Indicator.y = Underline.y;
		Indicator.w = TabWidth * 0.70f;
		Indicator.h = Underline.h;
		Indicator.Draw(Ctx.m_pTheme != nullptr ? Ctx.m_pTheme->m_Accent : ui_token::color::ACCENT_PRIMARY, IGraphics::CORNER_NONE, 0.0f);

		return *pActive;
	}

	bool ListItem(const IUiContext &Ctx, const void *pId, const char *pText, const CUIRect &Rect, const SListItemProps &Props)
	{
		if(Ctx.m_pUi == nullptr)
			return false;
		CUiScopedGaussianBlurSuppression GaussianBlurSuppression(Ctx.m_pUi);

		// Background: selected first, then hover blend on top.
		if(Props.m_Selected)
			Rect.Draw(Ctx.m_pTheme != nullptr ? Ctx.m_pTheme->m_Selected : ui_token::color::ACCENT_PRIMARY_DIM, IGraphics::CORNER_ALL, ui_token::radius::TIGHT);

		if(Ctx.m_pAnim != nullptr)
		{
			const bool HoverPrev = Ctx.m_pUi->HotItem() == pId;
			const float TargetAlpha = HoverPrev ? 1.0f : 0.0f;
			const float Alpha = AnimateStateValue(Ctx, pId, EUiAnimProperty::ALPHA, TargetAlpha, ui_curve::DECELERATE);
			if(Alpha > 0.01f)
			{
				ColorRGBA HoverBg = ui_token::color::SURFACE_HIGHLIGHT;
				HoverBg.a *= Alpha;
				Rect.Draw(HoverBg, IGraphics::CORNER_ALL, ui_token::radius::TIGHT);
			}
		}

		const int Result = Props.m_Disabled ? 0 : Ctx.m_pUi->DoButtonLogic(pId, 0, &Rect, BUTTONFLAG_LEFT);

		// Content layout
		CUIRect Content;
		Rect.VMargin(ui_token::spacing::SM, &Content);

		if(Props.m_pLeadingIcon != nullptr)
		{
			CUIRect Icon;
			Content.VSplitLeft(Rect.h, &Icon, &Content);
			Ctx.m_pUi->DoLabel(&Icon, Props.m_pLeadingIcon, ui_token::font::BODY, TEXTALIGN_MC);
			Content.VSplitLeft(ui_token::spacing::XS, nullptr, &Content);
		}

		if(Props.m_pTrailingText != nullptr)
		{
			CUIRect Trailing;
			Content.VSplitRight(64.0f, &Content, &Trailing);
			Ctx.m_pUi->DoLabel(&Trailing, Props.m_pTrailingText, ui_token::font::CAPTION, TEXTALIGN_MR);
		}

		Ctx.m_pUi->DoLabel(&Content, pText, ui_token::font::BODY, TEXTALIGN_ML);

		return Result != 0;
	}

} // namespace ui_widget
