/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_UIFORMS_H
#define GAME_CLIENT_QMUI_UIFORMS_H

#include "UiContext.h"
#include "UiTokens.h"

#include <engine/graphics.h>
#include <engine/textrender.h>

#include <game/client/ui.h>

#include <algorithm>
#include <cmath>

class CLineInput;
class CLineInputNumber;
class CUIRect;
class IScrollbarScale;

namespace ui_widget
{

	enum class EInputFieldCapability : unsigned
	{
		DOUBLE_CLICK_SELECT_ALL = 1u << 0,
		CLICK_AWAY_COMMIT = 1u << 1,
		CURSOR_INSERTION = 1u << 2,
		MOUSE_DRAG_SELECTION = 1u << 3,
		CLEAR_BUTTON = 1u << 4,
		SEARCH_HOTKEY = 1u << 5,
	};

	constexpr unsigned InputFieldCapabilities()
	{
		return static_cast<unsigned>(EInputFieldCapability::DOUBLE_CLICK_SELECT_ALL) |
		       static_cast<unsigned>(EInputFieldCapability::CLICK_AWAY_COMMIT) |
		       static_cast<unsigned>(EInputFieldCapability::CURSOR_INSERTION) |
		       static_cast<unsigned>(EInputFieldCapability::MOUSE_DRAG_SELECTION);
	}

	constexpr unsigned ClearableInputFieldCapabilities()
	{
		return InputFieldCapabilities() |
		       static_cast<unsigned>(EInputFieldCapability::CLEAR_BUTTON);
	}

	constexpr unsigned SearchFieldCapabilities()
	{
		return ClearableInputFieldCapabilities() |
		       static_cast<unsigned>(EInputFieldCapability::SEARCH_HOTKEY);
	}

	struct SInputFieldResult
	{
		bool m_Changed = false;
		bool m_Committed = false;
		bool m_Submitted = false;
		bool m_Deactivated = false;
		bool m_Cleared = false;
	};

	inline SInputFieldResult BuildInputFieldResult(bool WasActive, bool IsActive, bool Changed, bool SubmitPressed, bool WasEmpty, bool IsEmpty, bool Clearable)
	{
		SInputFieldResult Result;
		Result.m_Changed = Changed;
		Result.m_Deactivated = WasActive && !IsActive;
		Result.m_Committed = Result.m_Deactivated;
		Result.m_Submitted = Result.m_Deactivated && SubmitPressed;
		Result.m_Cleared = Clearable && Changed && !WasEmpty && IsEmpty;
		return Result;
	}

	struct SInputFieldLayout
	{
		CUIRect m_ShellRect;
		CUIRect m_ContentRect;
		CUIRect m_IconRect;
		CUIRect m_ClearRect;
	};

	inline SInputFieldLayout ResolveInputFieldLayout(const CUIRect &Rect, bool HasIcon, bool Clearable, float UiScale = 1.0f)
	{
		SInputFieldLayout Layout{};
		Layout.m_ShellRect = Rect;
		Layout.m_ContentRect = Rect;
		const float Scale = std::max(UiScale, 0.1f);
		const float SlotWidth = std::max(Rect.h, 18.0f * Scale);
		const float Gap = 6.0f * Scale;
		Layout.m_ContentRect.x += Gap;
		Layout.m_ContentRect.w = std::max(0.0f, Layout.m_ContentRect.w - Gap * 2.0f);
		if(HasIcon)
		{
			Layout.m_IconRect = Layout.m_ContentRect;
			Layout.m_IconRect.w = std::min(SlotWidth, Layout.m_ContentRect.w);
			const float Consumed = std::min(Layout.m_ContentRect.w, SlotWidth + Gap);
			Layout.m_ContentRect.x += Consumed;
			Layout.m_ContentRect.w = std::max(0.0f, Layout.m_ContentRect.w - Consumed);
		}
		if(Clearable)
		{
			Layout.m_ClearRect = Layout.m_ContentRect;
			Layout.m_ClearRect.w = std::min(SlotWidth, Layout.m_ContentRect.w);
			Layout.m_ClearRect.x = Layout.m_ContentRect.x + std::max(0.0f, Layout.m_ContentRect.w - Layout.m_ClearRect.w);
			const float Consumed = std::min(Layout.m_ContentRect.w, SlotWidth + Gap);
			Layout.m_ContentRect.w = std::max(0.0f, Layout.m_ContentRect.w - Consumed);
		}
		return Layout;
	}
	struct STextFieldOptions
	{
		const char *m_pPlaceholder = nullptr;
		float m_FontSize = ui_token::font::BODY;
		int m_Corners = IGraphics::CORNER_ALL;
		float m_CornerRadius = ui_token::radius::BASE;
		int m_TextAlign = TEXTALIGN_ML;
	};

	// 基础输入框（沿用设置页灰色按钮背景）。
	SInputFieldResult TextFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder = nullptr, float FontSize = ui_token::font::BODY);
	SInputFieldResult TextFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const STextFieldOptions &Options);
	bool TextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder = nullptr, float FontSize = ui_token::font::BODY);
	bool TextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const STextFieldOptions &Options);

	// 传统 DDNet 按钮风格输入框（保持旧版背景颜色）。
	SInputFieldResult LegacyTextFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const STextFieldOptions &Options);
	bool LegacyTextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder = nullptr, float FontSize = ui_token::font::BODY);

	// 只读输入框（不接收输入，仅渲染文本）。
	void ReadOnlyTextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder = nullptr, float FontSize = ui_token::font::BODY);

	// 带清除按钮的单行输入框。编辑行为委托给 CLineInput/CUi，保持光标、
	// 选择、双击全选、IME、提交和外部点击保存行为与 DDNet 一致。
	SInputFieldResult ClearableTextFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder = nullptr, float FontSize = ui_token::font::BODY);
	bool ClearableTextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder = nullptr, float FontSize = ui_token::font::BODY);

	// 带左侧图标和可选右侧清除按钮的输入框。图标默认使用搜索图标。
	SInputFieldResult IconTextFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const STextFieldOptions &Options, const char *pIcon = nullptr, bool Clearable = true);
	bool IconTextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder = nullptr, float FontSize = ui_token::font::BODY, const char *pIcon = nullptr, bool Clearable = true);

	// 搜索输入框，包含搜索图标和清除按钮。HotkeyEnabled 为 true 时 Ctrl+F
	// 会聚焦并全选。
	SInputFieldResult SearchFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, float FontSize = ui_token::font::BODY, bool HotkeyEnabled = true);
	bool SearchField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, float FontSize = ui_token::font::BODY, bool HotkeyEnabled = true);

	// 整数输入框，自动与外部 int 同步。
	SInputFieldResult IntegerField(const IUiContext &Ctx, CLineInputNumber *pInput, int *pValue, int Min, int Max, const CUIRect &Rect, const STextFieldOptions &Options);

	inline int SliderInputStoredMinimum(int DisplayMin, int ValueMultiplier) { return DisplayMin / std::max(1, ValueMultiplier); }
	inline int SliderInputStoredMaximum(int DisplayMax, int ValueMultiplier) { return DisplayMax / std::max(1, ValueMultiplier); }
	inline int SliderInputDisplayValue(int StoredValue, int ValueMultiplier) { return StoredValue * std::max(1, ValueMultiplier); }
	inline int SliderInputStoredValue(int DisplayValue, int ValueMultiplier) { return (int)std::round(DisplayValue / (float)std::max(1, ValueMultiplier)); }
	inline bool SliderInputIsInfiniteValue(int StoredValue, bool Infinite) { return Infinite && StoredValue == 0; }
	inline int SliderInputWheelStoredValue(int StoredValue, int SliderMin, int SliderMax, bool Infinite, int Increment)
	{
		const int SliderValue = SliderInputIsInfiniteValue(StoredValue, Infinite) ? SliderMax : std::clamp(StoredValue, SliderMin, SliderMax);
		const int NewSliderValue = std::clamp(SliderValue + Increment, SliderMin, SliderMax);
		return Infinite && NewSliderValue == SliderMax ? 0 : NewSliderValue;
	}
	CUIRect SliderInputFieldLabelRect(const CUIRect &Rect, bool HasLabel, unsigned Flags = 0u);

	enum class EInputCommitPolicy
	{
		LIVE,
		ON_RELEASE_OR_SUBMIT,
	};

	struct SNumericFieldCommitState
	{
		int m_PendingStoredValue = 0;
		bool m_HasPendingValue = false;
		bool m_SliderWasActive = false;
	};

	struct SNumericFieldState : public SNumericFieldCommitState
	{
		CLineInputNumber m_Input;
		int m_LastSyncedStoredValue = 0;
		bool m_HasSyncedValue = false;
	};

	// 横向滚动条 + 输入框 + 单位组合。支持整数/浮点、线性/对数刻度、
	// ♾️ 无限符号和最大值文本。
	struct SNumericFieldOptions
	{
		const char *m_pLabel = nullptr;
		const char *m_pSuffix = nullptr;
		const IScrollbarScale *m_pScale = nullptr; // nullptr = 线性
		unsigned m_Flags = 0; // CUi::SCROLLBAR_OPTION_* 标志
		const char *m_pMaxText = nullptr; // 当值为 Max 且非 Infinite 时显示
		float m_FontSize = ui_token::font::BODY;
		int m_LabelAlign = TEXTALIGN_ML;
		int m_ValueMultiplier = 1; // 滑动条以 Min/Multiplier..Max/Multiplier 为单位，显示/编辑真实值
		EInputCommitPolicy m_CommitPolicy = EInputCommitPolicy::LIVE;
		CUIElement *m_pLabelElement = nullptr;
	};
	bool NumericField(const IUiContext &Ctx, SNumericFieldState *pState, const void *pId, int *pValue, int Min, int Max, const CUIRect &Rect, const SNumericFieldOptions &Options);

	// Boolean switch. Slider position animates with a spring driver between left
	// (off) and right (on). Returns true on click.
	bool Toggle(const IUiContext &Ctx, const void *pId, bool *pValue, const CUIRect &Rect);

	// Horizontal slider with a numeric label on the right. Wraps DoScrollbarH.
	// Returns true when the value changed this frame.
	bool Slider(const IUiContext &Ctx, const void *pId, float *pValue, float Min, float Max, const CUIRect &Rect, const char *pSuffix = "");

} // namespace ui_widget

#endif
