// 请抬头享受阳光｜日子很好 我很我---------致咩子
/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_UINAVIGATION_H
#define GAME_CLIENT_QMUI_UINAVIGATION_H

#include "UiContext.h"

class CUIRect;

namespace ui_widget
{

	// Horizontal tab bar. Splits Rect into Count equal columns, renders each with
	// CMenus::DoButton_MenuTab and animates a 2px accent underline to the active
	// tab via the v2 animation runtime. Updates *pActive on click; returns the
	// new value (same as *pActive after this call).
	int TabBar(const IUiContext &Ctx, const char *const *ppLabels, int Count, int *pActive, const CUIRect &Rect);

	struct SListItemProps
	{
		const char *m_pLeadingIcon = nullptr;
		const char *m_pTrailingText = nullptr;
		bool m_Selected = false;
		bool m_Disabled = false;
	};

	// Row entry for lists. Hover state cross-fades a background tint; the
	// selected state paints ACCENT_PRIMARY_DIM as a persistent background.
	bool ListItem(const IUiContext &Ctx, const void *pId, const char *pText, const CUIRect &Rect, const SListItemProps &Props = {});

} // namespace ui_widget

#endif
