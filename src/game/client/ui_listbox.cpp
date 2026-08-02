/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ui_listbox.h"

#include <base/system.h>
#include <base/vmath.h>

#include <engine/config.h>
#include <engine/shared/config.h>

#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

CListBox::CListBox()
{
	Reset();
}

void CListBox::Reset()
{
	m_ListBoxView = m_RowView = CUIRect{0.0f, 0.0f, 0.0f, 0.0f};
	m_ListBoxUpdateScroll = false;
	m_ScrollbarShown = false;
	m_AutoSpacing = 0.0f;
	m_ScrollRegion.Reset();
	m_ScrollProfile = EQmScrollProfile::MENU_LIST;
	const CScrollRegionParams ScrollParams = QmScrollRegionParamsForSize(EQmScrollSize::MEDIUM);
	m_WheelOwnerPriority = EUiWheelOwnerPriority::PAGE;
	m_ScrollbarWidth = ScrollParams.m_ScrollbarThickness;
	m_ScrollbarMargin = ScrollParams.m_ScrollbarMargin;
	m_ScrollbarWidthOverridden = false;
	m_ScrollbarMarginOverridden = false;
	m_HasHeader = false;
	m_Active = true;
	m_SelectedItemActiveColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f);
	m_SelectedItemInactiveColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.33f);
	m_HoveredItemColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.33f);
}

void CListBox::DoHeader(const CUIRect *pRect, const char *pTitle, float HeaderHeight, float Spacing)
{
	CUIRect View = *pRect;
	CUIRect Header;

	// background
	View.HSplitTop(HeaderHeight + Spacing, &Header, nullptr);
	Header.Draw(Ui()->ScaleBackgroundAlpha(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f)), m_BackgroundCorners & IGraphics::CORNER_T, 5.0f);

	// draw header
	View.HSplitTop(HeaderHeight, &Header, &View);
	Ui()->DoLabel(&Header, pTitle, Header.h * CUi::ms_FontmodHeight * 0.8f, TEXTALIGN_MC);

	View.HSplitTop(Spacing, &Header, &View);

	// setup the variables
	m_ListBoxView = View;
	m_HasHeader = true;
}

void CListBox::DoSpacing(float Spacing)
{
	CUIRect View = m_ListBoxView;
	View.HSplitTop(Spacing, nullptr, &View);
	m_ListBoxView = View;
}

void CListBox::DoStart(float RowHeight, int NumItems, int ItemsPerRow, int RowsPerScroll, int SelectedIndex, const CUIRect *pRect, bool Background, int BackgroundCorners)
{
	CUIRect View;
	if(pRect)
		View = *pRect;
	else
		View = m_ListBoxView;

	// background
	m_BackgroundCorners = BackgroundCorners;
	if(Background)
		View.Draw(Ui()->ScaleBackgroundAlpha(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f)), m_BackgroundCorners & (m_HasHeader ? IGraphics::CORNER_B : IGraphics::CORNER_ALL), 5.0f);

	// setup the variables
	m_ListBoxView = View;
	m_RowView = {};
	m_ListBoxSelectedIndex = SelectedIndex;
	m_ListBoxNewSelected = SelectedIndex;
	m_ListBoxNewSelOffset = 0;
	m_ListBoxItemIndex = 0;
	m_ListBoxRowHeight = RowHeight;
	m_ListBoxNumItems = NumItems;
	m_ListBoxItemsPerRow = ItemsPerRow;
	m_ListBoxItemActivated = false;
	m_ListBoxItemSelected = false;

	// handle input
	if(m_Active && !Input()->ModifierIsPressed() && !Input()->ShiftIsPressed() && !Input()->AltIsPressed())
	{
		if(Ui()->ConsumeHotkey(CUi::HOTKEY_DOWN))
			m_ListBoxNewSelOffset += m_ListBoxItemsPerRow;
		else if(Ui()->ConsumeHotkey(CUi::HOTKEY_UP))
			m_ListBoxNewSelOffset -= m_ListBoxItemsPerRow;
		else if(Ui()->ConsumeHotkey(CUi::HOTKEY_RIGHT) && m_ListBoxItemsPerRow > 1)
			m_ListBoxNewSelOffset += 1;
		else if(Ui()->ConsumeHotkey(CUi::HOTKEY_LEFT) && m_ListBoxItemsPerRow > 1)
			m_ListBoxNewSelOffset -= 1;
		else if(Ui()->ConsumeHotkey(CUi::HOTKEY_PAGE_UP))
			m_ListBoxNewSelOffset = -ItemsPerRow * RowsPerScroll * 4;
		else if(Ui()->ConsumeHotkey(CUi::HOTKEY_PAGE_DOWN))
			m_ListBoxNewSelOffset = ItemsPerRow * RowsPerScroll * 4;
		else if(Ui()->ConsumeHotkey(CUi::HOTKEY_HOME))
			m_ListBoxNewSelOffset = 1 - m_ListBoxNumItems;
		else if(Ui()->ConsumeHotkey(CUi::HOTKEY_END))
			m_ListBoxNewSelOffset = m_ListBoxNumItems - 1;
	}

	// setup the scrollbar
	vec2 ScrollOffset = vec2(0.0f, 0.0f);
	SQmScrollRequest ScrollRequest;
	ScrollRequest.m_Profile = m_ScrollProfile;
	ScrollRequest.m_RowExtent = m_ListBoxRowHeight + m_AutoSpacing;
	ScrollRequest.m_RowsPerStep = RowsPerScroll;
	const SQmResolvedScrollPolicy ScrollPolicy = QmResolveScrollPolicy(ScrollRequest);
	CScrollRegionParams ScrollParams = QmScrollRegionParamsFromPolicy(ScrollPolicy);
	ScrollParams.m_WheelOwnerPriority = m_WheelOwnerPriority;
	m_ScrollbarWidth = QmListBoxScrollbarMetric(ScrollParams.m_ScrollbarThickness, m_ScrollbarWidth, m_ScrollbarWidthOverridden);
	m_ScrollbarMargin = QmListBoxScrollbarMetric(ScrollParams.m_ScrollbarMargin, m_ScrollbarMargin, m_ScrollbarMarginOverridden);
	ScrollParams.m_ScrollbarThickness = m_ScrollbarWidth;
	ScrollParams.m_ScrollbarMargin = m_ScrollbarMargin;
	const int NumRows = (m_ListBoxNumItems + maximum(1, m_ListBoxItemsPerRow) - 1) / maximum(1, m_ListBoxItemsPerRow);
	m_ScrollRegion.SetContentHeightForNextFrame(NumRows * m_ListBoxRowHeight + maximum(0, NumRows - 1) * m_AutoSpacing);
	m_ScrollRegion.Begin(&m_ListBoxView, &ScrollOffset, &ScrollParams);
	m_ListBoxView.y += ScrollOffset.y;
}

CListboxItem CListBox::DoNextRow()
{
	CListboxItem Item = {};

	if(m_ListBoxItemIndex % m_ListBoxItemsPerRow == 0)
		m_ListBoxView.HSplitTop(m_ListBoxRowHeight, &m_RowView, &m_ListBoxView);
	m_ScrollRegion.AddRect(m_RowView);
	if(m_ListBoxUpdateScroll && m_ListBoxSelectedIndex == m_ListBoxItemIndex)
	{
		m_ScrollRegion.ScrollHere(CScrollRegion::SCROLLHERE_KEEP_IN_VIEW);
		m_ListBoxUpdateScroll = false;
	}

	m_RowView.VSplitLeft(m_RowView.w / (m_ListBoxItemsPerRow - m_ListBoxItemIndex % m_ListBoxItemsPerRow), &Item.m_Rect, &m_RowView);

	Item.m_Selected = m_ListBoxSelectedIndex == m_ListBoxItemIndex;
	Item.m_Visible = !m_ScrollRegion.RectClipped(Item.m_Rect);

	m_ListBoxItemIndex++;
	return Item;
}

CListboxItem CListBox::DoNextItem(const void *pId, bool Selected, float CornerRadius)
{
	if(m_AutoSpacing > 0.0f && m_ListBoxItemIndex > 0)
		DoSpacing(m_AutoSpacing);

	const int ThisItemIndex = m_ListBoxItemIndex;
	if(Selected)
	{
		if(m_ListBoxSelectedIndex == m_ListBoxNewSelected)
			m_ListBoxNewSelected = ThisItemIndex;
		m_ListBoxSelectedIndex = ThisItemIndex;
	}

	CListboxItem Item = DoNextRow();
	const int ItemClicked = Item.m_Visible ? Ui()->DoButtonLogic(pId, 0, &Item.m_Rect, BUTTONFLAG_LEFT) : 0;
	if(ItemClicked)
	{
		m_ListBoxNewSelected = ThisItemIndex;
		m_ListBoxItemSelected = true;
		m_Active = true;
	}

	// process input, regard selected index
	if(m_ListBoxNewSelected == ThisItemIndex)
	{
		if(m_Active)
		{
			if(Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER) || (ItemClicked == 1 && Ui()->DoDoubleClickLogic(pId)))
			{
				m_ListBoxItemActivated = true;
				Ui()->SetActiveItem(nullptr);
			}
		}

		Item.m_Rect.Draw(Ui()->ScaleBackgroundAlpha(m_Active ? m_SelectedItemActiveColor : m_SelectedItemInactiveColor), IGraphics::CORNER_ALL, CornerRadius);
	}
	if(Ui()->HotItem() == pId && !m_ScrollRegion.Animating())
	{
		Item.m_Rect.Draw(Ui()->ScaleBackgroundAlpha(m_HoveredItemColor), IGraphics::CORNER_ALL, CornerRadius);
	}

	return Item;
}

void CListBox::SkipItems(int Count)
{
	Count = std::max(0, Count);
	while(Count > 0 && m_ListBoxItemIndex < m_ListBoxNumItems)
	{
		if(m_ListBoxItemIndex % m_ListBoxItemsPerRow == 0)
		{
			m_ListBoxView.HSplitTop(m_ListBoxRowHeight, &m_RowView, &m_ListBoxView);
			m_ScrollRegion.AddRect(m_RowView);
		}

		const int ItemIndexInRow = m_ListBoxItemIndex % m_ListBoxItemsPerRow;
		const int ItemsLeftInRow = m_ListBoxItemsPerRow - ItemIndexInRow;
		if(m_ListBoxUpdateScroll &&
			m_ListBoxSelectedIndex >= m_ListBoxItemIndex &&
			m_ListBoxSelectedIndex < m_ListBoxItemIndex + ItemsLeftInRow)
		{
			m_ScrollRegion.ScrollHere(CScrollRegion::SCROLLHERE_KEEP_IN_VIEW);
			m_ListBoxUpdateScroll = false;
		}

		const int ItemsToSkip = std::min(Count, ItemsLeftInRow);
		for(int i = 0; i < ItemsToSkip; ++i)
		{
			CUIRect Skipped;
			m_RowView.VSplitLeft(m_RowView.w / (m_ListBoxItemsPerRow - m_ListBoxItemIndex % m_ListBoxItemsPerRow), &Skipped, &m_RowView);
			++m_ListBoxItemIndex;
		}
		Count -= ItemsToSkip;
	}
}

CListboxItem CListBox::DoCustomRow(float Height, bool ScrollHere)
{
	CListboxItem Item = {};
	m_ListBoxView.HSplitTop(Height, &Item.m_Rect, &m_ListBoxView);
	Item.m_Visible = m_ScrollRegion.AddRect(Item.m_Rect, ScrollHere);
	return Item;
}

CListboxItem CListBox::DoSubheader()
{
	CListboxItem Item = DoNextRow();
	Item.m_Rect.Draw(Ui()->ScaleBackgroundAlpha(ColorRGBA(1.0f, 1.0f, 1.0f, 0.2f)), IGraphics::CORNER_NONE, 0.0f);
	return Item;
}

int CListBox::DoEnd()
{
	m_ScrollRegion.End();
	m_Active |= m_ScrollRegion.Active();

	m_ScrollbarShown = m_ScrollRegion.ScrollbarShown();
	if(m_ListBoxNewSelOffset != 0 && m_ListBoxNumItems > 0 && m_ListBoxSelectedIndex == m_ListBoxNewSelected)
	{
		if(m_ListBoxNewSelected == -1)
			m_ListBoxNewSelected = 0;
		else
			m_ListBoxNewSelected = std::clamp(m_ListBoxNewSelected + m_ListBoxNewSelOffset, 0, m_ListBoxNumItems - 1);
		ScrollToSelected();
	}
	return m_ListBoxNewSelected;
}
