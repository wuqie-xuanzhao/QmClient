/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ui_listbox.h"

#include <base/system.h>
#include <base/vmath.h>

#include <engine/config.h>
#include <engine/shared/config.h>

#include <game/client/QmUi/UiSurface.h>
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
	m_HideScrollbar = false;
	m_ScrollbarAlwaysReserved = false;
	m_InitialScrollPending = true;
	m_EntryAnimationOffset = 0.0f;
	m_LastRenderTime = 0;
	m_EntryAnimationStartTime = 0;
	m_BackgroundCorners = IGraphics::CORNER_ALL;
	m_SelectedItemActiveColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f);
	m_SelectedItemInactiveColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.33f);
	m_HoveredItemColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.33f);
}

void CListBox::DoHeader(const CUIRect *pRect, const char *pTitle, float HeaderHeight, float Spacing, int BackgroundCorners)
{
	CUIRect View = *pRect;
	CUIRect Header;
	m_BackgroundCorners = BackgroundCorners;

	// background
	View.HSplitTop(HeaderHeight + Spacing, &Header, nullptr);
	DrawRoundedSurface(Ui(), Header, Ui()->ScaleBackgroundAlpha(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f)), ColorRGBA(), 5.0f, 0.0f, m_BackgroundCorners & IGraphics::CORNER_T);

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
		DrawRoundedSurface(Ui(), View, Ui()->ScaleBackgroundAlpha(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f)), ColorRGBA(), 5.0f, 0.0f, m_BackgroundCorners & (m_HasHeader ? IGraphics::CORNER_B : IGraphics::CORNER_ALL));

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
	const int64_t Now = time_get();
	const int64_t Frequency = maximum<int64_t>(1, time_freq());
	const bool EntryAnimationEnabled = g_Config.m_QmUiListEntryAnimations != 0 && g_Config.m_QmUiMotionLevel > 0;
	const int64_t InactiveGap = Frequency * 2 / 5;
	const bool RenderOnly = Ui()->RenderOnly();
	if(!EntryAnimationEnabled && !RenderOnly)
		m_EntryAnimationStartTime = 0;
	else if(QmListBoxShouldStartEntryAnimation(EntryAnimationEnabled, RenderOnly, m_LastRenderTime, Now, InactiveGap))
		m_EntryAnimationStartTime = Now;
	m_EntryAnimationOffset = 0.0f;
	if(!RenderOnly && EntryAnimationEnabled && m_EntryAnimationStartTime > 0)
	{
		const float EntryDuration = g_Config.m_QmUiMotionLevel == 1 ? 0.10f : 0.16f;
		const float EntryDistance = g_Config.m_QmUiMotionLevel == 1 ? 6.0f : 12.0f;
		const float ElapsedSeconds = static_cast<float>(Now - m_EntryAnimationStartTime) / static_cast<float>(Frequency);
		m_EntryAnimationOffset = QmListBoxEntryOffset(ElapsedSeconds, EntryDuration, EntryDistance);
		if(QmListBoxEntryAnimationFinished(EntryAnimationEnabled, RenderOnly, ElapsedSeconds, EntryDuration))
			m_EntryAnimationStartTime = 0;
	}
	if(!RenderOnly)
		m_LastRenderTime = Now;
	if(QmListBoxShouldScrollToInitialSelection(m_InitialScrollPending, SelectedIndex))
	{
		m_ListBoxUpdateScroll = true;
	}
	m_InitialScrollPending = QmListBoxInitialScrollRemainsPending(m_InitialScrollPending, SelectedIndex);

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
	ScrollParams.m_HideScrollbar = m_HideScrollbar;
	ScrollParams.m_ScrollbarAlwaysReserved = m_ScrollbarAlwaysReserved;
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
	Item.m_Rect = QmListBoxEntryAnimatedRect(Item.m_Rect, m_EntryAnimationOffset);
	Item.m_Visible = !m_ScrollRegion.RectClipped(Item.m_Rect);

	m_ListBoxItemIndex++;
	return Item;
}

CListboxItem CListBox::DoNextItem(const void *pId, bool Selected, float CornerRadius)
{
	CUiScopedGaussianBlurSuppression GaussianBlurSuppression(Ui());
	if(m_AutoSpacing > 0.0f && m_ListBoxItemIndex > 0)
		DoSpacing(m_AutoSpacing);

	const int ThisItemIndex = m_ListBoxItemIndex;
	if(Selected)
	{
		if(QmListBoxShouldScrollToInitialSelection(m_InitialScrollPending, ThisItemIndex))
			m_ListBoxUpdateScroll = true;
		m_InitialScrollPending = QmListBoxInitialScrollRemainsPending(m_InitialScrollPending, ThisItemIndex);
		if(m_ListBoxSelectedIndex == m_ListBoxNewSelected)
			m_ListBoxNewSelected = ThisItemIndex;
		m_ListBoxSelectedIndex = ThisItemIndex;
	}

	CListboxItem Item = DoNextRow();
	Item.m_pUi = Ui();
	Item.m_GaussianBlurSuppressed = true;
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

		DrawRoundedSurface(Ui(), Item.m_Rect, Ui()->ScaleBackgroundAlpha(m_Active ? m_SelectedItemActiveColor : m_SelectedItemInactiveColor), ColorRGBA(), CornerRadius);
	}
	if(Ui()->HotItem() == pId && !m_ScrollRegion.Animating())
	{
		DrawRoundedSurface(Ui(), Item.m_Rect, Ui()->ScaleBackgroundAlpha(m_HoveredItemColor), ColorRGBA(), CornerRadius);
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
	m_ScrollRegion.AddRect(Item.m_Rect, ScrollHere);
	Item.m_Rect = QmListBoxEntryAnimatedRect(Item.m_Rect, m_EntryAnimationOffset);
	Item.m_Visible = !m_ScrollRegion.RectClipped(Item.m_Rect);
	return Item;
}

CListboxItem CListBox::DoSubheader()
{
	CListboxItem Item = DoNextRow();
	DrawRoundedSurface(Ui(), Item.m_Rect, Ui()->ScaleBackgroundAlpha(ColorRGBA(1.0f, 1.0f, 1.0f, 0.2f)), ColorRGBA(), 0.0f, 0.0f, IGraphics::CORNER_NONE);
	return Item;
}

int CListBox::DoEnd()
{
	m_ScrollRegion.End();
	m_Active |= m_ScrollRegion.Active();

	m_ScrollbarShown = m_ScrollRegion.ScrollbarShown();
	if(m_ListBoxItemSelected || m_ScrollRegion.WheelConsumedThisFrame() || m_ScrollRegion.Active())
		m_InitialScrollPending = false;
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
