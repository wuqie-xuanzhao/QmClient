# QmClient 设置页 UI 统一 P2 Deck、注册表、Search 与持久化 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立全部设置卡片将共用的单一 `QmCardRegistry`、`qm_card_order::CModel`、`CSettingsCardDeck`、Search 与 `qm_global_card_order` 平台，并仅以 Graphics 完成跨列拖拽、自动滚动、entry/reflow 和重启持久化的生产试点。

**Architecture:** `qm_card_registry` 继续持有全局 stable ID/default placement 和 Search metadata；Search 从 registry 取文案，从全局 `qm_card_order::CModel` 取当前 tab。renderer-free `SettingsCardDeckLogic.cpp` 独占 active order、drop target/commit 和边缘自动滚动纯决策，可独立链入 `testrunner`；client-only `SettingsCardDeck.cpp` 调用这些决策、P1 `SettingsCard(...)` 和既有动画 runtime 完成 Graphics 绘制。P2 不让 QmClient/TClient 调用 `CSettingsCardDeck::Render(...)`；它们在本阶段最多只把 legacy order data 迁入全局 model，完整 Deck 接入与私有 coordinator/shell/cache 清退归 P6。

**Tech Stack:** C++、QmUi、`QmCardRegistry`、`QmCardOrderModel`、`QmModuleLayoutAdapter`、`CUiV2AnimationRuntime`、GoogleTest、CMake/MSVC。

## Global Constraints

- P1 的 `SUiTheme`、`ResolveSettingsPageLayout(...)`、`SSettingsCardFrame`、`SettingsCard(...)` 和 `SCardMotionSpec` 必须已通过全量验证与只读 review。
- P2 新公共 Search/Deck/Graphics 路径的 stable ID 和 metadata 只来自 `qm_card_registry::Defaults()`，不建平行表。QmClient 旧 Search 表作为未迁 renderer 的过渡路径保留到 P6，禁止新增条目；条件卡在 defaults 注册，可见性由 definitions 决定。
- P2 新公共 API 与 Graphics 只使用 `qm_card_order::CModel`/`g_Config.m_QmGlobalCardOrder`。QmClient/TClient 只将 legacy order data 导入该 model；为保持旧 renderer 可构建，`m_SettingsCardDeckOrders` 与私有 order/drag/shell coordinator 保留到 P6 同步删除。
- `QmModuleLayoutAdapter` 在 P2 新增显式接收全局 model 的 API；旧 QmClient renderer 依赖的 singleton/wrapper 作为 P6 前过渡符号保留，P6 迁移调用后再删。Graphics/新公共 API 不调用过渡 wrapper。
- page/tab 是 placement，非 card 固有类型；跨列拖拽可改 column/order，Search 跳转只请求目标，不复制 card frame 或视觉。
- entry key 固定为 `page/tab/stable ID/display cycle`；滚动、搜索刷新、文本刷新和 reorder 不得重播 entry。
- 拖拽热路径禁止每帧全量排序和新增稳定 ID 字符串；model 的 `StateIndexForStableId(...)` 保持 O(1)。
- P2 不迁 card 内输入/scroll primitive；P3/P4 完成后 P5/P6 再批量迁页面内容。
- `SettingsCardDeck.cpp` 只进 QmUi client source list；`SettingsCardDeckLogic.cpp` 同时进 client source list 与 `TESTS_EXTRA`。不得为测试链入完整 Deck renderer、`SettingsCard.cpp`、`UiForms.cpp` 或其他 UI renderer。
- P2 唯一 production pilot 是 Graphics。QmClient/TClient 不调用 `CSettingsCardDeck::Render(...)`，不删除私有 shell、drag/drop coordinator 或 cache；P6 对这些页面做一次完整迁移和清退。
- 同一 `cmake-build-release` 目标串行；版本更新留给 P7。

---

## File Structure

- Create: `src/game/client/QmUi/SettingsCardDeckLogic.h/.cpp` — 无 UI 绘制依赖的 active order、drop/commit 与 auto-scroll 纯 owner。
- Create: `src/game/client/QmUi/SettingsCardDeck.h` — 声明式 card definition、drag input/result、client-only coordinator。
- Create: `src/game/client/QmUi/SettingsCardDeck.cpp` — 只负责 frame 收集、input/render/animation 协调，复用 `SettingsCardDeckLogic` 决策。
- Modify: `CMakeLists.txt` — client source list 登记四个 deck 文件；`TESTS_EXTRA` 只登记 `SettingsCardDeckLogic.cpp`。
- Modify: `src/game/client/QmUi/QmCardRegistry.h/.cpp` — 在 defaults 补 description source key，实现 model-aware search result 与 navigation target。
- Modify: `src/game/client/QmUi/QmCardOrderModel.h/.cpp` — 提供单模型 page/column move 结果和可测试 dirty/serialize contract。
- Modify: `src/game/client/QmUi/QmModuleLayoutAdapter.h/.cpp` — 新增显式接收全局 `CModel &` 的 load/move/serialize helper；旧 singleton/no-model wrappers 保留到 P6。
- Modify: `src/game/client/components/menus.h/.cpp` — 持有全局 model 和 Graphics pilot deck；旧 deck order map/helper 仅作为 QmClient/TClient P6 前的过渡 coordinator。
- Modify: `src/game/client/components/menus_settings.cpp` — Graphics P1 桥接改为声明式 deck，验证 registry Search result 的 reveal/navigation。
- Do not modify for rendering: `src/game/client/components/qmclient/menus_qmclient.cpp`、`src/game/client/components/tclient/menus_tclient.cpp` — P2 不在两个页面接 Deck 或删私有协调器。
- Modify: `qmclient_scripts/languages_qmclient/extracted_strings.txt`、`extracted_records_cache.json`、`extracted_audit_report.json` — 记录四个新增 description source key。
- Modify: `qmclient_scripts/languages_qmclient/translations/i18n/misc.toml`、`data/languages/*.txt` — 审核回填四个 description 翻译并重新生成运行时语言文件。
- Modify: `src/test/qm_card_registry_test.cpp`、`src/test/qm_module_layout_adapter_test.cpp`、`src/test/QmAnimTest.cpp`、`src/test/qmclient_monitoring_test.cpp`。`QmAnimTest.cpp` 只 include/link `SettingsCardDeckLogic.h/.cpp`，完整 coordinator 由 Graphics 结构/集成测试与 `game-client` build 覆盖。

---

### Task 1: 建立全局 CModel 并导入 legacy order data

**Files:**
- Modify: `src/game/client/QmUi/QmCardOrderModel.h`
- Modify: `src/game/client/QmUi/QmCardOrderModel.cpp`
- Modify: `src/game/client/QmUi/QmModuleLayoutAdapter.h`
- Modify: `src/game/client/QmUi/QmModuleLayoutAdapter.cpp`
- Modify: `src/game/client/components/menus.h`
- Modify: `src/game/client/components/menus.cpp`
- Test: `src/test/qm_card_registry_test.cpp`
- Test: `src/test/qm_module_layout_adapter_test.cpp`

**Interfaces:**
- Consumes: `qm_card_registry::BuildDefaultEntries()`、`g_Config.m_QmGlobalCardOrder`。
- Produces: `CMenus::SettingsCardOrderModel()`、`CMenus::LoadSettingsCardOrderModel()`、`CMenus::SaveSettingsCardOrderModel()`；adapter helper 全部接收 `qm_card_order::CModel &Model`。

- [ ] **Step 1: Write failing single-model persistence tests**

```cpp
TEST(QmCardRegistry, OneModelPersistsCrossColumnOrderAcrossRestart)
{
	qm_card_order::CModel FirstRun;
	FirstRun.LoadMerged("", qm_card_registry::BuildDefaultEntries());
	FirstRun.Move("deck:graphics-display", 2, 0);
	char aSerialized[65536];
	ASSERT_TRUE(FirstRun.Serialize(aSerialized, sizeof(aSerialized)));

	qm_card_order::CModel Restarted;
	ASSERT_TRUE(Restarted.LoadMerged(aSerialized, qm_card_registry::BuildDefaultEntries()));
	const int Index = Restarted.FindByStableId("deck:graphics-display");
	ASSERT_GE(Index, 0);
	EXPECT_EQ(Restarted.Entry(Index).m_Column, 2);
	EXPECT_EQ(Restarted.Entry(Index).m_OrderInColumn, 0);
	EXPECT_FALSE(Restarted.IsDirty());
}

TEST(QmModuleLayoutAdapter, LegacyMigrationWritesIntoProvidedGlobalModel)
{
	qm_card_order::CModel Model;
	const auto Defaults = qm_card_registry::BuildDefaultEntries();
	Model.LoadMerged("", Defaults);
	EXPECT_TRUE(qm_module::LoadLegacyQmLayoutIntoModel(Model, "chat_bubble:left:0"));
	EXPECT_GE(Model.FindByStableId("qm:chat_bubble"), 0);
}
```

- [ ] **Step 2: Run tests to verify red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.OneModelPersistsCrossColumnOrderAcrossRestart:QmModuleLayoutAdapter.LegacyMigrationWritesIntoProvidedGlobalModel
```

Expected: second test compile FAIL because adapter still owns/uses its static model; first test exposes any dirty/load mismatch.

- [ ] **Step 3: Make adapter functions explicitly consume the model**

Required signatures:

```cpp
bool LoadLegacyQmLayoutIntoModel(qm_card_order::CModel &Model, const char *pConfig);
bool LoadLegacyTClientLayoutIntoModel(qm_card_order::CModel &Model, const char *pConfig);
bool MoveQmModuleInModel(qm_card_order::CModel &Model, EQmModuleId Id, EQmModuleColumn TargetColumn, int TargetOrder);
bool MoveQmModuleToTabInModel(qm_card_order::CModel &Model, EQmModuleId Id, const char *pTargetTab, EQmModuleColumn TargetColumn, int TargetOrder);
bool SerializeLegacyQmLayoutFromModel(const qm_card_order::CModel &Model, char *pOut, int OutSize);
```

新 API 只显式接收 `qm_card_order::CModel &Model`；legacy migration 只在全局 model 尚未从 `qm_global_card_order` 加载有效用户条目时运行一次。`QmModuleLayoutModel()` 及旧 no-model wrappers 仍供未迁移 QmClient renderer 构建，禁止新调用，P6 删除。

- [ ] **Step 4: Add the single CMenus owner**

```cpp
qm_card_order::CModel &CMenus::SettingsCardOrderModel()
{
	if(!m_SettingsCardOrderLoaded)
		LoadSettingsCardOrderModel();
	return m_SettingsCardOrderModel;
}

void CMenus::LoadSettingsCardOrderModel()
{
	const std::vector<qm_card_order::SEntry> Defaults = qm_card_registry::BuildDefaultEntries();
	m_SettingsCardOrderModel.LoadMerged(g_Config.m_QmGlobalCardOrder, Defaults);
	m_SettingsCardOrderLoaded = true;
}

bool CMenus::SaveSettingsCardOrderModel()
{
	if(!m_SettingsCardOrderModel.IsDirty())
		return false;
	if(!m_SettingsCardOrderModel.Serialize(g_Config.m_QmGlobalCardOrder, sizeof(g_Config.m_QmGlobalCardOrder)))
		return false;
	m_SettingsCardOrderModel.ClearDirty();
	return true;
}
```

`m_SettingsCardDeckOrders`、`SettingsCardDeckOrder(...)`、`LoadSettingsCardDeckOrdersFromGlobalConfig()` 和 `SerializeMergedSettingsCardDeckOrdersToGlobalConfig()` 仍被 QmClient/TClient 旧 renderer/coordinator 使用，P2 不删除或重写该调用链。P2 只在加载全局 model 时导入 legacy Qm/TClient order data，Graphics 之后只读写全局 model；P6 迁移两页后再删除这些过渡符号。

- [ ] **Step 5: Run tests to verify green**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.*:QmModuleLayoutAdapter.*
```

Expected: all registry/model/adapter tests PASS；新 adapter/model 路径只使用 `CMenus::SettingsCardOrderModel()`，P6 前的旧 Qm singleton/wrappers 与页面过渡 coordinator 仍可构建但无新调用。

- [ ] **Step 6: Commit the model consolidation**

```powershell
git add src/game/client/QmUi/QmCardOrderModel.h src/game/client/QmUi/QmCardOrderModel.cpp src/game/client/QmUi/QmModuleLayoutAdapter.h src/game/client/QmUi/QmModuleLayoutAdapter.cpp src/game/client/components/menus.h src/game/client/components/menus.cpp src/test/qm_card_registry_test.cpp src/test/qm_module_layout_adapter_test.cpp
git commit -m "refactor(settings-ui): 建立全局卡片顺序模型" -m "refactor: 导入 Qm 与 TClient legacy order data" -m "test: 覆盖 Graphics 跨列重启持久化与旧配置迁移"
```

### Task 2: 实现唯一 CSettingsCardDeck coordinator

**Files:**
- Create: `src/game/client/QmUi/SettingsCardDeckLogic.h`
- Create: `src/game/client/QmUi/SettingsCardDeckLogic.cpp`
- Create: `src/game/client/QmUi/SettingsCardDeck.h`
- Create: `src/game/client/QmUi/SettingsCardDeck.cpp`
- Modify: `CMakeLists.txt:2637`
- Modify: `CMakeLists.txt:3830`
- Test: `src/test/QmAnimTest.cpp`
- Test: `src/test/qmclient_monitoring_test.cpp`

**Interfaces:**
- Consumes: `SSettingsPageLayoutFrame`、`SSettingsCardDefinition`、全局 `CModel`、现有 `CScrollRegion`、P1 `SettingsCard(...)` 和 `SCardMotionSpec`。
- Produces: pure `BuildSettingsCardDeckColumnOrder(...)`、`ApplySettingsCardDeckDragPlacement(...)`、`ResolveSettingsCardDeckDropOrder(...)`、`SettingsCardDeckAutoScrollDelta(...)`、`CommitSettingsCardDeckDrop(...)`；client-only `CSettingsCardDeck::Render(...)`、`RequestReveal(...)`、`SSettingsCardDeckResult`。

- [ ] **Step 1: Write failing deck behavior tests**

```cpp
TEST(SettingsCardDeck, CrossColumnDropMovesOnlyTheGlobalModel)
{
	qm_card_order::CModel Model;
	Model.LoadMerged("", qm_card_registry::BuildDefaultEntries());
	ASSERT_TRUE(CommitSettingsCardDeckDrop(Model, "graphics", "deck:graphics-display", 2, 0));
	const int Index = Model.FindByStableId("deck:graphics-display");
	ASSERT_GE(Index, 0);
	EXPECT_EQ(Model.Entry(Index).m_Column, 2);
	EXPECT_EQ(Model.Entry(Index).m_OrderInColumn, 0);
	EXPECT_STREQ(Model.Entry(Index).m_pDefaultTab, "graphics");
	EXPECT_TRUE(Model.IsDirty());
}

TEST(SettingsCardDeck, EdgeDragRequestsBoundedAutoScroll)
{
	EXPECT_LT(SettingsCardDeckAutoScrollDelta(101.0f, {0.0f, 100.0f, 600.0f, 400.0f}, 1.0f), 0.0f);
	EXPECT_GT(SettingsCardDeckAutoScrollDelta(499.0f, {0.0f, 100.0f, 600.0f, 400.0f}, 1.0f), 0.0f);
	EXPECT_FLOAT_EQ(SettingsCardDeckAutoScrollDelta(300.0f, {0.0f, 100.0f, 600.0f, 400.0f}, 1.0f), 0.0f);
}

TEST(SettingsCardDeck, DragPlacementUsesVisualOrderWithoutRendering)
{
	std::array<std::vector<int>, 3> aColumns{{{}, {4, 7}, {9}}};
	ApplySettingsCardDeckDragPlacement(aColumns, 7, 2, 1);
	EXPECT_EQ(aColumns[1], std::vector<int>({4}));
	EXPECT_EQ(aColumns[2], std::vector<int>({9, 7}));
	const std::vector<SSettingsCardDeckItemGeometry> vItems{
		{9, 2, 0, {500.0f, 100.0f, 300.0f, 120.0f}},
		{7, 2, 1, {500.0f, 236.0f, 300.0f, 120.0f}}};
	EXPECT_EQ(ResolveSettingsCardDeckDropOrder(300.0f, 2, vItems, 7), 1);
}
```

- [ ] **Step 2: Run tests to verify red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=SettingsCardDeck.*
```

Expected: compile FAIL because the renderer-free `SettingsCardDeckLogic` API does not exist. `QmAnimTest.cpp` does not include or construct `CSettingsCardDeck`.

- [ ] **Step 3: Define the pure logic and declarative renderer APIs**

P1 已在 `SSettingsCardVisualState` 定义 `m_DrawOffsetX/m_DrawOffsetY/m_DrawAlpha/m_DropFeedback/m_ReflowCompleteFeedback`；P2 不再改 P1 类型。`SettingsCardDeckLogic.h` 使用以下完整公开契约：

```cpp
#ifndef GAME_CLIENT_QMUI_SETTINGSCARDDECKLOGIC_H
#define GAME_CLIENT_QMUI_SETTINGSCARDDECKLOGIC_H

#include "QmCardOrderModel.h"

#include <game/client/ui_rect.h>

#include <array>
#include <vector>

struct SSettingsCardDeckItemGeometry
{
	int m_StateIndex = -1;
	int m_Column = 1;
	int m_VisualOrder = 0;
	CUIRect m_Rect;
};

std::array<std::vector<int>, 3> BuildSettingsCardDeckColumnOrder(const qm_card_order::CModel &Model, const char *pTab, const std::vector<int> &vActiveStateIndices);
void ApplySettingsCardDeckDragPlacement(std::array<std::vector<int>, 3> &aColumns, int ActiveStateIndex, int TargetColumn, int TargetOrder);
int ResolveSettingsCardDeckDropOrder(float MouseY, int TargetColumn, const std::vector<SSettingsCardDeckItemGeometry> &vItems, int IgnoredStateIndex);
float SettingsCardDeckAutoScrollDelta(float MouseY, const CUIRect &Viewport, float UiScale);
bool CommitSettingsCardDeckDrop(qm_card_order::CModel &Model, const char *pTab, const char *pStableId, int TargetColumn, int TargetOrder);

#endif
```

`SettingsCardDeckLogic.cpp` 是上述五个函数的唯一 owner，只依赖 model、`CUIRect`、`<algorithm>` 和 STL container；不 include `SettingsCard.h`、`UiContext.h`、`QmAnim.h`、`ui_scrollregion.h` 或 graphics API。`BuildSettingsCardDeckColumnOrder(...)` 按 model 中 `tab/column/order` 筛选 active state index；`Apply...` 先从三列移除 active index，再 clamp 后插入目标列；`Resolve...` 按目标列卡片中线返回 `0..count` 的 visual order；commit 只在 tab/stable ID/column 有效且 placement 变化时调用 `Model.Move(...)`。

`SettingsCardDeck.h` 使用以下完整内容：

```cpp
#ifndef GAME_CLIENT_QMUI_SETTINGSCARDDECK_H
#define GAME_CLIENT_QMUI_SETTINGSCARDDECK_H

#include "SettingsCardDeckLogic.h"
#include "SettingsCard.h"
#include "SettingsPageLayout.h"

#include <array>
#include <cstdint>
#include <vector>

class CScrollRegion;

struct SSettingsCardDefinition
{
	SSettingsCardSpec m_Spec;
	FSettingsCardMeasure m_Measure;
	FSettingsCardRender m_Render;
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

class CSettingsCardDeck
{
public:
	SSettingsCardDeckResult Render(const IUiContext &Ctx, const SSettingsPageLayoutFrame &Layout, const char *pTab, const std::vector<SSettingsCardDefinition> &vCards, qm_card_order::CModel &Model, CScrollRegion *pScrollRegion, const SSettingsCardDeckInput &Input, const SCardMotionSpec &Motion, const SSettingsCardDeckVisualOptions &VisualOptions);
	void RequestReveal(const char *pStableId);
	void BeginDisplayCycle(uint64_t DisplayCycle);

private:
	struct SRuntimeState
	{
		const char *m_pStableId = nullptr;
		uint64_t m_LastDisplayCycle = 0;
		float m_DropFeedbackRemaining = 0.0f;
		float m_ReflowCompleteFeedbackRemaining = 0.0f;
		bool m_ReflowWasActive = false;
		bool m_HasDisplayCycle = false;
	};

	struct SDragState
	{
		const char *m_pStableId = nullptr;
		int m_SourceColumn = 1;
		int m_TargetColumn = 1;
		int m_TargetOrder = 0;
		float m_GrabOffsetX = 0.0f;
		float m_GrabOffsetY = 0.0f;

		bool Active() const { return m_pStableId != nullptr; }
	};

	struct SRenderedCard
	{
		const char *m_pStableId = nullptr;
		int m_StateIndex = -1;
		int m_Column = 1;
		int m_Order = 0;
		SSettingsCardFrame m_Frame;
	};

	void PrepareDefinitions(const std::vector<SSettingsCardDefinition> &vCards, const qm_card_order::CModel &Model);
	uint64_t m_DisplayCycle = 0;
	const char *m_pPendingRevealStableId = nullptr;
	SDragState m_Drag;
	std::vector<SRuntimeState> m_vRuntimeStates;
	std::vector<const SSettingsCardDefinition *> m_vDefinitionsByState;
	std::array<std::vector<int>, 3> m_aColumnStateIndices;
	std::vector<SRenderedCard> m_vRenderedCards;
};

#endif
```

该 header 锁定三个 public method；`Render(...)` 显式消费 P1 `SSettingsCardDeckVisualOptions`，页面不自行解释彩虹配置。runtime state 只按 `CModel::StateIndexForStableId(...)` 的连续 index 存储，不在每帧创建 stable ID 字符串。

- [ ] **Step 4: Wire drag, auto-scroll and entry/reflow motion**

`SettingsCardDeck.cpp` 的 renderer 协调流程如下。Ctrl+canonical header press 启动 drag；content press 不启动；dragging 时调用 pure `ApplySettingsCardDeckDragPlacement(...)` 产生让位目标，release 通过 pure `CommitSettingsCardDeckDrop(...)` 更新 model。`SettingsCard(...)` 每张 definition 仍只调用一次，proxy 只通过 visual offset 移动 DrawFrame。下面代码只展示 renderer-owned 协调与动画消费，pure 决策不得复制回该文件。

```cpp
#include "SettingsCardDeck.h"

#include "QmAnim.h"
#include "QmAnimResolve.h"
#include "QmCardRegistry.h"
#include "SettingsCardDeckLogic.h"
#include "UiTheme.h"

#include <base/system.h>

#include <game/client/ui_scrollregion.h>

namespace
{
	uint64_t MixDeckKey(uint64_t Seed, uint64_t Value)
	{
		return Seed ^ (Value + 0x9e3779b97f4a7c15ULL + (Seed << 6U) + (Seed >> 2U));
	}

	uint64_t DeckNodeKey(const IUiContext &Ctx, const char *pTab, const char *pStableId, uint64_t Salt)
	{
		uint64_t Key = MixDeckKey(Ctx.m_ScopeHash, Salt);
		Key = MixDeckKey(Key, pTab != nullptr ? str_quickhash(pTab) : 0U);
		Key = MixDeckKey(Key, pStableId != nullptr ? str_quickhash(pStableId) : 0U);
		return BuildUiAnimNodeKey(Key, Salt);
	}

	bool SameText(const char *pA, const char *pB)
	{
		return pA != nullptr && pB != nullptr && str_comp(pA, pB) == 0;
	}

	bool InsideX(const CUIRect &Rect, float X)
	{
		return X >= Rect.x && X <= Rect.x + Rect.w;
	}

	SUiAnimTransition DeckTransition(float Duration)
	{
		SUiAnimTransition Transition;
		Transition.m_DurationSec = maximum(0.0f, Duration);
		Transition.m_Interrupt = EUiAnimInterruptPolicy::MERGE_TARGET;
		Transition.m_Easing = EEasing::EASE_OUT_QUART;
		return Transition;
	}
}

void CSettingsCardDeck::RequestReveal(const char *pStableId)
{
	const qm_card_registry::SCardDefault *pDefault = qm_card_registry::FindByStableId(pStableId);
	m_pPendingRevealStableId = pDefault != nullptr ? pDefault->m_pStableId : nullptr;
}

void CSettingsCardDeck::BeginDisplayCycle(uint64_t DisplayCycle)
{
	if(m_DisplayCycle == DisplayCycle)
		return;
	m_DisplayCycle = DisplayCycle;
	m_Drag = {};
}

void CSettingsCardDeck::PrepareDefinitions(const std::vector<SSettingsCardDefinition> &vCards, const qm_card_order::CModel &Model)
{
	m_vDefinitionsByState.assign(Model.Count(), nullptr);
	if(m_vRuntimeStates.size() != (size_t)Model.Count())
		m_vRuntimeStates.resize(Model.Count());

	for(const SSettingsCardDefinition &Definition : vCards)
	{
		const qm_card_registry::SCardDefault *pDefault = qm_card_registry::FindByStableId(Definition.m_Spec.m_pStableId);
		if(pDefault == nullptr)
			continue;
		const int StateIndex = Model.StateIndexForStableId(pDefault->m_pStableId);
		if(StateIndex < 0 || m_vDefinitionsByState[StateIndex] != nullptr)
			continue;
		m_vDefinitionsByState[StateIndex] = &Definition;
		SRuntimeState &State = m_vRuntimeStates[StateIndex];
		if(!SameText(State.m_pStableId, pDefault->m_pStableId))
		{
			State = {};
			State.m_pStableId = pDefault->m_pStableId;
		}
	}
}

SSettingsCardDeckResult CSettingsCardDeck::Render(const IUiContext &Ctx, const SSettingsPageLayoutFrame &Layout, const char *pTab, const std::vector<SSettingsCardDefinition> &vCards, qm_card_order::CModel &Model, CScrollRegion *pScrollRegion, const SSettingsCardDeckInput &Input, const SCardMotionSpec &Motion, const SSettingsCardDeckVisualOptions &VisualOptions)
{
	SSettingsCardDeckResult Result;
	Result.m_vFrames.reserve(vCards.size());
	m_vRenderedCards.clear();
	m_vRenderedCards.reserve(vCards.size());

	PrepareDefinitions(vCards, Model);
	std::vector<int> vActiveStateIndices;
	for(int StateIndex = 0; StateIndex < Model.Count(); ++StateIndex)
	{
		if(m_vDefinitionsByState[StateIndex] != nullptr)
			vActiveStateIndices.push_back(StateIndex);
	}
	m_aColumnStateIndices = BuildSettingsCardDeckColumnOrder(Model, pTab, vActiveStateIndices);
	if(m_Drag.Active())
	{
		const int ActiveStateIndex = Model.StateIndexForStableId(m_Drag.m_pStableId);
		if(ActiveStateIndex >= 0)
			ApplySettingsCardDeckDragPlacement(m_aColumnStateIndices, ActiveStateIndex, m_Drag.m_TargetColumn, m_Drag.m_TargetOrder);
	}

	auto RenderColumn = [&](int Column, const CUIRect &ColumnRect, float &ColumnY) {
		int VisualOrder = 0;
		for(const int StateIndex : m_aColumnStateIndices[Column])
		{
			const SSettingsCardDefinition *pDefinition = m_vDefinitionsByState[StateIndex];
			if(pDefinition == nullptr)
				continue;
			const qm_card_order::SEntry &Entry = Model.Entry(StateIndex);
			SRuntimeState &RuntimeState = m_vRuntimeStates[StateIndex];
			CUIRect Slot{ColumnRect.x, ColumnY, ColumnRect.w, 0.0f};

			const bool NewDisplayCycle = !RuntimeState.m_HasDisplayCycle || RuntimeState.m_LastDisplayCycle != m_DisplayCycle;
			const uint64_t ReflowKey = DeckNodeKey(Ctx, pTab, Entry.m_pStableId, 0x5245464c4f57ULL);
			const uint64_t EntryKey = DeckNodeKey(Ctx, pTab, Entry.m_pStableId, MixDeckKey(0x454e545259ULL, m_DisplayCycle));
			if(NewDisplayCycle)
			{
				RuntimeState.m_HasDisplayCycle = true;
				RuntimeState.m_LastDisplayCycle = m_DisplayCycle;
				if(Ctx.m_pAnim != nullptr)
				{
					Ctx.m_pAnim->SetValue(ReflowKey, EUiAnimProperty::POS_Y, Slot.y);
					Ctx.m_pAnim->SetValue(EntryKey, EUiAnimProperty::POS_Y, Motion.m_EntryDistance);
					Ctx.m_pAnim->SetValue(EntryKey, EUiAnimProperty::ALPHA, Motion.m_EntryDuration > 0.0f ? 0.0f : 1.0f);
				}
			}

			float EntryOffset = 0.0f;
			float EntryAlpha = 1.0f;
			float PresentedY = Slot.y;
			if(Ctx.m_pAnim != nullptr)
			{
				if(Motion.m_EntryDistance > 0.0f && Motion.m_EntryDuration > 0.0f)
				{
					EntryOffset = Ctx.m_pAnim->ResolveTargetValue(EntryKey, EUiAnimProperty::POS_Y, 0.0f, DeckTransition(Motion.m_EntryDuration));
					EntryAlpha = Ctx.m_pAnim->ResolveTargetValue(EntryKey, EUiAnimProperty::ALPHA, 1.0f, DeckTransition(Motion.m_EntryDuration));
				}
				else
				{
					Ctx.m_pAnim->SetValue(EntryKey, EUiAnimProperty::POS_Y, 0.0f);
					Ctx.m_pAnim->SetValue(EntryKey, EUiAnimProperty::ALPHA, 1.0f);
				}
				if(!NewDisplayCycle && Motion.m_ReflowDuration > 0.0f)
					PresentedY = Ctx.m_pAnim->ResolveTargetValue(ReflowKey, EUiAnimProperty::POS_Y, Slot.y, DeckTransition(Motion.m_ReflowDuration));
				else
					Ctx.m_pAnim->SetValue(ReflowKey, EUiAnimProperty::POS_Y, Slot.y);
				const bool ReflowActive = Ctx.m_pAnim->HasActiveAnimation(ReflowKey, EUiAnimProperty::POS_Y);
				if(RuntimeState.m_ReflowWasActive && !ReflowActive && Motion.m_KeepReflowCompleteFeedback)
				{
					RuntimeState.m_ReflowCompleteFeedbackRemaining = Motion.m_ReflowCompleteFeedbackDuration;
					Result.m_ReflowCompleteFeedbackConsumed = true;
				}
				RuntimeState.m_ReflowWasActive = ReflowActive;
			}

			SSettingsCardVisualState VisualState{};
			VisualState.m_Focused = SameText(m_pPendingRevealStableId, Entry.m_pStableId);
			VisualState.m_Dragged = SameText(m_Drag.m_pStableId, Entry.m_pStableId);
			RuntimeState.m_DropFeedbackRemaining = maximum(0.0f, RuntimeState.m_DropFeedbackRemaining - maximum(Input.m_FrameDt, 0.0f));
			RuntimeState.m_ReflowCompleteFeedbackRemaining = maximum(0.0f, RuntimeState.m_ReflowCompleteFeedbackRemaining - maximum(Input.m_FrameDt, 0.0f));
			VisualState.m_DropFeedback = RuntimeState.m_DropFeedbackRemaining > 0.0f;
			VisualState.m_ReflowCompleteFeedback = RuntimeState.m_ReflowCompleteFeedbackRemaining > 0.0f;
			VisualState.m_DrawAlpha = EntryAlpha;
			if(VisualState.m_Dragged)
			{
				VisualState.m_DrawOffsetX = Input.m_MouseX - m_Drag.m_GrabOffsetX - Slot.x;
				VisualState.m_DrawOffsetY = Input.m_MouseY - m_Drag.m_GrabOffsetY - Slot.y;
				VisualState.m_DrawAlpha = 0.92f;
			}
			else
				VisualState.m_DrawOffsetY = EntryOffset + PresentedY - Slot.y;

			const SSettingsCardFrame Frame = SettingsCard(Ctx, Slot, pDefinition->m_Spec, VisualState, VisualOptions, pDefinition->m_Measure, pDefinition->m_Render);
			Result.m_vFrames.push_back(Frame);
			m_vRenderedCards.push_back({Entry.m_pStableId, StateIndex, Column, VisualOrder, Frame});
			ColumnY = Frame.m_Rect.y + Frame.m_Rect.h + Layout.m_CardGap;
			++VisualOrder;
		}
	};

	float FullY = Layout.m_ContentViewport.y;
	RenderColumn(0, Layout.m_ContentViewport, FullY);
	if(Layout.m_TwoColumns)
	{
		float LeftY = FullY;
		float RightY = FullY;
		RenderColumn(1, Layout.m_aColumns[0], LeftY);
		RenderColumn(2, Layout.m_aColumns[1], RightY);
	}
	else
	{
		float SingleY = FullY;
		RenderColumn(1, Layout.m_aColumns[0], SingleY);
		RenderColumn(2, Layout.m_aColumns[0], SingleY);
	}

	if(!m_Drag.Active() && Input.m_MousePressed && Input.m_CtrlPressed)
	{
		for(const SRenderedCard &Card : m_vRenderedCards)
		{
			if(!Card.m_Frame.m_HeaderRect.Inside(vec2(Input.m_MouseX, Input.m_MouseY)))
				continue;
			m_Drag.m_pStableId = Card.m_pStableId;
			m_Drag.m_SourceColumn = Card.m_Column;
			m_Drag.m_TargetColumn = Card.m_Column;
			m_Drag.m_TargetOrder = Card.m_Order;
			m_Drag.m_GrabOffsetX = Input.m_MouseX - Card.m_Frame.m_Rect.x;
			m_Drag.m_GrabOffsetY = Input.m_MouseY - Card.m_Frame.m_Rect.y;
			break;
		}
	}

	if(m_Drag.Active())
	{
		if(Layout.m_TwoColumns)
		{
			if(InsideX(Layout.m_aColumns[0], Input.m_MouseX))
				m_Drag.m_TargetColumn = 1;
			else if(InsideX(Layout.m_aColumns[1], Input.m_MouseX))
				m_Drag.m_TargetColumn = 2;
		}
		std::vector<SSettingsCardDeckItemGeometry> vItemGeometry;
		vItemGeometry.reserve(m_vRenderedCards.size());
		for(const SRenderedCard &Card : m_vRenderedCards)
			vItemGeometry.push_back({Card.m_StateIndex, Card.m_Column, Card.m_Order, Card.m_Frame.m_Rect});
		m_Drag.m_TargetOrder = ResolveSettingsCardDeckDropOrder(Input.m_MouseY, m_Drag.m_TargetColumn, vItemGeometry, Model.StateIndexForStableId(m_Drag.m_pStableId));

		if(pScrollRegion != nullptr && InsideX(Layout.m_ScrollViewport, Input.m_MouseX))
		{
			Result.m_AutoScrollDelta = SettingsCardDeckAutoScrollDelta(Input.m_MouseY, Layout.m_ScrollViewport, Ctx.m_UiScale);
			if(Result.m_AutoScrollDelta != 0.0f)
				pScrollRegion->ScrollRelativeDirect(Result.m_AutoScrollDelta);
		}

		const CUIRect TargetColumnRect = m_Drag.m_TargetColumn == 0 ? Layout.m_ContentViewport :
			Layout.m_TwoColumns ? Layout.m_aColumns[m_Drag.m_TargetColumn - 1] : Layout.m_aColumns[0];
		float IndicatorY = TargetColumnRect.y;
		int Seen = 0;
		bool Positioned = false;
		for(const SRenderedCard &Card : m_vRenderedCards)
		{
			if(Card.m_Column != m_Drag.m_TargetColumn || SameText(Card.m_pStableId, m_Drag.m_pStableId))
				continue;
			if(Seen == m_Drag.m_TargetOrder)
			{
				IndicatorY = Card.m_Frame.m_Rect.y - Layout.m_CardGap * 0.5f;
				Positioned = true;
				break;
			}
			IndicatorY = Card.m_Frame.m_Rect.y + Card.m_Frame.m_Rect.h + Layout.m_CardGap * 0.5f;
			++Seen;
		}
		if(!Positioned && Seen == 0)
			IndicatorY = TargetColumnRect.y;
		CUIRect Indicator{TargetColumnRect.x, IndicatorY - Ctx.m_UiScale, TargetColumnRect.w, maximum(2.0f, 2.0f * Ctx.m_UiScale)};
		const ColorRGBA IndicatorColor = Ctx.m_pTheme != nullptr ? Ctx.m_pTheme->m_Accent : ColorRGBA(1.0f, 1.0f, 1.0f, 0.85f);
		Indicator.Draw(IndicatorColor, IGraphics::CORNER_ALL, Indicator.h * 0.5f);

		if(Input.m_MouseReleased)
		{
			const char *pDroppedStableId = m_Drag.m_pStableId;
			Result.m_OrderChanged = CommitSettingsCardDeckDrop(Model, pTab, pDroppedStableId, m_Drag.m_TargetColumn, m_Drag.m_TargetOrder);
			const int DroppedStateIndex = Model.StateIndexForStableId(pDroppedStableId);
			if(Result.m_OrderChanged && Motion.m_KeepDropFeedback)
			{
				if(DroppedStateIndex >= 0)
					m_vRuntimeStates[DroppedStateIndex].m_DropFeedbackRemaining = Motion.m_DropFeedbackDuration;
				Result.m_DropFeedbackConsumed = true;
			}
			if(Result.m_OrderChanged && Motion.m_KeepReflowCompleteFeedback && Motion.m_ReflowDuration == 0.0f)
			{
				if(DroppedStateIndex >= 0)
					m_vRuntimeStates[DroppedStateIndex].m_ReflowCompleteFeedbackRemaining = Motion.m_ReflowCompleteFeedbackDuration;
				Result.m_ReflowCompleteFeedbackConsumed = true;
			}
			m_Drag = {};
		}
		else if(!Input.m_MouseDown)
			m_Drag = {};
	}

	if(m_pPendingRevealStableId != nullptr)
	{
		for(const SRenderedCard &Card : m_vRenderedCards)
		{
			if(!SameText(Card.m_pStableId, m_pPendingRevealStableId))
				continue;
			Result.m_pRevealedStableId = m_pPendingRevealStableId;
			if(pScrollRegion != nullptr)
			{
				float RevealDelta = 0.0f;
				if(Card.m_Frame.m_Rect.y < Layout.m_ScrollViewport.y)
					RevealDelta = Card.m_Frame.m_Rect.y - Layout.m_ScrollViewport.y;
				else if(Card.m_Frame.m_Rect.y + Card.m_Frame.m_Rect.h > Layout.m_ScrollViewport.y + Layout.m_ScrollViewport.h)
					RevealDelta = Card.m_Frame.m_Rect.y + Card.m_Frame.m_Rect.h - (Layout.m_ScrollViewport.y + Layout.m_ScrollViewport.h);
				if(RevealDelta != 0.0f)
					pScrollRegion->ScrollRelativeDirect(RevealDelta);
			}
			m_pPendingRevealStableId = nullptr;
			break;
		}
	}
	return Result;
}
```

Pure owner 按已连续化的 `m_OrderInColumn` 生成 reusable order，drag placement 只移动 int state index。full/left/right 分别使用 model column `0/1/2`；窄屏按 full→left→right 合并到唯一列。entry key 由 `Ctx.m_ScopeHash + tab + stable ID + display cycle` 组成，reflow key 不含 cycle，因此 reorder 只重定向同一 target。entry 同时消费 Y offset 与 alpha；非零 reflow 在 `HasActiveAnimation(ReflowKey, POS_Y)` 由 true 转 false 的那一帧启动 `m_ReflowCompleteFeedbackRemaining`，level 0 则在 release/commit 同帧启动。drop/reflow-complete 仅驱动 P1 shell border/alpha，不延迟 `Result.m_OrderChanged` 或 save。

- [ ] **Step 5: Register pure/client owners and verify green**

根 `CMakeLists.txt` 的既有 QmUi client source list 加入：

```cmake
QmUi/SettingsCardDeck.cpp
QmUi/SettingsCardDeck.h
QmUi/SettingsCardDeckLogic.cpp
QmUi/SettingsCardDeckLogic.h
```

`TESTS_EXTRA` 只加入：

```cmake
src/game/client/QmUi/SettingsCardDeckLogic.cpp
```

`QmAnimTest.cpp` 只 include `SettingsCardDeckLogic.h`。完整 `SettingsCardDeck.cpp` 只在 client source list，不进 `TESTS_EXTRA`；生产协调器由 `QmMonitoringHelpers.GraphicsDeckConsumesPureLogicAndVisualFeedback` 结构测试、Task 4 Graphics 集成测试和 `game-client` build 覆盖，不宣称 `testrunner` 链入了 renderer。

```cpp
TEST(QmMonitoringHelpers, GraphicsDeckConsumesPureLogicAndVisualFeedback)
{
	const std::string Source = ReadRepoFile("src/game/client/QmUi/SettingsCardDeck.cpp");
	EXPECT_NE(Source.find("BuildSettingsCardDeckColumnOrder("), std::string::npos);
	EXPECT_NE(Source.find("ResolveSettingsCardDeckDropOrder("), std::string::npos);
	EXPECT_NE(Source.find("SettingsCardDeckAutoScrollDelta("), std::string::npos);
	EXPECT_NE(Source.find("EUiAnimProperty::ALPHA"), std::string::npos);
	EXPECT_NE(Source.find("m_DropFeedbackRemaining"), std::string::npos);
	EXPECT_NE(Source.find("m_ReflowCompleteFeedbackRemaining"), std::string::npos);
	EXPECT_NE(Source.find("VisualOptions"), std::string::npos);
}
```

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=SettingsCardDeck.*:QmMonitoringHelpers.GraphicsDeckConsumesPureLogicAndVisualFeedback
```

Expected: pure deck tests 与 client structure test PASS；CMake configure 不新增 warning。

- [ ] **Step 6: Commit deck coordinator**

```powershell
git add CMakeLists.txt src/game/client/QmUi/SettingsCardDeckLogic.h src/game/client/QmUi/SettingsCardDeckLogic.cpp src/game/client/QmUi/SettingsCardDeck.h src/game/client/QmUi/SettingsCardDeck.cpp src/test/QmAnimTest.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "feat(settings-ui): 建立统一卡片 Deck" -m "feat: 统一跨列拖拽、自动滚动与 motion 接入" -m "test: 覆盖 canonical frame drop 与边缘滚动"
```

### Task 3: 让 registry 同时服务注册、Search 与导航

**Files:**
- Modify: `src/game/client/QmUi/QmCardRegistry.h`
- Modify: `src/game/client/QmUi/QmCardRegistry.cpp`
- Modify: `src/game/client/components/menus.h`
- Modify: `src/game/client/components/menus.cpp`
- Modify: `qmclient_scripts/languages_qmclient/extracted_strings.txt`
- Modify: `qmclient_scripts/languages_qmclient/extracted_records_cache.json`
- Modify: `qmclient_scripts/languages_qmclient/extracted_audit_report.json`
- Modify: `qmclient_scripts/languages_qmclient/translations/i18n/misc.toml`
- Modify: `data/languages/*.txt`
- Test: `src/test/qm_card_registry_test.cpp`

**Interfaces:**
- Consumes: 补齐 title/description/keywords 的 `SCardDefault` 与全局 `qm_card_order::CModel`。
- Produces: `SearchCards(const char *, const qm_card_order::CModel &)`、`SCardNavigationTarget`、`CMenus::SetSettingsPageFromCardTab(...)`、`CMenus::NavigateToSettingsCard(...)`。

- [ ] **Step 1: Write failing search/navigation tests**

```cpp
TEST(QmCardRegistry, SearchReturnsCanonicalStableIdAndNavigationTarget)
{
	qm_card_order::CModel Model;
	Model.LoadMerged("", qm_card_registry::BuildDefaultEntries());
	const auto Results = qm_card_registry::SearchCards("monitor", Model);
	const auto It = std::find_if(Results.begin(), Results.end(), [](const qm_card_registry::SCardSearchResult &Result) {
		return str_comp(Result.m_pStableId, "deck:graphics-display") == 0;
	});
	ASSERT_NE(It, Results.end());
	EXPECT_EQ(std::count_if(Results.begin(), Results.end(), [](const qm_card_registry::SCardSearchResult &Result) {
		return str_comp(Result.m_pStableId, "deck:graphics-display") == 0;
	}), 1);
	EXPECT_STREQ(It->m_Target.m_pTab, "graphics");
	EXPECT_STREQ(It->m_Target.m_pStableId, It->m_pStableId);
	EXPECT_EQ(It->m_Description, Localize("Window and monitor"));
}

TEST(QmCardRegistry, SearchUsesCurrentModelTabWithoutDuplicatingMetadata)
{
	qm_card_order::CModel Model;
	Model.LoadMerged("", qm_card_registry::BuildDefaultEntries());
	Model.MoveToTab("deck:graphics-display", "appearance-hud", 2, 0);
	const auto Results = qm_card_registry::SearchCards("monitor", Model);
	const auto It = std::find_if(Results.begin(), Results.end(), [](const qm_card_registry::SCardSearchResult &Result) {
		return str_comp(Result.m_pStableId, "deck:graphics-display") == 0;
	});
	ASSERT_NE(It, Results.end());
	EXPECT_STREQ(It->m_Target.m_pTab, "appearance-hud");
}
```

测试文件显式 include `<game/localization.h>` 与 `<algorithm>`。查询 `monitor` 合法地同时命中既有 `qm:debug_graph` 的 `monitoring`；测试验证目标 stable ID 在集合中恰好出现一次，不再把“一个 query 只有一个结果”和“一个 stable ID 不重复”混为一谈。

- [ ] **Step 2: Run tests to verify red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.SearchReturnsCanonicalStableIdAndNavigationTarget:QmCardRegistry.SearchUsesCurrentModelTabWithoutDuplicatingMetadata
```

Expected: compile FAIL because registry has no model-aware search/navigation API and `SCardDefault` has no description field；不会因为既有 `monitoring` 关键词产生错误的唯一结果假设。

- [ ] **Step 3: Implement registry views without a second metadata table**

在 `QmCardRegistry.h` include `<string>`，并把现有 `SCardDefault` 与 Search types 定义为：

```cpp
struct SCardDefault
{
	const char *m_pStableId;
	const char *m_pDefaultTab;
	ECardColumn m_DefaultColumn;
	int m_DefaultOrder;
	const char *m_pTitle;
	const char *m_pSearchKeywords;
	const char *m_pDescription = nullptr;
};

struct SCardNavigationTarget
{
	const char *m_pTab = nullptr;
	const char *m_pStableId = nullptr;
};

struct SCardSearchResult
{
	const char *m_pStableId = nullptr;
	std::string m_Title;
	std::string m_Description;
	SCardNavigationTarget m_Target;
};

std::vector<SCardSearchResult> SearchCards(const char *pQuery, const qm_card_order::CModel &Model);
```

`m_pDescription` 放在 aggregate 最后并提供 `nullptr` 默认值，现有 initializer 不需要机械补空值。`QmCardRegistry.cpp` include `<engine/shared/localization.h>` 与 `<game/localization.h>`，把四个 Graphics initializer 精确改为：

```cpp
{"deck:graphics-display", "graphics", ECardColumn::Left, 0, "Graphics display", "graphics display monitor window", Localizable("Window and monitor")},
{"deck:graphics-visual", "graphics", ECardColumn::Left, 1, "Visual", "graphics visual rendering", Localizable("Rendering options")},
{"deck:graphics-backend", "graphics", ECardColumn::Left, 2, "Graphics backend", "graphics backend renderer selection", Localizable("Renderer selection")},
{"deck:graphics-modes", "graphics", ECardColumn::Left, 3, "Display modes", "display modes graphics resolutions", Localizable("Available resolutions")},
```

`Localizable(...)` 只登记 source key，registry 永远保存英文 source pointer；不在 static defaults 中缓存 `Localize(...)` 返回值。随后在 `QmCardRegistry.cpp` namespace 内增加完整实现：

```cpp
namespace
{
	bool SearchTextMatches(const char *pText, const char *pQuery)
	{
		return pText != nullptr && pText[0] != 0 && str_utf8_find_nocase(pText, pQuery) != nullptr;
	}
}

std::vector<SCardSearchResult> SearchCards(const char *pQuery, const qm_card_order::CModel &Model)
{
	std::vector<SCardSearchResult> vResults;
	if(pQuery == nullptr || pQuery[0] == 0)
		return vResults;
	const std::vector<SCardDefault> &vDefaults = Defaults();
	vResults.reserve(vDefaults.size());
	for(const SCardDefault &Default : vDefaults)
	{
		if(Default.m_pStableId == nullptr)
			continue;
		const char *pLocalizedTitle = Default.m_pTitle != nullptr ? Localize(Default.m_pTitle) : "";
		const char *pLocalizedDescription = Default.m_pDescription != nullptr ? Localize(Default.m_pDescription) : "";
		if(!SearchTextMatches(pLocalizedTitle, pQuery) &&
			!SearchTextMatches(pLocalizedDescription, pQuery) &&
			!SearchTextMatches(Default.m_pSearchKeywords, pQuery))
			continue;

		const int StateIndex = Model.FindByStableId(Default.m_pStableId);
		const char *pCurrentTab = StateIndex >= 0 ? Model.Entry(StateIndex).m_pDefaultTab : Default.m_pDefaultTab;
		SCardSearchResult Result;
		Result.m_pStableId = Default.m_pStableId;
		Result.m_Title = pLocalizedTitle;
		Result.m_Description = pLocalizedDescription;
		Result.m_Target = {pCurrentTab, Default.m_pStableId};
		vResults.push_back(std::move(Result));
	}
	return vResults;
}
```

实现文件再 include `<utility>`。`SearchCards(...)` 只遍历 `Defaults()`，对每个 stable ID 在 `Model` 中取当前 tab；只有 model 未命中才使用 default tab。title/description 每次查询重新 `Localize(...)` 并复制到 `std::string`，因此语言切换不会留下 display pointer。`Defaults()` 的唯一性测试保证一个 stable ID 至多产生一个结果，Search 不建立第二张 metadata/去重表。

- [ ] **Step 4: Extract, translate, review and generate the new description keys**

四个 `Localizable(...)` 位于 `QmCardRegistry.cpp`，当前 source-module 路由会把它们归入 `misc`。按以下顺序运行，draft 未人工审核前不得 write-back：

```powershell
$Languages = 'simplified_chinese,traditional_chinese,japanese,korean,russian,german,spanish,french,brazilian_portuguese,portuguese,turkish,polish'
python qmclient_scripts/languages_qmclient/extract_strings.py
python qmclient_scripts/languages_qmclient/translate_with_local_http.py --languages $Languages --modules misc --base-url http://127.0.0.1:1337/v1 --model local-model --resume
rg -n -F -e 'Window and monitor' -e 'Rendering options' -e 'Renderer selection' -e 'Available resolutions' qmclient_scripts/languages_qmclient/translations_draft
```

Expected: `extracted_strings.txt`、cache 与 audit report 都包含四个 key；每种目标语言的 `translations_draft/<language>/misc.toml` 都有四条非空译文。逐条核对含义、标点和 UI 长度后再运行：

```powershell
python qmclient_scripts/languages_qmclient/translate_with_local_http.py --languages $Languages --modules misc --write-back --resume
python qmclient_scripts/languages_qmclient/generate_all.py
python qmclient_scripts/languages_qmclient/validate.py
python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0
```

Expected: `translations/i18n/misc.toml` 包含四个 key 的 12 种审核译文，`data/languages/*.txt` 已重新生成；四条命令均退出 `0`，没有 placeholder、缺 key、重复 active entry 或格式错误。

- [ ] **Step 5: Add the common navigation bridge for the Graphics pilot**

P2 不修改 QmClient Search UI；它只在 `menus.h/.cpp` 建立公共 navigation bridge，供 Graphics pilot 的 registry result 集成测试调用：

```cpp
bool CMenus::SetSettingsPageFromCardTab(const char *pTab);

void CMenus::NavigateToSettingsCard(const qm_card_registry::SCardNavigationTarget &Target)
{
	if(!SetSettingsPageFromCardTab(Target.m_pTab))
		return;
	m_SettingsCardDeck.RequestReveal(Target.m_pStableId);
}
```

`SetSettingsPageFromCardTab(...)` 明确映射现有 page/subtab，不更改设置枚举值；未知 tab 返回 `false` 且不跳转。P6 迁移 QmClient 时再删除 `SQmCardSearchEntry`/`BuildQmCardSearchEntries(...)`，并让真实 Search UI 调用 `SearchCards(Query, SettingsCardOrderModel())`。

- [ ] **Step 6: Run tests to verify green**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.*
```

Expected: registry tests PASS；`monitor` 可同时返回多个相关结果，但 `deck:graphics-display` 恰好出现一次且使用 model 中的当前 tab。

- [ ] **Step 7: Commit registry search and reviewed translations**

```powershell
git add src/game/client/QmUi/QmCardRegistry.h src/game/client/QmUi/QmCardRegistry.cpp src/game/client/components/menus.h src/game/client/components/menus.cpp src/test/qm_card_registry_test.cpp qmclient_scripts/languages_qmclient/extracted_strings.txt qmclient_scripts/languages_qmclient/extracted_records_cache.json qmclient_scripts/languages_qmclient/extracted_audit_report.json qmclient_scripts/languages_qmclient/translations/i18n/misc.toml data/languages/simplified_chinese.txt data/languages/traditional_chinese.txt data/languages/japanese.txt data/languages/korean.txt data/languages/russian.txt data/languages/german.txt data/languages/spanish.txt data/languages/french.txt data/languages/brazilian_portuguese.txt data/languages/portuguese.txt data/languages/turkish.txt data/languages/polish.txt
git commit -m "feat(settings-ui): 统一卡片注册与搜索跳转" -m "feat: Search 复用 stable ID、描述和导航目标" -m "test: 覆盖去重注册与 canonical navigation"
```

### Task 4: 迁移 Graphics production deck 并删除 P1 bridge

**Files:**
- Modify: `src/game/client/components/menus_settings.cpp`
- Modify: `src/game/client/components/menus.h`
- Modify: `src/game/client/components/menus.cpp`
- Modify: `src/test/qmclient_monitoring_test.cpp`

**Interfaces:**
- Consumes: Tasks 1–3 的单模型、`CSettingsCardDeck` 和 registry search。
- Produces: Graphics 唯一端到端切片；QmClient/TClient 保持现有 renderer/coordinator，P6 一次性迁 Deck/shell/cache/content 并清退私有 drag/order。

- [ ] **Step 1: Write failing deletion and end-to-end tests**

```cpp
TEST(QmMonitoringHelpers, GraphicsDeckRemovesOnlyThePublicBridge)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	EXPECT_EQ(Header.find("RegisterSettingsCardDeckItemFromFrame"), std::string::npos);
	EXPECT_EQ(Menus.find("CMenus::RegisterSettingsCardDeckItemFromFrame"), std::string::npos);
}

TEST(QmMonitoringHelpers, GraphicsDeckRegistersSearchAndRevealFromOneDefinition)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsGraphics(CUIRect MainView)");
	EXPECT_NE(Body.find("SSettingsCardDefinition"), std::string::npos);
	EXPECT_NE(Body.find("m_SettingsCardDeck.Render("), std::string::npos);
	EXPECT_EQ(Body.find("RegisterSettingsCardDeckItemFromFrame"), std::string::npos);
}
```

- [ ] **Step 2: Run tests to verify red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.GraphicsDeckRemovesOnlyThePublicBridge:QmMonitoringHelpers.GraphicsDeckRegistersSearchAndRevealFromOneDefinition
```

Expected: FAIL on the P1 bridge. QmClient/TClient transitional order/coordinator symbols are deliberately not deletion targets in P2.

- [ ] **Step 3: Move Graphics to declarative definitions**

```cpp
// menus.h：所有页面共用这三个 display-cycle 字段。
uint64_t m_SettingsCardDeckDisplayKey = 0;
uint64_t m_SettingsCardDeckDisplayCycle = 0;
bool m_HasSettingsCardDeckDisplayKey = false;
```

`RenderSettingsGraphics(...)` 在构造 definitions 前按 page/tab transition 推进 cycle；同一页滚动、搜索刷新、语言刷新和 reorder 不改变 `DisplayKey`：

```cpp
const char *pDeckTab = "graphics";
const uint64_t DisplayKey = (uint64_t)(unsigned)g_Config.m_UiSettingsPage << 32U | str_quickhash(pDeckTab);
if(!m_HasSettingsCardDeckDisplayKey || m_SettingsCardDeckDisplayKey != DisplayKey)
{
	m_HasSettingsCardDeckDisplayKey = true;
	m_SettingsCardDeckDisplayKey = DisplayKey;
	m_SettingsCardDeck.BeginDisplayCycle(++m_SettingsCardDeckDisplayCycle);
}

std::vector<SSettingsCardDefinition> vCards;
const auto AddCard = [&](const char *pStableId, FSettingsCardMeasure Measure, FSettingsCardRender Render) {
	const qm_card_registry::SCardDefault *pDefault = qm_card_registry::FindByStableId(pStableId);
	dbg_assert(pDefault != nullptr, "settings card must exist in QmCardRegistry");
	if(pDefault == nullptr)
		return;
	SSettingsCardDefinition Definition;
	Definition.m_Spec = {
		pDefault->m_pStableId,
		pDefault->m_pTitle != nullptr ? Localize(pDefault->m_pTitle) : "",
		pDefault->m_pDescription != nullptr ? Localize(pDefault->m_pDescription) : nullptr};
	Definition.m_Measure = std::move(Measure);
	Definition.m_Render = std::move(Render);
	vCards.push_back(std::move(Definition));
};
AddCard("deck:graphics-display", MeasureGraphicsDisplayCard, RenderGraphicsDisplayCard);
AddCard("deck:graphics-visual", MeasureGraphicsVisualCard, RenderGraphicsVisualCard);
AddCard("deck:graphics-backend", MeasureGraphicsBackendCard, RenderGraphicsBackendCard);
AddCard("deck:graphics-modes", MeasureGraphicsModesCard, RenderGraphicsModesCard);

SSettingsCardDeckInput InputState;
InputState.m_MouseX = Ui()->MouseX();
InputState.m_MouseY = Ui()->MouseY();
InputState.m_MousePressed = Ui()->MouseButtonClicked(0);
InputState.m_MouseDown = Ui()->MouseButton(0);
InputState.m_MouseReleased = !Ui()->MouseButton(0) && Ui()->LastMouseButton(0);
InputState.m_CtrlPressed = Input()->ModifierIsPressed();
InputState.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();
const SSettingsCardDeckResult DeckResult = m_SettingsCardDeck.Render(CardCtx, Page, pDeckTab, vCards, SettingsCardOrderModel(), &s_GraphicsScrollRegion, InputState, SettingsCardMotionSpec(), SettingsCardDeckVisualOptions());
if(DeckResult.m_OrderChanged)
	SaveSettingsCardOrderModel();
```

definition 的 title/description、Search metadata、frame、drag item 与 reveal 均来自同一 registry/model/`Render(...)` 路径；页面只在当帧 localize registry source key，不注册第二张 runtime metadata 表。删除 P1 `RegisterSettingsCardDeckItemFromFrame(...)` bridge 和页面局部列游标。

- [ ] **Step 4: Prove the QmClient/TClient P6 boundary remains intact**

不改 `menus_qmclient.cpp` 或 `menus_tclient.cpp`。结构测试反向锁定两个页面未提前接入 `m_SettingsCardDeck.Render(...)`，同时 adapter/model 测试证明 legacy Qm/TClient order data 可写入 `SettingsCardOrderModel()`。P6 负责删除 `m_TClientSettingsCardDragState`、Qm module private drag/drop/order、私有 shell/cache 和本地 Search 表；P2 不写这些文件，不对其做删除断言。

```cpp
TEST(QmMonitoringHelpers, P2DoesNotHalfMigrateQmClientOrTClientRenderers)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string TClient = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	EXPECT_EQ(QmClient.find("m_SettingsCardDeck.Render("), std::string::npos);
	EXPECT_EQ(TClient.find("m_SettingsCardDeck.Render("), std::string::npos);
}
```

- [ ] **Step 5: Run tests and restart persistence scenario**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=SettingsCardDeck.*:QmCardRegistry.*:QmModuleLayoutAdapter.*:QmMonitoringHelpers.GraphicsDeckConsumesPureLogicAndVisualFeedback:QmMonitoringHelpers.GraphicsDeckRemovesOnlyThePublicBridge:QmMonitoringHelpers.GraphicsDeckRegistersSearchAndRevealFromOneDefinition:QmMonitoringHelpers.P2DoesNotHalfMigrateQmClientOrTClientRenderers
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
```

Expected: tests PASS，build 退出码 `0`。人工拖动 Graphics 卡到另一列，正常退出并重启 `DDNet.exe` 后顺序保持；Search “monitor” 同时允许出现 Debug graph 等相关结果，点击 Graphics display 后跳到 Graphics 并 reveal `deck:graphics-display`。

- [ ] **Step 6: Commit production deck migration**

```powershell
git add src/game/client/components/menus_settings.cpp src/game/client/components/menus.h src/game/client/components/menus.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "refactor(settings-ui): 接通 Graphics 卡片 Deck" -m "refactor: 删除公共旧顺序与 P1 桥接" -m "test: 覆盖 Graphics 搜索、拖拽与持久化入口"
```

### Task 5: P2 全量验证、人工矩阵与只读审查

**Files:**
- Modify: none unless review findings require a scoped fix
- Test: model/registry/deck tests, full C++ regression, docs and default gate

**Interfaces:**
- Consumes: Tasks 1–4。
- Produces: P3–P6 唯一可用的 deck/search/order contract。

- [ ] **Step 1: Run serial automated verification**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=SettingsCardDeck.*:QmCardRegistry.*:QmModuleLayoutAdapter.*:QmMonitoringHelpers.GraphicsDeckConsumesPureLogicAndVisualFeedback:QmMonitoringHelpers.GraphicsDeckRemovesOnlyThePublicBridge:QmMonitoringHelpers.GraphicsDeckRegistersSearchAndRevealFromOneDefinition:QmMonitoringHelpers.P2DoesNotHalfMigrateQmClientOrTClientRenderers
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
python qmclient_scripts/gate/check_docs.py
python qmclient_scripts/gate/check_gate.py --mode default
git diff --check
```

Expected: all commands exit `0`。

- [ ] **Step 2: Execute manual deck matrix**

```text
Graphics：左→右、右→左、列尾、空白区 drop
Graphics：上下边缘拖拽自动滚动，release 后无回弹错误
QmClient/TClient：旧内容与私有 coordinator 行为不变；仅 legacy order data 已可迁入全局 model
Restart：跨列顺序在正常退出后保持
Search：标题、描述、关键词各命中一次且无重复结果
Entry：首次展示播放一次，滚动/reorder/search refresh 不重播
Motion level 0/1/2：交互完成时机和持久化结果一致
```

Expected: 每项记录 viewport、UI scale、操作和结果；未验收项为 gap。

- [ ] **Step 3: Dispatch independent read-only review**

review 重点：单模型所有权、config buffer 边界、stable ID 指针生命周期、runtime registration 去重、跨 tab/column normalize、drag release、auto-scroll、entry key、Search 导航不复制视觉。等待完整 findings-first 报告。

Expected: P0/P1 finding 全部修复并重跑 Step 1；报告未返回前 P2 不完成。

---

## Self-review

- Spec coverage: 覆盖单 registry/model/deck、Graphics 跨列 drag/proxy/auto-scroll/restart persistence、Search/navigation、entry alpha+Y、drop/reflow-complete 和 description 本地化流水线。
- Link consistency: `QmAnimTest.cpp` 只链入 `SettingsCardDeckLogic.cpp`；完整 `SettingsCardDeck.cpp`/`SettingsCard.cpp`/UI renderer 不进 `TESTS_EXTRA`，协调器由结构测试、Graphics integration 和 `game-client` 覆盖。
- Type consistency: `CSettingsCardDeck::Render(...)` 显式消费 `SCardMotionSpec` 与 `SSettingsCardDeckVisualOptions`；pure logic API 与 `SettingsCardGeometry` 类型同 P1/索引/规格。
- Scope boundary: P2 只迁入公共 model/registry/Search/deck + Graphics pilot；QmClient/TClient 只可迁 legacy order data，不调 `Render(...)`。两页完整 Deck/私有 coordinator/shell/cache/Search 清退全归 P6。
