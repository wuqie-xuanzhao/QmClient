/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_UIFORMS_H
#define GAME_CLIENT_QMUI_UIFORMS_H

#include "UiContext.h"
#include "UiTokens.h"

#include <engine/graphics.h>

#include <game/client/ui.h>

class CLineInput;
class CUIRect;

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

	struct STextFieldOptions
	{
		const char *m_pPlaceholder = nullptr;
		float m_FontSize = ui_token::font::BODY;
		int m_Corners = IGraphics::CORNER_ALL;
		float m_CornerRadius = ui_token::radius::BASE;
		int m_TextAlign = TEXTALIGN_ML;
	};

	SInputFieldResult TextFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder = nullptr, float FontSize = ui_token::font::BODY);
	SInputFieldResult TextFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const STextFieldOptions &Options);
	SInputFieldResult ClearableTextFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder = nullptr, float FontSize = ui_token::font::BODY);
	SInputFieldResult SearchFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, float FontSize = ui_token::font::BODY, bool HotkeyEnabled = true);

	// Single-line text input with placeholder and animated focus ring.
	// Returns true when the input value changed this frame.
	bool TextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder = nullptr, float FontSize = ui_token::font::BODY);
	bool TextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const STextFieldOptions &Options);

	// 带清除按钮的单行输入框。编辑行为委托给 CLineInput/CUi，保持光标、
	// 选择、双击全选、IME、提交和外部点击保存行为与 DDNet 一致。
	bool ClearableTextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder = nullptr, float FontSize = ui_token::font::BODY);

	// 搜索输入框，包含搜索图标和清除按钮。HotkeyEnabled 为 true 时 Ctrl+F
	// 会聚焦并全选。
	bool SearchField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, float FontSize = ui_token::font::BODY, bool HotkeyEnabled = true);

	// Boolean switch. Slider position animates with a spring driver between left
	// (off) and right (on). Returns true on click.
	bool Toggle(const IUiContext &Ctx, const void *pId, bool *pValue, const CUIRect &Rect);

	// Horizontal slider with a numeric label on the right. Wraps DoScrollbarH.
	// Returns true when the value changed this frame.
	bool Slider(const IUiContext &Ctx, const void *pId, float *pValue, float Min, float Max, const CUIRect &Rect, const char *pSuffix = "");

} // namespace ui_widget

#endif
