/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QmDropdown.h"

#include <algorithm>

namespace
{
	bool RectsOverlap(const CUIRect &A, const CUIRect &B)
	{
		return A.x < B.x + B.w && A.x + A.w > B.x && A.y < B.y + B.h && A.y + A.h > B.y;
	}
}

SQmDropdownGeometryResult QmComputeDropdownPopupGeometry(const CUIRect &AnchorRect, const CUIRect &ViewportRect, const SQmDropdownGeometryConfig &Config)
{
	SQmDropdownGeometryResult Result;
	Result.m_AnchorVisible = RectsOverlap(AnchorRect, ViewportRect);

	const float Margin = std::max(0.0f, Config.m_Margin);
	const float Gap = std::max(0.0f, Config.m_Gap);
	const float MinX = ViewportRect.x + Margin;
	const float MinY = ViewportRect.y + Margin;
	const float MaxX = ViewportRect.x + std::max(0.0f, ViewportRect.w - Margin);
	const float MaxY = ViewportRect.y + std::max(0.0f, ViewportRect.h - Margin);
	const float AvailableWidth = std::max(0.0f, MaxX - MinX);
	const float AvailableHeight = std::max(0.0f, MaxY - MinY);

	Result.m_Rect.w = std::min(std::max(0.0f, Config.m_Width), AvailableWidth);
	Result.m_Rect.h = std::min(std::max(0.0f, Config.m_Height), AvailableHeight);

	const float BelowY = AnchorRect.y + AnchorRect.h + Gap;
	const float AboveY = AnchorRect.y - Gap - Result.m_Rect.h;
	const bool FitsBelow = BelowY + Result.m_Rect.h <= MaxY;
	const bool FitsAbove = AboveY >= MinY;
	Result.m_PlacedBelow = Config.m_PreferBelow ? (FitsBelow || !FitsAbove) : (!FitsAbove && FitsBelow);

	Result.m_Rect.x = AnchorRect.x;
	Result.m_Rect.y = Result.m_PlacedBelow ? BelowY : AboveY;

	const float ClampedX = std::clamp(Result.m_Rect.x, MinX, std::max(MinX, MaxX - Result.m_Rect.w));
	const float ClampedY = std::clamp(Result.m_Rect.y, MinY, std::max(MinY, MaxY - Result.m_Rect.h));
	Result.m_Clamped = ClampedX != Result.m_Rect.x || ClampedY != Result.m_Rect.y || Result.m_Rect.w != Config.m_Width || Result.m_Rect.h != Config.m_Height;
	Result.m_Rect.x = ClampedX;
	Result.m_Rect.y = ClampedY;
	Result.m_PopupVisible = Result.m_Rect.w > 0.0f && Result.m_Rect.h > 0.0f && RectsOverlap(Result.m_Rect, ViewportRect);
	return Result;
}

SQmDropdownPopupPolicy QmResolveDropdownPopupPolicy(int ItemCount, float EntryHeight, float EntrySpacing, bool HasMessage, float MessageHeight, float OuterHeight)
{
	SQmDropdownPopupPolicy Policy;
	const int ClampedItemCount = std::max(0, ItemCount);
	const int VisibleItemCount = std::min(ClampedItemCount, Policy.m_MaxVisibleItems);
	const float ResolvedEntryHeight = std::max(0.0f, EntryHeight);
	const float ResolvedEntrySpacing = std::max(0.0f, EntrySpacing);
	const float ResolvedMessageHeight = HasMessage ? std::max(0.0f, MessageHeight) : 0.0f;
	const float ResolvedOuterHeight = std::max(0.0f, OuterHeight);
	const auto ItemsHeight = [ResolvedEntryHeight, ResolvedEntrySpacing, HasMessage](int Count) {
		if(Count <= 0)
			return 0.0f;
		return Count * ResolvedEntryHeight + (HasMessage ? Count : Count - 1) * ResolvedEntrySpacing;
	};
	Policy.m_ContentHeight = ResolvedMessageHeight + ItemsHeight(ClampedItemCount) + ResolvedOuterHeight;
	Policy.m_PreferredHeight = ResolvedMessageHeight + ItemsHeight(VisibleItemCount) + ResolvedOuterHeight;
	return Policy;
}

bool QmDropdownPopupOwnsWheel(const SQmDropdownPopupPolicy &Policy, float PopupHeight)
{
	return std::max(0.0f, PopupHeight) + 0.001f < Policy.m_ContentHeight;
}

void CQmDropdownState::Reset()
{
	m_Open = false;
	m_ActiveIndex = -1;
}

SQmDropdownUpdateResult CQmDropdownState::Update(const SQmDropdownInput &Input, int ItemCount)
{
	SQmDropdownUpdateResult Result;
	ItemCount = std::max(0, ItemCount);

	if(!m_Open)
	{
		if(Input.m_TogglePressed && ItemCount > 0)
		{
			m_Open = true;
			m_ActiveIndex = 0;
			Result.m_Opened = true;
		}
		return Result;
	}

	if(ItemCount <= 0 || Input.m_KeyEscape || Input.m_ClickOutside || Input.m_TogglePressed)
	{
		Reset();
		Result.m_Closed = true;
		return Result;
	}

	m_ActiveIndex = std::clamp(m_ActiveIndex, 0, ItemCount - 1);
	if(Input.m_HoveredIndex >= 0 && Input.m_HoveredIndex < ItemCount)
		m_ActiveIndex = Input.m_HoveredIndex;

	if(Input.m_KeyUp)
		m_ActiveIndex = (m_ActiveIndex + ItemCount - 1) % ItemCount;
	if(Input.m_KeyDown)
		m_ActiveIndex = (m_ActiveIndex + 1) % ItemCount;

	if(Input.m_KeyEnter || (Input.m_MouseSelectPressed && Input.m_HoveredIndex >= 0 && Input.m_HoveredIndex < ItemCount))
	{
		if(Input.m_MouseSelectPressed)
			m_ActiveIndex = Input.m_HoveredIndex;
		Result.m_Selected = true;
		Result.m_SelectedIndex = m_ActiveIndex;
		Reset();
		Result.m_Closed = true;
	}

	return Result;
}
