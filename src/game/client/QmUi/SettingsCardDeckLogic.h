#ifndef GAME_CLIENT_QMUI_SETTINGSCARDDECKLOGIC_H
#define GAME_CLIENT_QMUI_SETTINGSCARDDECKLOGIC_H

#include <game/client/QmUi/QmCardOrderModel.h>
#include <game/client/ui_rect.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct SSettingsCardDeckItemGeometry
{
	int m_StateIndex = -1;
	int m_Column = 0;
	CUIRect m_Rect;
};

struct SSettingsCardAnimationWork
{
	bool m_ResolveEntry = false;
	bool m_ResetEntry = false;
	bool m_ResolveReflow = false;
	bool m_SetReflowTarget = false;
};

// 以下函数是公共 Deck 的无渲染决策层；仅处理 model/order/geometry，不能依赖 UI renderer。
// 活动 state 集合由当前页面 definitions 决定，未注册的条件卡不得占用任何 layout slot。
std::array<std::vector<int>, 3> BuildSettingsCardDeckColumnOrder(const qm_card_order::CModel &Model, const char *pTab, const std::vector<int> &vActiveStateIndices);

// 单列沿用宽屏的阅读层级：每层先左、再右、最后全宽分隔卡。
// 回调签名为 void(int StateIndex, int CanonicalColumn)，遍历过程不分配内存。
template<typename F>
void ForEachSettingsCardDeckVisualOrder(const std::array<std::vector<int>, 3> &aColumns, F &&Callback)
{
	const size_t NumLayers = std::max({aColumns[0].size(), aColumns[1].size(), aColumns[2].size()});
	for(size_t Layer = 0; Layer < NumLayers; ++Layer)
	{
		if(Layer < aColumns[1].size())
			Callback(aColumns[1][Layer], 1);
		if(Layer < aColumns[2].size())
			Callback(aColumns[2][Layer], 2);
		if(Layer < aColumns[0].size())
			Callback(aColumns[0][Layer], 0);
	}
}

// 运行时列投影缓存：仅在全局布局版本、tab 或活动 definition 集合变更时重建排序结果。
namespace settings_card_deck_logic
{
	class CProjectionCache
	{
	public:
		const std::array<std::vector<int>, 3> &Resolve(const qm_card_order::CModel &Model, const char *pTab, const std::vector<int> &vActiveStateIndices);
		uint64_t RebuildCount() const { return m_RebuildCount; }

	private:
		uint64_t m_LayoutRevision = UINT64_MAX;
		uint64_t m_RebuildCount = 0;
		std::string m_Tab;
		std::vector<int> m_vActiveStateIndices;
		std::array<std::vector<int>, 3> m_aColumns;
	};
} // namespace settings_card_deck_logic

void ApplySettingsCardDeckDragPlacement(std::array<std::vector<int>, 3> &aColumns, int ActiveStateIndex, int TargetColumn, int TargetOrder);
// 单列仅改变视觉顺序：full 卡保持 full 语义，普通卡按原左右可见容量稳定拆回 canonical 列。
void ApplySettingsCardDeckSingleColumnDragPlacement(std::array<std::vector<int>, 3> &aColumns, int ActiveStateIndex, int TargetOrder);
// vItems 必须由 layout 阶段按每列视觉顺序输出；该约束让拖拽热路径保持线性且零分配。
int ResolveSettingsCardDeckDropOrder(float MouseY, int TargetColumn, const std::vector<SSettingsCardDeckItemGeometry> &vItems, int IgnoredStateIndex);
float SettingsCardDeckAutoScrollDelta(float MouseY, const CUIRect &Viewport, float UiScale);
bool CommitSettingsCardDeckDrop(qm_card_order::CModel &Model, const char *pTab, const char *pStableId, int TargetColumn, int TargetOrder, const std::vector<int> *pActiveStateIndices = nullptr);
bool CommitSettingsCardDeckSingleColumnDrop(qm_card_order::CModel &Model, const char *pTab, const char *pStableId, int TargetOrder, const std::vector<int> &vActiveStateIndices);
// 折叠卡保留 header shell，但不得触发内容测量或内容绘制。
bool SettingsCardDeckNeedsContentMeasure(bool Collapsed, bool MeasureEachFrame, float CachedContentHeight);
bool SettingsCardDeckRendersContent(bool Collapsed);
// 已缓存卡片重新测量后高度发生变化时，必须像折叠/可见性变化一样直接同步几何。
bool SettingsCardDeckContentHeightChanged(float PreviousContentHeight, float ContentHeight);
// 折叠或条件可见性改变时直接同步几何；拖拽排序仍保留重排动画。
bool SettingsCardDeckShouldSnapReflow(bool GeometryStateChanged, bool DragActive);
// 滚动改变卡片在静止指针下的命中对象时，仅抑制当前滚动帧的 hover 反馈。
bool SettingsCardDeckScrollMoved(bool HasPreviousOffset, float PreviousOffsetY, float CurrentOffsetY);
// 卡片视觉位置与静态布局不一致时，禁止用静态 header 几何开始拖拽。
bool SettingsCardDeckAllowsDragStart(bool EntryPending, bool EntryPositionActive, bool ReflowTargetChanged, bool ReflowPositionActive);
// 稳定帧不得访问动画 runtime；仅目标变化、活动轨道或关闭动效后的复位需要工作。
SSettingsCardAnimationWork ResolveSettingsCardAnimationWork(float EntryDuration, bool EntryWasActive, bool ReflowInitializedThisFrame, bool SnapReflow, float ReflowDuration, bool ReflowTargetChanged, bool ReflowWasActive);
#endif // GAME_CLIENT_QMUI_SETTINGSCARDDECKLOGIC_H
