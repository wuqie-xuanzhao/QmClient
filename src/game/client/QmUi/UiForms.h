/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_UIFORMS_H
#define GAME_CLIENT_QMUI_UIFORMS_H

#include "UiContext.h"
#include "UiTokens.h"

class CLineInput;
class CUIRect;

namespace ui_widget
{

	// Single-line text input with placeholder and animated focus ring.
	// Returns true when the input value changed this frame.
	bool TextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder = nullptr, float FontSize = ui_token::font::BODY);

	// Boolean switch. Slider position animates with a spring driver between left
	// (off) and right (on). Returns true on click.
	bool Toggle(const IUiContext &Ctx, const void *pId, bool *pValue, const CUIRect &Rect);

	// Horizontal slider with a numeric label on the right. Wraps DoScrollbarH.
	// Returns true when the value changed this frame.
	bool Slider(const IUiContext &Ctx, const void *pId, float *pValue, float Min, float Max, const CUIRect &Rect, const char *pSuffix = "");

} // namespace ui_widget

#endif
