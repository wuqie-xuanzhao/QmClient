/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_QMDROPDOWN_H
#define GAME_CLIENT_QMUI_QMDROPDOWN_H

#include <game/client/ui_rect.h>

struct SQmDropdownGeometryConfig
{
	float m_Width = 0.0f;
	float m_Height = 0.0f;
	float m_Gap = 0.0f;
	float m_Margin = 0.0f;
	bool m_PreferBelow = true;
};

struct SQmDropdownGeometryResult
{
	CUIRect m_Rect{};
	bool m_AnchorVisible = false;
	bool m_PopupVisible = false;
	bool m_PlacedBelow = true;
	bool m_Clamped = false;
};

struct SQmDropdownPopupPolicy
{
	int m_MaxVisibleItems = 8;
	float m_ContentHeight = 0.0f;
	float m_PreferredHeight = 0.0f;
};

struct SQmDropdownInput
{
	bool m_TogglePressed = false;
	bool m_ClickOutside = false;
	bool m_KeyUp = false;
	bool m_KeyDown = false;
	bool m_KeyEnter = false;
	bool m_KeyEscape = false;
	int m_HoveredIndex = -1;
	bool m_MouseSelectPressed = false;
};

struct SQmDropdownUpdateResult
{
	bool m_Opened = false;
	bool m_Closed = false;
	bool m_Selected = false;
	int m_SelectedIndex = -1;
};

SQmDropdownGeometryResult QmComputeDropdownPopupGeometry(const CUIRect &AnchorRect, const CUIRect &ViewportRect, const SQmDropdownGeometryConfig &Config);
SQmDropdownPopupPolicy QmResolveDropdownPopupPolicy(int ItemCount, float EntryHeight, float EntrySpacing, bool HasMessage, float MessageHeight, float OuterHeight);
bool QmDropdownPopupOwnsWheel(const SQmDropdownPopupPolicy &Policy, float PopupHeight);

class CQmDropdownState
{
public:
	void Reset();
	SQmDropdownUpdateResult Update(const SQmDropdownInput &Input, int ItemCount);

	bool IsOpen() const { return m_Open; }
	int ActiveIndex() const { return m_ActiveIndex; }

private:
	bool m_Open = false;
	int m_ActiveIndex = -1;
};

#endif
