#include "SettingsCardDeckLogic.h"

#include <base/system.h>

#include <game/client/QmUi/QmCardRegistry.h>

#include <algorithm>
#include <cmath>

std::array<std::vector<int>, 3> BuildSettingsCardDeckColumnOrder(const qm_card_order::CModel &Model, const char *pTab, const std::vector<int> &vActiveStateIndices)
{
	std::array<std::vector<int>, 3> aColumns;
	for(int Column = 0; Column < (int)aColumns.size(); ++Column)
	{
		const std::vector<int> vModelColumn = Model.ColumnIndices(pTab, Column);
		for(const int StateIndex : vModelColumn)
		{
			if(std::find(vActiveStateIndices.begin(), vActiveStateIndices.end(), StateIndex) != vActiveStateIndices.end())
				aColumns[Column].push_back(StateIndex);
		}
	}
	return aColumns;
}

void ApplySettingsCardDeckDragPlacement(std::array<std::vector<int>, 3> &aColumns, int ActiveStateIndex, int TargetColumn, int TargetOrder)
{
	if(ActiveStateIndex < 0 || TargetColumn < 0 || TargetColumn >= (int)aColumns.size())
		return;
	for(std::vector<int> &vColumn : aColumns)
		vColumn.erase(std::remove(vColumn.begin(), vColumn.end(), ActiveStateIndex), vColumn.end());
	std::vector<int> &vTarget = aColumns[TargetColumn];
	const int InsertAt = std::clamp(TargetOrder, 0, (int)vTarget.size());
	vTarget.insert(vTarget.begin() + InsertAt, ActiveStateIndex);
}

int ResolveSettingsCardDeckDropOrder(float MouseY, int TargetColumn, const std::vector<SSettingsCardDeckItemGeometry> &vItems, int IgnoredStateIndex)
{
	int Order = 0;
	for(const SSettingsCardDeckItemGeometry &Item : vItems)
	{
		if(Item.m_Column != TargetColumn || Item.m_StateIndex == IgnoredStateIndex)
			continue;
		if(MouseY < Item.m_Rect.y + Item.m_Rect.h * 0.5f)
			return Order;
		++Order;
	}
	return Order;
}

float SettingsCardDeckAutoScrollDelta(float MouseY, const CUIRect &Viewport, float UiScale)
{
	const float Scale = std::max(0.1f, UiScale);
	const float EdgeSize = 32.0f * Scale;
	const float MaxSpeed = 180.0f * Scale;
	if(EdgeSize <= 0.0f || Viewport.h <= 0.0f)
		return 0.0f;
	const float TopDistance = MouseY - Viewport.y;
	if(TopDistance < EdgeSize)
		return -MaxSpeed * std::clamp((EdgeSize - TopDistance) / EdgeSize, 0.0f, 1.0f);
	const float BottomDistance = Viewport.y + Viewport.h - MouseY;
	if(BottomDistance < EdgeSize)
		return MaxSpeed * std::clamp((EdgeSize - BottomDistance) / EdgeSize, 0.0f, 1.0f);
	return 0.0f;
}

bool CommitSettingsCardDeckDrop(qm_card_order::CModel &Model, const char *pTab, const char *pStableId, int TargetColumn, int TargetOrder)
{
	if(pTab == nullptr || pStableId == nullptr || TargetColumn < 0 || TargetColumn > 2)
		return false;
	const int Index = Model.FindByStableId(pStableId);
	if(Index < 0)
		return false;
	const qm_card_order::SEntry &Entry = Model.Entry(Index);
	if(Entry.m_pDefaultTab == nullptr || str_comp(Entry.m_pDefaultTab, pTab) != 0)
		return false;
	const std::vector<int> vTargetEntries = Model.ColumnIndices(pTab, TargetColumn);
	const bool SameColumn = Entry.m_Column == TargetColumn;
	const int MaxOrder = std::max(0, (int)vTargetEntries.size() - (SameColumn ? 1 : 0));
	const int ClampedOrder = std::clamp(TargetOrder, 0, MaxOrder);
	if(SameColumn && Entry.m_OrderInColumn == ClampedOrder)
		return false;
	Model.Move(pStableId, TargetColumn, ClampedOrder);
	return true;
}

namespace settings_card_deck_logic
{
	void CLogic::Load(const char *pDeckId, const char *pGlobalOrder)
	{
		m_DeckId = pDeckId != nullptr ? pDeckId : "";
		const std::vector<qm_card_order::SEntry> vDefaults = qm_card_registry::BuildDefaultEntries();
		m_Model.LoadMerged(pGlobalOrder, vDefaults);
		for(const qm_card_order::SEntry &Default : vDefaults)
		{
			if(Default.m_pStableId == nullptr || Default.m_pDefaultTab == nullptr || str_startswith(Default.m_pStableId, "deck:") == nullptr || str_comp(Default.m_pDefaultTab, m_DeckId.c_str()) != 0)
				continue;
			const int Index = m_Model.FindByStableId(Default.m_pStableId);
			if(Index < 0 || m_Model.Entry(Index).m_pDefaultTab == nullptr || str_comp(m_Model.Entry(Index).m_pDefaultTab, m_DeckId.c_str()) != 0)
				m_Model.MoveToTab(Default.m_pStableId, m_DeckId.c_str(), Default.m_Column, Default.m_OrderInColumn);
		}
	}

	bool CLogic::Move(const char *pStableId, int Column, int Order)
	{
		const int Index = m_Model.FindByStableId(pStableId);
		if(Index < 0 || m_DeckId.empty() || Column < 0 || Column > 2)
			return false;
		const qm_card_order::SEntry &Entry = m_Model.Entry(Index);
		if(Entry.m_pDefaultTab == nullptr || str_comp(Entry.m_pDefaultTab, m_DeckId.c_str()) != 0 || str_startswith(Entry.m_pStableId, "deck:") == nullptr)
			return false;
		m_Model.Move(pStableId, Column, Order);
		return true;
	}

	int CLogic::ColumnForStableId(const char *pStableId) const
	{
		const int Index = m_Model.FindByStableId(pStableId);
		return Index >= 0 ? m_Model.Entry(Index).m_Column : -1;
	}
	std::vector<std::string> CLogic::StableIdOrder(int Column) const

	{
		return m_Model.StableIdOrder("deck:", m_DeckId.c_str(), Column);
	}

	bool CLogic::SerializeMerged(const char *pExistingGlobalOrder, char *pOut, int OutSize) const
	{
		std::vector<qm_card_order::SEntry> vEntries;
		vEntries.reserve(m_Model.Count());
		for(int i = 0; i < m_Model.Count(); ++i)
		{
			const qm_card_order::SEntry &Entry = m_Model.Entry(i);
			if(Entry.m_pStableId != nullptr && str_startswith(Entry.m_pStableId, "deck:") != nullptr)
				vEntries.push_back(Entry);
		}
		return qm_card_order::SerializeMergedReplacingPrefix(pExistingGlobalOrder, "deck:", vEntries, pOut, OutSize);
	}
} // namespace settings_card_deck_logic
