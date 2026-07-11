#ifndef GAME_CLIENT_QMUI_SETTINGSCARDDECKLOGIC_H
#define GAME_CLIENT_QMUI_SETTINGSCARDDECKLOGIC_H

#include <game/client/QmUi/QmCardOrderModel.h>

#include <string>
#include <vector>

// 设置卡片排序的无渲染状态 owner。
namespace settings_card_deck_logic
{
	class CLogic
	{
	public:
		void Load(const char *pDeckId, const char *pGlobalOrder);
		bool Move(const char *pStableId, int Column, int Order);
		int ColumnForStableId(const char *pStableId) const;
		std::vector<std::string> StableIdOrder(int Column) const;
		bool SerializeMerged(const char *pExistingGlobalOrder, char *pOut, int OutSize) const;

	private:
		std::string m_DeckId;
		qm_card_order::CModel m_Model;
	};
} // namespace settings_card_deck_logic

#endif // GAME_CLIENT_QMUI_SETTINGSCARDDECKLOGIC_H
