#include "QmCardOrderModel.h"

#include <base/system.h>

#include <algorithm>

namespace qm_card_order
{
	void CModel::SetEntries(std::vector<SEntry> Entries)
	{
		m_vEntries = std::move(Entries);
		m_Dirty = true;
		BuildStateIndex(); // 维护 stableId→index 注册表（让位 lerp O(1) 地基）
	}

	int CModel::FindByStableId(const char *pStableId) const
	{
		if(pStableId == nullptr)
			return -1;
		for(size_t i = 0; i < m_vEntries.size(); ++i)
		{
			if(m_vEntries[i].m_pStableId != nullptr && str_comp(m_vEntries[i].m_pStableId, pStableId) == 0)
				return (int)i;
		}
		return -1;
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

	void CModel::NormalizeColumns()
	{
		std::vector<int> vIdx(m_vEntries.size());
		for(size_t i = 0; i < m_vEntries.size(); ++i)
			vIdx[i] = (int)i;
		std::stable_sort(vIdx.begin(), vIdx.end(), [&](int a, int b) {
			if(m_vEntries[a].m_Column != m_vEntries[b].m_Column)
				return m_vEntries[a].m_Column < m_vEntries[b].m_Column;
			return m_vEntries[a].m_OrderInColumn < m_vEntries[b].m_OrderInColumn;
		});
		int CurCol = -1;
		int CurOrder = 0;
		for(int i : vIdx)
		{
			if(m_vEntries[i].m_Column != CurCol)
			{
				CurCol = m_vEntries[i].m_Column;
				CurOrder = 0;
			}
			m_vEntries[i].m_OrderInColumn = CurOrder++;
		}
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

	void CModel::Serialize(char *pBuf, int BufSize) const
	{
		if(pBuf == nullptr || BufSize <= 0)
			return;
		pBuf[0] = '\0';
		for(const SEntry &E : m_vEntries)
		{
			if(E.m_pStableId == nullptr)
				continue;
			char aEntry[80];
			str_format(aEntry, sizeof(aEntry), "%s:%d:%d;", E.m_pStableId, E.m_Column, E.m_OrderInColumn);
			str_append(pBuf, aEntry, BufSize);
		}
	}

	bool CModel::Parse(const char *pStr, const std::vector<const char *> &vValidIds)
	{
		if(pStr == nullptr)
			return false;
		m_vEntries.clear();
		std::vector<bool> vSeen(vValidIds.size(), false);
		const char *p = pStr;
		char aToken[80];
		while((p = str_next_token(p, ";", aToken, (int)sizeof(aToken))) != nullptr)
		{
			if(aToken[0] == '\0')
				continue;
			// 拆分 "id:col:order"
			char aId[64];
			const char *pRest = str_next_token(aToken, ":", aId, (int)sizeof(aId));
			if(pRest == nullptr)
				continue;
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
			// 解析 col:order（先校验 column，再设 vSeen——避免非法 column 占用 id 导致后续合法被当重复）
			char aCol[16];
			const char *pOrder = str_next_token(pRest, ":", aCol, (int)sizeof(aCol));
			if(pOrder == nullptr)
				continue;
			int Col = atoi(aCol);
			int Order = atoi(pOrder);
			if(Col < 0)
				continue;
			vSeen[ValidIdx] = true;
			SEntry E;
			E.m_pStableId = vValidIds[ValidIdx];
			E.m_Column = Col;
			E.m_OrderInColumn = Order < 0 ? 0 : Order;
			m_vEntries.push_back(E);
		}
		m_Dirty = true;
		BuildStateIndex(); // 解析后重建 stableId→index 注册表
		return true;
	}
} // namespace qm_card_order
