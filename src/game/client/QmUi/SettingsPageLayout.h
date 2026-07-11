/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_SETTINGSPAGELAYOUT_H
#define GAME_CLIENT_QMUI_SETTINGSPAGELAYOUT_H

#include "UiTokens.h"

#include <game/client/ui_rect.h>

#include <algorithm>

struct SSettingsPageLayoutFrame
{
	CUIRect m_PageRect;
	CUIRect m_ScrollViewport;
	CUIRect m_ContentViewport;
	CUIRect m_SubTabRect;
	CUIRect m_aColumns[2];
	float m_CardGap = 0.0f;
	bool m_TwoColumns = false;
};

inline SSettingsPageLayoutFrame ResolveSettingsPageLayout(const CUIRect &PageRect, const bool HasSubTabs, const float UiScale = 1.0f)
{
	const float Scale = UiScale > 0.0f ? UiScale : 1.0f;
	const float Inset = ui_token::settings::PAGE_INSET * Scale;
	SSettingsPageLayoutFrame Frame{};
	Frame.m_PageRect = PageRect;
	float ContentTop = PageRect.y;
	float ContentHeight = std::max(0.0f, PageRect.h);
	if(HasSubTabs)
	{
		const float SubTabHeight = std::min(ui_token::settings::SUB_TAB_HEIGHT * Scale, ContentHeight);
		Frame.m_SubTabRect = {PageRect.x, PageRect.y, PageRect.w, SubTabHeight};
		const float SubTabGap = std::min(ui_token::settings::SUB_TAB_GAP * Scale, ContentHeight - SubTabHeight);
		ContentTop += SubTabHeight + SubTabGap;
		ContentHeight -= SubTabHeight + SubTabGap;
	}
	Frame.m_ScrollViewport = {
		PageRect.x + Inset,
		ContentTop + Inset,
		std::max(0.0f, PageRect.w - Inset * 2.0f),
		std::max(0.0f, ContentHeight - Inset * 2.0f),
	};
	Frame.m_ContentViewport = Frame.m_ScrollViewport;
	Frame.m_CardGap = ui_token::settings::CARD_GAP * Scale;
	Frame.m_TwoColumns = Frame.m_ContentViewport.w >= ui_token::settings::TWO_COLUMN_MIN_WIDTH * Scale;
	if(Frame.m_TwoColumns)
	{
		const float ColumnWidth = std::max(0.0f, (Frame.m_ContentViewport.w - Frame.m_CardGap) * 0.5f);
		Frame.m_aColumns[0] = {Frame.m_ContentViewport.x, Frame.m_ContentViewport.y, ColumnWidth, Frame.m_ContentViewport.h};
		Frame.m_aColumns[1] = {Frame.m_aColumns[0].x + ColumnWidth + Frame.m_CardGap, Frame.m_ContentViewport.y, ColumnWidth, Frame.m_ContentViewport.h};
	}
	else
		Frame.m_aColumns[0] = Frame.m_ContentViewport;
	return Frame;
}

#endif
