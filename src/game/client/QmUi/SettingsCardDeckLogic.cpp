#include "SettingsCardDeckLogic.h"

#include <base/system.h>

#include <game/client/QmUi/QmCardRegistry.h>

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
