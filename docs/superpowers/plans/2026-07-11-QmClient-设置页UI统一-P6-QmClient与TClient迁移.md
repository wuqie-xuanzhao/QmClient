# QmClient 设置页 UI 统一 P6 QmClient 与 TClient 迁移 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**当前状态（2026-07-12，基于 `646dc4f20e`）：** Overview 与 Contributors 已接入公共 page/layout/deck/scroll，并有注册表与结构测试；Visual、Functions、HUD、Config 搜索以及 TClient 主页面和复杂子页仍未迁移。以下任务从 Task 2 继续，不能把已完成切片扩展解释为 P6 完成。

**Goal:** 在不重造 P1–P4 primitive 的前提下，把 QmClient 与 TClient 主页面及复杂子页迁到唯一 page/card/scroll 平台，保持 P3 已收口的 input/numeric 路径，并从生产路径删除 QmClient glass/cached-height 与 TClient cache box/inset/cached-height 双路径。

**Architecture:** 页面外壳只通过 `ResolveSettingsPageLayout(...)` 和 P4 `CScrollRegion::State()` 中的 `CQmScrollState` 取得 viewport/列/滚动状态；页面提交 `SSettingsCardDefinition`，P2 `CSettingsCardDeck::Render(...)` 内部唯一调用 `SettingsCard(...)` 并返回 canonical `SSettingsCardFrame`。QmClient 继续以 `QmCardRegistry`、`CMenus::SettingsCardOrderModel()` 中的 `qm_card_order::CModel` 和 `QmModuleLayoutAdapter` 为唯一注册/顺序/兼容层；TClient 的 `CSectionLoader` 只测量、缓存并返回 content height，placeholder/compact/full 共用该值，完整 card frame 高度只由 Deck/`SettingsCard(...)` 组合 header、padding 与 shell 得出。

**Tech Stack:** C++、QmUi、DDNet immediate-mode UI、GoogleTest、CMake/MSVC、Python gate、运行时 telemetry。

## Global Constraints

- 权威规格：`docs/superpowers/specs/2026-07-10-QmClient-设置页UI统一与滚动体系规格.md`。
- P0–P5 必须已完成；P6 继承 P5 已验证的页面迁移模式，只消费稳定公共 API，不在页面文件中增加兼容 overload、第二套 token、第二套动画 runtime、第二套滚动 controller 或第二套 card model。
- P2 只建立公共 model/registry/Search/deck 并完成 Graphics pilot；QmClient/TClient 在 P2 最多只迁 legacy order data，未调用 `CSettingsCardDeck::Render(...)`。P6 负责两页完整 Deck 接入，并删除页面私有 drag/drop/order coordinator、shell/cache 与 QmClient 本地 Search 表。
- QmClient 顺序事实源保持 `QmCardRegistry` → `CMenus::SettingsCardOrderModel()` → `QmModuleLayoutAdapter`；禁止恢复 `QmModuleLayoutModel()` 单例或在 `menus_qmclient.cpp` 新建局部 `qm_card_order::CModel`、顺序 vector 或序列化器。
- 页面顶部子 tab 使用 `SSettingsPageLayoutFrame::m_SubTabRect`，始终全宽且不可拖拽；不进行设置页信息架构重组。
- `SSettingsCardFrame::m_Rect` 同时是 display/hit/drag/proxy source rect；业务内容只使用 `m_ContentRect`，不得从 cached height 或另一层 box rect 重建 shell。
- P3 已保证文本输入只用 `ui_widget::InputField(...)`、普通数值 slider+input 只用 `ui_widget::NumericField(...)`。P6 只在移动 content callback 时保持这些调用与结构删除断言，不再设计输入 API。颜色轨道保持其专用路径。
- 所有本计划页面只保留一个 P4 `CScrollRegion` adapter，其唯一可变状态为 `CScrollRegion::State()` 返回的 `CQmScrollState`；policy 由 `QmResolveScrollPolicy(...)` 的 `EQmScrollProfile::SETTINGS_PAGE` 解析。页面不得再保存 scroll-y/content-height/velocity、私有 wheel scale、rail width、平滑时间或 `CQmScrollContainer`。
- `CSectionLoader` 的 `m_MeasureContentFn`、placeholder、`m_RenderCompactContentFn` 与 `m_RenderFullContentFn` 使用同一 content-height contract；完整 card 高度只由 Deck 返回的 `SSettingsCardFrame::m_Rect.h` 派生，`section_height_measured` 与 `section_height_rendered` 差值阈值固定为 `0.01f`。
- BindWheel 与 StatusBar 已接入 shared deck；P6 保留其 stable ID/order 和 P3 input/numeric 语义，只迁 canonical shell/content callback 与公共 scroll。
- 每完成一个页面切片就同时删除该切片旧 shell/scroll/cache 路径，并证明 P3 input/numeric 旧路径没有回归；不允许“公共 wrapper + 旧绘制”双路径进入 commit。
- 不新增英文 source key；卡片标题复用现有 `Localize`/`Localizable` key。因实现发现必须新增 key 时，该切片先补完整 i18n 生成链再提交，不能以英文占位交付。
- 保留 `PrewarmOnly`、tab transition、Search 跳转、usage/collapse、新功能标记、配置读写、资源预触发与 telemetry 行为；P6 不改变游戏、协议、物理、预测、Demo/地图/skin/配置格式语义。
- 注释使用中文；保持原文件 UTF-8、BOM、换行与 Tab 风格；不格式化或回退用户/脚本产生的无关改动。
- 同一 `cmake-build-release` 中的 `testrunner`、`game-client`、`run_cxx_tests` 和 default gate 严格串行。
- P6 不更新功能版本；P7 在 P0–P7 全部收口后执行一次 MMP 版本更新。
- R1 公共组件扩面、R2 信息架构/完整 L0–L2、R3 Phosphor/MSDF/SDF/渲染管线均不进入本计划。

---

## File Structure

- Modify: `src/game/client/components/qmclient/menus_qmclient.cpp:784-859` — Overview page layout/card/scroll migration。
- Modify: `src/game/client/components/qmclient/menus_qmclient.cpp:1023-7901` — QmClient 五个子 tab 的 module/card/content/scroll migration、P3 input/numeric 保持与旧 glass/cached-height 清退。
- Modify: `src/game/client/components/menus.cpp:4482-4524` — 删除 P6 后无调用的旧 Qm card style/glass 实现。
- Modify: `src/game/client/components/tclient/menus_tclient.cpp:918-1074` — TClient 全宽 tab shell 与公共 page frame。
- Modify: `src/game/client/components/tclient/menus_tclient.cpp:1076-3657` — Settings 主页面、`CSectionLoader` content-height contract、cache box/inset/cached-height 清退。
- Modify: `src/game/client/components/section_loader.h/.cpp` — 从“自己排列/推进列 rect”收窄为“按 stable ID 测量内容、选择 placeholder/compact/full 模式并调度渲染”，不再与 Deck 竞争布局所有权。
- Modify: `src/game/client/components/tclient/menus_tclient.cpp:3734-4933` — BindWheel、ChatBinds、WarList、StatusBar migration。
- Modify: `src/game/client/components/tclient/menus_tclient.cpp:4936-5800` — Info、Profiles、Configs migration。
- Modify: `src/game/client/components/menus.h:2550-2783` — 删除 P6 后无调用的 Qm scroll/glass/drag/order 与 TClient drag/cache box/inset 声明。
- Modify: `src/game/client/QmUi/QmModuleLayoutAdapter.h/.cpp` — QmClient 改用显式 `SettingsCardOrderModel()` API 后，删除 P2 为旧 renderer 保留的 `QmModuleLayoutModel()` 与 no-model wrappers。
- Modify: `src/game/client/QmUi/QmCardRegistry.cpp:11-110` — 只补复杂子页新的稳定卡片 placement；不复制 registry 或 model。
- Modify: `src/test/qm_card_registry_test.cpp` — 新 stable ID、默认 tab/column/order 与唯一性行为测试。
- Modify: `src/test/section_loader_test.cpp:808-834` — callback 计数证明 placeholder → compact → full，并验证三条路径共用 measured content height、Deck card 高度稳定。
- Modify: `src/test/qm_new_ui_menu_branch_test.cpp:1630-1970` — 页面真实生产路径与旧符号删除检查。
- Modify: `src/test/qmclient_monitoring_test.cpp:1620-1840,2907-2920,6250-6410` — telemetry、model 保留与禁止双路径检查。
- Do not modify: `src/game/client/QmUi/QmCardOrderModel.*` 与 P1–P4 公共 primitive 实现；P6 只消费它们。`QmModuleLayoutAdapter.*` 仅允许删除过渡 singleton/wrappers，`CSectionLoader` 是 P6 明确允许收窄的页面调度器。

## Fixed Shared Interfaces

P6 只按 P1–P4 已落地声明调用以下接口；若调用不匹配，修页面，不新增页面专用 overload。`BuildSettingsCardFrame(...)` 只供 P1 `SettingsCard(...)` 与纯几何单元测试消费；`SettingsCard(...)` 在下表只用来锁定 Deck 内部的 canonical shell，页面不直接调用：

```cpp
SSettingsPageLayoutFrame ResolveSettingsPageLayout(const CUIRect &PageRect, bool HasSubTabs, float UiScale = 1.0f);

SSettingsCardFrame BuildSettingsCardFrame(
	const CUIRect &Slot,
	const SSettingsCardSpec &Spec,
	float ContentHeight,
	float UiScale);

SSettingsCardFrame SettingsCard(
	const IUiContext &Ctx,
	const CUIRect &Slot,
	const SSettingsCardSpec &Spec,
	const SSettingsCardVisualState &State,
	const SSettingsCardDeckVisualOptions &VisualOptions,
	const FSettingsCardMeasure &Measure,
	const FSettingsCardRender &Render);

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
};

namespace ui_widget
{
	SInputFieldResult InputField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const SInputFieldOptions &Options);
	SNumericFieldResult NumericField(const IUiContext &Ctx, SNumericFieldState &State, const void *pId, int *pStoredValue, int StoredMin, int StoredMax, const CUIRect &Rect, const SNumericFieldOptions &Options);
}

SQmResolvedScrollPolicy QmResolveScrollPolicy(
	const SQmScrollRequest &Request,
	float UiScale,
	float SmoothScrollTimeSec);
class CQmScrollState
{
public:
	SQmScrollContainerFrame Update(const CUIRect &ViewRect, float ContentSize, float Dt, const SQmScrollContainerInput &Input, const SQmResolvedScrollPolicy &Policy);
	float Offset() const;
};
CQmScrollState &CScrollRegion::State();
```

统一调用形状如下；页面不直接重画 shell 或注册第二份 drag item：

```cpp
const IUiContext CardCtx = SettingsUiContext("settings_page_scope", UiScale);
const SSettingsPageLayoutFrame Page = ResolveSettingsPageLayout(MainView, true, UiScale);
std::vector<SSettingsCardDefinition> vCards;
vCards.push_back({{pStableId, pTitle, pSubtitle},
	[&](float ContentWidth) { return MeasureContent(ContentWidth); },
	[&](CUIRect ContentRect) { RenderContent(ContentRect); }});
const SQmResolvedScrollPolicy ScrollPolicy = QmResolveScrollPolicy({EQmScrollProfile::SETTINGS_PAGE, EQmScrollAxis::VERTICAL, 0.0f, 0}, UiScale, g_Config.m_UiSmoothScrollTime / 1000.0f);
CQmScrollState &ScrollState = s_ScrollRegion.State();
const SSettingsCardDeckResult DeckResult = m_SettingsCardDeck.Render(CardCtx, Page, pTab, vCards, SettingsCardOrderModel(), &s_ScrollRegion, DeckInput, SettingsCardMotionSpec(), SettingsCardDeckVisualOptions());
if(DeckResult.m_OrderChanged)
	SaveSettingsCardOrderModel();
```

## P6 Entry Gate

### 当前执行边界

- 已完成：`RenderSettingsQmClientOverview`、`RenderSettingsQmClientContributors`，以及对应 stable ID、导航/结构测试。
- 本轮继续：QmClient Visual/Functions/HUD module 卡片、QmClient Config 搜索入口、TClient Settings 主页面和未迁移复杂子页。
- 明确保留到后续切片：P7 非卡片菜单；R1/R2/R3 公共组件扩面、信息架构重组和渲染管线改造。
- 不能作为完成条件：只新增 shared wrapper、只通过结构字符串测试、只完成 `game-client` build，或保留旧 shell/私有 coordinator 再称“迁移完成”。

- [ ] **Verify P1–P4 contracts and the P2 Graphics-only boundary before Task 1**

Run:

```powershell
New-Item -ItemType Directory -Force -Path tmp | Out-Null
rg -n "ResolveSettingsPageLayout|struct SSettingsPageLayoutFrame|SettingsCard\(|struct SSettingsCardFrame|class CSettingsCardDeck|InputField\(|NumericField\(|class CQmScrollState|QmResolveScrollPolicy" src/game/client/QmUi src/game/client/components
rg -n "m_TClientSettingsCardDragState|SQmModuleDragState|SQmModuleDropPreview|BuildQmCardSearchEntries" src/game/client/components/qmclient src/game/client/components/tclient src/game/client/components/menus.h
git rev-parse HEAD | Set-Content -Encoding ascii tmp/settings-ui-p6-start.txt
```

Expected: 第一条为公共契约返回定义或 Graphics 调用；第二条命中当前 QmClient/TClient 私有 coordinator/Search，这些命中就是 P6 删除清单；起点记录为 40 位 commit。若两页已提前调用 `m_SettingsCardDeck.Render(...)` 却仍保留私有 coordinator，视为跨阶段半迁移，先校正责任边界。

### Task 1: 迁移 QmClient Overview 与全宽页面 shell

**Files:**
- Modify: `src/game/client/components/qmclient/menus_qmclient.cpp:784-859`
- Modify: `src/game/client/QmUi/QmCardRegistry.cpp`
- Test: `src/test/qm_card_registry_test.cpp`
- Test: `src/test/qm_new_ui_menu_branch_test.cpp`

**Interfaces:**
- Consumes: `ResolveSettingsPageLayout(...) -> SSettingsPageLayoutFrame`、`SSettingsCardDefinition`、`CSettingsCardDeck::Render(...) -> SSettingsCardDeckResult`、`CQmScrollState`、`QmResolveScrollPolicy(...)`。
- Produces: `deck:qmclient-overview-intro` 与 `deck:qmclient-overview-guide` 两张 full-column 卡；Overview 不再调用 Qm 私有 scroll/glass helper。

- [ ] **Step 1: Write failing production and registry tests**

```cpp
TEST(QmNewUiMenuBranches, P6QmClientOverviewUsesCanonicalPageCardAndScroll)
{
	const std::string Source = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = FunctionBody(Source, "void CMenus::RenderSettingsQmClientOverview(CUIRect MainView, bool PrewarmOnly)");
	ASSERT_FALSE(Body.empty());
	EXPECT_NE(Body.find("ResolveSettingsPageLayout("), std::string::npos);
	EXPECT_NE(Body.find("SSettingsCardDefinition"), std::string::npos);
	EXPECT_NE(Body.find("m_SettingsCardDeck.Render("), std::string::npos);
	EXPECT_NE(Body.find(".State()"), std::string::npos);
	EXPECT_NE(Body.find("QmResolveScrollPolicy("), std::string::npos);
	EXPECT_EQ(Body.find("BeginSettingsQmScrollContainer("), std::string::npos);
	EXPECT_EQ(Body.find("RenderQmSettingsGlassCard("), std::string::npos);
}

TEST(QmCardRegistry, P6RegistersQmClientOverviewCards)
{
	const auto *pIntro = qm_card_registry::FindByStableId("deck:qmclient-overview-intro");
	const auto *pGuide = qm_card_registry::FindByStableId("deck:qmclient-overview-guide");
	ASSERT_NE(pIntro, nullptr);
	ASSERT_NE(pGuide, nullptr);
	EXPECT_STREQ(pIntro->m_pDefaultTab, "qmclient-overview");
	EXPECT_STREQ(pGuide->m_pDefaultTab, "qmclient-overview");
	EXPECT_EQ(pIntro->m_DefaultColumn, qm_card_registry::ECardColumn::Full);
	EXPECT_LT(pIntro->m_DefaultOrder, pGuide->m_DefaultOrder);
}
```

- [ ] **Step 2: Rebuild testrunner and verify red**

Run:

```powershell
cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmNewUiMenuBranches.P6QmClientOverviewUsesCanonicalPageCardAndScroll:QmCardRegistry.P6RegistersQmClientOverviewCards
```

Expected: FAIL because Overview still uses the private Qm scroll/glass path and the two IDs are not registered.

- [ ] **Step 3: Implement the minimal Overview slice**

Add exactly these defaults without changing existing `qm:*` entries:

```cpp
{"deck:qmclient-overview-intro", "qmclient-overview", ECardColumn::Full, 0, "QmClient overview", "qmclient overview guide"},
{"deck:qmclient-overview-guide", "qmclient-overview", ECardColumn::Full, 1, "Page guide", "qmclient page guide tabs"},
```

`QmCardRegistry.CoversAllCardsNoDuplicates` 按测试名/断言语义清理任何“全局固定总数”断言，不匹配某个历史数字字面量；只保留遍历全表的 duplicate 断言。完整性由现有 Qm/TClient/deck namespace coverage 与本计划逐页 ID 测试负责。不得用 P6 的数量覆盖 P5 或用户并行新增的 registry entry。

Use `ResolveSettingsPageLayout(MainView, false, UiScale)`, a `SETTINGS_PAGE` policy and one `CScrollRegion` whose `State()` is the only `CQmScrollState`. Move the two existing text bodies unchanged into `SSettingsCardDefinition::m_Render`; their `m_Measure` callbacks compute title/body/small typography from the same line and spacing tokens used by rendering. Submit both definitions once through `m_SettingsCardDeck.Render(...)`. Delete `DrawFullWidthCard`, `QmSettingsCardStyle`, `BeginSettingsQmScrollContainer`, `FinishSettingsQmScrollContainer` and fixed outer-margin calculations from this function.

- [ ] **Step 4: Run focused green checks**

Run:

```powershell
cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmNewUiMenuBranches.P6QmClientOverviewUsesCanonicalPageCardAndScroll:QmCardRegistry.P6RegistersQmClientOverviewCards:QmCardRegistry.CoversAllCardsNoDuplicates
git diff --check
```

Expected: all named tests PASS and `git diff --check` emits no output.

**Visual matrix:**

| Page | View/UI scale/language | Operation | Expected |
|---|---|---|---|
| Overview | 1280×720 / 100% / English | enter, wheel, hover both headers | two single-layer full-width cards; title/body/small roles are distinct; rail appears only on overflow |
| Overview | 960×720 / 125% / Simplified Chinese | enter, resize, Alt+wheel | no clipped title/body or phantom right column; Alt wheel is 3×; card rect and hit rect stay aligned |

- [ ] **Step 5: Commit Overview slice**

```powershell
git add src/game/client/components/qmclient/menus_qmclient.cpp src/game/client/QmUi/QmCardRegistry.cpp src/test/qm_card_registry_test.cpp src/test/qm_new_ui_menu_branch_test.cpp
git commit -m "refactor(settings-ui): 迁移栖梦概览卡片" -m "refactor: 使用公共页面布局、卡片 deck 与滚动状态" -m "test: 锚定 Overview stable ID 并禁止旧 glass 路径"
```

### Task 2: 迁移 QmClient Visual/Functions/HUD/Contributors 主内容

**Files:**
- Modify: `src/game/client/components/qmclient/menus_qmclient.cpp:1023-7901`
- Modify: `src/game/client/components/menus.cpp:4482-4524`
- Modify: `src/game/client/components/menus.h:2550-2635,2782-2783`
- Modify: `src/game/client/QmUi/QmModuleLayoutAdapter.h/.cpp`
- Modify: `src/game/client/QmUi/QmCardRegistry.cpp`
- Test: `src/test/qm_card_registry_test.cpp`
- Test: `src/test/qm_new_ui_menu_branch_test.cpp`
- Test: `src/test/qmclient_monitoring_test.cpp`

**Interfaces:**
- Consumes: Task 1 shell、`CSettingsCardDeck`、`ui_widget::InputField(...)`、`ui_widget::NumericField(...)`、`CScrollRegion::State()`/`CQmScrollState`、`QmResolveScrollPolicy(...)`、`CMenus::SettingsCardOrderModel()` 与现有 adapter serialization/migration functions。
- Produces: Qm modules 的 canonical card frames；`deck:qmclient-contributors-community` 与 `deck:qmclient-contributors-sponsors`；无 Qm 页面私有 glass/layout/drag/cached-height/input/scroll 路径。

- [ ] **Step 1: Write failing structure/model tests**

```cpp
TEST(QmNewUiMenuBranches, P6QmClientModulesUseSharedPlatformOnly)
{
	const std::string Source = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = FunctionBody(Source, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(Body.empty());
	EXPECT_NE(Body.find("ResolveSettingsPageLayout("), std::string::npos);
	EXPECT_NE(Body.find("SSettingsCardDefinition"), std::string::npos);
	EXPECT_NE(Body.find("m_SettingsCardDeck.Render("), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField("), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::NumericField("), std::string::npos);
	EXPECT_NE(Body.find("QmResolveScrollPolicy("), std::string::npos);
	for(const char *pForbidden : {"s_GlassCards", "DrawGlassCardBackground", "RenderQmModuleHeadline", "SQmModuleDragState", "SQmModuleDropPreview", "RegisterModuleCard", "s_aQmModuleLastHeights", "RenderQmSettingsGlassCard(", "BeginSettingsQmScrollContainer(", "ui_widget::TextField(", "ui_widget::SearchField(", "DoSettingsScrollbarOption("})
		EXPECT_EQ(Body.find(pForbidden), std::string::npos) << pForbidden;
}

TEST(QmMonitoringHelpers, P6QmClientPreservesGlobalCardModelAuthority)
{
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(Body.empty());
	EXPECT_NE(Body.find("SettingsCardOrderModel()"), std::string::npos);
	EXPECT_NE(Body.find("qm_module::MoveQmModuleInModel(SettingsCardOrderModel(),"), std::string::npos);
	EXPECT_NE(Body.find("SaveSettingsCardOrderModel()"), std::string::npos);
	EXPECT_EQ(Body.find("QmModuleLayoutModel()"), std::string::npos);
	EXPECT_EQ(Body.find("SerializeMergedGlobalCardOrderFromQmModel("), std::string::npos);
	EXPECT_EQ(Body.find("qm_card_order::CModel "), std::string::npos);
}
```

Add registry assertions for these exact defaults:

```cpp
{"deck:qmclient-contributors-community", "qmclient-contributors", ECardColumn::Full, 0, "QmClient Community", "community links qmclient"},
{"deck:qmclient-contributors-sponsors", "qmclient-contributors", ECardColumn::Full, 1, "Sponsor support", "sponsor support qmclient"},
```

```cpp
TEST(QmCardRegistry, P6QmClientContributorsCards)
{
	const auto *pCommunity = qm_card_registry::FindByStableId("deck:qmclient-contributors-community");
	const auto *pSponsors = qm_card_registry::FindByStableId("deck:qmclient-contributors-sponsors");
	ASSERT_NE(pCommunity, nullptr);
	ASSERT_NE(pSponsors, nullptr);
	EXPECT_STREQ(pCommunity->m_pDefaultTab, "qmclient-contributors");
	EXPECT_STREQ(pSponsors->m_pDefaultTab, "qmclient-contributors");
	EXPECT_EQ(pCommunity->m_DefaultColumn, qm_card_registry::ECardColumn::Full);
	EXPECT_EQ(pSponsors->m_DefaultColumn, qm_card_registry::ECardColumn::Full);
	EXPECT_LT(pCommunity->m_DefaultOrder, pSponsors->m_DefaultOrder);
}
```

- [ ] **Step 2: Rebuild testrunner and verify red**

Run:

```powershell
cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmNewUiMenuBranches.P6QmClientModulesUseSharedPlatformOnly:QmMonitoringHelpers.P6QmClientPreservesGlobalCardModelAuthority:QmCardRegistry.P6QmClientContributorsCards
```

Expected: at least the production-path and registry tests FAIL on the old Qm layout/glass/input/scroll anchors.

- [ ] **Step 3: Replace the Qm page shell without changing module behavior**

Keep the existing module IDs, Search/usage/collapse/new-feature behavior and explicit-model adapter calls, but build one `SSettingsCardDefinition` per module and pass all definitions plus `SettingsCardOrderModel()` to `m_SettingsCardDeck.Render(...)`. The deck invokes `SettingsCard(...)` exactly once per definition, and the existing content lambda receives only its canonical content rect. Full-column protection remains in `qm_module::MoveQmModuleInModel(SettingsCardOrderModel(), ...)`; `DeckResult.m_OrderChanged` only calls `SaveSettingsCardOrderModel()`，不恢复旧 global-order merge serializer。删除 adapter 中的 `QmModuleLayoutModel()` 与 no-model wrappers，保留显式接收 `qm_card_order::CModel &` 的 migration/move helper。

P6 删除 `SQmModuleDragState`、`SQmModuleDropPreview`、manual drop indicator/proxy、private six-dot coordinator，以及仍用于旧内容布局的 `SQmModuleCardInfo`、`RegisterModuleCard` 与 local display/preview/cached-height arrays，不把它们翻译成另一套局部 layout helper。

Delete `s_GlassCards`, `DrawGlassCardBackground`, `RenderQmModuleHeadline*`, local rainbow cache and `s_aQmModuleLastHeights`; deck 内部的 `SettingsCard(...)` owns surface/header/subtitle/rainbow and measures content every invalidated layout. Move the already-unified P3 `InputField`/`NumericField` calls into definition callbacks without changing IME, clear, `∞`, unit or commit policy. Replace private Qm scroll begin/finish with one `CScrollRegion` adapter、其 `State()` 返回的唯一 `CQmScrollState` 和 resolved settings policy。

After QmClient and all P1–P5 callers no longer use them, remove `SSettingsQmScrollFrame`, `SQmSettingsCardStyle`, `BeginSettingsQmScrollContainer`, `FinishSettingsQmScrollContainer`, `QmSettingsCardStyle`, `RenderQmSettingsGlassCard`, and old global-search `vGlassCards` parameters from `menus.h`/implementations. If a non-P6 caller remains, migrate that caller to the already available P1/P2 shell rather than retaining the alias.

- [ ] **Step 4: Run focused green checks**

Run:

```powershell
cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmNewUiMenuBranches.P6QmClientModulesUseSharedPlatformOnly:QmMonitoringHelpers.P6QmClientPreservesGlobalCardModelAuthority:QmCardRegistry.P6QmClientContributorsCards:QmModuleLayoutAdapter.*
git diff --check
```

Expected: all named tests PASS; adapter tests prove legacy/global order semantics remain intact; whitespace check is clean.

**Visual matrix:**

| Subpage | View/UI scale/language | Operation | Expected |
|---|---|---|---|
| Visual | 1280×720 / 100% / English | drag a non-full card across columns, release, reopen client | one shell/handle/proxy; displaced cards do not overlap; global order persists after restart |
| Visual | 960×720 / 125% / Simplified Chinese | edit text/numeric fields, focus/IME/clear, wheel | gray fill stays unchanged on focus; thick ring aligns; no square slider track or clipped unit |
| Functions | 1280×720 / 100% / Simplified Chinese | search, collapse/expand, drag while scrolled | Search jumps to the registered card; collapse and usage state remain; auto-scroll owns wheel |
| HUD | 1920×1080 / 75% / English | change numeric values and drag tall cards | card/content/hit rect stay aligned; no cached-height jump; preview controls remain interactive |
| Contributors | 960×720 / 125% / Simplified Chinese | hover links/sponsor cards and scroll | two full cards have one surface; links hit only inside content; subtitle appears without changing height |

- [ ] **Step 5: Commit QmClient module slice**

```powershell
git add src/game/client/components/qmclient/menus_qmclient.cpp src/game/client/components/menus.cpp src/game/client/components/menus.h src/game/client/QmUi/QmModuleLayoutAdapter.h src/game/client/QmUi/QmModuleLayoutAdapter.cpp src/game/client/QmUi/QmCardRegistry.cpp src/test/qm_card_registry_test.cpp src/test/qm_new_ui_menu_branch_test.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "refactor(settings-ui): 迁移栖梦模块页面" -m "refactor: 复用全局卡片模型并删除私有 glass 与 cached-height 路径" -m "test: 禁止私有拖拽回归并保留布局持久化语义"
```

### Task 3: 迁移 TClient Settings 与 CSectionLoader content-height contract

**Files:**
- Modify: `src/game/client/components/tclient/menus_tclient.cpp:918-3657`
- Modify: `src/game/client/components/menus.h:2712-2755`
- Modify: `src/game/client/components/section_loader.h`
- Modify: `src/game/client/components/section_loader.cpp`
- Test: `src/test/section_loader_test.cpp`
- Test: `src/test/qm_new_ui_menu_branch_test.cpp`
- Test: `src/test/qmclient_monitoring_test.cpp`

**Interfaces:**
- Consumes: `SSettingsPageLayoutFrame`、`SSettingsCardDefinition`、P2 `CSettingsCardDeck::Render(...)`、收窄后的 `CSectionLoader::Register/BeginCardFrame/MeasureCardContent/RenderCardContent/EndCardFrame`、现有 19 个 `tclient:*` registry IDs。
- Produces: definition measure 返回 content height，loader placeholder/compact/full 均使用该测量，完整外框高度只来自 canonical `SSettingsCardFrame::m_Rect.h`；`tclient_settings_section_height` telemetry；无 cache box/inset/page-cached-height helper。

- [ ] **Step 1: Write failing content-height progression and deletion tests**

```cpp
TEST(SectionLoader, PlaceholderCompactAndFullShareMeasuredContentHeight)
{
	CSectionLoader Loader;
	Loader.SetProgressiveEnabled(true);
	Loader.SetMaxSectionsPerFrame(1);
	int MeasureCalls = 0;
	int CompactCalls = 0;
	int FullCalls = 0;
	auto MakeSection = [&]() {
		SSettingsSection Section{};
		Section.m_pName = "Canonical card";
		Section.m_pStableCardId = "tclient:canonical-card";
		Section.m_MeasureContentFn = [&](float ContentWidth) {
			++MeasureCalls;
			return ContentWidth > 0.0f ? 128.0f : 0.0f;
		};
		Section.m_RenderCompactContentFn = [&](const CUIRect &ContentRect) {
			++CompactCalls;
			return ContentRect.h;
		};
		Section.m_RenderFullContentFn = [&](const CUIRect &ContentRect) {
			++FullCalls;
			return ContentRect.h;
		};
		return Section;
	};
	const int aExpectedCompactCalls[3] = {0, 1, 1};
	const int aExpectedFullCalls[3] = {0, 0, 1};
	float aMeasuredContentHeights[3] = {};
	float aRenderedContentHeights[3] = {};
	float aFrameHeights[3] = {};
	for(int Frame = 0; Frame < 3; ++Frame)
	{
		// 每帧用同一 stable ID 重新注册，只刷新 frame-local callback，保留渐进状态与测量缓存。
		Loader.Register({MakeSection()});
		Loader.BeginCardFrame({0.0f, 0.0f, 400.0f, 600.0f}, 100.0f);
		const float MeasuredContentHeight = Loader.MeasureCardContent("tclient:canonical-card", 372.0f);
		const SSettingsCardFrame CardFrame = BuildSettingsCardFrame({0.0f, 0.0f, 400.0f, 0.0f}, {"tclient:canonical-card", "Canonical card", nullptr}, MeasuredContentHeight, 1.0f);
		const float RenderedContentHeight = Loader.RenderCardContent("tclient:canonical-card", CardFrame.m_ContentRect);
		Loader.EndCardFrame();
		aMeasuredContentHeights[Frame] = MeasuredContentHeight;
		aRenderedContentHeights[Frame] = RenderedContentHeight;
		aFrameHeights[Frame] = CardFrame.m_Rect.h;
		EXPECT_EQ(MeasureCalls, 1) << Frame;
		EXPECT_EQ(CompactCalls, aExpectedCompactCalls[Frame]) << Frame;
		EXPECT_EQ(FullCalls, aExpectedFullCalls[Frame]) << Frame;
		EXPECT_FLOAT_EQ(RenderedContentHeight, MeasuredContentHeight);
	}
	EXPECT_FLOAT_EQ(aMeasuredContentHeights[0], 128.0f);
	EXPECT_FLOAT_EQ(aMeasuredContentHeights[0], aMeasuredContentHeights[1]);
	EXPECT_FLOAT_EQ(aMeasuredContentHeights[1], aMeasuredContentHeights[2]);
	EXPECT_FLOAT_EQ(aRenderedContentHeights[0], aMeasuredContentHeights[0]);
	EXPECT_FLOAT_EQ(aRenderedContentHeights[1], aMeasuredContentHeights[1]);
	EXPECT_FLOAT_EQ(aRenderedContentHeights[2], aMeasuredContentHeights[2]);
	EXPECT_FLOAT_EQ(aFrameHeights[0], aFrameHeights[1]);
	EXPECT_FLOAT_EQ(aFrameHeights[1], aFrameHeights[2]);
}

TEST(QmNewUiMenuBranches, P6TClientSettingsDeletesCacheBoxAndPrivateHeightPaths)
{
	const std::string Source = ReadTextFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Body = FunctionBody(Source, "void CMenus::RenderSettingsTClientSettings(CUIRect MainView, bool PrewarmOnly)");
	ASSERT_FALSE(Body.empty());
	EXPECT_NE(Body.find("ResolveSettingsPageLayout("), std::string::npos);
	EXPECT_NE(Body.find("SSettingsCardDefinition"), std::string::npos);
	EXPECT_NE(Body.find("m_SettingsCardDeck.Render("), std::string::npos);
	EXPECT_NE(Body.find(".State()"), std::string::npos);
	EXPECT_NE(Body.find("QmResolveScrollPolicy("), std::string::npos);
	for(const char *pForbidden : {"TClientCacheSectionBoxRect", "InsetTClientCacheSectionContent", "DrawTClientCacheSectionBox", "RenderBoxedFullSection", "FillCachedStaticLayer", "CachedHeight", "CQmScrollContainer"})
		EXPECT_EQ(Body.find(pForbidden), std::string::npos) << pForbidden;
}
```

Replace the old `RenderBoxedFullSection`/`FillCachedStaticLayer` assertions in `QmMonitoringHelpers.TClientSectionMeasuredHeightMatchesRenderedHeight` with the Deck-owned frame telemetry contract:

```cpp
EXPECT_NE(Body.find("section_height_measured=%.3f section_height_rendered=%.3f height_delta=%.3f stable=%d"), std::string::npos);
EXPECT_NE(Body.find("absolute(HeightDelta) <= 0.01f"), std::string::npos);
EXPECT_NE(Body.find("SSettingsCardDefinition"), std::string::npos);
EXPECT_NE(Body.find("DeckResult.m_vFrames"), std::string::npos);
EXPECT_NE(Body.find("MeasuredFrameHeight = DeckResult.m_vFrames[CardIndex].m_Rect.h"), std::string::npos);
EXPECT_NE(Body.find("RenderedFrameHeight = MeasuredFrameHeight + (RenderedContentHeight - MeasuredContentHeight)"), std::string::npos);
EXPECT_EQ(Body.find("RenderBoxedFullSection"), std::string::npos);
EXPECT_EQ(Body.find("FillCachedStaticLayer"), std::string::npos);
EXPECT_EQ(Header.find("m_TClientSettingsCardDragState"), std::string::npos);
```

- [ ] **Step 2: Rebuild testrunner and verify red**

Run:

```powershell
cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=SectionLoader.PlaceholderCompactAndFullShareMeasuredContentHeight:QmNewUiMenuBranches.P6TClientSettingsDeletesCacheBoxAndPrivateHeightPaths:QmMonitoringHelpers.TClientSectionMeasuredHeightMatchesRenderedHeight
```

Expected: compile FAIL on the new scheduling-only SectionLoader API and production deletion test; this is the intentional red state before the loader and TClient page are migrated together.

- [ ] **Step 3: Make loader content height the only layout input**

Use `ResolveSettingsPageLayout(MainView, false, UiScale)` for columns and one P4 `CScrollRegion` whose `State()` is the only `CQmScrollState`. Keep the 19 registered `tclient:*` IDs, build `SSettingsCardDefinition` values and pass them with `SettingsCardOrderModel()` to `m_SettingsCardDeck.Render(...)`; do not recreate left/right order vectors.

First narrow `CSectionLoader` to this exact content-only production API. `SSettingsSection` keeps persistent measurement/render state, while callbacks are refreshed by `Register(...)` every frame. Replace the loader's old column-owned state with the complete member set below; keep the existing warmup、invalidation、session-cache、setter and perf public entry points, but update them to the renamed content fields and remove `Begin(CUIRect, ...)`、`Process()`、`GetRunningColumn()`、`m_MainView`、`m_RunningColumn`、`m_CurrentIndex` and `m_Initialized`:

```cpp
struct SSettingsSection
{
	const char *m_pName = nullptr;
	const char *m_pStableCardId = nullptr;
	ESettingsSectionState m_State = ESettingsSectionState::UNINITIALIZED;
	float m_CachedContentHeight = 0.0f;
	float m_LastMeasuredContentWidth = -1.0f;
	bool m_HasCachedContentHeight = false;
	SSettingsSectionCacheRuntimeKey m_MeasuredRuntimeKey;
	bool m_HasMeasuredRuntimeKey = false;
	std::function<float(float ContentWidth)> m_MeasureContentFn;
	std::function<float(const CUIRect &ContentRect)> m_RenderCompactContentFn;
	std::function<float(const CUIRect &ContentRect)> m_RenderFullContentFn;
	std::vector<const int *> m_DependencyConfigInts;
	std::vector<const unsigned *> m_DependencyConfigCols;
	uint64_t m_LastConfigHash = 0;
	bool m_Dirty = true;
};

class CSectionLoader
{
public:
	CSectionLoader();
	~CSectionLoader();
	void Register(std::vector<SSettingsSection> vSections);
	void BeginCardFrame(const CUIRect &Viewport, float TimeBudgetMs = 5.0f);
	float MeasureCardContent(const char *pStableId, float ContentWidth);
	float RenderCardContent(const char *pStableId, const CUIRect &ContentRect);
	void EndCardFrame();
	bool IsComplete() const;
	void Reset();
	bool Warmup(const SSessionUiCache *pCache, float TimeBudgetMs = 3.0f);
	bool IsWarmupComplete() const;
	void InvalidateCache(ESettingsCacheDirtyReason Reason = ESettingsCacheDirtyReason::CONFIG);
	void SetDirtyByConfig(const void *pConfigVar);
	static bool LoadSessionCache(SSessionUiCache &Cache, const char *pFilename, class IStorage *pStorage);
	static void SaveSessionCache(const SSessionUiCache &Cache, const char *pFilename, class IStorage *pStorage);
	static bool IsVisibleSummarySectionName(const char *pName);
	void SetRuntimeKey(const SSettingsSectionCacheRuntimeKey &RuntimeKey);
	void SetProgressiveEnabled(bool Enabled);
	void SetMaxSectionsPerFrame(int MaxSectionsPerFrame);
	int m_ActiveTab = -1;
	const char *GetPerfReport() const;
	const SSectionLoaderFrameStats &LastFrameStats() const { return m_LastFrameStats; }

private:
	std::vector<SSettingsSection> m_vSections;
	CUIRect m_Viewport;
	double m_BudgetPerFrameMs = 5.0;
	int64_t m_FrameStartTime = 0;
	bool m_FrameOpen = false;
	int m_FullSectionsUnlockedThisFrame = 0;
	bool m_Complete = false;
	bool m_ProgressiveEnabled = false;
	int m_MaxSectionsPerFrame = 2;
	SSettingsSectionCacheRuntimeKey m_RuntimeKey;

	bool m_WarmupActive = false;
	int m_WarmupIndex = 0;
	float m_WarmupBudgetMs = 0.0f;
	const SSessionUiCache *m_pWarmupCache = nullptr;

	double m_TotalFrameTimeMs = 0.0;
	SSectionLoaderFrameStats m_LastFrameStats;
	ESettingsCacheDirtyReason m_LastDirtyReason = ESettingsCacheDirtyReason::NONE;

	SSettingsSection *FindSection(const char *pStableId);
	bool IsInViewport(const CUIRect &ContentRect) const;
	void ClearFrameCallbacks();
	static uint64_t ComputeConfigHash(const SSettingsSection &Section);
};

void CSectionLoader::Register(std::vector<SSettingsSection> vSections)
{
	for(SSettingsSection &NewSection : vSections)
	{
		if(NewSection.m_pStableCardId == nullptr)
			continue;
		for(const SSettingsSection &OldSection : m_vSections)
		{
			if(OldSection.m_pStableCardId == nullptr || str_comp(NewSection.m_pStableCardId, OldSection.m_pStableCardId) != 0)
				continue;
			NewSection.m_State = OldSection.m_State;
			NewSection.m_CachedContentHeight = OldSection.m_CachedContentHeight;
			NewSection.m_LastMeasuredContentWidth = OldSection.m_LastMeasuredContentWidth;
			NewSection.m_HasCachedContentHeight = OldSection.m_HasCachedContentHeight;
			NewSection.m_MeasuredRuntimeKey = OldSection.m_MeasuredRuntimeKey;
			NewSection.m_HasMeasuredRuntimeKey = OldSection.m_HasMeasuredRuntimeKey;
			NewSection.m_LastConfigHash = OldSection.m_LastConfigHash;
			NewSection.m_Dirty = OldSection.m_Dirty || ComputeConfigHash(NewSection) != OldSection.m_LastConfigHash;
			break;
		}
	}
	m_vSections = std::move(vSections);
}

SSettingsSection *CSectionLoader::FindSection(const char *pStableId)
{
	if(pStableId == nullptr)
		return nullptr;
	for(SSettingsSection &Section : m_vSections)
	{
		if(Section.m_pStableCardId != nullptr && str_comp(Section.m_pStableCardId, pStableId) == 0)
			return &Section;
	}
	return nullptr;
}

bool CSectionLoader::IsInViewport(const CUIRect &ContentRect) const
{
	return ContentRect.x + ContentRect.w >= m_Viewport.x &&
		ContentRect.x <= m_Viewport.x + m_Viewport.w &&
		ContentRect.y + ContentRect.h >= m_Viewport.y &&
		ContentRect.y <= m_Viewport.y + m_Viewport.h;
}

void CSectionLoader::ClearFrameCallbacks()
{
	for(SSettingsSection &Section : m_vSections)
	{
		Section.m_MeasureContentFn = nullptr;
		Section.m_RenderCompactContentFn = nullptr;
		Section.m_RenderFullContentFn = nullptr;
	}
}

uint64_t CSectionLoader::ComputeConfigHash(const SSettingsSection &Section)
{
	uint64_t Hash = 14695981039346656037ull;
	for(const int *pValue : Section.m_DependencyConfigInts)
	{
		const uint8_t *pBytes = reinterpret_cast<const uint8_t *>(pValue);
		for(size_t i = 0; i < sizeof(*pValue); ++i)
		{
			Hash ^= pBytes[i];
			Hash *= 1099511628211ull;
		}
	}
	for(const unsigned *pValue : Section.m_DependencyConfigCols)
	{
		const uint8_t *pBytes = reinterpret_cast<const uint8_t *>(pValue);
		for(size_t i = 0; i < sizeof(*pValue); ++i)
		{
			Hash ^= pBytes[i];
			Hash *= 1099511628211ull;
		}
	}
	return Hash;
}

void CSectionLoader::BeginCardFrame(const CUIRect &Viewport, float TimeBudgetMs)
{
	m_Viewport = Viewport;
	m_BudgetPerFrameMs = maximum(0.0f, TimeBudgetMs);
	m_FrameStartTime = time_get();
	m_FrameOpen = true;
	m_FullSectionsUnlockedThisFrame = 0;
	m_Complete = false;
	m_TotalFrameTimeMs = 0.0;
	m_LastFrameStats = {};
	m_LastFrameStats.m_SectionsTotal = (int)m_vSections.size();
}

float CSectionLoader::MeasureCardContent(const char *pStableId, float ContentWidth)
{
	SSettingsSection *pSection = FindSection(pStableId);
	if(pSection == nullptr)
		return 0.0f;

	const float ClampedContentWidth = maximum(0.0f, ContentWidth);
	const uint64_t ConfigHash = ComputeConfigHash(*pSection);
	const bool WidthChanged = absolute(pSection->m_LastMeasuredContentWidth - ClampedContentWidth) > 0.01f;
	const bool RuntimeKeyChanged = !pSection->m_HasMeasuredRuntimeKey || !(pSection->m_MeasuredRuntimeKey == m_RuntimeKey);
	const bool ConfigChanged = ConfigHash != pSection->m_LastConfigHash;
	const bool MustMeasure = !pSection->m_HasCachedContentHeight || pSection->m_Dirty || WidthChanged || RuntimeKeyChanged || ConfigChanged;
	if(MustMeasure)
	{
		const float MeasuredContentHeight = pSection->m_MeasureContentFn ? pSection->m_MeasureContentFn(ClampedContentWidth) : 0.0f;
		pSection->m_CachedContentHeight = maximum(0.0f, MeasuredContentHeight);
		pSection->m_LastMeasuredContentWidth = ClampedContentWidth;
		pSection->m_HasCachedContentHeight = true;
		pSection->m_MeasuredRuntimeKey = m_RuntimeKey;
		pSection->m_HasMeasuredRuntimeKey = true;
		pSection->m_LastConfigHash = ConfigHash;
		pSection->m_Dirty = false;
		++m_LastFrameStats.m_LayoutDirtySections;
		m_LastFrameStats.m_DirtyReason = m_LastDirtyReason;
	}
	return pSection->m_CachedContentHeight;
}

float CSectionLoader::RenderCardContent(const char *pStableId, const CUIRect &ContentRect)
{
	SSettingsSection *pSection = FindSection(pStableId);
	if(pSection == nullptr)
		return 0.0f;
	const float MeasuredContentHeight = MeasureCardContent(pStableId, ContentRect.w);
	if(!m_FrameOpen)
		return MeasuredContentHeight;
	if(!IsInViewport(ContentRect))
	{
		++m_LastFrameStats.m_SectionsSkipped;
		return MeasuredContentHeight;
	}
	++m_LastFrameStats.m_SectionsVisible;

	const auto RenderForTelemetry = [&](const std::function<float(const CUIRect &)> &RenderFn) {
		return RenderFn ? maximum(0.0f, RenderFn(ContentRect)) : MeasuredContentHeight;
	};
	if(!m_ProgressiveEnabled)
	{
		pSection->m_State = ESettingsSectionState::FULL;
		return RenderForTelemetry(pSection->m_RenderFullContentFn);
	}

	switch(pSection->m_State)
	{
	case ESettingsSectionState::UNINITIALIZED:
		pSection->m_State = ESettingsSectionState::MEASURING;
		return MeasuredContentHeight;
	case ESettingsSectionState::MEASURING:
		pSection->m_State = ESettingsSectionState::COMPACT;
		return RenderForTelemetry(pSection->m_RenderCompactContentFn);
	case ESettingsSectionState::COMPACT:
	{
		const double ElapsedMs = (double)(time_get() - m_FrameStartTime) * 1000.0 / (double)time_freq();
		if(ElapsedMs < m_BudgetPerFrameMs && m_FullSectionsUnlockedThisFrame < m_MaxSectionsPerFrame)
		{
			++m_FullSectionsUnlockedThisFrame;
			pSection->m_State = ESettingsSectionState::FULL;
			return RenderForTelemetry(pSection->m_RenderFullContentFn);
		}
		return RenderForTelemetry(pSection->m_RenderCompactContentFn);
	}
	case ESettingsSectionState::FULL:
		return RenderForTelemetry(pSection->m_RenderFullContentFn);
	}
	return MeasuredContentHeight;
}

void CSectionLoader::EndCardFrame()
{
	if(!m_FrameOpen)
		return;
	m_Complete = std::all_of(m_vSections.begin(), m_vSections.end(), [](const SSettingsSection &Section) {
		return Section.m_State == ESettingsSectionState::FULL;
	});
	m_TotalFrameTimeMs = (double)(time_get() - m_FrameStartTime) * 1000.0 / (double)time_freq();
	ClearFrameCallbacks();
	m_FrameOpen = false;
	m_LastDirtyReason = ESettingsCacheDirtyReason::NONE;
}
```

`MeasureCardContent(...)` is the only measurement owner and never advances `m_State`; a dirty、width、runtime-key、dependency-hash or missing-cache condition synchronously remeasures before Deck layout. `RenderCardContent(...)` never writes callback results back to `m_CachedContentHeight`: placeholder returns the measured cache without drawing, compact/full return actual callback height for telemetry only, and an offscreen section returns the cache without invoking either render callback. `EndCardFrame()` computes completion and elapsed stats, then clears all frame-local callbacks before closing the frame. `Warmup(...)` uses `MeasureCardContent(...)` with the remembered content width, invokes the same compact callback against an offscreen `CUIRect` whose `h` is the measured content height, and likewise must not replace the measured cache with the callback result. Update `Reset()`、`InvalidateCache(...)` and existing `section_loader_test.cpp` cases to the renamed fields while preserving state、budget、invalidation、visibility、warmup、session metadata and perf assertions. Rename `SectionLoader.MeasureFullHeightConsistency` to `SectionLoader.MeasureAndFullShareContentHeight` and make it compare the full callback's telemetry return against `m_CachedContentHeight`, never against or into a card-frame cache. A valid far-card measurement is reused unconditionally; delete `SetDeferredFarMeasurementEnabled(...)` and its flag because invalid measurement is never deferred and visibility only controls drawing.

For every `SSettingsSection`, share one content-height function between measurement and rendering. The callbacks must have this semantic shape:

```cpp
SSettingsSection Section{};
Section.m_pName = pSectionName;
Section.m_pStableCardId = pStableId;
Section.m_MeasureContentFn = [&](float ContentWidth) { return LayoutSectionContent(ContentWidth, nullptr); };
Section.m_RenderCompactContentFn = [&](const CUIRect &ContentRect) { return LayoutSectionContent(ContentRect.w, nullptr); };
Section.m_RenderFullContentFn = [&](const CUIRect &ContentRect) { return LayoutSectionContent(ContentRect.w, &ContentRect); };

SSettingsCardDefinition Definition;
Definition.m_Spec = {pStableId, pTitle, pSubtitle};
Definition.m_Measure = [&](float ContentWidth) { return Loader.MeasureCardContent(pStableId, ContentWidth); };
Definition.m_Render = [&](CUIRect ContentRect) {
	RenderedContentHeight = Loader.RenderCardContent(pStableId, ContentRect);
};
```

The definition measure returns content height only. Deck adds header/padding exactly once through P1 `SettingsCard(...)`, and Deck alone adds inter-card gap. Compute telemetry after `DeckResult` is available, without feeding `RenderedContentHeight` back into any frame or layout input:

```cpp
const float MeasuredFrameHeight = DeckResult.m_vFrames[CardIndex].m_Rect.h;
const float RenderedFrameHeight = MeasuredFrameHeight + (RenderedContentHeight - MeasuredContentHeight);
const float HeightDelta = RenderedFrameHeight - MeasuredFrameHeight;
const bool Stable = absolute(HeightDelta) <= 0.01f;
```

Emit `tclient_settings_section_height` with section stable ID, these measured/rendered frame heights, `HeightDelta` and `Stable`; `Stable == false` is a behavior failure, not a performance warning. Keep invalidation、prewarm and deferred-far drawing behavior; a config/language/width change invalidates measurement before Deck layout.

Delete definitions/declarations and callers of `TClientCacheSectionBoxRect`, `InsetTClientCacheSectionContent`, `DrawTClientCacheSectionBox`, `RenderTClientCacheSectionFallback`, `m_TClientSettingsCardDragState`, old private six-dot/drop/order/cache wrappers and all page-static `s_*SectionCachedHeight`. P6 adds the deletion assertions after replacing these paths with the public Deck.

- [ ] **Step 4: Run focused green checks**

Run:

```powershell
cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=SectionLoader.PlaceholderCompactAndFullShareMeasuredContentHeight:SectionLoader.MeasureAndFullShareContentHeight:QmNewUiMenuBranches.P6TClientSettingsDeletesCacheBoxAndPrivateHeightPaths:QmMonitoringHelpers.TClientSectionMeasuredHeightMatchesRenderedHeight:QmCardRegistry.CoversCurrentTClientSectionIds
git diff --check
```

Expected: all tests PASS; callback counts prove placeholder → compact → full, each path reports the measured content height, all three Deck frame heights match, no cache/private-height symbol remains, and whitespace is clean.

**Visual and telemetry matrix:**

| Page | View/UI scale/language | Operation | Expected |
|---|---|---|---|
| Settings | 1280×720 / 100% / English | cold-open, wait for progressive load, drag across columns | no double card/transparent vertical block/text escape; placeholder never jumps when FULL replaces it |
| Settings | 960×720 / 125% / Simplified Chinese | cold-open mid-scroll, change a height-affecting option, reopen | one-column fallback; scroll content height changes once and remains stable; no overlap |
| Settings telemetry | both cases with `qm_perf_debug 1` | capture `perf/tclient` lines | every visible section logs measured/rendered/delta; `stable=1` and `abs(height_delta) <= 0.01` |

- [ ] **Step 5: Commit TClient Settings slice**

```powershell
git add src/game/client/components/tclient/menus_tclient.cpp src/game/client/components/menus.h src/game/client/components/section_loader.h src/game/client/components/section_loader.cpp src/test/section_loader_test.cpp src/test/qm_new_ui_menu_branch_test.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "fix(settings-ui): 统一 TClient 内容高度契约" -m "fix: 让 placeholder、compact 与 full 共用 measured content height" -m "refactor: 由 Deck 独占 card frame、cache box、私有 inset 与 cached-height 双路径" -m "test: 覆盖渐进状态推进、Deck 高度和 telemetry contract"
```

### Task 4: 迁移 BindWheel 与 StatusBar 的 shell/content/scroll

**Files:**
- Modify: `src/game/client/components/tclient/menus_tclient.cpp:3734-3919,4572-4933`
- Test: `src/test/qm_card_registry_test.cpp`
- Test: `src/test/qm_new_ui_menu_branch_test.cpp`

**Interfaces:**
- Consumes: existing IDs `deck:tclient-bind-wheel-editor`、`deck:tclient-bind-wheel-preview`、`deck:tclient-status-bar-settings`、`deck:tclient-status-bar-items`、`deck:tclient-status-bar-preview`; P2 `CSettingsCardDeck`; P3 `InputField`/`NumericField`; P4 scroll state/policy。
- Produces: same card order and user behavior with canonical frames; no legacy deck-card shell, `TextField`, `DoSettingsScrollbarOption` or page-local scroll state。

- [ ] **Step 1: Write failing migration tests**

```cpp
TEST(QmNewUiMenuBranches, P6BindWheelAndStatusBarKeepDeckButUseNewShellInputAndScroll)
{
	const std::string Source = ReadTextFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string BindWheel = FunctionBody(Source, "void CMenus::RenderSettingsTClientBindWheel(CUIRect MainView)");
	const std::string StatusBar = FunctionBody(Source, "void CMenus::RenderSettingsTClientStatusBar(CUIRect MainView)");
	for(const std::string *pBody : {&BindWheel, &StatusBar})
	{
		ASSERT_FALSE(pBody->empty());
		EXPECT_NE(pBody->find("ResolveSettingsPageLayout("), std::string::npos);
		EXPECT_NE(pBody->find("SSettingsCardDefinition"), std::string::npos);
		EXPECT_NE(pBody->find("m_SettingsCardDeck.Render("), std::string::npos);
		EXPECT_NE(pBody->find(".State()"), std::string::npos);
		EXPECT_NE(pBody->find("QmResolveScrollPolicy("), std::string::npos);
		EXPECT_EQ(pBody->find("BeginSettingsCardDeck("), std::string::npos);
		EXPECT_EQ(pBody->find("BeginSettingsCardDeckCard("), std::string::npos);
		EXPECT_EQ(pBody->find("CQmScrollContainer"), std::string::npos);
		EXPECT_EQ(pBody->find("ui_widget::TextField("), std::string::npos);
		EXPECT_EQ(pBody->find("DoSettingsScrollbarOption("), std::string::npos);
	}
	EXPECT_NE(BindWheel.find("ui_widget::InputField("), std::string::npos);
	EXPECT_NE(StatusBar.find("ui_widget::InputField("), std::string::npos);
	EXPECT_NE(StatusBar.find("ui_widget::NumericField("), std::string::npos);
}
```

- [ ] **Step 2: Rebuild testrunner and verify red**

Run:

```powershell
cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmNewUiMenuBranches.P6BindWheelAndStatusBarKeepDeckButUseNewShellInputAndScroll:QmCardRegistry.SettingsDeckDefaultOrdersAreLocalToTabAndColumn
```

Expected: production migration test FAILS on legacy shell/scroll anchors or any regressed P3 input token; registry order test remains PASS.

- [ ] **Step 3: Migrate only shell/content/scroll and preserve P3 input**

Build P2 `SSettingsCardDefinition` values with the existing IDs and registry placement, then call the existing `m_SettingsCardDeck.Render(...)`; do not add IDs or change order. The deck replaces each old shell through its internal `SettingsCard(...)` call and returns canonical frames. Remove `s_*CardHeight` and manual bottom-to-card-height calculations; editor/preview drawing remains inside each definition's provided content rect.

BindWheel name/command already use `InputField`; move those calls unchanged and preserve add/remove/override, mouse segment selection and key-reader behavior. StatusBar scheme already uses `InputField`, while height/alpha/text-alpha use `NumericField`; preserve ranges and commit timing. Both pages use one `CScrollRegion` adapter and its sole `CQmScrollState` plus settings policy; dropdown child scroll remains P4-owned and must not leak its first wheel to the page.

- [ ] **Step 4: Run focused green checks**

Run:

```powershell
cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmNewUiMenuBranches.P6BindWheelAndStatusBarKeepDeckButUseNewShellInputAndScroll:QmCardRegistry.SettingsDeckDefaultOrdersAreLocalToTabAndColumn:InputField.*:NumericField.*:UiV2ScrollState.*:UiV2ScrollPolicy.*:UiV2DropdownIntegration.*
git diff --check
```

Expected: all named suites PASS and the registry test proves no order/model regression.

**Visual matrix:**

| Subpage | View/UI scale/language | Operation | Expected |
|---|---|---|---|
| BindWheel | 1280×720 / 100% / English | edit name/command, add, override, delete, scroll | two canonical cards; IME/cursor/focus are correct; preview remains circular and unclipped |
| BindWheel | 960×720 / 125% / Simplified Chinese | select/swap wheel entries, focus field, Alt+wheel | cards stack without overlap; preview and editor hit regions remain aligned; page scroll is 3× with Alt |
| StatusBar | 1280×720 / 100% / English | edit scheme, height and alpha, reorder cards | three cards keep stable IDs/order; numeric field shows slider+input; preview updates in the same frame |
| StatusBar | 960×720 / 125% / Simplified Chinese | open dropdown then wheel, close and page-wheel | first popup wheel never moves page; no duplicate title, square slider track or clipped preview |

- [ ] **Step 5: Commit BindWheel/StatusBar slice**

```powershell
git add src/game/client/components/tclient/menus_tclient.cpp src/test/qm_card_registry_test.cpp src/test/qm_new_ui_menu_branch_test.cpp
git commit -m "refactor(settings-ui): 迁移绑定轮盘与状态栏" -m "refactor: 保留 shared deck 顺序并统一卡片、输入与滚动" -m "test: 禁止两个子页回退旧 shell 与控件"
```

### Task 5: 迁移 ChatBinds 与 WarList 复杂子页

**Files:**
- Modify: `src/game/client/components/tclient/menus_tclient.cpp:3921-4571`
- Modify: `src/game/client/QmUi/QmCardRegistry.cpp`
- Test: `src/test/qm_card_registry_test.cpp`
- Test: `src/test/qm_new_ui_menu_branch_test.cpp`

**Interfaces:**
- Consumes: canonical page/card/deck/input/scroll contracts and existing ChatBind/WarList models。
- Produces: three ChatBinds cards and four WarList cards with exact stable IDs; no cache box, direct search/text alias or page-private scroll。

- [ ] **Step 1: Write failing stable-ID and production tests**

Add these registry defaults exactly:

```cpp
{"deck:tclient-chat-binds-kaomoji", "tclient-chat-binds", ECardColumn::Left, 0, "Kaomoji", "chat binds kaomoji"},
{"deck:tclient-chat-binds-warlist", "tclient-chat-binds", ECardColumn::Right, 0, "Warlist", "chat binds warlist"},
{"deck:tclient-chat-binds-other", "tclient-chat-binds", ECardColumn::Left, 1, "Other", "chat binds other"},
{"deck:tclient-war-list-entries", "tclient-war-list", ECardColumn::Left, 0, "War Entries", "war list entries"},
{"deck:tclient-war-list-editor", "tclient-war-list", ECardColumn::Right, 0, "Edit", "war list entry editing group editing"},
{"deck:tclient-war-list-groups", "tclient-war-list", ECardColumn::Left, 1, "Warlist", "war list group types"},
{"deck:tclient-war-list-recent", "tclient-war-list", ECardColumn::Right, 1, "Players", "war list recent players"},
```

```cpp
TEST(QmCardRegistry, P6RegistersChatBindsAndWarListCards)
{
	const char *apChatIds[] = {
		"deck:tclient-chat-binds-kaomoji",
		"deck:tclient-chat-binds-warlist",
		"deck:tclient-chat-binds-other",
	};
	for(const char *pId : apChatIds)
	{
		const auto *pCard = qm_card_registry::FindByStableId(pId);
		ASSERT_NE(pCard, nullptr) << pId;
		EXPECT_STREQ(pCard->m_pDefaultTab, "tclient-chat-binds") << pId;
	}
	const char *apWarIds[] = {
		"deck:tclient-war-list-entries",
		"deck:tclient-war-list-editor",
		"deck:tclient-war-list-groups",
		"deck:tclient-war-list-recent",
	};
	for(const char *pId : apWarIds)
	{
		const auto *pCard = qm_card_registry::FindByStableId(pId);
		ASSERT_NE(pCard, nullptr) << pId;
		EXPECT_STREQ(pCard->m_pDefaultTab, "tclient-war-list") << pId;
	}
}
```

```cpp
TEST(QmNewUiMenuBranches, P6ChatBindsAndWarListUseCanonicalPlatform)
{
	const std::string Source = ReadTextFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Chat = FunctionBody(Source, "void CMenus::RenderSettingsTClientChatBinds(CUIRect MainView)");
	const std::string War = FunctionBody(Source, "void CMenus::RenderSettingsTClientWarList(CUIRect MainView)");
	for(const std::string *pBody : {&Chat, &War})
	{
		ASSERT_FALSE(pBody->empty());
		EXPECT_NE(pBody->find("ResolveSettingsPageLayout("), std::string::npos);
		EXPECT_NE(pBody->find("SSettingsCardDefinition"), std::string::npos);
		EXPECT_NE(pBody->find("m_SettingsCardDeck.Render("), std::string::npos);
		EXPECT_NE(pBody->find("ui_widget::InputField("), std::string::npos);
		EXPECT_NE(pBody->find(".State()"), std::string::npos);
		EXPECT_NE(pBody->find("QmResolveScrollPolicy("), std::string::npos);
		for(const char *pForbidden : {"TClientCacheSectionBoxRect", "InsetTClientCacheSectionContent", "DrawTClientCacheSectionBox", "CQmScrollContainer", "ui_widget::TextField(", "ui_widget::SearchField("})
			EXPECT_EQ(pBody->find(pForbidden), std::string::npos) << pForbidden;
	}
}
```

- [ ] **Step 2: Rebuild testrunner and verify red**

Run:

```powershell
cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmNewUiMenuBranches.P6ChatBindsAndWarListUseCanonicalPlatform:QmCardRegistry.P6RegistersChatBindsAndWarListCards
```

Expected: both tests FAIL before the new stable defaults and production migration exist.

- [ ] **Step 3: Migrate logical groups without changing data behavior**

ChatBinds maps the existing `Kaomoji`/`Warlist`/`Other` groups one-to-one to the three registered cards; keep `CBindChat::BIND_DEFAULTS` as content truth and use `InputField` for each bind name. WarList maps existing entries/editor+group-editor/group-types/recent-player regions to four registered cards; keep selection pointers, reverse/filter, add/edit/delete and revision invalidation behavior.

Both pages use `ResolveSettingsPageLayout`, `SSettingsCardDefinition`/`m_SettingsCardDeck.Render(...)`, one `CScrollRegion::State()` and the settings policy. Search/plain text fields use `InputField`, card contents draw only in the canonical content rect passed to the definition, and narrow mode stacks cards by deck reading order. Delete ChatBinds cache-box culling and both pages' manual left/right/four-column rect math where page/deck already owns it.

- [ ] **Step 4: Run focused green checks**

Run:

```powershell
cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmNewUiMenuBranches.P6ChatBindsAndWarListUseCanonicalPlatform:QmCardRegistry.P6RegistersChatBindsAndWarListCards:QmCardRegistry.CoversAllCardsNoDuplicates:InputField.*:UiV2ScrollState.*:UiV2ScrollPolicy.*
git diff --check
```

Expected: all named tests PASS; registry uniqueness remains PASS; no whitespace errors.

**Visual matrix:**

| Subpage | View/UI scale/language | Operation | Expected |
|---|---|---|---|
| ChatBinds | 1280×720 / 100% / English | edit each group, wheel, drag one card across columns | exactly three single-layer cards; names save as before; no first-wheel leak or text escape |
| ChatBinds | 960×720 / 125% / Simplified Chinese | IME edit longest label, clear, scroll bottom | one-column order is deterministic; focus ring/content inset stay aligned; last row is reachable |
| WarList | 1280×720 / 100% / English | filter/reverse/select/edit/group/recent actions | four cards retain selection and mutations; list rows do not draw outside card/clip |
| WarList | 960×720 / 125% / Simplified Chinese | resize with selections active, drag and Alt+wheel | no stale pointer display, overlap or four-column squeeze; cards reflow and wheel is 3× |

- [ ] **Step 5: Commit ChatBinds/WarList slice**

```powershell
git add src/game/client/components/tclient/menus_tclient.cpp src/game/client/QmUi/QmCardRegistry.cpp src/test/qm_card_registry_test.cpp src/test/qm_new_ui_menu_branch_test.cpp
git commit -m "refactor(settings-ui): 迁移聊天绑定与战争名单" -m "refactor: 注册复杂子页卡片并统一输入、布局与滚动" -m "test: 覆盖 stable ID 和旧 cache 路径删除"
```

### Task 6: 迁移 Info/Profiles/Configs 并完成 P6 总验收

**Files:**
- Modify: `src/game/client/components/tclient/menus_tclient.cpp:4936-5800`
- Modify: `src/game/client/components/qmclient/menus_qmclient.cpp:1232-1245`
- Modify: `src/game/client/QmUi/QmCardRegistry.cpp`
- Modify: `src/game/client/components/menus.h` only if P6 cleanup leaves unused declarations
- Test: `src/test/qm_card_registry_test.cpp`
- Test: `src/test/qm_new_ui_menu_branch_test.cpp`
- Test: `src/test/qmclient_monitoring_test.cpp`
- Test: `src/test/section_loader_test.cpp`

**Interfaces:**
- Consumes: all P1–P4 shared contracts, existing profile/config models and Task 1–5 stable placements。
- Produces: Info four-card layout、Profiles three-card layout、Qm Config one full browser card；P6-wide structural deletion evidence, serial automated evidence, manual matrix and returned independent review report。

- [ ] **Step 1: Write failing final-slice and all-pages tests**

Add these defaults exactly:

```cpp
{"deck:tclient-info-links", "tclient-info", ECardColumn::Left, 0, "TClient Links", "tclient links"},
{"deck:tclient-info-files", "tclient-info", ECardColumn::Left, 1, "Config Files", "tclient config files"},
{"deck:tclient-info-developers", "tclient-info", ECardColumn::Right, 0, "TClient Developers", "tclient developers"},
{"deck:tclient-info-hidden-tabs", "tclient-info", ECardColumn::Right, 1, "Hide Settings Tabs", "tclient hide settings tabs"},
{"deck:tclient-profiles-preview", "tclient-profiles", ECardColumn::Left, 0, "Your profile", "profile preview after load"},
{"deck:tclient-profiles-options", "tclient-profiles", ECardColumn::Right, 0, "Profiles", "profile save load options"},
{"deck:tclient-profiles-list", "tclient-profiles", ECardColumn::Full, 0, "Profiles", "profile list apply delete"},
{"deck:qmclient-config-browser", "qmclient-config", ECardColumn::Full, 0, "Config", "config browser search tags changes"},
```

```cpp
TEST(QmCardRegistry, P6RegistersInfoProfilesAndConfigCards)
{
	const char *aExpected[][2] = {
		{"deck:tclient-info-links", "tclient-info"},
		{"deck:tclient-info-files", "tclient-info"},
		{"deck:tclient-info-developers", "tclient-info"},
		{"deck:tclient-info-hidden-tabs", "tclient-info"},
		{"deck:tclient-profiles-preview", "tclient-profiles"},
		{"deck:tclient-profiles-options", "tclient-profiles"},
		{"deck:tclient-profiles-list", "tclient-profiles"},
		{"deck:qmclient-config-browser", "qmclient-config"},
	};
	for(const auto &aEntry : aExpected)
	{
		const char *pId = aEntry[0];
		const char *pTab = aEntry[1];
		const auto *pCard = qm_card_registry::FindByStableId(pId);
		ASSERT_NE(pCard, nullptr) << pId;
		EXPECT_STREQ(pCard->m_pDefaultTab, pTab) << pId;
	}
}
```

```cpp
TEST(QmNewUiMenuBranches, P6InfoProfilesAndConfigsUseCanonicalPlatform)
{
	const std::string Source = ReadTextFile("src/game/client/components/tclient/menus_tclient.cpp");
	for(const char *pSignature : {
		"void CMenus::RenderSettingsTClientInfo(CUIRect MainView)",
		"void CMenus::RenderSettingsTClientProfiles(CUIRect MainView)",
		"void CMenus::RenderSettingsTClientConfigs(CUIRect MainView)"})
	{
		const std::string Body = FunctionBody(Source, pSignature);
		ASSERT_FALSE(Body.empty()) << pSignature;
		EXPECT_NE(Body.find("ResolveSettingsPageLayout("), std::string::npos) << pSignature;
		EXPECT_NE(Body.find("SSettingsCardDefinition"), std::string::npos) << pSignature;
		EXPECT_NE(Body.find("m_SettingsCardDeck.Render("), std::string::npos) << pSignature;
		EXPECT_NE(Body.find(".State()"), std::string::npos) << pSignature;
		EXPECT_NE(Body.find("QmResolveScrollPolicy("), std::string::npos) << pSignature;
		for(const char *pForbidden : {"CQmScrollContainer", "RenderQmSettingsGlassCard(", "TClientCacheSectionBoxRect", "ui_widget::TextField(", "ui_widget::SearchField("})
			EXPECT_EQ(Body.find(pForbidden), std::string::npos) << pSignature << ": " << pForbidden;
	}
}

TEST(QmMonitoringHelpers, P6SettingsPagesContainNoLegacyProductionPaths)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Qm = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string TClient = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	for(const char *pForbidden : {"RenderQmSettingsGlassCard", "TClientCacheSectionBoxRect", "InsetTClientCacheSectionContent", "DrawTClientCacheSectionBox", "m_TClientSettingsCardDragState"})
	{
		EXPECT_EQ(Header.find(pForbidden), std::string::npos) << pForbidden;
		EXPECT_EQ(Menus.find(pForbidden), std::string::npos) << pForbidden;
		EXPECT_EQ(Qm.find(pForbidden), std::string::npos) << pForbidden;
		EXPECT_EQ(TClient.find(pForbidden), std::string::npos) << pForbidden;
	}
	EXPECT_EQ(Qm.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(Qm.find("s_GlassCards"), std::string::npos);
}
```

- [ ] **Step 2: Rebuild testrunner and verify red**

Run:

```powershell
cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmNewUiMenuBranches.P6InfoProfilesAndConfigsUseCanonicalPlatform:QmMonitoringHelpers.P6SettingsPagesContainNoLegacyProductionPaths:QmCardRegistry.P6RegistersInfoProfilesAndConfigCards
```

Expected: final-slice/registry tests FAIL until these pages use shared contracts; any legacy-symbol failure identifies an unfinished earlier slice that must be fixed before proceeding.

- [ ] **Step 3: Implement Info, Profiles and Configs minimally**

Info maps links/config-files/developers/hidden-tabs one-to-one to four registered cards and preserves URL/file/tab-visibility behavior. Profiles maps current+after-load preview, save/load options/actions, and profile list to three registered cards; preserve `m_Dummy`, selection bounds, apply/delete/save and skin/color/flag semantics. Configs uses one full registered browser card; preserve apply/clear changes, domain/tag filters, compact/modified toggles, incremental filtering and config write behavior.

All three functions use `ResolveSettingsPageLayout`, `SSettingsCardDefinition`/`m_SettingsCardDeck.Render(...)`, one `CScrollRegion::State()` and settings policy；`SettingsCard(...)` remains deck-internal. Config search uses `InputField`; any numeric editor row uses `NumericField`. The Tee Profiles caller and QmClient Config caller only pass the full-width tab content viewport; they do not begin another deck/scroll. `RenderSettingsTClientConfigs(ContentView)` owns the sole config-browser card and sole config scroll, so the QmClient shell must not wrap it in another card or scroll region.

Remove final unused P6 declarations and run the all-pages structure test. Do not delete registry/model/adapter compatibility functions, profile/config business models, or P4 popup/list adapters.

- [ ] **Step 4: Run focused green checks**

Run:

```powershell
cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmNewUiMenuBranches.P6*:QmMonitoringHelpers.P6*:QmCardRegistry.P6*:SectionLoader.PlaceholderCompactAndFullShareMeasuredContentHeight:SettingsCardDeck.*:InputField.*:NumericField.*:UiV2ScrollState.*:UiV2ScrollPolicy.*:UiV2DropdownIntegration.*:QmModuleLayoutAdapter.*
git diff --check
```

Expected: every P6 test and adapter suite PASS; no legacy production symbol or whitespace error remains.

**Visual matrix:**

| Subpage | View/UI scale/language | Operation | Expected |
|---|---|---|---|
| Info | 1280×720 / 100% / English | open links/files, toggle visible tabs, scroll | four single-layer cards; buttons hit correctly; tab bar remains full width and stable |
| Info | 960×720 / 125% / Simplified Chinese | resize and inspect developer rows | one-column order; tee/name/link rows stay inside content; no transparent block or text escape |
| Profiles | 1280×720 / 100% / English | select, preview, save, apply and delete profile | current/after-load preview is correct; selection clamps after deletion; full list card is reachable |
| Profiles | 960×720 / 125% / Simplified Chinese | switch player/dummy, scroll and resize | no stale profile preview or overlap; options remain associated with the active side |
| Configs | 1280×720 / 100% / English | search, toggle domain/tags, edit, apply/clear | one full card; InputField focus is canonical; change count and writes behave unchanged |
| Configs | 960×720 / 125% / Simplified Chinese | IME search, Alt+wheel, resize with modified rows | filter text/cursor stay aligned; list clip/rail are correct; modified state survives reflow |

- [ ] **Step 5: Run the mandatory serial final verification**

Run these commands in this exact order; do not start a second target in `cmake-build-release` while one is running:

```powershell
cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmNewUiMenuBranches.P6*:QmMonitoringHelpers.P6*:QmCardRegistry.P6*:SectionLoader.PlaceholderCompactAndFullShareMeasuredContentHeight:SettingsCardDeck.*:InputField.*:NumericField.*:UiV2ScrollState.*:UiV2ScrollPolicy.*:UiV2DropdownIntegration.*:QmModuleLayoutAdapter.*
cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
python qmclient_scripts/gate/check_docs.py
python qmclient_scripts/gate/check_gate.py --mode default
git diff --check
```

Expected: every command exits `0`; `run_cxx_tests` is the full C++ evidence; default gate covers full C++ and Rust suites; `git diff --check` emits no output. A focused test, build or quick gate cannot substitute for a failed/skipped command.

- [ ] **Step 6: Execute every manual matrix row**

Run:

```powershell
New-Item -ItemType Directory -Force -Path tmp/settings-ui-p6 | Out-Null
Start-Process -FilePath (Resolve-Path 'cmake-build-release/DDNet.exe') -WorkingDirectory (Resolve-Path 'cmake-build-release')
```

Execute every row in Tasks 1–6. Save screenshots/results under `tmp/settings-ui-p6/` using `<page>-<viewport>-<scale>-<language>.png`. For every page also verify: single shell, full-width sub tab, title/body/small typography, hover subtitle, cross-column drag/release, restart persistence, normal/Alt wheel, IME/focus/clear where present, and no card overlap during resize/progressive load.

Expected: all rows have an observed result; any unexecuted row is reported as an explicit visual gap and prevents claiming full visual completion.

- [ ] **Step 7: Wait for an independent read-only review and close findings**

Dispatch one fresh reviewer with this exact scope; the reviewer must not modify files or dispatch another agent:

```text
只读审查从 tmp/settings-ui-p6-start.txt 记录的起点到当前工作树的 P6 diff。先读 docs/ai-workflow/review.md，禁止修改文件，禁止派发子代理。先按严重度列 findings，再给总体结论。重点检查：QmCardRegistry/SettingsCardOrderModel/QmModuleLayoutAdapter 是否仍为单一事实源；SSettingsCardFrame canonical rect；CSectionLoader 是否只拥有 content height、Deck 是否独占完整 frame；旧 glass/cache/inset/private drag 是否从生产路径删除；InputField/NumericField/scroll policy 是否无旧双路径；PrewarmOnly、配置、selection、持久化和热路径是否回归；测试是否用 callback 计数证明真实状态推进，而不只是字符串或相等高度假完成。
```

Expected: reviewer 的完整报告已返回，不能在 reviewer 未返回时结束。每个 finding 用最小补丁修复；修复后重跑 Step 4、Step 5 中全部自动命令，并重做受影响的人工矩阵行，直到总体结论为 `正确` 且没有未处理 finding。

- [ ] **Step 8: Commit final slice after review**

```powershell
git add -p -- src/game/client/components/qmclient/menus_qmclient.cpp src/game/client/components/tclient/menus_tclient.cpp src/game/client/components/menus.cpp src/game/client/components/menus.h src/game/client/QmUi/QmModuleLayoutAdapter.h src/game/client/QmUi/QmModuleLayoutAdapter.cpp src/game/client/QmUi/QmCardRegistry.cpp src/test/qm_card_registry_test.cpp src/test/section_loader_test.cpp src/test/qm_new_ui_menu_branch_test.cpp src/test/qmclient_monitoring_test.cpp
git diff --cached --name-only
git diff --cached --stat
git diff --cached --check
git diff --cached -- src/game/client/components/qmclient/menus_qmclient.cpp src/game/client/components/tclient/menus_tclient.cpp src/game/client/components/menus.cpp src/game/client/components/menus.h
git commit -m "refactor(settings-ui): 完成栖梦与 TClient 页面迁移" -m "refactor: 统一复杂子页卡片、输入与滚动并清退旧 shell" -m "fix: 统一渐进加载 content-height 并由 Deck 独占完整 frame" -m "test: 完成 focused、全量回归、default gate、人工矩阵与独立审查"
```

`git add -p` 只选择 P6 计划中已审查的 hunk；若 shared file 中有用户/其他 AI 并行修改，留在 unstaged 工作树。只有 cached name list、cached stat、cached whitespace 和四个高并发大文件的 cached diff 都人工确认无越界 hunk 后才 commit。

Expected: commit 只包含 P6 文件；`git show --stat --oneline HEAD` 不含用户并行改动、临时截图或 R1–R3 内容。

---

## Self-review

- Spec coverage: 覆盖 QmClient Overview/Visual/Functions/HUD/Contributors/Config，TClient Settings/BindWheel/ChatBinds/WarList/StatusBar/Info/Profiles/Configs，旧 glass/cache/inset/cached-height 清退、shared model 保留和复杂子页人工验收。
- Deletion ownership: P2 只交付公共 model/registry/Search/deck 与 Graphics pilot；P6 负责 QmClient/TClient Deck 接入及私有 drag/drop/order/shell/cache/Search 删除。P3 input/numeric 清退只作为保持性契约。
- Height contract: `CSectionLoader` 只缓存 measured content height；placeholder/compact/full 都返回该 content height，渲染回调返回值只供 telemetry 使用且不改变 Deck 布局。header、padding、shell 与 gap 只由 Deck/`SettingsCard(...)` 加一次，行为测试证明三帧状态推进、content height 一致与 `SSettingsCardFrame::m_Rect.h` 稳定，并保留 `0.01f` telemetry 门槛。
- Type consistency: 页面只提交 `SSettingsCardDefinition` 并消费 `CSettingsCardDeck::Render(...)`/`SSettingsCardDeckResult`、`SettingsCardOrderModel()`、`CScrollRegion::State()`/`CQmScrollState`、`QmResolveScrollPolicy(...)`、`ui_widget::InputField(...)` 与 `ui_widget::NumericField(...)`；canonical shell 仍唯一来自 `SettingsCard(...) -> SSettingsCardFrame`。
- Marker scan: 文档没有未决占位标记、虚构版本号或“照上一任务做”的省略步骤。
- Exit gate: testrunner rebuild、P6 focused tests、game-client、full `run_cxx_tests`、docs check、default gate、全部人工矩阵和已返回的独立只读 review 均收口后，P6 才可标记完成。
- Scope boundary: R1–R3、非卡片菜单、版本更新、信息架构重组和渲染管线均留在 P7/后续专项。
