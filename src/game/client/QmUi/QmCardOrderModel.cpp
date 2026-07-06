#include "QmCardOrderModel.h"

#include <base/system.h>

#include <algorithm>

namespace qm_card_order
{
	namespace
	{
		const char *ColumnToString(int Column)
		{
			switch(Column)
			{
			case 0:
				return "full";
			case 1:
				return "left";
			case 2:
				return "right";
			default:
				return nullptr;
			}
		}

		bool ParseColumn(const char *pColumn, bool AllowNumericColumn, int *pOutColumn)
		{
			if(pColumn == nullptr || pOutColumn == nullptr)
				return false;
			if(str_comp(pColumn, "full") == 0)
			{
				*pOutColumn = 0;
				return true;
			}
			if(str_comp(pColumn, "left") == 0)
			{
				*pOutColumn = 1;
				return true;
			}
			if(str_comp(pColumn, "right") == 0)
			{
				*pOutColumn = 2;
				return true;
			}
			if(!AllowNumericColumn)
				return false;
			int Column = 0;
			if(!str_toint(pColumn, &Column))
				return false;
			if(Column < 0)
				return false;
			*pOutColumn = Column;
			return true;
		}
	}

	void CModel::SetEntries(std::vector<SEntry> Entries)
	{
		m_vOwnedTabs.clear();
		m_vEntries.clear();
		m_vEntries.reserve(Entries.size());
		for(SEntry Entry : Entries)
		{
			if(Entry.m_pDefaultTab != nullptr)
			{
				m_vOwnedTabs.emplace_back(Entry.m_pDefaultTab);
				Entry.m_pDefaultTab = m_vOwnedTabs.back().c_str();
			}
			m_vEntries.push_back(Entry);
		}
		m_Dirty = true;
		BuildStateIndex(); // 维护 stableId→index 注册表（让位 lerp O(1) 地基）
	}

	int CModel::FindByStableId(const char *pStableId) const
	{
		return StateIndexForStableId(pStableId);
	}

	void CModel::Move(const char *pStableId, int ToColumn, int ToOrder)
	{
		const int Idx = FindByStableId(pStableId);
		if(Idx < 0)
			return;
		m_vEntries[Idx].m_Column = ToColumn;
		// 重建目标列 order（erase + insert 语义）
		std::vector<int> vOthers;
		for(int i : ColumnIndices(ToColumn))
		{
			if(i != Idx)
				vOthers.push_back(i);
		}
		ToOrder = std::clamp(ToOrder, 0, (int)vOthers.size());
		vOthers.insert(vOthers.begin() + ToOrder, Idx);
		for(int o = 0; o < (int)vOthers.size(); ++o)
			m_vEntries[vOthers[o]].m_OrderInColumn = o;
		m_Dirty = true;
		// Move 仅改 column/order，不改 stableId 集合与 vector 位置，state index 注册表无需重建
	}

	void CModel::MoveToTab(const char *pStableId, const char *pToTab, int ToColumn, int ToOrder)
	{
		const int Idx = FindByStableId(pStableId);
		if(Idx < 0 || pToTab == nullptr)
			return;
		m_vOwnedTabs.emplace_back(pToTab);
		m_vEntries[Idx].m_pDefaultTab = m_vOwnedTabs.back().c_str();
		m_vEntries[Idx].m_Column = ToColumn;
		// 重建目标 tab+column order（跨页组件编辑器 erase + insert 语义）
		std::vector<int> vOthers;
		for(int i : ColumnIndices(pToTab, ToColumn))
		{
			if(i != Idx)
				vOthers.push_back(i);
		}
		ToOrder = std::clamp(ToOrder, 0, (int)vOthers.size());
		vOthers.insert(vOthers.begin() + ToOrder, Idx);
		for(int o = 0; o < (int)vOthers.size(); ++o)
			m_vEntries[vOthers[o]].m_OrderInColumn = o;
		m_Dirty = true;
		// MoveToTab 仅改 tab/column/order，不改 stableId 集合与 vector 位置，state index 注册表无需重建
	}

	void CModel::NormalizeColumns()
	{
		std::vector<int> vIdx(m_vEntries.size());
		for(size_t i = 0; i < m_vEntries.size(); ++i)
			vIdx[i] = (int)i;
		std::stable_sort(vIdx.begin(), vIdx.end(), [&](int a, int b) {
			const char *pTabA = m_vEntries[a].m_pDefaultTab != nullptr ? m_vEntries[a].m_pDefaultTab : "";
			const char *pTabB = m_vEntries[b].m_pDefaultTab != nullptr ? m_vEntries[b].m_pDefaultTab : "";
			const int TabCmp = str_comp(pTabA, pTabB);
			if(TabCmp != 0)
				return TabCmp < 0;
			if(m_vEntries[a].m_Column != m_vEntries[b].m_Column)
				return m_vEntries[a].m_Column < m_vEntries[b].m_Column;
			return m_vEntries[a].m_OrderInColumn < m_vEntries[b].m_OrderInColumn;
		});
		const char *pCurTab = nullptr;
		int CurCol = -1;
		int CurOrder = 0;
		bool Changed = false;
		for(int i : vIdx)
		{
			const char *pEntryTab = m_vEntries[i].m_pDefaultTab != nullptr ? m_vEntries[i].m_pDefaultTab : "";
			if(pCurTab == nullptr || str_comp(pEntryTab, pCurTab) != 0 || m_vEntries[i].m_Column != CurCol)
			{
				pCurTab = pEntryTab;
				CurCol = m_vEntries[i].m_Column;
				CurOrder = 0;
			}
			if(m_vEntries[i].m_OrderInColumn != CurOrder)
			{
				m_vEntries[i].m_OrderInColumn = CurOrder;
				Changed = true;
			}
			CurOrder++;
		}
		if(Changed)
			m_Dirty = true;
	}

	std::vector<int> CModel::ColumnIndices(int Column) const
	{
		std::vector<int> v;
		for(size_t i = 0; i < m_vEntries.size(); ++i)
		{
			if(m_vEntries[i].m_Column == Column)
				v.push_back((int)i);
		}
		std::stable_sort(v.begin(), v.end(), [&](int a, int b) {
			return m_vEntries[a].m_OrderInColumn < m_vEntries[b].m_OrderInColumn;
		});
		return v;
	}

	std::vector<int> CModel::ColumnIndices(const char *pTab, int Column) const
	{
		std::vector<int> v;
		for(size_t i = 0; i < m_vEntries.size(); ++i)
		{
			const SEntry &E = m_vEntries[i];
			if(E.m_Column != Column)
				continue;
			// tab 为可变位置维度：仅当卡有归属 tab 且与查询 tab 一致时命中（tab=nullptr 的卡不归属任何页）
			if(E.m_pDefaultTab == nullptr || pTab == nullptr || str_comp(E.m_pDefaultTab, pTab) != 0)
				continue;
			v.push_back((int)i);
		}
		std::stable_sort(v.begin(), v.end(), [&](int a, int b) {
			return m_vEntries[a].m_OrderInColumn < m_vEntries[b].m_OrderInColumn;
		});
		return v;
	}

	std::vector<std::string> CModel::StableIdOrder(const char *pStableIdPrefix, const char *pTab, int Column) const
	{
		std::vector<std::string> vOrder;
		for(int Index : ColumnIndices(pTab, Column))
		{
			const SEntry &Entry = m_vEntries[Index];
			if(Entry.m_pStableId == nullptr)
				continue;
			if(pStableIdPrefix != nullptr && pStableIdPrefix[0] != '\0' && str_startswith(Entry.m_pStableId, pStableIdPrefix) == nullptr)
				continue;
			vOrder.emplace_back(Entry.m_pStableId);
		}
		return vOrder;
	}

	void CModel::BuildStateIndex()
	{
		m_StableIdToState.clear();
		for(size_t i = 0; i < m_vEntries.size(); ++i)
		{
			if(m_vEntries[i].m_pStableId == nullptr)
				continue;
			// 首次出现的 stableId 入表（重复 id 取首次，与 Parse 容错语义一致）
			m_StableIdToState.try_emplace(m_vEntries[i].m_pStableId, (int)i);
		}
	}

	int CModel::StateIndexForStableId(const char *pStableId) const
	{
		if(pStableId == nullptr)
			return -1;
		const auto It = m_StableIdToState.find(pStableId);
		if(It == m_StableIdToState.end())
			return -1;
		return It->second;
	}

	bool CModel::Serialize(char *pBuf, int BufSize) const
	{
		if(pBuf == nullptr || BufSize <= 0)
			return false;
		pBuf[0] = '\0';
		bool Complete = true;
		for(const SEntry &E : m_vEntries)
		{
			if(E.m_pStableId == nullptr)
				continue;
			char aEntry[160];
			const char *pColumn = ColumnToString(E.m_Column);
			if(pColumn == nullptr)
				continue;
			str_format(aEntry, sizeof(aEntry), "%s|%s|%s|%d;", E.m_pStableId, E.m_pDefaultTab != nullptr ? E.m_pDefaultTab : "", pColumn, E.m_OrderInColumn);
			if(str_length(pBuf) + str_length(aEntry) >= BufSize)
			{
				Complete = false;
				break;
			}
			str_append(pBuf, aEntry, BufSize);
		}
		return Complete;
	}

	bool CModel::Parse(const char *pStr, const std::vector<const char *> &vValidIds)
	{
		if(pStr == nullptr)
			return false;
		m_vEntries.clear();
		m_vOwnedTabs.clear();
		std::vector<bool> vSeen(vValidIds.size(), false);
		const char *p = pStr;
		char aToken[160];
		while((p = str_next_token(p, ";", aToken, (int)sizeof(aToken))) != nullptr)
		{
			if(aToken[0] == '\0')
				continue;
			char aId[64];
			char aTab[64];
			char aCol[16];
			char aOrder[16];
			const char *pOrderField = nullptr;
			const bool PipeFormat = str_find(aToken, "|") != nullptr;
			if(PipeFormat)
			{
				// 拆分 "id|tab|col|order"。stableId 自身可含 ':'，字段分隔必须使用 '|'
				const char *pTab = str_next_token(aToken, "|", aId, (int)sizeof(aId));
				if(pTab == nullptr)
					continue;
				const char *pCol = str_next_token(pTab, "|", aTab, (int)sizeof(aTab));
				if(pCol == nullptr)
					continue;
				pOrderField = str_next_token(pCol, "|", aCol, (int)sizeof(aCol));
				if(pOrderField == nullptr || str_next_token(pOrderField, "|", aOrder, (int)sizeof(aOrder)) == nullptr)
					continue;
			}
			else
			{
				// 兼容旧 "id:col:order"。旧格式不携带 tab，保留 nullptr 让调用方按默认补全。
				aTab[0] = '\0';
				const char *pRest = str_next_token(aToken, ":", aId, (int)sizeof(aId));
				if(pRest == nullptr)
					continue;
				pOrderField = str_next_token(pRest, ":", aCol, (int)sizeof(aCol));
				if(pOrderField == nullptr || str_next_token(pOrderField, ":", aOrder, (int)sizeof(aOrder)) == nullptr)
					continue;
			}
			// 校验 id 在 vValidIds
			int ValidIdx = -1;
			for(size_t i = 0; i < vValidIds.size(); ++i)
			{
				if(vValidIds[i] != nullptr && str_comp(vValidIds[i], aId) == 0)
				{
					ValidIdx = (int)i;
					break;
				}
			}
			if(ValidIdx < 0 || vSeen[ValidIdx]) // 未知/重复 跳过
				continue;
			// 解析 column/order（先校验 column，再设 vSeen——避免非法 column 占用 id 导致后续合法被当重复）
			int Col = 0;
			int Order = 0;
			if(!ParseColumn(aCol, true, &Col))
				continue;
			if(!str_toint(aOrder, &Order))
				continue;
			vSeen[ValidIdx] = true;
			SEntry E;
			E.m_pStableId = vValidIds[ValidIdx];
			if(PipeFormat && aTab[0] != '\0')
			{
				m_vOwnedTabs.emplace_back(aTab);
				E.m_pDefaultTab = m_vOwnedTabs.back().c_str();
			}
			E.m_Column = Col;
			E.m_OrderInColumn = Order < 0 ? 0 : Order;
			m_vEntries.push_back(E);
		}
		m_Dirty = true;
		BuildStateIndex(); // 解析后重建 stableId→index 注册表
		return true;
	}

	bool CModel::LoadExplicit(const char *pStr, const std::vector<SEntry> &vDefaults)
	{
		std::vector<const char *> vValidIds;
		vValidIds.reserve(vDefaults.size());
		for(const SEntry &Default : vDefaults)
		{
			if(Default.m_pStableId != nullptr)
				vValidIds.push_back(Default.m_pStableId);
		}
		return pStr != nullptr && pStr[0] != '\0' && Parse(pStr, vValidIds) && Count() > 0;
	}

	bool CModel::LoadMerged(const char *pStr, const std::vector<SEntry> &vDefaults)
	{
		CModel ParsedModel;
		const bool Parsed = ParsedModel.LoadExplicit(pStr, vDefaults);

		std::vector<SEntry> vMerged;
		vMerged.reserve(vDefaults.size());
		for(const SEntry &Default : vDefaults)
		{
			if(Default.m_pStableId == nullptr)
				continue;
			const int ParsedIndex = ParsedModel.FindByStableId(Default.m_pStableId);
			if(ParsedIndex < 0)
			{
				vMerged.push_back(Default);
				continue;
			}

			SEntry Entry = ParsedModel.Entry(ParsedIndex);
			if(Entry.m_pDefaultTab == nullptr)
				Entry.m_pDefaultTab = Default.m_pDefaultTab;
			vMerged.push_back(Entry);
		}

		SetEntries(vMerged);
		NormalizeColumns();
		ClearDirty();
		return Parsed;
	}
} // namespace qm_card_order
