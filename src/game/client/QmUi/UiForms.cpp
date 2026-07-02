/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "UiForms.h"

#include "QmAnimResolve.h"

#include <engine/graphics.h>

#include <game/client/lineinput.h>
#include <game/client/ui.h>
#include <game/client/ui_rect.h>

#include <algorithm>
#include <cstdio>

namespace ui_widget
{

	bool TextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder, float FontSize)
	{
		if(Ctx.m_pUi == nullptr || pInput == nullptr)
			return false;

		// Background plate
		Rect.Draw(ui_token::color::SURFACE_ELEVATED, IGraphics::CORNER_ALL, ui_token::radius::BASE);

		// Animated focus ring as an inset 1px border. DDNet has no stroke
		// primitive so we paint BORDER_FOCUS at the Rect size and then mask the
		// inner area with SURFACE_ELEVATED — net effect is a 1px ring sitting
		// inside the Rect, not protruding.
		if(Ctx.m_pAnim != nullptr)
		{
			const uint64_t NodeKey = BuildUiAnimNodeKey(Ctx.m_ScopeHash, reinterpret_cast<uint64_t>(pInput));
			const float TargetAlpha = pInput->IsActive() ? 1.0f : 0.0f;
			const float Alpha = ResolveUiAnimValue(*Ctx.m_pAnim, NodeKey, EUiAnimProperty::ALPHA, TargetAlpha, ui_curve::DECELERATE.m_DurationSec, ui_curve::DECELERATE.m_Easing);
			if(Alpha > 0.01f)
			{
				ColorRGBA RingColor = ui_token::color::BORDER_FOCUS;
				RingColor.a *= Alpha;
				Rect.Draw(RingColor, IGraphics::CORNER_ALL, ui_token::radius::BASE);
				CUIRect Inside;
				Rect.Margin(1.0f, &Inside);
				Inside.Draw(ui_token::color::SURFACE_ELEVATED, IGraphics::CORNER_ALL, ui_token::radius::BASE - 1.0f);
			}
		}

		// Inner padding to keep text off the rounded corners.
		CUIRect Inner;
		Rect.VMargin(ui_token::spacing::SM, &Inner);

		const bool Changed = Ctx.m_pUi->DoEditBox(pInput, &Inner, FontSize);

		if(pPlaceholder != nullptr && pInput->IsEmpty() && !pInput->IsActive())
		{
			SLabelProperties LabelProps;
			LabelProps.m_EllipsisAtEnd = true;
			Ctx.m_pUi->DoLabel(&Inner, pPlaceholder, FontSize, TEXTALIGN_ML, LabelProps);
		}

		return Changed;
	}

	bool Toggle(const IUiContext &Ctx, const void *pId, bool *pValue, const CUIRect &Rect)
	{
		if(Ctx.m_pUi == nullptr || pValue == nullptr)
			return false;

		const int Result = Ctx.m_pUi->DoButtonLogic(pId, 0, &Rect, BUTTONFLAG_LEFT);
		const bool Clicked = Result != 0;
		if(Clicked)
			*pValue = !*pValue;

		// Track
		const ColorRGBA TrackOn = ui_token::color::ACCENT_PRIMARY;
		const ColorRGBA TrackOff = ui_token::color::BORDER_SUBTLE;
		ColorRGBA Track = *pValue ? TrackOn : TrackOff;
		if(Ctx.m_pAnim != nullptr)
		{
			const uint64_t TrackKey = BuildUiAnimNodeKey(Ctx.m_ScopeHash ^ 0xA5A5ull, reinterpret_cast<uint64_t>(pId));
			Track = ResolveUiAnimValueColor(*Ctx.m_pAnim, TrackKey, Track, ui_curve::DECELERATE.m_DurationSec, ui_curve::DECELERATE.m_Easing);
		}
		Rect.Draw(Track, IGraphics::CORNER_ALL, Rect.h * 0.5f);

		// Knob — slides between left and right ends. Uses a SPRING request so the
		// motion has the expected snappy bounce on the v2 runtime.
		const float Padding = std::min(Rect.h * 0.15f, 3.0f);
		const float KnobSize = Rect.h - Padding * 2.0f;
		const float LeftX = Rect.x + Padding;
		const float RightX = Rect.x + Rect.w - KnobSize - Padding;
		float KnobX = *pValue ? RightX : LeftX;
		if(Ctx.m_pAnim != nullptr)
		{
			const uint64_t KnobKey = BuildUiAnimNodeKey(Ctx.m_ScopeHash ^ 0x5A5Aull, reinterpret_cast<uint64_t>(pId));
			const float Target = *pValue ? RightX : LeftX;
			const float Current = Ctx.m_pAnim->GetValue(KnobKey, EUiAnimProperty::POS_X, Target);
			if(std::abs(Current - Target) > 0.5f || !Ctx.m_pAnim->HasActiveAnimation(KnobKey, EUiAnimProperty::POS_X))
			{
				SUiAnimRequest Request;
				Request.m_NodeKey = KnobKey;
				Request.m_Property = EUiAnimProperty::POS_X;
				Request.m_Target = Target;
				Request.m_Transition.m_Driver = EUiAnimDriver::SPRING;
				Request.m_Transition.m_Spring = ui_token::motion::TOGGLE;
				Request.m_Transition.m_Interrupt = EUiAnimInterruptPolicy::MERGE_TARGET;
				Ctx.m_pAnim->RequestAnimation(Request);
			}
			KnobX = Ctx.m_pAnim->GetValue(KnobKey, EUiAnimProperty::POS_X, Target);
		}

		CUIRect Knob;
		Knob.x = KnobX;
		Knob.y = Rect.y + Padding;
		Knob.w = KnobSize;
		Knob.h = KnobSize;
		Knob.Draw(ui_token::color::TEXT_PRIMARY, IGraphics::CORNER_ALL, KnobSize * 0.5f);

		return Clicked;
	}

	bool Slider(const IUiContext &Ctx, const void *pId, float *pValue, float Min, float Max, const CUIRect &Rect, const char *pSuffix)
	{
		if(Ctx.m_pUi == nullptr || pValue == nullptr || Max <= Min)
			return false;

		CUIRect Track, Label;
		Rect.VSplitRight(48.0f, &Track, &Label);
		Track.VSplitRight(ui_token::spacing::SM, &Track, nullptr);

		const float Normalized = std::clamp((*pValue - Min) / (Max - Min), 0.0f, 1.0f);
		const ColorRGBA Inner = ui_token::color::ACCENT_PRIMARY;
		const float NewNormalized = Ctx.m_pUi->DoScrollbarH(pId, &Track, Normalized, &Inner);
		const float NewValue = Min + NewNormalized * (Max - Min);
		const bool Changed = std::abs(NewValue - *pValue) > 1e-4f;
		*pValue = NewValue;

		char aBuf[32];
		if(pSuffix != nullptr && pSuffix[0] != '\0')
			std::snprintf(aBuf, sizeof(aBuf), "%.2f%s", *pValue, pSuffix);
		else
			std::snprintf(aBuf, sizeof(aBuf), "%.2f", *pValue);
		Ctx.m_pUi->DoLabel(&Label, aBuf, ui_token::font::BODY, TEXTALIGN_MR);

		return Changed;
	}

} // namespace ui_widget
