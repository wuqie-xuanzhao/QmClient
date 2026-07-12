#ifndef GAME_CLIENT_QMUI_SETTINGSCARDDECK_H
#define GAME_CLIENT_QMUI_SETTINGSCARDDECK_H

#include "SettingsCard.h"
#include "SettingsCardDeckLogic.h"
#include "SettingsPageLayout.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

class CScrollRegion;
struct CScrollRegionParams;
struct IUiContext;

struct SSettingsCardDefinition
{
	SSettingsCardSpec m_Spec;
	FSettingsCardMeasure m_Measure;
	FSettingsCardRender m_Render;
	FSettingsCardPreLayoutInput m_PreLayoutInput;
	std::function<bool()> m_IsVisible;
	// 控制其他卡片可见性的输入在最终布局前处理。
	bool m_VisibilityController = false;
	// 内容高度依赖配置或运行时状态时，每帧重新测量。
	bool m_MeasureEachFrame = false;
	bool m_RenderWhenClipped = false;
};

struct SSettingsCardDeckInput
{
	float m_MouseX = 0.0f;
	float m_MouseY = 0.0f;
	bool m_MousePressed = false;
	bool m_MouseDown = false;
	bool m_MouseReleased = false;
	bool m_CtrlPressed = false;
	float m_FrameDt = 1.0f / 60.0f;
	const CScrollRegionParams *m_pScrollParams = nullptr;
};

struct SSettingsCardDeckResult
{
	std::vector<SSettingsCardFrame> m_vFrames;
	const char *m_pRevealedStableId = nullptr;
	float m_AutoScrollDelta = 0.0f;
	bool m_DropFeedbackConsumed = false;
	bool m_ReflowCompleteFeedbackConsumed = false;
	bool m_OrderChanged = false;
};

// 设置卡片的 client-only 协调器：唯一负责声明式 definition 的排序、拖拽、滚动与 canonical shell 接线。
class CSettingsCardDeck
{
public:
	SSettingsCardDeckResult Render(const IUiContext &Ctx, const SSettingsPageLayoutFrame &Layout, const char *pTab, const std::vector<SSettingsCardDefinition> &vCards, qm_card_order::CModel &Model, CScrollRegion *pScrollRegion, const SSettingsCardDeckInput &Input, const SCardMotionSpec &Motion, const SSettingsCardDeckVisualOptions &VisualOptions);
	void RequestReveal(const char *pStableId);
	void BeginDisplayCycle(uint64_t DisplayCycle);

private:
	struct SRuntimeState
	{
		float m_DropFeedbackRemaining = 0.0f;
		float m_ReflowCompleteFeedbackRemaining = 0.0f;
		float m_LastReflowTargetY = 0.0f;
		uint64_t m_EntryDisplayCycle = UINT64_MAX;
		bool m_ReflowInitialized = false;
		bool m_ReflowWasActive = false;
	};

	struct SDragState
	{
		int m_StateIndex = -1;
		int m_SourceColumn = 1;
		int m_TargetColumn = 1;
		int m_TargetOrder = 0;
		float m_GrabOffsetX = 0.0f;
		float m_GrabOffsetY = 0.0f;

		bool Active() const { return m_StateIndex >= 0; }
		void Reset() { *this = {}; }
	};

	void PrepareDefinitions(const std::vector<SSettingsCardDefinition> &vCards, const qm_card_order::CModel &Model);

	uint64_t m_DisplayCycle = 0;
	std::string m_PendingRevealStableId;
	SDragState m_Drag;
	std::vector<SRuntimeState> m_vRuntimeStates;
	std::vector<float> m_vContentHeights;
	std::vector<const SSettingsCardDefinition *> m_vDefinitionsByState;
	std::vector<int> m_vActiveStateIndices;
	settings_card_deck_logic::CProjectionCache m_ProjectionCache;
};

// 过渡适配器：尚未迁移到 CSettingsCardDeck 的页面继续通过它读写既有全局配置。
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
