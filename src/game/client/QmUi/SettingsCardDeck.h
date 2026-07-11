#ifndef GAME_CLIENT_QMUI_SETTINGSCARDDECK_H
#define GAME_CLIENT_QMUI_SETTINGSCARDDECK_H

#include <game/client/QmUi/SettingsCardDeckLogic.h>

#include <string>
#include <vector>

namespace settings_card_deck
{
	class CDeck
	{
	public:
		void Load(const char *pDeckId, char *pGlobalOrder, int GlobalOrderSize);
		bool CommitDrop(const char *pStableId, int Column, int Order);
		const std::vector<std::string> &OrderedStableIds() const { return m_vOrderedStableIds; }
		int ColumnForStableId(const char *pStableId) const;

	private:
		void RebuildProjection();

		std::string m_DeckId;
		char *m_pGlobalOrder = nullptr;
		int m_GlobalOrderSize = 0;
		settings_card_deck_logic::CLogic m_Logic;
		std::vector<std::string> m_vOrderedStableIds;
	};
} // namespace settings_card_deck

#endif // GAME_CLIENT_QMUI_SETTINGSCARDDECK_H
