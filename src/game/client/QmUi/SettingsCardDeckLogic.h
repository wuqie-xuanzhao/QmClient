#ifndef GAME_CLIENT_QMUI_SETTINGSCARDDECKLOGIC_H
#define GAME_CLIENT_QMUI_SETTINGSCARDDECKLOGIC_H

#include <game/client/QmUi/QmCardOrderModel.h>
#include <game/client/ui_rect.h>

#include <array>
#include <string>
#include <vector>

struct SSettingsCardDeckItemGeometry
{
	int m_StateIndex = -1;
	int m_Column = 0;
	CUIRect m_Rect;
};

// 以下函数是公共 Deck 的无渲染决策层；仅处理 model/order/geometry，不能依赖 UI renderer。
// 活动 state 集合由当前页面 definitions 决定，未注册的条件卡不得占用任何 layout slot。
std::array<std::vector<int>, 3> BuildSettingsCardDeckColumnOrder(const qm_card_order::CModel &Model, const char *pTab, const std::vector<int> &vActiveStateIndices);
void ApplySettingsCardDeckDragPlacement(std::array<std::vector<int>, 3> &aColumns, int ActiveStateIndex, int TargetColumn, int TargetOrder);
// vItems 必须由 layout 阶段按每列视觉顺序输出；该约束让拖拽热路径保持线性且零分配。
int ResolveSettingsCardDeckDropOrder(float MouseY, int TargetColumn, const std::vector<SSettingsCardDeckItemGeometry> &vItems, int IgnoredStateIndex);
float SettingsCardDeckAutoScrollDelta(float MouseY, const CUIRect &Viewport, float UiScale);
bool CommitSettingsCardDeckDrop(qm_card_order::CModel &Model, const char *pTab, const char *pStableId, int TargetColumn, int TargetOrder);
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
