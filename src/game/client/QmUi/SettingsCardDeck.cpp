#include "SettingsCardDeck.h"

#include <base/system.h>

namespace settings_card_deck
{
	void CDeck::Load(const char *pDeckId, char *pGlobalOrder, int GlobalOrderSize)
	{
		m_DeckId = pDeckId != nullptr ? pDeckId : "";
		m_pGlobalOrder = pGlobalOrder;
		m_GlobalOrderSize = GlobalOrderSize;
		m_Logic.Load(m_DeckId.c_str(), m_pGlobalOrder);
		RebuildProjection();
	}

	bool CDeck::CommitDrop(const char *pStableId, int Column, int Order)
	{
		if(m_pGlobalOrder == nullptr || m_GlobalOrderSize <= 0 || !m_Logic.Move(pStableId, Column, Order))
			return false;
		std::vector<char> vMergedGlobalOrder(m_GlobalOrderSize);
		if(!m_Logic.SerializeMerged(m_pGlobalOrder, vMergedGlobalOrder.data(), (int)vMergedGlobalOrder.size()))
		{
			m_Logic.Load(m_DeckId.c_str(), m_pGlobalOrder);
			RebuildProjection();
			return false;
		}
		str_copy(m_pGlobalOrder, vMergedGlobalOrder.data(), m_GlobalOrderSize);
		m_Logic.Load(m_DeckId.c_str(), m_pGlobalOrder);
		RebuildProjection();
		return true;
	}

	int CDeck::ColumnForStableId(const char *pStableId) const
	{
		return m_Logic.ColumnForStableId(pStableId);
	}

	void CDeck::RebuildProjection()
	{
		m_vOrderedStableIds.clear();
		for(const int Column : {1, 2, 0})
		{
			const std::vector<std::string> vColumnIds = m_Logic.StableIdOrder(Column);
			m_vOrderedStableIds.insert(m_vOrderedStableIds.end(), vColumnIds.begin(), vColumnIds.end());
		}
	}
} // namespace settings_card_deck
