/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "UiForms.h"

#include "UiFormLogic.h"
#include "UiMotion.h"
#include "UiTheme.h"

#include <engine/graphics.h>
#include <engine/keys.h>

#include <game/client/lineinput.h>
#include <game/client/ui.h>
#include <game/client/ui_rect.h>
#include <game/localization.h>

#include <algorithm>
#include <cstdio>

namespace ui_widget
{

	namespace
	{
		void DrawTextFieldFocusBorder(const IUiContext &Ctx, const CUIRect &Rect, float Alpha);
		void DrawTextFieldFocusBorder(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect);

		const SUiTheme &ThemeFor(const IUiContext &Ctx)
		{
			static const SUiTheme s_FallbackTheme = ResolveUiTheme(ColorHSLA(0.0f, 0.0f, 0.29f, 1.0f), 1.0f);
			return Ctx.m_pTheme != nullptr ? *Ctx.m_pTheme : s_FallbackTheme;
		}

		SInputFieldResult BuildInputFieldResult(const IUiContext &Ctx, CLineInput *pInput, bool Changed, bool WasActive, bool WasEmpty, bool Clearable)
		{
			const bool SubmitPressed = Ctx.m_pUi != nullptr && (Ctx.m_pUi->Input()->KeyPress(KEY_RETURN) || Ctx.m_pUi->Input()->KeyPress(KEY_KP_ENTER));
			return ui_widget::BuildInputFieldResult(WasActive, pInput->IsActive(), Changed, SubmitPressed, WasEmpty, pInput->IsEmpty(), Clearable);
		}

		void DrawTextFieldPlate(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const STextFieldOptions &Options)
		{
			const SUiTheme &Theme = ThemeFor(Ctx);
			const bool Active = pInput->IsActive();
			const bool Hovered = Ctx.m_pUi->HotItem() == pInput;
			const ColorRGBA PlateColor = Active ? Theme.m_InputSurfaceFocused : (Hovered ? Theme.m_SurfaceHovered : Theme.m_InputSurface);
			Rect.Draw(Ctx.m_pUi->ScaleBackgroundAlpha(PlateColor), Options.m_Corners, Options.m_CornerRadius);
			if(Ctx.m_pAnim == nullptr)
				return;

			const float TargetAlpha = pInput->IsActive() ? 1.0f : 0.0f;
			const float Alpha = AnimateStateValue(Ctx, pInput, EUiAnimProperty::ALPHA, TargetAlpha, ui_curve::DECELERATE);
			DrawTextFieldFocusBorder(Ctx, Rect, Alpha);
		}

		void DrawTextFieldFocusBorder(const IUiContext &Ctx, const CUIRect &Rect, float Alpha)
		{
			if(Alpha <= 0.01f)
				return;

			const SUiTheme &Theme = ThemeFor(Ctx);
			ColorRGBA RingColor = Theme.m_FocusRing;
			RingColor.a *= Alpha;
			CUIRect OuterRing = Rect;
			OuterRing.Margin(Theme.m_FocusRingInset * 0.5f, &OuterRing);
			OuterRing.DrawOutline(RingColor);

			ColorRGBA InnerRingColor = RingColor;
			InnerRingColor.a *= 0.45f;
			CUIRect InnerRing = Rect;
			InnerRing.Margin(1.5f, &InnerRing);
			InnerRing.DrawOutline(InnerRingColor);
		}

		void DrawTextFieldFocusBorder(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect)
		{
			if(Ctx.m_pAnim == nullptr)
				return;
			const float TargetAlpha = pInput->IsActive() ? 1.0f : 0.0f;
			const float Alpha = AnimateStateValue(Ctx, pInput, EUiAnimProperty::ALPHA, TargetAlpha, ui_curve::DECELERATE);
			DrawTextFieldFocusBorder(Ctx, Rect, Alpha);
		}

		void DrawInputFieldIcon(const IUiContext &Ctx, const CUIRect &Rect, const char *pIcon)
		{
			if(pIcon == nullptr || Rect.w <= 0.0f || Rect.h <= 0.0f)
				return;
			Ctx.m_pUi->TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
			Ctx.m_pUi->TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
			Ctx.m_pUi->DoLabel(&Rect, pIcon, Rect.h * 0.65f, TEXTALIGN_MC);
			Ctx.m_pUi->TextRender()->SetRenderFlags(0);
			Ctx.m_pUi->TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		}

	} // namespace

	SInputFieldResult InputField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const SInputFieldOptions &Options)
	{
		if(Ctx.m_pUi == nullptr || pInput == nullptr)
			return {};

		const bool WasActive = pInput->IsActive();
		const bool WasEmpty = pInput->IsEmpty();
		const bool Search = Options.m_Mode == EInputFieldMode::SEARCH;
		const bool HasIcon = Options.m_pLeadingIcon != nullptr || Search;
		const SInputFieldLayout Layout = ResolveInputFieldLayout(Rect, HasIcon, Options.m_Clearable, Ctx.m_UiScale);
		const SUiTheme &Theme = ThemeFor(Ctx);
		const bool Hovered = Ctx.m_pUi->HotItem() == pInput;
		const ColorRGBA PlateColor = pInput->IsActive() ? Theme.m_InputSurfaceFocused : (Hovered ? Theme.m_SurfaceHovered : Theme.m_InputSurface);
		Layout.m_ShellRect.Draw(Ctx.m_pUi->ScaleBackgroundAlpha(PlateColor), Options.m_Corners, ui_token::radius::BASE);
		pInput->SetEmptyText(Options.m_pPlaceholder != nullptr ? Options.m_pPlaceholder : (Search ? Localize("Search") : nullptr));

		if(Options.m_SearchHotkeyEnabled && Search && Ctx.m_pUi->Input()->ModifierIsPressed() && Ctx.m_pUi->Input()->KeyPress(KEY_F))
		{
			Ctx.m_pUi->SetActiveItem(pInput);
			pInput->SelectAll();
		}

		DrawInputFieldIcon(Ctx, Layout.m_IconRect, Options.m_pLeadingIcon != nullptr ? Options.m_pLeadingIcon : (Search ? FontIcons::FONT_ICON_MAGNIFYING_GLASS : nullptr));
		CUi::SEditBoxRenderOptions RenderOptions;
		RenderOptions.m_DrawBackground = false;
		bool Changed = false;
		if(Options.m_Mode == EInputFieldMode::MULTILINE)
			Changed = Ctx.m_pUi->DoEditBoxMultiLine(pInput, &Layout.m_ContentRect, Options.m_FontSize, Options.m_LineSpacing, ResolveInputFieldTextAlign(Options), RenderOptions);
		else
			Changed = Ctx.m_pUi->DoEditBox(pInput, &Layout.m_ContentRect, Options.m_FontSize, Options.m_Corners, {}, ResolveInputFieldTextAlign(Options), RenderOptions);

		if(Options.m_Clearable)
		{
			const CUIRect &ClearRect = Layout.m_ClearRect;
			const ColorRGBA ClearColor = Ctx.m_pUi->ScaleBackgroundAlpha(ColorRGBA(1.0f, 1.0f, 1.0f, 0.33f * Ctx.m_pUi->ButtonColorMul(pInput->GetClearButtonId())));
			ClearRect.Draw(ClearColor, IGraphics::CORNER_R, ui_token::radius::BASE);
			DrawInputFieldIcon(Ctx, ClearRect, FontIcons::FONT_ICON_XMARK);
			if(Ctx.m_pUi->DoButtonLogic(pInput->GetClearButtonId(), 0, &ClearRect, BUTTONFLAG_LEFT))
			{
				pInput->Clear();
				Ctx.m_pUi->SetActiveItem(pInput);
				Changed = true;
			}
		}

		DrawTextFieldFocusBorder(Ctx, pInput, Layout.m_FocusRingRect);
		return BuildInputFieldResult(Ctx, pInput, Changed, WasActive, WasEmpty, Options.m_Clearable);
	}
	bool InputField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder, float FontSize)
	{
		SInputFieldOptions Options;
		Options.m_pPlaceholder = pPlaceholder;
		Options.m_FontSize = FontSize;
		return InputField(Ctx, pInput, Rect, Options).m_Changed;
	}

	bool InputField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, float FontSize, bool SearchHotkeyEnabled)
	{
		SInputFieldOptions Options;
		Options.m_Mode = EInputFieldMode::SEARCH;
		Options.m_Clearable = true;
		Options.m_SearchHotkeyEnabled = SearchHotkeyEnabled;
		Options.m_FontSize = FontSize;
		return InputField(Ctx, pInput, Rect, Options).m_Changed;
	}

	bool InputField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder, float FontSize, const char *pIcon, bool Clearable)
	{
		SInputFieldOptions Options;
		Options.m_pPlaceholder = pPlaceholder;
		Options.m_pLeadingIcon = pIcon;
		Options.m_Clearable = Clearable;
		Options.m_FontSize = FontSize;
		return InputField(Ctx, pInput, Rect, Options).m_Changed;
	}
	bool InputField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const STextFieldOptions &Options)
	{
		SInputFieldOptions InputOptions;
		InputOptions.m_pPlaceholder = Options.m_pPlaceholder;
		InputOptions.m_FontSize = Options.m_FontSize;
		InputOptions.m_Corners = Options.m_Corners;
		InputOptions.m_TextAlign = Options.m_TextAlign;
		return InputField(Ctx, pInput, Rect, InputOptions).m_Changed;
	}
	SInputFieldResult TextFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder, float FontSize)
	{
		STextFieldOptions Options;
		Options.m_pPlaceholder = pPlaceholder;
		Options.m_FontSize = FontSize;
		return TextFieldEx(Ctx, pInput, Rect, Options);
	}

	SInputFieldResult TextFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const STextFieldOptions &Options)
	{
		SInputFieldOptions InputOptions;
		InputOptions.m_pPlaceholder = Options.m_pPlaceholder;
		InputOptions.m_FontSize = Options.m_FontSize;
		InputOptions.m_Corners = Options.m_Corners;
		InputOptions.m_TextAlign = Options.m_TextAlign;
		return InputField(Ctx, pInput, Rect, InputOptions);
	}
	bool TextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder, float FontSize)
	{
		return TextFieldEx(Ctx, pInput, Rect, pPlaceholder, FontSize).m_Changed;
	}

	bool TextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const STextFieldOptions &Options)
	{
		return TextFieldEx(Ctx, pInput, Rect, Options).m_Changed;
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
		SInputFieldOptions InputOptions;
		InputOptions.m_pPlaceholder = Options.m_pPlaceholder;
		InputOptions.m_pLeadingIcon = pIcon != nullptr ? pIcon : FontIcons::FONT_ICON_MAGNIFYING_GLASS;
		InputOptions.m_Clearable = Clearable;
		InputOptions.m_FontSize = Options.m_FontSize;
		InputOptions.m_Corners = Options.m_Corners;
		InputOptions.m_TextAlign = Options.m_TextAlign;
		return InputField(Ctx, pInput, Rect, InputOptions);
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
		SInputFieldOptions InputOptions;
		InputOptions.m_pPlaceholder = pPlaceholder;
		InputOptions.m_Clearable = true;
		InputOptions.m_FontSize = FontSize;
		return InputField(Ctx, pInput, Rect, InputOptions);
	}
	bool ClearableTextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder, float FontSize)
	{
		return ClearableTextFieldEx(Ctx, pInput, Rect, pPlaceholder, FontSize).m_Changed;
	}

	SInputFieldResult SearchFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, float FontSize, bool HotkeyEnabled)
	{
		SInputFieldOptions InputOptions;
		InputOptions.m_Mode = EInputFieldMode::SEARCH;
		InputOptions.m_Clearable = true;
		InputOptions.m_SearchHotkeyEnabled = HotkeyEnabled;
		InputOptions.m_FontSize = FontSize;
		return InputField(Ctx, pInput, Rect, InputOptions);
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

	bool NumericField(const IUiContext &Ctx, SNumericFieldState *pState, const void *pId, int *pValue, int Min, int Max, const CUIRect &Rect, const SNumericFieldOptions &Options)
	{
		if(Ctx.m_pUi == nullptr || pState == nullptr || pValue == nullptr || Max <= Min)
			return false;

		CLineInputNumber *pInput = &pState->m_Input;
		const IScrollbarScale *pScale = Options.m_pScale != nullptr ? Options.m_pScale : &CUi::ms_LinearScrollbarScale;
		const bool Infinite = Options.m_Flags & CUi::SCROLLBAR_OPTION_INFINITE;
		const bool NoClampValue = Options.m_Flags & CUi::SCROLLBAR_OPTION_NOCLAMPVALUE;
		const bool MultiLine = Options.m_Flags & CUi::SCROLLBAR_OPTION_MULTILINE;
		const int Multiplier = std::max(1, Options.m_ValueMultiplier);
		const int SliderMin = SliderInputStoredMinimum(Min, Multiplier);
		const int SliderMax = SliderInputStoredMaximum(Max, Multiplier) + (Infinite ? 1 : 0);
		const bool HasLabel = Options.m_pLabel != nullptr && Options.m_pLabel[0] != '\0';
		const bool RenderOnly = Ctx.m_pUi->RenderOnly();
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
		const bool HasSlider = MultiLine || Controls.w > ValueWidth + SuffixWidth + 42.0f;
		bool HasSuffixRect = false;
		if(!MultiLine && HasSlider)
			Controls.VSplitRight(ValueWidth + SuffixWidth, &ScrollBar, &ValueRect);
		else if(!MultiLine)
			InputField = Controls;
		if(InputField.w <= 0.0f && SuffixWidth > 0.0f)
			ValueRect.VSplitRight(SuffixWidth, &InputField, &SuffixRect);
		else if(InputField.w <= 0.0f)
			InputField = ValueRect;
		HasSuffixRect = SuffixRect.w > 0.0f;
		InputField.VMargin(std::min(5.0f, ValueWidth * 0.1f), &InputField);

		if(HasLabel)
		{
			const float LabelFontSize = MultiLine ? std::min(Options.m_FontSize, Label.h * CUi::ms_FontmodHeight * 0.8f) : Options.m_FontSize;
			if(Options.m_pLabelElement != nullptr)
			{
				SLabelProperties Props;
				Props.m_MaxWidth = Label.w;
				Ctx.m_pUi->DoLabelStreamed(*Options.m_pLabelElement->Rect(0), &Label, Options.m_pLabel, LabelFontSize, Options.m_LabelAlign, Props, -1, nullptr, !Ctx.m_pUi->RenderOnly());
			}
			else
				Ctx.m_pUi->DoLabel(&Label, Options.m_pLabel, LabelFontSize, Options.m_LabelAlign);
		}

		bool Changed = false;
		const int Increment = std::max(1, (SliderMax - SliderMin) / 35);
		const bool WheelEligible = !RenderOnly && Ctx.m_pUi->Input()->ModifierIsPressed() && Ctx.m_pUi->MouseInside(&Rect);
		Ctx.m_pUi->RegisterWheelOwner(pState, EUiWheelOwnerPriority::COMPOSITE_CONTROL, Rect, WheelEligible);
		float WheelDelta = 0.0f;
		if(Ctx.m_pUi->TryConsumeWheel(pState, &WheelDelta))
		{
			const int NewValue = SliderInputWheelStoredValue(*pValue, SliderMin, SliderMax, Infinite, WheelDelta > 0.0f ? Increment : -Increment);
			Changed = NewValue != *pValue;
			*pValue = NewValue;
		}

		if(!pState->m_HasSyncedValue || (!pState->m_HasPendingValue && pState->m_LastSyncedStoredValue != *pValue))
		{
			pState->m_LastSyncedStoredValue = *pValue;
			pState->m_HasSyncedValue = true;
		}

		const int PreviewStoredValue = pState->m_HasPendingValue ? pState->m_PendingStoredValue : *pValue;
		const bool IsInfinite = SliderInputIsInfiniteValue(PreviewStoredValue, Infinite);
		int StoredValue = PreviewStoredValue;
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
		const bool SliderWasActive = pState->m_SliderWasActive;
		if(HasSlider && !RenderOnly && !pInput->IsActive())
		{
			const float NewNormalized = Ctx.m_pUi->DoScrollbarH(pId, &ScrollBar, Normalized);
			const bool SliderActive = Ctx.m_pUi->CheckActiveItem(pId);
			const bool SliderReleased = SliderWasActive && !SliderActive;
			const int NewSliderValue = pScale->ToAbsolute(NewNormalized, SliderMin, SliderMax);
			if(NewSliderValue != SliderValue || SliderReleased)
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
					pState->m_SliderWasActive = SliderActive;
				}
				else
				{
					Changed = UpdateNumericFieldSliderCommit(*pState, Options.m_CommitPolicy, SliderActive, SliderReleased, CandidateStored, pValue) || Changed;
					const int VisibleStoredValue = pState->m_HasPendingValue ? pState->m_PendingStoredValue : *pValue;
					DisplayValue = SliderInputIsInfiniteValue(VisibleStoredValue, Infinite) ? Max : SliderInputDisplayValue(VisibleStoredValue, Multiplier);
					pInput->SetInteger(DisplayValue);
					pInput->SelectAll();
					pState->m_LastSyncedStoredValue = *pValue;
					pState->m_HasSyncedValue = true;
				}
			}
			else
				pState->m_SliderWasActive = SliderActive;
		}
		else if(HasSlider)
		{
			// 编辑数值时保留 DDNet 横向滑条的视觉，但不接管文本输入焦点。
			CUIRect Rail;
			ScrollBar.HMargin(5.0f, &Rail);
			CUIRect Handle;
			Rail.VSplitLeft(std::clamp(33.0f, Rail.h, Rail.w / 3.0f), &Handle, nullptr);
			Handle.x += (Rail.w - Handle.w) * Normalized;
			Rail.Draw(Ctx.m_pUi->ScaleBackgroundAlpha(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f)), IGraphics::CORNER_ALL, Rail.h / 2.0f);
			Handle.Draw(Ctx.m_pUi->ScaleBackgroundAlpha(CUi::ms_ScrollBarColorFunction.GetColor(false, false)), IGraphics::CORNER_ALL, Rail.h / 2.0f);
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
		const float FieldFontSize = std::min(Options.m_FontSize, InputField.h * CUi::ms_FontmodHeight * 0.8f);
		FieldOptions.m_FontSize = FieldFontSize;
		FieldOptions.m_TextAlign = TEXTALIGN_MC;
		SInputFieldResult Result = TextFieldEx(Ctx, pInput, InputField, FieldOptions);

		if(bShowMaxText)
			pInput->Set(aSavedInput);

		if(!RenderOnly && (Result.m_Deactivated || Result.m_Submitted))
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
			pState->m_HasPendingValue = false;
			pState->m_LastSyncedStoredValue = *pValue;
			pState->m_HasSyncedValue = true;
			DisplayValue = SliderInputIsInfiniteValue(*pValue, Infinite) ? Max : SliderInputDisplayValue(*pValue, Multiplier);
			pInput->SetInteger(DisplayValue);
			pInput->SelectAll();
		}

		if(HasSuffixRect)
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
