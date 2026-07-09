/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "UiForms.h"

#include "UiMotion.h"

#include <engine/graphics.h>
#include <engine/keys.h>

#include <game/client/lineinput.h>
#include <game/client/ui.h>
#include <game/client/ui_rect.h>

#include <algorithm>
#include <cstdio>

namespace ui_widget
{

	namespace
	{
		SInputFieldResult BuildInputFieldResult(const IUiContext &Ctx, CLineInput *pInput, bool Changed, bool WasActive, bool WasEmpty, bool Clearable)
		{
			const bool SubmitPressed = Ctx.m_pUi != nullptr && (Ctx.m_pUi->Input()->KeyPress(KEY_RETURN) || Ctx.m_pUi->Input()->KeyPress(KEY_KP_ENTER));
			return ui_widget::BuildInputFieldResult(WasActive, pInput->IsActive(), Changed, SubmitPressed, WasEmpty, pInput->IsEmpty(), Clearable);
		}

		void DrawTextFieldPlate(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const STextFieldOptions &Options)
		{
			Rect.Draw(ui_token::color::SURFACE_ELEVATED, Options.m_Corners, Options.m_CornerRadius);

			if(Ctx.m_pAnim == nullptr)
				return;

			const float TargetAlpha = pInput->IsActive() ? 1.0f : 0.0f;
			const float Alpha = AnimateStateValue(Ctx, pInput, EUiAnimProperty::ALPHA, TargetAlpha, ui_curve::DECELERATE);
			if(Alpha <= 0.01f)
				return;

			ColorRGBA RingColor = ui_token::color::BORDER_FOCUS;
			RingColor.a *= Alpha;
			Rect.Draw(RingColor, Options.m_Corners, Options.m_CornerRadius);
			CUIRect Inside;
			Rect.Margin(1.0f, &Inside);
			Inside.Draw(ui_token::color::SURFACE_ELEVATED, Options.m_Corners, maximum(Options.m_CornerRadius - 1.0f, 0.0f));
		}

		void DrawLegacyTextFieldPlate(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, int Corners)
		{
			if(Ctx.m_pUi == nullptr)
				return;
			const bool Active = pInput != nullptr && pInput->IsActive();
			const bool Hovered = pInput != nullptr && Ctx.m_pUi->HotItem() == pInput;
			Rect.Draw(Ctx.m_pUi->ScaleBackgroundAlpha(CUi::ms_LightButtonColorFunction.GetColor(Active, Hovered)), Corners, ui_token::radius::BASE);
		}

		void DrawTextFieldFocusBorder(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect)
		{
			if(Ctx.m_pAnim == nullptr)
				return;

			const float TargetAlpha = pInput->IsActive() ? 1.0f : 0.0f;
			const float Alpha = AnimateStateValue(Ctx, pInput, EUiAnimProperty::ALPHA, TargetAlpha, ui_curve::DECELERATE);
			if(Alpha <= 0.01f)
				return;

			ColorRGBA RingColor = ui_token::color::BORDER_FOCUS;
			RingColor.a *= Alpha;
			CUIRect OuterRing = Rect;
			OuterRing.Margin(0.5f, &OuterRing);
			OuterRing.DrawOutline(RingColor);

			ColorRGBA InnerRingColor = RingColor;
			InnerRingColor.a *= 0.45f;
			CUIRect InnerRing = Rect;
			InnerRing.Margin(1.5f, &InnerRing);
			InnerRing.DrawOutline(InnerRingColor);
		}

		void DrawTextFieldPlaceholder(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder, float FontSize, int TextAlign)
		{
			if(pPlaceholder == nullptr || !pInput->IsEmpty() || pInput->IsActive())
				return;

			SLabelProperties LabelProps;
			LabelProps.m_EllipsisAtEnd = true;
			Ctx.m_pUi->DoLabel(&Rect, pPlaceholder, FontSize, TextAlign, LabelProps);
		}
	} // namespace

	SInputFieldResult TextFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder, float FontSize)
	{
		STextFieldOptions Options;
		Options.m_pPlaceholder = pPlaceholder;
		Options.m_FontSize = FontSize;
		return TextFieldEx(Ctx, pInput, Rect, Options);
	}

	SInputFieldResult TextFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const STextFieldOptions &Options)
	{
		if(Ctx.m_pUi == nullptr || pInput == nullptr)
			return {};

		const bool WasActive = pInput->IsActive();
		pInput->SetEmptyText(Options.m_pPlaceholder);
		DrawTextFieldPlate(Ctx, pInput, Rect, Options);

		CUi::SEditBoxRenderOptions RenderOptions;
		RenderOptions.m_DrawBackground = false;
		const bool Changed = Ctx.m_pUi->DoEditBox(pInput, &Rect, Options.m_FontSize, Options.m_Corners, {}, Options.m_TextAlign, RenderOptions);

		return BuildInputFieldResult(Ctx, pInput, Changed, WasActive, false, false);
	}

	bool TextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder, float FontSize)
	{
		return TextFieldEx(Ctx, pInput, Rect, pPlaceholder, FontSize).m_Changed;
	}

	bool TextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const STextFieldOptions &Options)
	{
		return TextFieldEx(Ctx, pInput, Rect, Options).m_Changed;
	}

	SInputFieldResult LegacyTextFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const STextFieldOptions &Options)
	{
		if(Ctx.m_pUi == nullptr || pInput == nullptr)
			return {};

		const bool WasActive = pInput->IsActive();
		pInput->SetEmptyText(Options.m_pPlaceholder);
		DrawLegacyTextFieldPlate(Ctx, pInput, Rect, Options.m_Corners);
		CUi::SEditBoxRenderOptions RenderOptions;
		RenderOptions.m_DrawBackground = false;
		const bool Changed = Ctx.m_pUi->DoEditBox(pInput, &Rect, Options.m_FontSize, Options.m_Corners, {}, Options.m_TextAlign, RenderOptions);
		DrawTextFieldFocusBorder(Ctx, pInput, Rect);

		return BuildInputFieldResult(Ctx, pInput, Changed, WasActive, false, false);
	}

	bool LegacyTextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder, float FontSize)
	{
		STextFieldOptions Options;
		Options.m_pPlaceholder = pPlaceholder;
		Options.m_FontSize = FontSize;
		return LegacyTextFieldEx(Ctx, pInput, Rect, Options).m_Changed;
	}

	void ReadOnlyTextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder, float FontSize)
	{
		if(Ctx.m_pUi == nullptr || pInput == nullptr)
			return;

		pInput->SetEmptyText(pPlaceholder);
		CUIRect TextRect = Rect;
		TextRect.VMargin(2.0f, &TextRect);
		Ctx.m_pUi->ClipEnable(&Rect);
		pInput->Render(&TextRect, FontSize, TEXTALIGN_ML, false, -1.0f, 0.0f);
		Ctx.m_pUi->ClipDisable();
		DrawTextFieldFocusBorder(Ctx, pInput, Rect);
	}

	SInputFieldResult IconTextFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const STextFieldOptions &Options, const char *pIcon, bool Clearable)
	{
		if(Ctx.m_pUi == nullptr || pInput == nullptr)
			return {};

		const bool WasActive = pInput->IsActive();
		const bool WasEmpty = pInput->IsEmpty();

		pInput->SetEmptyText(Options.m_pPlaceholder);
		DrawLegacyTextFieldPlate(Ctx, pInput, Rect, Options.m_Corners);

		const float IconSize = Rect.h * 0.65f;
		const float IconMargin = ui_token::spacing::XS;
		const float IconAreaWidth = IconSize + 2.0f * IconMargin;
		const float ClearButtonWidth = Clearable ? Rect.h : 0.0f;

		CUIRect IconRect, InputRect, ClearRect;
		Rect.VSplitLeft(IconAreaWidth, &IconRect, &InputRect);
		if(Clearable)
		{
			InputRect.VSplitRight(ClearButtonWidth, &InputRect, &ClearRect);
			InputRect.VMargin(ui_token::spacing::XS, &InputRect);
		}
		else
			InputRect.VMargin(ui_token::spacing::XS, &InputRect);

		const char *pIconToUse = pIcon != nullptr ? pIcon : FontIcons::FONT_ICON_MAGNIFYING_GLASS;
		Ctx.m_pUi->TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		Ctx.m_pUi->TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
		Ctx.m_pUi->DoLabel(&IconRect, pIconToUse, IconSize, TEXTALIGN_MC);
		Ctx.m_pUi->TextRender()->SetRenderFlags(0);
		Ctx.m_pUi->TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

		CUi::SEditBoxRenderOptions RenderOptions;
		RenderOptions.m_DrawBackground = false;
		bool Changed = Ctx.m_pUi->DoEditBox(pInput, &InputRect, Options.m_FontSize, Options.m_Corners & ~IGraphics::CORNER_R, {}, Options.m_TextAlign, RenderOptions);
		DrawTextFieldFocusBorder(Ctx, pInput, Rect);

		if(Clearable)
		{
			const ColorRGBA ClearColor = Ctx.m_pUi->ScaleBackgroundAlpha(ColorRGBA(1.0f, 1.0f, 1.0f, 0.33f * Ctx.m_pUi->ButtonColorMul(pInput->GetClearButtonId())));
			ClearRect.Draw(ClearColor, IGraphics::CORNER_R, ui_token::radius::BASE);
			Ctx.m_pUi->TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
			Ctx.m_pUi->DoLabel(&ClearRect, FontIcons::FONT_ICON_XMARK, ClearRect.h * CUi::ms_FontmodHeight * 0.8f, TEXTALIGN_MC);
			Ctx.m_pUi->TextRender()->SetRenderFlags(0);
			if(Ctx.m_pUi->DoButtonLogic(pInput->GetClearButtonId(), 0, &ClearRect, BUTTONFLAG_LEFT))
			{
				pInput->Clear();
				Ctx.m_pUi->SetActiveItem(pInput);
				Changed = true;
			}
		}

		return BuildInputFieldResult(Ctx, pInput, Changed, WasActive, WasEmpty, Clearable);
	}

	bool IconTextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder, float FontSize, const char *pIcon, bool Clearable)
	{
		STextFieldOptions Options;
		Options.m_pPlaceholder = pPlaceholder;
		Options.m_FontSize = FontSize;
		return IconTextFieldEx(Ctx, pInput, Rect, Options, pIcon, Clearable).m_Changed;
	}

	SInputFieldResult ClearableTextFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder, float FontSize)
	{
		if(Ctx.m_pUi == nullptr || pInput == nullptr)
			return {};

		const bool WasActive = pInput->IsActive();
		const bool WasEmpty = pInput->IsEmpty();
		STextFieldOptions Options;
		pInput->SetEmptyText(pPlaceholder);
		DrawTextFieldPlate(Ctx, pInput, Rect, Options);

		CUi::SEditBoxRenderOptions RenderOptions;
		RenderOptions.m_DrawBackground = false;
		const bool Changed = Ctx.m_pUi->DoClearableEditBox(pInput, &Rect, FontSize, IGraphics::CORNER_ALL, {}, RenderOptions);

		return BuildInputFieldResult(Ctx, pInput, Changed, WasActive, WasEmpty, true);
	}

	bool ClearableTextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder, float FontSize)
	{
		return ClearableTextFieldEx(Ctx, pInput, Rect, pPlaceholder, FontSize).m_Changed;
	}

	SInputFieldResult SearchFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, float FontSize, bool HotkeyEnabled)
	{
		if(Ctx.m_pUi == nullptr || pInput == nullptr)
			return {};

		const bool WasActive = pInput->IsActive();
		const bool WasEmpty = pInput->IsEmpty();
		STextFieldOptions Options;
		DrawTextFieldPlate(Ctx, pInput, Rect, Options);

		CUi::SEditBoxRenderOptions RenderOptions;
		RenderOptions.m_DrawBackground = false;
		const bool Changed = Ctx.m_pUi->DoEditBox_Search(pInput, &Rect, FontSize, HotkeyEnabled, RenderOptions);
		return BuildInputFieldResult(Ctx, pInput, Changed, WasActive, WasEmpty, true);
	}

	bool SearchField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, float FontSize, bool HotkeyEnabled)
	{
		return SearchFieldEx(Ctx, pInput, Rect, FontSize, HotkeyEnabled).m_Changed;
	}

	SInputFieldResult IntegerField(const IUiContext &Ctx, CLineInputNumber *pInput, int *pValue, int Min, int Max, const CUIRect &Rect, const STextFieldOptions &Options)
	{
		if(Ctx.m_pUi == nullptr || pInput == nullptr || pValue == nullptr)
			return {};

		const int ClampedValue = std::clamp(*pValue, Min, Max);
		if(ClampedValue != *pValue)
			*pValue = ClampedValue;

		if(!pInput->IsActive() && (pInput->IsEmpty() || pInput->GetInteger() != *pValue))
		{
			pInput->SetInteger(*pValue);
			pInput->SelectAll();
		}

		STextFieldOptions FieldOptions = Options;
		FieldOptions.m_pPlaceholder = Options.m_pPlaceholder;
		const SInputFieldResult Result = TextFieldEx(Ctx, pInput, Rect, FieldOptions);
		if(!pInput->IsActive())
		{
			if(pInput->GetLength() > 0 && (Result.m_Changed || Result.m_Deactivated || pInput->GetInteger() != *pValue))
				*pValue = std::clamp(pInput->GetInteger(), Min, Max);
			pInput->SetInteger(*pValue);
			pInput->SelectAll();
		}

		return Result;
	}

	CUIRect SliderInputFieldLabelRect(const CUIRect &Rect, bool HasLabel, unsigned Flags)
	{
		if(!HasLabel)
			return {};

		CUIRect Label;
		if(Flags & CUi::SCROLLBAR_OPTION_MULTILINE)
		{
			CUIRect Header;
			Rect.HSplitMid(&Header, nullptr);
			Header.VSplitLeft(Header.w * 0.68f, &Label, nullptr);
			return Label;
		}

		const float LabelWidth = std::clamp(Rect.w * 0.25f, 108.0f, 180.0f);
		Rect.VSplitLeft(LabelWidth, &Label, nullptr);
		return Label;
	}

	bool SliderInputField(const IUiContext &Ctx, CLineInputNumber *pInput, const void *pId, int *pValue, int Min, int Max, const CUIRect &Rect, const SSliderInputFieldOptions &Options)
	{
		if(Ctx.m_pUi == nullptr || pInput == nullptr || pValue == nullptr || Max <= Min)
			return false;

		const IScrollbarScale *pScale = Options.m_pScale != nullptr ? Options.m_pScale : &CUi::ms_LinearScrollbarScale;
		const bool Infinite = Options.m_Flags & CUi::SCROLLBAR_OPTION_INFINITE;
		const bool NoClampValue = Options.m_Flags & CUi::SCROLLBAR_OPTION_NOCLAMPVALUE;
		const bool MultiLine = Options.m_Flags & CUi::SCROLLBAR_OPTION_MULTILINE;
		const int Multiplier = std::max(1, Options.m_ValueMultiplier);
		const int SliderMin = SliderInputStoredMinimum(Min, Multiplier);
		const int SliderMax = SliderInputStoredMaximum(Max, Multiplier) + (Infinite ? 1 : 0);
		const bool HasLabel = Options.m_pLabel != nullptr && Options.m_pLabel[0] != '\0';
		// 布局：label | scrollbar | input | suffix
		CUIRect Label, Controls, ValueRect, SuffixRect, ScrollBar, InputField;
		if(MultiLine)
		{
			CUIRect Header;
			Rect.HSplitMid(&Header, &ScrollBar);
			Header.VSplitLeft(Header.w * 0.68f, &Label, &ValueRect);
		}
		else if(HasLabel)
		{
			Label = SliderInputFieldLabelRect(Rect, true, Options.m_Flags);
			Rect.VSplitLeft(Label.w, nullptr, &Controls);
			Controls.VMargin(std::min(10.0f, Controls.w * 0.025f), &Controls);
		}
		else
		{
			Controls = Rect;
		}

		const float ValueWidth = std::clamp((MultiLine ? ValueRect.w : Controls.w) * 0.18f, 42.0f, 80.0f);
		const float SuffixWidth = Options.m_pSuffix != nullptr && Options.m_pSuffix[0] != '\0' ? ValueWidth * 0.5f : 0.0f;
		if(!MultiLine)
			Controls.VSplitRight(ValueWidth + SuffixWidth, &ScrollBar, &ValueRect);
		if(SuffixWidth > 0.0f)
			ValueRect.VSplitRight(SuffixWidth, &InputField, &SuffixRect);
		else
			InputField = ValueRect;
		InputField.VMargin(std::min(5.0f, ValueWidth * 0.1f), &InputField);

		if(HasLabel)
		{
			if(Options.m_pLabelElement != nullptr)
			{
				SLabelProperties Props;
				Props.m_MaxWidth = Label.w;
				Ctx.m_pUi->DoLabelStreamed(*Options.m_pLabelElement->Rect(0), &Label, Options.m_pLabel, Options.m_FontSize, Options.m_LabelAlign, Props, -1, nullptr, !Ctx.m_pUi->RenderOnly());
			}
			else
				Ctx.m_pUi->DoLabel(&Label, Options.m_pLabel, Options.m_FontSize, Options.m_LabelAlign);
		}

		if(Ctx.m_pUi->RenderOnly())
			return false;

		bool Changed = false;
		const int Increment = std::max(1, (SliderMax - SliderMin) / 35);
		if(Ctx.m_pUi->Input()->ModifierIsPressed() && Ctx.m_pUi->Input()->KeyPress(KEY_MOUSE_WHEEL_UP) && Ctx.m_pUi->MouseInside(&Rect))
		{
			const int NewValue = SliderInputWheelStoredValue(*pValue, SliderMin, SliderMax, Infinite, Increment);
			Changed = NewValue != *pValue;
			*pValue = NewValue;
		}
		if(Ctx.m_pUi->Input()->ModifierIsPressed() && Ctx.m_pUi->Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN) && Ctx.m_pUi->MouseInside(&Rect))
		{
			const int NewValue = SliderInputWheelStoredValue(*pValue, SliderMin, SliderMax, Infinite, -Increment);
			Changed = Changed || NewValue != *pValue;
			*pValue = NewValue;
		}

		const bool IsInfinite = SliderInputIsInfiniteValue(*pValue, Infinite);
		int StoredValue = *pValue;
		if(!IsInfinite && !NoClampValue)
			StoredValue = std::clamp(StoredValue, SliderMin, SliderInputStoredMaximum(Max, Multiplier));

		int DisplayValue = SliderInputDisplayValue(StoredValue, Multiplier);
		if(IsInfinite)
			DisplayValue = Max;

		if(!pInput->IsActive() && IsInfinite)
		{
			if(str_comp(pInput->GetString(), "\xe2\x88\x9e") != 0)
				pInput->Set("\xe2\x88\x9e");
		}
		else if(!pInput->IsActive() && (pInput->IsEmpty() || pInput->GetInteger() != DisplayValue))
		{
			pInput->SetInteger(DisplayValue);
			pInput->SelectAll();
		}

		// 滑动条以缩放单位工作；输入框激活时仅绘制静态滑块，不响应拖拽
		int SliderValue = IsInfinite ? SliderMax : StoredValue;

		const float Normalized = std::clamp(pScale->ToRelative(SliderValue, SliderMin, SliderMax), 0.0f, 1.0f);
		const ColorRGBA Inner = ui_token::color::ACCENT_PRIMARY;
		if(!pInput->IsActive())
		{
			const float NewNormalized = Ctx.m_pUi->DoScrollbarH(pId, &ScrollBar, Normalized, &Inner);
			int NewSliderValue = pScale->ToAbsolute(NewNormalized, SliderMin, SliderMax);
			if(NewSliderValue != SliderValue)
			{
				int CandidateStored = NewSliderValue;
				if(Infinite && NewSliderValue == SliderMax)
					CandidateStored = 0;

				if(NoClampValue && ((CandidateStored <= SliderMin && *pValue < SliderMin) || (CandidateStored >= SliderInputStoredMaximum(Max, Multiplier) && *pValue > SliderInputStoredMaximum(Max, Multiplier))))
				{
					// 保留越界值
					DisplayValue = SliderInputIsInfiniteValue(*pValue, Infinite) ? Max : SliderInputDisplayValue(*pValue, Multiplier);
					pInput->SetInteger(DisplayValue);
					pInput->SelectAll();
				}
				else
				{
					*pValue = CandidateStored;
					Changed = true;
					DisplayValue = SliderInputIsInfiniteValue(CandidateStored, Infinite) ? Max : SliderInputDisplayValue(CandidateStored, Multiplier);
					pInput->SetInteger(DisplayValue);
					pInput->SelectAll();
				}
			}
		}
		else
		{
			// 静态绘制与 DoScrollbarH(pColorInner) 的非激活状态一致
			CUIRect Rail = ScrollBar;
			CUIRect Handle;
			Rail.VSplitLeft(8.0f, &Handle, nullptr);
			Handle.x += (Rail.w - Handle.w) * Normalized;
			CUIRect Slider;
			Handle.VMargin(-2.0f, &Slider);
			Slider.HMargin(-3.0f, &Slider);
			Slider.Draw(Ctx.m_pUi->ScaleBackgroundAlpha(ColorRGBA(0.15f, 0.15f, 0.15f, 1.0f)), IGraphics::CORNER_ALL, 5.0f);
			Slider.Margin(2.0f, &Slider);
			Slider.Draw(Ctx.m_pUi->ScaleBackgroundAlpha(Inner), IGraphics::CORNER_ALL, 3.0f);
		}

		// 输入框：特殊值（♾️ 或 pMaxText）在非编辑状态下显示为文本
		const bool bShowMaxText = !pInput->IsActive() && !SliderInputIsInfiniteValue(*pValue, Infinite) && Options.m_pMaxText != nullptr && DisplayValue == Max;
		char aSavedInput[32];
		if(bShowMaxText)
		{
			str_copy(aSavedInput, pInput->GetString(), sizeof(aSavedInput));
			pInput->Set(Options.m_pMaxText);
		}

		STextFieldOptions FieldOptions;
		FieldOptions.m_FontSize = Options.m_FontSize;
		FieldOptions.m_TextAlign = TEXTALIGN_MC;
		SInputFieldResult Result = TextFieldEx(Ctx, pInput, InputField, FieldOptions);

		if(bShowMaxText)
			pInput->Set(aSavedInput);

		if(Result.m_Deactivated || Result.m_Submitted)
		{
			const bool ParsedInfinite = Infinite && str_comp(pInput->GetString(), "\xe2\x88\x9e") == 0;
			int Parsed = ParsedInfinite ? 0 : SliderInputStoredValue(pInput->GetInteger(), Multiplier);
			if(NoClampValue && ((Parsed <= SliderMin && *pValue < SliderMin) || (Parsed >= SliderInputStoredMaximum(Max, Multiplier) && *pValue > SliderInputStoredMaximum(Max, Multiplier))))
			{
				// 保留越界值
			}
			else
			{
				Parsed = std::clamp(Parsed, SliderMin, SliderInputStoredMaximum(Max, Multiplier));
			}
			if(*pValue != Parsed)
			{
				*pValue = Parsed;
				Result.m_Changed = true;
				Changed = true;
			}
			DisplayValue = SliderInputIsInfiniteValue(*pValue, Infinite) ? Max : SliderInputDisplayValue(*pValue, Multiplier);
			pInput->SetInteger(DisplayValue);
			pInput->SelectAll();
		}

		if(Options.m_pSuffix != nullptr && Options.m_pSuffix[0] != '\0')
			Ctx.m_pUi->DoLabel(&SuffixRect, Options.m_pSuffix, Options.m_FontSize, TEXTALIGN_ML);

		return Result.m_Changed || Changed;
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
