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

namespace settings_card_deck_logic
{
	const std::array<std::vector<int>, 3> &CProjectionCache::Resolve(const qm_card_order::CModel &Model, const char *pTab, const std::vector<int> &vActiveStateIndices)
	{
		const char *pResolvedTab = pTab != nullptr ? pTab : "";
		if(m_LayoutRevision == Model.LayoutRevision() && str_comp(m_Tab.c_str(), pResolvedTab) == 0 && m_vActiveStateIndices == vActiveStateIndices)
			return m_aColumns;

		m_aColumns = BuildSettingsCardDeckColumnOrder(Model, pResolvedTab, vActiveStateIndices);
		m_LayoutRevision = Model.LayoutRevision();
		m_Tab = pResolvedTab;
		m_vActiveStateIndices = vActiveStateIndices;
		++m_RebuildCount;
		return m_aColumns;
	}
} // namespace settings_card_deck_logic

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

void ApplySettingsCardDeckSingleColumnDragPlacement(std::array<std::vector<int>, 3> &aColumns, int ActiveStateIndex, int TargetOrder)
{
	int SourceColumn = -1;
	for(int Column = 0; Column < (int)aColumns.size(); ++Column)
	{
		if(std::find(aColumns[Column].begin(), aColumns[Column].end(), ActiveStateIndex) != aColumns[Column].end())
		{
			SourceColumn = Column;
			break;
		}
	}
	if(SourceColumn < 0)
		return;
	if(SourceColumn == 0)
	{
		ApplySettingsCardDeckDragPlacement(aColumns, ActiveStateIndex, 0, TargetOrder);
		return;
	}

	const int LeftCount = (int)aColumns[1].size();
	std::vector<int> vVisualOrder = aColumns[1];
	vVisualOrder.insert(vVisualOrder.end(), aColumns[2].begin(), aColumns[2].end());
	vVisualOrder.erase(std::remove(vVisualOrder.begin(), vVisualOrder.end(), ActiveStateIndex), vVisualOrder.end());
	const int InsertAt = std::clamp(TargetOrder, 0, (int)vVisualOrder.size());
	vVisualOrder.insert(vVisualOrder.begin() + InsertAt, ActiveStateIndex);
	aColumns[1].assign(vVisualOrder.begin(), vVisualOrder.begin() + std::min(LeftCount, (int)vVisualOrder.size()));
	aColumns[2].assign(vVisualOrder.begin() + aColumns[1].size(), vVisualOrder.end());
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

bool SettingsCardDeckNeedsContentMeasure(const bool Collapsed, const bool MeasureEachFrame, const float CachedContentHeight)
{
	return !Collapsed && (CachedContentHeight < 0.0f || MeasureEachFrame);
}

bool SettingsCardDeckRendersContent(const bool Collapsed)
{
	return !Collapsed;
}

bool CommitSettingsCardDeckDrop(qm_card_order::CModel &Model, const char *pTab, const char *pStableId, int TargetColumn, int TargetOrder, const std::vector<int> *pActiveStateIndices)
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
	std::vector<int> vTargetEntriesWithoutDragged;
	vTargetEntriesWithoutDragged.reserve(vTargetEntries.size());
	for(const int TargetEntry : vTargetEntries)
	{
		if(TargetEntry != Index)
			vTargetEntriesWithoutDragged.push_back(TargetEntry);
	}
	int ClampedOrder = std::clamp(TargetOrder, 0, (int)vTargetEntriesWithoutDragged.size());
	if(pActiveStateIndices != nullptr)
	{
		std::vector<int> vVisibleTargetEntries;
		for(const int TargetEntry : vTargetEntriesWithoutDragged)
		{
			if(std::find(pActiveStateIndices->begin(), pActiveStateIndices->end(), TargetEntry) != pActiveStateIndices->end())
				vVisibleTargetEntries.push_back(TargetEntry);
		}
		const int VisibleOrder = std::clamp(TargetOrder, 0, (int)vVisibleTargetEntries.size());
		if(SameColumn)
		{
			int CurrentVisibleOrder = 0;
			bool FoundDraggedEntry = false;
			for(const int TargetEntry : vTargetEntries)
			{
				if(std::find(pActiveStateIndices->begin(), pActiveStateIndices->end(), TargetEntry) == pActiveStateIndices->end())
					continue;
				if(TargetEntry == Index)
				{
					FoundDraggedEntry = true;
					break;
				}
				++CurrentVisibleOrder;
			}
			if(FoundDraggedEntry && CurrentVisibleOrder == VisibleOrder)
				return false;
		}
		if(vVisibleTargetEntries.empty())
		{
			ClampedOrder = 0;
		}
		else if(VisibleOrder < (int)vVisibleTargetEntries.size())
		{
			const auto It = std::find(vTargetEntriesWithoutDragged.begin(), vTargetEntriesWithoutDragged.end(), vVisibleTargetEntries[VisibleOrder]);
			ClampedOrder = It != vTargetEntriesWithoutDragged.end() ? (int)(It - vTargetEntriesWithoutDragged.begin()) : ClampedOrder;
		}
		else if(!vVisibleTargetEntries.empty())
		{
			const auto It = std::find(vTargetEntriesWithoutDragged.begin(), vTargetEntriesWithoutDragged.end(), vVisibleTargetEntries.back());
			ClampedOrder = It != vTargetEntriesWithoutDragged.end() ? (int)(It - vTargetEntriesWithoutDragged.begin()) + 1 : ClampedOrder;
		}
	}
	else if(SameColumn)
	{
		ClampedOrder = std::clamp(TargetOrder, 0, (int)vTargetEntriesWithoutDragged.size());
	}
	if(SameColumn && Entry.m_OrderInColumn == ClampedOrder)
		return false;
	Model.Move(pStableId, TargetColumn, ClampedOrder);
	return true;
}

bool CommitSettingsCardDeckSingleColumnDrop(qm_card_order::CModel &Model, const char *pTab, const char *pStableId, int TargetOrder, const std::vector<int> &vActiveStateIndices)
{
	const int Index = Model.FindByStableId(pStableId);
	if(Index < 0 || Model.Entry(Index).m_pDefaultTab == nullptr || str_comp(Model.Entry(Index).m_pDefaultTab, pTab != nullptr ? pTab : "") != 0)
		return false;
	const int SourceColumn = Model.Entry(Index).m_Column;
	if(SourceColumn == 0)
		return CommitSettingsCardDeckDrop(Model, pTab, pStableId, 0, TargetOrder, &vActiveStateIndices);
	if(SourceColumn != 1 && SourceColumn != 2)
		return false;

	std::array<std::vector<int>, 3> aColumns = BuildSettingsCardDeckColumnOrder(Model, pTab, vActiveStateIndices);
	const std::array<std::vector<int>, 3> aPreviousColumns = aColumns;
	ApplySettingsCardDeckSingleColumnDragPlacement(aColumns, Index, TargetOrder);
	if(aColumns == aPreviousColumns)
		return false;

	std::array<std::vector<int>, 3> aCanonicalColumns;
	for(const int Column : {1, 2})
	{
		const std::vector<int> vCurrentColumn = Model.ColumnIndices(pTab, Column);
		size_t VisibleIndex = 0;
		aCanonicalColumns[Column].reserve(vCurrentColumn.size());
		for(const int StateIndex : vCurrentColumn)
		{
			if(std::find(vActiveStateIndices.begin(), vActiveStateIndices.end(), StateIndex) == vActiveStateIndices.end())
				aCanonicalColumns[Column].push_back(StateIndex);
			else if(VisibleIndex < aColumns[Column].size())
				aCanonicalColumns[Column].push_back(aColumns[Column][VisibleIndex++]);
		}
	}

	// 先固定完整 canonical 顺序再提交，避免连续移动时动态可见锚点把隐藏卡推离原槽位。
	for(const int Column : {1, 2})
	{
		for(int Order = 0; Order < (int)aCanonicalColumns[Column].size(); ++Order)
		{
			const int StateIndex = aCanonicalColumns[Column][Order];
			if(StateIndex >= 0 && StateIndex < Model.Count())
				Model.Move(Model.Entry(StateIndex).m_pStableId, Column, Order);
		}
	}
	return true;
}
