# QmClient 设置页 UI 统一 P7 非卡片菜单、性能与最终收口 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不把业务条目卡片化的前提下，让服务器、好友、Demo、资产、皮肤、国旗、语言等列表/网格只使用 P1–P4 的公共 theme、input、scroll 与 dropdown runtime，并用有界缓存、固定性能预算、全量自动验证和人工矩阵完成 P0–P7 最终收口。

**Architecture:** 非卡片页面保留现有列表、表格和网格的数据模型，只在页面边界向公共 runtime 提交 `SUiTheme`、`IUiContext`、`SQmScrollRequest`、父 viewport 与 popup ownership；页面不得拥有第二套 wheel、policy 或 card/deck 状态。P7 新建一个薄的 `QmUiPerf` 计数/格式 facade，复用现有 `CurrentQmUiPerfPage()`、`CurrentQmUiPerfOperation()`、`QmPerfLogPayload(...)` 和既有 perf streams，不创建第二个 logger；`qmclient_scripts/perf` 继续负责解析、统计、质量判定和 HTML report。缓存只在明确 key 命中时复用，并具有可测试的失效、容量和清理边界。

**Tech Stack:** C++17、QmUi、DDNet `CUi`/`CListBox`/`CScrollRegion` 适配层、GoogleTest、TypeScript/Bun 性能报表、CMake/MSVC、Python gate、Markdown。

## Global Constraints

- 权威规格只有 `docs/superpowers/specs/2026-07-10-QmClient-设置页UI统一与滚动体系规格.md`；归档规格和历史截图不作为实现依据。
- P1–P6 已完成是硬前置。开工前必须确认 `SUiTheme ResolveUiTheme(ColorHSLA BaseColor, float Opacity)`、`IUiContext::m_pTheme`、`SInputFieldResult ui_widget::InputField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const SInputFieldOptions &Options)`、`CQmScrollState`、`SQmResolvedScrollPolicy QmResolveScrollPolicy(const SQmScrollRequest &Request, float UiScale, float SmoothScrollTimeSec)`、`void CUi::RegisterWheelOwner(const void *pOwnerId, EUiWheelOwnerPriority Priority, const CUIRect &HotRect, bool Eligible)` 和 `bool CUi::TryConsumeWheel(const void *pOwnerId, float *pDelta)` 均已存在且测试通过；缺任一项就回到对应前置阶段收口。当前基线没有 `QmUiPerf.h/.cpp`，其唯一 facade 明确由 Task 6 创建。
- 非卡片页面保留列表、表格或网格语义；服务器、好友、Demo、资产、皮肤、国旗、语言业务条目禁止注册到 `QmCardRegistry`，禁止调用 `SettingsCard(...)`、`CSettingsCardDeck` 或设置卡片顺序持久化。
- P7 不新增 wheel owner、wheel queue、scroll policy、rail visibility 或 dropdown policy；只调用 `CQmScrollState`、`QmResolveScrollPolicy(...)`、`CUi::RegisterWheelOwner(...)` 和 `CUi::TryConsumeWheel(...)`。
- P4 wheel 结构是不可移动的 exact contract：`CUi::Update()` 首行幂等调用 `BeginWheelOwnershipFrame()`；`ShowPopupSelection(...)` 在 `DoPopupMenu(...)` 前以 `pContext` 登记 popup owner；`PopupSelection(...)` 把 `pSelectionPopup` 同时设为 `m_pWheelOwnerId` 且 `m_WheelOwnerPreRegistered = true`；`CScrollRegion::DoScrollInput()` 只以解析后的 `pWheelOwnerId` 调 `TryConsumeWheel(...)`。P7 不得把登记移到 `DoDropDown(...)`，也不得把消费退回 `this`。
- 公共滚动矩阵必须完整保留 `large`、`medium`、`small`、`horizontal` 四个 preset；`AUTO` 仅 overflow 时绘制且不预留无效宽度，`HIDDEN` 不绘制、不预留、热区只等于 clip，但 wheel/键盘仍可用。
- `Alt + wheel` 精确为普通步长的 3 倍；Ctrl/GUI/Shift 不消费 wheel。菜单列表步长必须由真实 row extent 与 rows-per-step 解析，国旗等 filter grid 固定两行步长并使用 hidden rail。
- dropdown 最多八项可见；短列表保持打开并把 wheel 留给父 viewport，长列表或受父 viewport 限高时同帧注册 owner、消费首个 wheel 并使用 `small` rail。anchor 部分可见时 popup 保持，完全离开父 viewport 才关闭。
- 所有文本输入都通过 `ui_widget::InputField(...)`；P7 触达的生产函数不得调用 `ui_widget::TextField(...)`、`SearchField(...)`、`ClearableTextField(...)`、`IconTextField(...)` 或直接 `CUi::DoEditBox`。
- theme 每帧只由 `ResolveUiTheme(ColorHSLA(g_Config.m_QmUiColor), g_Config.m_QmUiOpacity / 100.0f)` 解析，随后通过 `IUiContext::m_pTheme` 注入；业务页不得重新解释颜色、focus ring、输入背景或 rail 色。
- 性能优化顺序固定为：跳过 clip 外业务工作、复用不变布局/文本/筛选 plan、按既有预算分帧消费、最后才缓存；不得删除缩略图、选中态、hover/focus、必要动画、文本或业务信息来换取数字。
- Dogfood 只验证 primitive、ownership 和调参证据，不扩展公共组件覆盖，不作为真实页面接入或人工验收的替代。
- R1（完整公共组件覆盖）、R2（11 tab 与完整 L0/L1/L2）、R3（Phosphor/MSDF/SDF shader）只进入最终报告的后续专项表；P7 不实现、不建空接口、不新增资源或 shader。
- P7 不改变协议、物理、预测、碰撞、地图、Demo/skin/配置格式、回放、服务端玩法或 rank 语义。
- 临时日志、截图和生成的 perf report 全部写入 `tmp/`；版本化证据只写 `docs/superpowers/reports/2026-07-11-settings-ui-p7-acceptance.md`。
- 同一 `cmake-build-release` 的 `testrunner`、`game-client`、`run_cxx_tests`、`run_rust_tests` 和 `package_default` 始终串行；filtered test 只作红绿灯，不能替代全量入口。
- P7 版本目标以 P0 报告确认的 `2.74.23` 为前置，结束时按 MMP 更新为 `2.74.24`；若 P0 因版本审查证明基线不是 `2.74.23`，必须先同步更新 active spec、本索引与本计划的 exact version test，再开始 P1，不能在 P7 临时猜版本。不创建 tag、不发布 Release。
- C++ 注释使用中文；保持文件现有 UTF-8/BOM、CRLF/LF 与 Tab/空格风格；不回退或混入用户/其他代理的并行改动。

## File Structure

- Modify: `src/game/client/components/menus_browser.cpp` — 服务器、筛选、好友、收藏地图与 Qm 列表的非卡片适配。
- Modify: `src/game/client/components/menus_demo.cpp` — Demo/截图浏览器、搜索、source dropdown 与列表滚动。
- Modify: `src/game/client/components/menus_settings_assets.cpp` — 资产筛选与资源网格适配，保留资源 jobs/preview 业务语义。
- Modify: `src/game/client/components/menus_settings.cpp` — 皮肤、国旗、语言列表/网格和其输入/dropdown 适配。
- Modify: `src/game/client/QmUi/UiForms.h`、`src/game/client/QmUi/UiForms.cpp` — P7 最后删除 P3 的 forwarding-only input aliases。
- Modify: `src/game/client/components/menus.h`、`src/game/client/components/menus.cpp` — 仅补菜单级 perf operation、缓存失效入口和统一证据采集；不放页面业务逻辑。
- Create: `src/game/client/QmUi/QmUiPerf.h`、`src/game/client/QmUi/QmUiPerf.cpp` — 薄的 UI perf frame/counter facade，最终仍调用既有 `QmPerfLogPayload(...)`；不创建平行 logger。
- Modify: `CMakeLists.txt` — 只在既有 QmUi client/test source 列表登记 `QmUiPerf.h/.cpp`，不改 target 或构建选项。
- Modify: `qmclient_scripts/perf/lib/stats.ts`、`qmclient_scripts/perf/lib/quality.ts`、`qmclient_scripts/perf/lib/report.ts` — 消费非卡片菜单 telemetry、预算 verdict 与报告表。
- Modify: `qmclient_scripts/perf/analyze.ts` — 接受显式 `--output`，保证本计划报告写入 `tmp/`。
- Modify: `qmclient_scripts/perf/test.ts` — C++ 日志合同与固定预算回归。
- Modify: `src/test/QmAnimTest.cpp` — preset、rail、Alt、行步长、filter grid、dropdown/父 viewport/首轮 wheel 的纯行为测试。
- Modify: `src/test/qmclient_monitoring_test.cpp` — 真实生产函数接入、缓存边界、telemetry schema 与旧路径清退测试。
- Modify: `src/game/version.h`、`docs/info.json` — 最终一次 MMP 版本更新。
- Create: `docs/superpowers/reports/2026-07-11-settings-ui-p7-acceptance.md` — 自动验证、人工矩阵、性能前后对比、独立 review 与后续专项的单一证据文档。

---

### Task 1: 服务器浏览器与好友列表接入公共 runtime

**Files:**
- Modify: `src/game/client/components/menus_browser.cpp:386` (`RenderServerbrowserServerList`)
- Modify: `src/game/client/components/menus_browser.cpp:1048` (`RenderServerbrowserStatusBox`)
- Modify: `src/game/client/components/menus_browser.cpp:1277` (`RenderServerbrowserFilters`)
- Modify: `src/game/client/components/menus_browser.cpp:1500` (`RenderServerbrowserDDNetFilter`)
- Modify: `src/game/client/components/menus_browser.cpp:1620` (`RenderServerbrowserCommunitiesFilter`)
- Modify: `src/game/client/components/menus_browser.cpp:1703` (`RenderServerbrowserCountriesFilter`)
- Modify: `src/game/client/components/menus_browser.cpp:1728` (`RenderServerbrowserTypesFilter`)
- Modify: `src/game/client/components/menus_browser.cpp:1911` (`RenderServerbrowserInfoScoreboard`)
- Modify: `src/game/client/components/menus_browser.cpp:2068` (`RenderServerbrowserFriends`)
- Modify: `src/game/client/components/menus_browser.cpp:3306` (`RenderServerbrowserQm`)
- Modify: `src/game/client/components/menus_browser.cpp:3424` (`RenderServerbrowserFavoriteMaps`)
- Test: `src/test/QmAnimTest.cpp`
- Test: `src/test/qmclient_monitoring_test.cpp`

**Interfaces:**
- Consumes: `SUiTheme`/`ResolveUiTheme(ColorHSLA, float)` through `IUiContext::m_pTheme`; `ui_widget::InputField(...)`; `CQmScrollState`; `SQmResolvedScrollPolicy QmResolveScrollPolicy(const SQmScrollRequest &, float, float)`; P4 `CListBox`/`CScrollRegion` adapters; `CUi::RegisterWheelOwner(...)`/`CUi::TryConsumeWheel(...)`.
- Produces: `MENU_LIST` requests with the actual server/friend row extent and three rows per wheel; `FILTER_GRID` requests with two rows per wheel and hidden rail; page IDs `server_browser`, `friends`, `favorite_maps` for the existing perf context/streams and the Task 6 facade. Wheel is consumed by the P4 adapters, not directly in these page functions.

- [ ] **Step 1: Verify the P1–P4 dependency surface before editing**

Run:

```powershell
rg -n "struct SUiTheme|ResolveUiTheme\(|m_pTheme|InputField\(|class CQmScrollState|QmResolveScrollPolicy\(|RegisterWheelOwner\(|TryConsumeWheel\(" src/game/client/QmUi src/game/client/ui.h src/game/client/ui.cpp
```

Expected: every required symbol has one authoritative declaration; wheel ownership is declared only on `CUi`, policy is declared only in `QmScroll.h`, and no P7-local replacement exists.

- [ ] **Step 2: Capture the unchanged fixed-scenario baseline**

先按 Task 7 的固定环境和八个 operation 完成未修改代码的基线采集；客户端退出后把最新日志复制到版本库 `tmp/`：

```powershell
$PerfDir = Join-Path $env:APPDATA 'DDNet/dumps/QmClient_Perf'
$BeforeLog = Get-ChildItem -LiteralPath $PerfDir -Filter 'qm_perf_*.log' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
Copy-Item -LiteralPath $BeforeLog.FullName -Destination 'tmp/settings-ui-p7-before.log'
```

Expected: `tmp/settings-ui-p7-before.log` 存在，八个场景均有可定位的 `page`/`list_frame`/`fps_summary` frame window；新增的 operation 名在改前日志中不存在时，由场景顺序和 frame window 映射，绝不以改后数据冒充 before。

- [ ] **Step 3: Write failing policy and production-path tests**

Add a behavior test that fixes the row/grid contract:

```cpp
TEST(UiV2ScrollPolicy, NonCardMenuListAndFilterGridUseResolvedSteps)
{
	SQmScrollRequest ListRequest;
	ListRequest.m_Profile = EQmScrollProfile::MENU_LIST;
	ListRequest.m_RowExtent = 24.0f;
	ListRequest.m_RowsPerStep = 3;
	const SQmResolvedScrollPolicy List = QmResolveScrollPolicy(ListRequest, 1.0f, 0.0f);
	EXPECT_FLOAT_EQ(List.m_Config.m_WheelScale, 72.0f);
	EXPECT_EQ(List.m_RailVisibility, EQmScrollRailVisibility::AUTO);
	EXPECT_FLOAT_EQ(List.m_AltMultiplier, 3.0f);

	SQmScrollRequest GridRequest;
	GridRequest.m_Profile = EQmScrollProfile::FILTER_GRID;
	GridRequest.m_RowExtent = 30.0f;
	GridRequest.m_RowsPerStep = 2;
	const SQmResolvedScrollPolicy Grid = QmResolveScrollPolicy(GridRequest, 1.0f, 0.0f);
	EXPECT_FLOAT_EQ(Grid.m_Config.m_WheelScale, 60.0f);
	EXPECT_EQ(Grid.m_RailVisibility, EQmScrollRailVisibility::HIDDEN);
	EXPECT_FALSE(Grid.m_ContentDragAllowed);
}
```

Add `QmMonitoringHelpers.NonCardServerAndFriendsUseSharedRuntime` that extracts every function listed in **Files**, requires `InputField(` where an input exists, `SettingsUiContext(` for `IUiContext::m_pTheme`, and `QmResolveScrollPolicy(` or the P4 adapter request at every list/grid. Separately retain P4's `DropdownRegistersWheelOwnerBeforeParentCanConsume` assertion against `ui.cpp`/`ui_scrollregion.cpp`; page functions must not consume raw wheel directly. Reject:

```cpp
const std::array<const char *, 12> Forbidden = {
	"ui_widget::TextField(", "ui_widget::SearchField(", "ui_widget::ClearableTextField(",
	"ui_widget::IconTextField(", "DoEditBox(", "QmScrollRegionParamsForSize(",
	"m_ScrollUnit =", "ForceShowScrollbar", "KEY_MOUSE_WHEEL_UP", "KEY_MOUSE_WHEEL_DOWN",
	"SettingsCard(", "RegisterSettingsCardDeckItem(",
};
```

- [ ] **Step 4: Rebuild the test binary and verify red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiV2ScrollPolicy.NonCardMenuListAndFilterGridUseResolvedSteps:QmMonitoringHelpers.NonCardServerAndFriendsUseSharedRuntime
```

Expected: policy behavior passes after P4; production-path test fails on legacy input wrappers, local `m_ScrollUnit = 80.0f`, incomplete policy requests or missing same-frame popup ownership.

- [ ] **Step 5: Apply the minimal server/friends adaptation**

For each render entry, obtain one shared context and pass the P4 request to the existing list/scroll adapter. The concrete call shape is:

```cpp
const IUiContext MenuCtx = SettingsUiContext("server_browser");

SQmScrollRequest ListRequest;
ListRequest.m_Profile = EQmScrollProfile::MENU_LIST;
ListRequest.m_Axis = EQmScrollAxis::VERTICAL;
ListRequest.m_RowExtent = RowHeight;
ListRequest.m_RowsPerStep = 3;
const SQmResolvedScrollPolicy ListPolicy = QmResolveScrollPolicy(ListRequest, 1.0f, 0.0f);
```

Use the exact P3 call for search fields:

```cpp
ui_widget::SInputFieldOptions SearchOptions;
SearchOptions.m_Mode = ui_widget::EInputFieldMode::SEARCH;
SearchOptions.m_pLeadingIcon = FontIcons::FONT_ICON_MAGNIFYING_GLASS;
SearchOptions.m_Clearable = true;
SearchOptions.m_SearchHotkeyEnabled = false;
ui_widget::InputField(MenuCtx, &s_FilterInput, QuickSearch, SearchOptions);
```

Address, friend name/clan/category/note use `EInputFieldMode::TEXT` with the existing placeholder, clearability and commit behavior copied into `SInputFieldOptions`. `RenderServerbrowserDDNetFilter`, country/type/community grids use `FILTER_GRID`, actual grid row height and `m_RowsPerStep = 2`; remove local width/speed/Alt interpretation. Friends keeps category drag/reorder and friend actions unchanged, but its list offset lives only in the P4 adapter's `CQmScrollState`. Dropdown/popups pass state, anchor and parent viewport into the P4 adapter; `ui.cpp`/`ui_scrollregion.cpp` remain the only wheel owner/consumer implementation.

- [ ] **Step 6: Run green tests and commit**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiV2ScrollPolicy.NonCardMenuListAndFilterGridUseResolvedSteps:QmMonitoringHelpers.NonCardServerAndFriendsUseSharedRuntime
git diff --check
git add src/game/client/components/menus_browser.cpp src/test/QmAnimTest.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "refactor(menu-browser): 接入非卡片公共 UI runtime" -m "fix: 统一服务器筛选与好友列表的 theme、input、scroll 和 popup ownership" -m "test: 覆盖列表步长、hidden rail 与旧路径清退"
```

Expected: focused tests pass；commit 不包含设置卡片注册、业务语义变化或并行工作区文件。

### Task 2: Demo 与截图浏览器接入公共列表和 dropdown

**Files:**
- Modify: `src/game/client/components/menus_demo.cpp:1982` (`RenderDemoBrowser`)
- Modify: `src/game/client/components/menus_demo.cpp:2017` (`RenderDemoBrowserList`)
- Modify: `src/game/client/components/menus_demo.cpp:2397` (`RenderDemoBrowserDetails`)
- Modify: `src/game/client/components/menus_demo.cpp:2569` (`RenderDemoBrowserButtons`)
- Test: `src/test/QmAnimTest.cpp`
- Test: `src/test/qmclient_monitoring_test.cpp`

**Interfaces:**
- Consumes: Task 1/P1–P4 theme/input/scroll contract; P4 `CUi::SDropDownState` parent viewport and same-frame owner adapter; existing Demo metadata scheduler and `perf/interaction event=list_frame` contract.
- Produces: `demo_browser` medium list request using the base row height and three rows per wheel; source dropdown is a two-item short popup that yields wheel to the parent; long popup paths cap at eight visible items and consume their first wheel.

- [ ] **Step 1: Write failing Demo integration tests**

Add a real parent/popup ordering test using the P4 wheel API:

```cpp
TEST(UiV2DropdownIntegration, PopupRegistersBeforeDemoParentConsumesFirstWheel)
{
	CScrollWheelOwnership Ownership;
	int ParentRegion = 0;
	int SelectionPopupContext = 0;
	const CUIRect ParentRect{0.0f, 0.0f, 300.0f, 300.0f};
	const CUIRect PopupRect{20.0f, 40.0f, 160.0f, 160.0f};
	const vec2 Pointer{50.0f, 80.0f};
	ASSERT_TRUE(Ownership.BeginFrame(41, -120.0f, false));
	// ShowPopupSelection 在父 region End 前登记 pContext。
	QmRegisterWheelOwnerCandidate(Ownership,
		{&SelectionPopupContext, EUiWheelOwnerPriority::POPUP, PopupRect, true}, Pointer, true);
	QmRegisterWheelOwnerCandidate(Ownership,
		{&ParentRegion, EUiWheelOwnerPriority::PAGE, ParentRect, true}, Pointer, true);
	float PopupDelta = 0.0f;
	float ParentDelta = 0.0f;
	EXPECT_FALSE(QmTryConsumeWheel(Ownership, &ParentRegion, &ParentDelta));
	// PopupSelection 把 pSelectionPopup 预登记为 region owner ID。
	EXPECT_TRUE(QmTryConsumeWheel(Ownership, &SelectionPopupContext, &PopupDelta));
	EXPECT_FLOAT_EQ(PopupDelta, -120.0f);
}
```

Add `QmMonitoringHelpers.DemoBrowserUsesSharedNonCardRuntime`, requiring `InputField`, `MENU_LIST`, parent viewport and P4 `DoDropDown` use while rejecting the Task 1 forbidden set and all `SettingsCard`/deck calls. Pair it with P4's global production-order tests: `WheelOwnershipFrameBeginsOnlyFromUiUpdate` proves the idempotent frame begin has one call site, while `DropdownRegistersWheelOwnerBeforeParentCanConsume` proves `ShowPopupSelection(...)` registers `pContext` before `DoPopupMenu(...)`, `PopupSelection(...)` passes `pSelectionPopup` as a pre-registered `m_pWheelOwnerId`, and `CScrollRegion::DoScrollInput()` consumes `pWheelOwnerId` without raw wheel keys.

- [ ] **Step 2: Verify red with a rebuilt binary**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiV2DropdownIntegration.PopupRegistersBeforeDemoParentConsumesFirstWheel:QmMonitoringHelpers.DemoBrowserUsesSharedNonCardRuntime:QmMonitoringHelpers.WheelOwnershipFrameBeginsOnlyFromUiUpdate:QmMonitoringHelpers.DropdownRegistersWheelOwnerBeforeParentCanConsume
```

Expected: FAIL on Demo search/source dropdown/list production wiring; if the pure ownership test already passes from P4, retain it as the integration anchor and use the production-path failure as the red light.

- [ ] **Step 3: Implement the minimal Demo adaptation**

Use one `SettingsUiContext("demo_browser")`; replace both Demo search render branches and slice-name input with `ui_widget::InputField(...)`. Pass this request to the list adapter:

```cpp
SQmScrollRequest DemoRequest;
DemoRequest.m_Profile = EQmScrollProfile::MENU_LIST;
DemoRequest.m_Axis = EQmScrollAxis::VERTICAL;
DemoRequest.m_RowExtent = RowHeight;
DemoRequest.m_RowsPerStep = 3;
const SQmResolvedScrollPolicy DemoPolicy = QmResolveScrollPolicy(DemoRequest, 1.0f, 0.0f);
```

Keep variable-height screenshot preview rows, selection range, sort, metadata budgets and `items_total/rows_visible/rows_processed/rows_skipped/dur_ms` telemetry unchanged. The two-item source dropdown remains open and does not consume wheel; any long duration/list popup uses `POPUP_LIST`, `m_MaxVisibleItems == 8`, the anchor's parent viewport and same-frame owner registration.

- [ ] **Step 4: Run green tests and commit**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiV2DropdownIntegration.PopupRegistersBeforeDemoParentConsumesFirstWheel:QmMonitoringHelpers.DemoBrowserUsesSharedNonCardRuntime:QmMonitoringHelpers.WheelOwnershipFrameBeginsOnlyFromUiUpdate:QmMonitoringHelpers.DropdownRegistersWheelOwnerBeforeParentCanConsume:QmMonitoringHelpers.DemoBrowser*
git diff --check
git add src/game/client/components/menus_demo.cpp src/test/QmAnimTest.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "refactor(demo-browser): 统一列表输入滚动与 dropdown" -m "fix: 保留 Demo 业务语义并接入父 viewport 与首轮 wheel ownership" -m "test: 覆盖短长 popup 和真实生产路径"
```

Expected: focused tests pass，Demo metadata/list telemetry tests仍通过，commit 不修改 Demo 文件格式或回放逻辑。

### Task 3: 资产列表与网格接入公共 runtime，保留资源调度

**Files:**
- Modify: `src/game/client/components/menus_settings_assets.cpp:4135` (`RenderSettingsCustom`)
- Modify: `src/game/client/components/menus_settings_assets.cpp:5765` (本地资产列表)
- Modify: `src/game/client/components/menus_settings_assets.cpp:6763` (Workshop 资产列表)
- Test: `src/test/qmclient_monitoring_test.cpp`
- Test: `src/test/settings_warmup_test.cpp`

**Interfaces:**
- Consumes: `SUiTheme`/`IUiContext::m_pTheme`、`ui_widget::InputField(...)`、P4 menu-list/grid adapter、既有 `SSettingsAdaptiveBudgetOutput`、`CSettingsResourcePreviewCache`、preview upload scheduler 和 `perf/settings-resource` / `perf/ui_budget` stage logging。
- Produces: 资产 tile 的非卡片 viewport/scroll/input 接入；保留 `SETTINGS_ASSETS_CARD_METADATA_CACHE_MAX_ENTRIES == 512`、generation checks、visible-range admission 和 resource telemetry。

- [ ] **Step 1: Write failing asset-path tests**

Add `QmMonitoringHelpers.AssetsGridUsesSharedNonCardRuntimeWithoutSettingsCards`:

```cpp
const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
ASSERT_FALSE(Body.empty());
EXPECT_NE(Body.find("ui_widget::InputField("), std::string::npos);
EXPECT_NE(Body.find("QmResolveScrollPolicy("), std::string::npos);
EXPECT_NE(Body.find("SettingsUiContext(\"assets\")"), std::string::npos);
EXPECT_EQ(Body.find("SettingsCard("), std::string::npos);
EXPECT_EQ(Body.find("CSettingsCardDeck"), std::string::npos);
EXPECT_EQ(Body.find("RegisterSettingsCardDeckItem("), std::string::npos);
EXPECT_EQ(Body.find("ui_widget::SearchField("), std::string::npos);
EXPECT_NE(Source.find("SETTINGS_ASSETS_CARD_METADATA_CACHE_MAX_ENTRIES = 512"), std::string::npos);
```

Extend the existing warmup tests to assert clip-external tiles do not hydrate metadata/preview, visible tiles remain admitted, and a filter/source generation change invalidates the visibility plan before reuse.

- [ ] **Step 2: Verify the focused red state**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.AssetsGridUsesSharedNonCardRuntimeWithoutSettingsCards:SettingsResourceJobs.*Visible*:SettingsResourceJobs.*Generation*
```

Expected: production-path test fails on the legacy search/scroll shell or missing theme injection; existing resource scheduling tests remain green and define behavior that P7 must preserve.

- [ ] **Step 3: Implement only the asset UI adapter change**

Use `SettingsUiContext("assets")` and `ui_widget::InputField(...)` for every local/workshop filter. The local and workshop tile viewports submit `MENU_LIST` with their real tile-row extent and configured rows per step; only category/filter strips that scroll as a grid submit `FILTER_GRID` with two rows and hidden rail. Keep these cache key fields intact:

```text
asset_id, tab, locale_hash, ui_scale, tile_width, status_hash,
installed, download_failed, local_only
```

Keep the 512-entry metadata cap, preview resident byte budget, visible-first uploads, generation checks and background-job/GPU ownership unchanged. Do not wrap a tile in `SettingsCard`, do not register a tile stable ID, and do not skip preview/title/status rendering for performance.

- [ ] **Step 4: Run green tests and commit**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.AssetsGridUsesSharedNonCardRuntimeWithoutSettingsCards:QmMonitoringHelpers.Assets*:SettingsResourceJobs.*
git diff --check
git add src/game/client/components/menus_settings_assets.cpp src/test/qmclient_monitoring_test.cpp src/test/settings_warmup_test.cpp
git commit -m "refactor(assets): 统一资产网格公共 UI runtime" -m "fix: 接入 theme、InputField 与共享 scroll policy，不改变资源调度" -m "test: 固化可见性、generation 与 512 项缓存边界"
```

Expected: focused tests pass；资源 preview/job telemetry、安装/下载/选择行为保持，业务 tile 未进入设置卡片体系。

### Task 4: 皮肤、国旗与语言列表/网格最终适配

**Files:**
- Modify: `src/game/client/components/menus_settings.cpp:971` (`RenderSettingsTeeIdentity`)
- Modify: `src/game/client/components/menus_settings.cpp:1172` (Player 国旗列表)
- Modify: `src/game/client/components/menus_settings.cpp:2235` (Tee 皮肤列表)
- Modify: `src/game/client/components/menus_settings.cpp:4697` (`RenderLanguageSettings`)
- Modify: `src/game/client/components/menus_settings.cpp:4728` (`RenderLanguageSelection`)
- Test: `src/test/QmAnimTest.cpp`
- Test: `src/test/qmclient_monitoring_test.cpp`
- Test: `src/test/skins_test.cpp`

**Interfaces:**
- Consumes: P5 已完成的 Player/Tee card shell，仅在其 content viewport 内适配业务列表；`ui_widget::InputField(...)`、`MENU_LIST`、`FILTER_GRID`、父 viewport dropdown ownership。
- Produces: flag/skin filter grid 两行 hidden-rail wheel、language medium list、skin-sort dropdown 八项上限；保留 tee preview cache `MAX_ENTRIES == 192` 和 language cache `MAX_LANGUAGE_CACHE == 128`。

- [ ] **Step 1: Write failing list/grid and cache-boundary tests**

Add the production structure test:

```cpp
TEST(QmMonitoringHelpers, SkinFlagLanguageBusinessItemsStayNonCardAndUseSharedRuntime)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	for(const char *pSignature : {
		"void CMenus::RenderSettingsTeeIdentity(CUIRect MainView, CUIRect *pFlagButton)",
		"bool CMenus::RenderLanguageSelection(CUIRect MainView)"})
	{
		const std::string Body = ExtractSourceFunctionBody(Source, pSignature);
		ASSERT_FALSE(Body.empty());
		EXPECT_EQ(Body.find("ui_widget::TextField("), std::string::npos);
		EXPECT_EQ(Body.find("ui_widget::SearchField("), std::string::npos);
		EXPECT_EQ(Body.find("SettingsCard("), std::string::npos);
	}
	EXPECT_NE(Source.find("ui_widget::InputField("), std::string::npos);
	EXPECT_NE(Source.find("MAX_ENTRIES = 192"), std::string::npos);
	EXPECT_NE(Source.find("MAX_LANGUAGE_CACHE = 128"), std::string::npos);
}
```

Add pure tests proving `FILTER_GRID` advances exactly `2 * row extent`, stays scrollable with `HIDDEN`, and rejects Ctrl/GUI/Shift wheel while multiplying Alt by 3. Extend skin tests so source reload, selected dummy/custom colors and preview key changes cannot return a stale tee preview.

- [ ] **Step 2: Rebuild and verify red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.SkinFlagLanguageBusinessItemsStayNonCardAndUseSharedRuntime:UiV2ScrollPolicy.FilterGrid*:SkinsTest.*PreviewCache*
```

Expected: FAIL on legacy input/list wiring or missing invalidation assertion; existing preview behavior tests remain green.

- [ ] **Step 3: Implement the minimal business-list adaptation**

- Player country and Tee skin filters call `ui_widget::InputField(...)` with `IUiContext::m_pTheme` set.
- Country/filter grids pass `FILTER_GRID`, exact tile-row extent, `m_RowsPerStep = 2`, `HIDDEN`; clip remains the only wheel/hit region.
- Skin list and Language selection pass `MENU_LIST`, real row extent and three rows per wheel; `AUTO` does not reserve rail width when content fits.
- Skin sort and country popup use the P4 dropdown adapter, parent viewport, eight-item maximum and same-frame owner registration.
- Keep preview cache capacity 192; clear on skin source generation/reload and shutdown, while dummy/custom-color/emote remain key fields.
- Keep language cache capacity 128; invalidate on language inventory, active language, font generation, UI scale or theme generation; above 128 languages render uncached visible rows instead of growing the cache.

Do not change Player/Tee card placement, skin loading jobs, country values, language file semantics or navigation.

- [ ] **Step 4: Run green tests and commit**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.SkinFlagLanguageBusinessItemsStayNonCardAndUseSharedRuntime:UiV2ScrollPolicy.FilterGrid*:SkinsTest.*:QmMonitoringHelpers.*Language*
git diff --check
git add src/game/client/components/menus_settings.cpp src/test/QmAnimTest.cpp src/test/qmclient_monitoring_test.cpp src/test/skins_test.cpp
git commit -m "refactor(settings-lists): 统一皮肤国旗与语言列表" -m "fix: 接入公共 input、scroll、dropdown 并保持业务条目非卡片" -m "test: 固化 hidden rail、Alt 三倍和缓存清理边界"
```

Expected: focused tests pass；P5 card frame/drag/registry 测试不变，列表与网格业务语义保持。

### Task 5: 收口 preset、dropdown ownership 与源码旧路径

**Files:**
- Modify: `src/game/client/QmUi/UiForms.h`
- Modify: `src/game/client/QmUi/UiForms.cpp`
- Modify: `src/test/QmAnimTest.cpp`
- Modify: `src/test/qmclient_monitoring_test.cpp`
- Modify: `src/game/client/components/menus_browser.cpp`
- Modify: `src/game/client/components/menus_demo.cpp`
- Modify: `src/game/client/components/menus_settings_assets.cpp`
- Modify: `src/game/client/components/menus_settings.cpp`
- Modify: `src/game/client/components/menus.cpp`
- Modify: `src/game/client/components/menus_ingame.cpp`
- Modify: `src/game/client/components/menus_ingame_touch_controls.cpp`
- Modify: `src/game/client/components/menus_assets_editor.cpp`
- Modify: `src/game/client/components/menus_settings7.cpp`

**Interfaces:**
- Consumes: Tasks 1–4 的全部生产切片与 P1–P6 已通过的 settings `large`、NumericField `horizontal` 路径。
- Produces: 四 preset、两种 rail、修饰键、行步长、八项 popup、父 viewport 与首轮 wheel 的一张自动化矩阵；全部菜单生产路径只调用 `InputField(...)`，P3 forwarding aliases 从 `UiForms.h/.cpp` 删除，P7 范围内没有旧 input/scroll/wheel 双路径。

- [ ] **Step 1: Write the failing cross-phase contract test**

Add one table-driven test，使用 P4 已落地的 exact enum/request：

```cpp
TEST(UiV2ScrollPolicy, FinalPresetMatrixCoversLargeMediumSmallAndHorizontal)
{
	const SQmResolvedScrollPolicy Large = QmResolveScrollPolicy({EQmScrollProfile::SETTINGS_PAGE, EQmScrollAxis::VERTICAL, 0.0f, 0}, 1.0f, 0.12f);
	const SQmResolvedScrollPolicy Medium = QmResolveScrollPolicy({EQmScrollProfile::MENU_LIST, EQmScrollAxis::VERTICAL, 24.0f, 3}, 1.0f, 0.0f);
	const SQmResolvedScrollPolicy Small = QmResolveScrollPolicy({EQmScrollProfile::POPUP_LIST, EQmScrollAxis::VERTICAL, 24.0f, 1}, 1.0f, 0.0f);
	const SQmResolvedScrollPolicy Horizontal = QmResolveScrollPolicy({EQmScrollProfile::NUMERIC_FIELD, EQmScrollAxis::HORIZONTAL, 0.0f, 0}, 1.0f, 0.0f);
	EXPECT_GT(Large.m_Style.m_ScrollbarWidth, Medium.m_Style.m_ScrollbarWidth);
	EXPECT_GT(Medium.m_Style.m_ScrollbarWidth, Small.m_Style.m_ScrollbarWidth);
	EXPECT_EQ(Horizontal.m_Style.m_Axis, EQmScrollAxis::HORIZONTAL);
	EXPECT_EQ(Small.m_MaxVisibleItems, 8);
	EXPECT_FLOAT_EQ(Medium.m_AltMultiplier, 3.0f);
}
```

Add `QmMonitoringHelpers.NonCardMenuLegacyUiPathsAreGone` that extracts only the Task 1–4 function bodies and rejects local wheel keys, `ForceShowScrollbar`, local `m_ScrollUnit`, direct scrollbar geometry/colors, old input wrappers and any `SettingsCard`/registry use for business entries. Add `QmMonitoringHelpers.InputFieldForwardingAliasesAreDeletedAfterP7`, which scans `src/game/client/components/` and `UiForms.h/.cpp` and requires zero declarations, definitions or calls of `TextFieldEx`/`TextField`、`SearchFieldEx`/`SearchField`、`ClearableTextFieldEx`/`ClearableTextField`、`IconTextFieldEx`/`IconTextField` and `LegacyTextFieldEx`; `ReadOnlyTextField` is retained only if its own non-editable semantic remains a P3 primitive rather than a forwarding alias. Reuse P4's exact structure tests without rewriting their expectations: `WheelOwnershipFrameBeginsOnlyFromUiUpdate` locks the sole idempotent call in `CUi::Update()`; `DropdownRegistersWheelOwnerBeforeParentCanConsume` requires `ShowPopupSelection(...)` to call `RegisterWheelOwner(pContext, ...)` before `DoPopupMenu(...)`, `PopupSelection(...)` to set `ScrollParams.m_pWheelOwnerId = pSelectionPopup` and `m_WheelOwnerPreRegistered = true` before `CScrollRegion::Begin(...)`, and `CScrollRegion::DoScrollInput()` to call `TryConsumeWheel(pWheelOwnerId, &WheelDelta)`.

- [ ] **Step 2: Verify red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiV2ScrollPolicy.FinalPresetMatrixCoversLargeMediumSmallAndHorizontal:QmMonitoringHelpers.NonCardMenuLegacyUiPathsAreGone:QmMonitoringHelpers.InputFieldForwardingAliasesAreDeletedAfterP7:QmMonitoringHelpers.WheelOwnershipFrameBeginsOnlyFromUiUpdate:QmMonitoringHelpers.DropdownRegistersWheelOwnerBeforeParentCanConsume
```

Expected: FAIL for any remaining page-local alias, wheel key, width/speed override, ownership ordering or business card registration.

- [ ] **Step 3: Delete only the proven obsolete paths**

First run:

```powershell
rg -n "ui_widget::(TextField|SearchField|ClearableTextField|IconTextField|TextFieldEx|SearchFieldEx|ClearableTextFieldEx|IconTextFieldEx)\(" src/game/client/components src/game/client/QmUi/UiForms.h src/game/client/QmUi/UiForms.cpp
```

Convert every remaining menu callsite to the exact P3 `SInputFieldOptions` + `InputField(...)` signature, including global popup, ingame menu, touch-controls editor, assets editor and sixup skin selection callsites. Then delete the forwarding declarations/definitions from `UiForms.h/.cpp`. This is a shell-only migration: retain each caller's buffer, placeholder, search hotkey, clear affordance, IME, commit and business behavior.

Remove obsolete local scroll parameters, duplicate wheel modifier handling and compatibility aliases inside the named functions. Do not delete the P4 `CListBox`/`CScrollRegion` adapters, `CQmScrollState`, `QmResolveScrollPolicy(...)`, `RegisterWheelOwner(...)` or `TryConsumeWheel(...)`. Do not weaken the test to allow a known legacy call.

- [ ] **Step 4: Run green and commit**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiV2ScrollPolicy.*:UiV2WheelOwnership.*:UiV2DropdownPolicy.*:UiV2DropdownIntegration.*:QmMonitoringHelpers.NonCardMenu*:QmMonitoringHelpers.InputFieldForwardingAliasesAreDeletedAfterP7:QmMonitoringHelpers.WheelOwnershipFrameBeginsOnlyFromUiUpdate:QmMonitoringHelpers.DropdownRegistersWheelOwnerBeforeParentCanConsume
git diff --check
git add src/game/client/QmUi/UiForms.h src/game/client/QmUi/UiForms.cpp src/game/client/components/menus.cpp src/game/client/components/menus_browser.cpp src/game/client/components/menus_demo.cpp src/game/client/components/menus_ingame.cpp src/game/client/components/menus_ingame_touch_controls.cpp src/game/client/components/menus_assets_editor.cpp src/game/client/components/menus_settings.cpp src/game/client/components/menus_settings7.cpp src/game/client/components/menus_settings_assets.cpp src/test/QmAnimTest.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "refactor(qmui): 清退非卡片菜单旧 UI 路径" -m "fix: 保留唯一 scroll policy、wheel owner 与 InputField 入口" -m "test: 锁定四 preset、两种 rail 和首轮 wheel 顺序"
```

Expected: focused matrix passes，源码中只剩公共 runtime 和明确的非菜单豁免。

### Task 6: 建立有界缓存、菜单 telemetry 与固定性能预算

**Files:**
- Create: `src/game/client/QmUi/QmUiPerf.h`
- Create: `src/game/client/QmUi/QmUiPerf.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/game/client/components/menus.h`
- Modify: `src/game/client/components/menus.cpp`
- Modify: `src/game/client/components/menus_browser.cpp`
- Modify: `src/game/client/components/menus_demo.cpp`
- Modify: `src/game/client/components/menus_settings_assets.cpp`
- Modify: `src/game/client/components/menus_settings.cpp`
- Modify: `src/test/qmclient_monitoring_test.cpp`
- Modify: `qmclient_scripts/perf/lib/stats.ts`
- Modify: `qmclient_scripts/perf/lib/quality.ts`
- Modify: `qmclient_scripts/perf/lib/report.ts`
- Modify: `qmclient_scripts/perf/analyze.ts`
- Modify: `qmclient_scripts/perf/test.ts`

**Interfaces:**
- Consumes: `CMenus::CurrentQmUiPerfPage()`、`CurrentQmUiPerfOperation()`、`QmPerfEnabled()`、`QmPerfLogPayload(...)` and existing `perf/interaction`/`perf/ui_budget` streams.
- Produces: `SQmMenuUiFramePerf`、`QmLogMenuUiFramePerf(...)` and one `event=menu_ui_frame` schema with `page operation items_total items_visible items_processed items_skipped layout_ms text_ms heap_allocs cache_hits cache_misses cache_evictions`; fixed operations `server_browser_scroll`, `friends_scroll`, `demo_browser_scroll`, `assets_grid_scroll`, `skins_grid_scroll`, `flags_grid_scroll`, `language_list_scroll`, `dropdown_first_wheel`.

- [ ] **Step 1: Write failing C++ cache/telemetry tests**

Add tests that require these exact boundaries:

| Cache | Capacity | Key / invalidation | Cleanup |
|---|---:|---|---|
| page layout | 不保留缓存 | `ResolveSettingsPageLayout(...)` 保持纯计算，不引入跨帧状态 | 无清理责任；若量化后需要缓存，必须先定义真实 owner 和完整 key |
| menu text layout | 4096 entries | text hash, font preset/size/flags, alignment/max width, locale/font/theme generation, UI scale | evict entries unused for 600 UI frames; hard trim to 4096; clear on language/font/backend reset |
| list filter/visibility plan | 不新增共享缓存 | 直接消费各页面现有数据 owner 的当前筛选/排序结果 | 不制造跨页面 generation；未来只能在真实 owner 内按量化结果引入 |
| animation target | soft 4096, hard 8192 | node key, property, target/driver | retain existing prune every 1024 uses and age 8192; clear runtime reset |
| asset metadata | 512 entries | asset/tab/locale/UI scale/tile width/status/install flags | bounded eviction; clear resource directory/backend/shutdown |
| tee preview | 192 entries | skin/dummy/custom colors/emote/source generation | LRU; clear skin reload/shutdown |
| language rows | 128 entries | language inventory/file, font, UI scale, theme generation | invalidate generation; uncached visible-row fallback above 128 |

The test must assert capacities from real cache owners and reject ownerless page-layout/filter cache constants. For retained caches, verify stable keys reuse entries and invalidation clears stale state. Add a telemetry contract test that searches the format string for every `menu_ui_frame` field and proves payload formatting is skipped when `QmPerfEnabled()` is false.

- [ ] **Step 2: Write failing TypeScript budget/report tests**

Add log fixtures for all eight operations and assert `quality.ts` uses these exact budgets:

```ts
export const NON_CARD_MENU_BASELINES = {
  server_browser_scroll: { p95: 8.33, p99: 16.7, max: 33.4, menuMax: 8.0, onePctLow: 60 },
  friends_scroll:        { p95: 8.33, p99: 16.7, max: 33.4, menuMax: 8.0, onePctLow: 60 },
  demo_browser_scroll:   { p95: 8.33, p99: 16.7, max: 33.4, menuMax: 8.0, onePctLow: 60 },
  assets_grid_scroll:    { p95: 12.5, p99: 16.7, max: 33.4, menuMax: 12.0, onePctLow: 60 },
  skins_grid_scroll:     { p95: 12.5, p99: 16.7, max: 33.4, menuMax: 12.0, onePctLow: 60 },
  flags_grid_scroll:     { p95: 8.33, p99: 16.7, max: 33.4, menuMax: 8.0, onePctLow: 60 },
  language_list_scroll:  { p95: 8.33, p99: 16.7, max: 33.4, menuMax: 8.0, onePctLow: 60 },
  dropdown_first_wheel:  { p95: 8.33, p99: 16.7, max: 33.4, menuMax: 8.0, onePctLow: 60 },
} as const;
```

Expected report behavior: missing real-sampled one-percent-low is `FAIL`, missing cache fields is `WARN`, a budget exceedance names the operation and threshold, and the HTML report displays cache hit/miss/eviction plus processed/skipped rows. No scene may pass because its visual work was omitted.

- [ ] **Step 3: Verify both red states**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.*MenuUiPerf*:QmMonitoringHelpers.*CacheBoundary*
Push-Location qmclient_scripts/perf
bun test.ts
npx tsc --noEmit
Pop-Location
```

Expected: C++ fails on missing boundary/schema evidence; TypeScript fails on missing operations, verdicts or report cells.

- [ ] **Step 4: Implement the one facade and bounded reuse in existing owners**

Create this exact facade; it formats one summary and delegates to the existing logger:

```cpp
struct SQmMenuUiFramePerf
{
	const char *m_pPage = nullptr;
	const char *m_pOperation = nullptr;
	int m_ItemsTotal = 0;
	int m_ItemsVisible = 0;
	int m_ItemsProcessed = 0;
	int m_ItemsSkipped = 0;
	float m_LayoutMs = 0.0f;
	float m_TextMs = 0.0f;
	int m_HeapAllocs = 0;
	int m_CacheHits = 0;
	int m_CacheMisses = 0;
	int m_CacheEvictions = 0;
};

void QmLogMenuUiFramePerf(const SQmMenuUiFramePerf &Frame, const IClient *pClient);
```

`QmLogMenuUiFramePerf(...)` 首行检查 `QmPerfEnabled()`；关闭时不得格式化 payload。开启时只调用一次 `QmPerfLogPayload("perf/menu-ui", ...)`，不拥有文件、线程、配置或第二套 fixed-window state。只在根 `CMakeLists.txt` 的既有 QmUi source/test list 登记该 `.cpp/.h`。

Per page frame, record one summary after layout/list processing；increment hits/misses/evictions at the existing cache owner. Cache invalidation must occur before lookup and before drawing, so stale text/rect/input coordinates cannot be displayed for one frame. Visible work is never deferred past input/hit-test；only clip-external metadata/layout work may be skipped or budgeted.

In `menus.cpp/.h`, route page/operation through existing `CurrentQmUiPerf*` and `QmPerfLogPayload`; do not add another config variable. In TypeScript, parse the new fields, aggregate by operation, apply `NON_CARD_MENU_BASELINES`, and render the fixed budget/cache table in the existing report. Extend `analyze.ts` with one deterministic CLI form, `bun analyze.ts <log> --output <html>`; when supplied, it writes HTML to that exact path and the sibling summary JSON under the same directory instead of `%APPDATA%`.

- [ ] **Step 5: Run green tests and commit**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.*MenuUiPerf*:QmMonitoringHelpers.*CacheBoundary*:QmMonitoringHelpers.*Assets*:QmMonitoringHelpers.*Language*
Push-Location qmclient_scripts/perf
bun test.ts
npx tsc --noEmit
Pop-Location
git diff --check
git add CMakeLists.txt src/game/client/QmUi/QmUiPerf.h src/game/client/QmUi/QmUiPerf.cpp src/game/client/components/menus.h src/game/client/components/menus.cpp src/game/client/components/menus_browser.cpp src/game/client/components/menus_demo.cpp src/game/client/components/menus_settings_assets.cpp src/game/client/components/menus_settings.cpp src/test/qmclient_monitoring_test.cpp qmclient_scripts/perf/analyze.ts qmclient_scripts/perf/lib/stats.ts qmclient_scripts/perf/lib/quality.ts qmclient_scripts/perf/lib/report.ts qmclient_scripts/perf/test.ts
git commit -m "perf(menu): 固化非卡片菜单预算与缓存边界" -m "perf: 复用现有 perf telemetry 并限制布局、文本、列表与资源缓存" -m "test: 增加八个固定场景 verdict 和跨语言日志合同"
```

Expected: C++/TypeScript focused tests and typecheck pass；report 对缺失/超预算数据不会给出伪通过。

### Task 7: Dogfood 调参、真实固定场景与单一验收报告

**Files:**
- Modify: `src/game/client/components/qmclient/menus_qmclient.cpp` only if existing Dogfood primitive parameters need evidence-backed value adjustment
- Modify: `src/test/qmclient_monitoring_test.cpp`
- Create: `docs/superpowers/reports/2026-07-11-settings-ui-p7-acceptance.md`
- Create in `tmp/`: `settings-ui-p7-*.log`, `settings-ui-p7-*.html`, screenshots
- Test: real `cmake-build-release/DDNet.exe`

**Interfaces:**
- Consumes: Tasks 1–6、existing `RenderQmUiDogfood`/`dbg_qm_ui_dogfood`、`qm_perf_debug`/`qm_perf_logfile`/`qm_perf_debug_threshold_ms` and `qmclient_scripts/perf/analyze.ts`.
- Produces: primitive tuning evidence, before/after fixed-scene reports, complete manual interaction matrix and an explicit visual-gap ledger.

- [ ] **Step 1: Write the failing acceptance document skeleton with exact matrices**

Create the report with `**P7 evidence status:** collecting` and these completed-at-execution sections; every cell must contain command/result/evidence or an explicit visual gap, never a blank cell:

```markdown
# Settings UI P7 Acceptance Report

## Automated evidence
| Command | Result | Scope | Gap |

## Fixed performance scenes
| Operation | Viewport / UI scale / locale | Repetitions | p50 | p95 | p99 | max | 1% low | menu max | Verdict | Report |

## Manual matrix
| Page | Viewport | UI scale | Locale | Action | Expected | Actual | Screenshot |

## Review findings
| Severity | File | Finding | Resolution | Recheck |

## Remaining visual gaps
| Page | Exact visual difference | Evidence | Follow-up owner |
|---|---|---|---|

## Follow-up specialties outside P7
| Track | Registered scope | P7 status |
| R1 | SegmentedControl、ColorPicker shell、Toggle、Button、slider、modal、toast、font icon 完整公共组件覆盖 | 仅登记，未实施 |
| R2 | 11 tab、Root Panel、完整 L0/L1/L2、导航配置迁移与 Search 跳转语义 | 仅登记，未实施 |
| R3 | Phosphor/MSDF 图标、SDF 圆角/文本及 shader command、GL、Vulkan 管线 | 仅登记，未实施 |
```

Add a red evidence test:

```cpp
TEST(QmMonitoringHelpers, P7AcceptanceEvidenceIsCollected)
{
	const std::string Report = ReadRepoFile("docs/superpowers/reports/2026-07-11-settings-ui-p7-acceptance.md");
	EXPECT_NE(Report.find("**P7 evidence status:** collected"), std::string::npos);
	for(const char *pHeading : {"## Automated evidence", "## Fixed performance scenes", "## Manual matrix", "## Remaining visual gaps", "## Follow-up specialties outside P7"})
		EXPECT_NE(Report.find(pHeading), std::string::npos) << pHeading;

	const auto Trim = [](std::string Value) {
		const size_t First = Value.find_first_not_of(" \t\r\n");
		if(First == std::string::npos)
			return std::string{};
		const size_t Last = Value.find_last_not_of(" \t\r\n");
		return Value.substr(First, Last - First + 1);
	};
	const auto RequireCompleteRow = [&](const char *pKey, int MinimumCells) {
		const size_t KeyPos = Report.find(pKey);
		ASSERT_NE(KeyPos, std::string::npos) << pKey;
		const size_t LineBegin = Report.rfind('\n', KeyPos);
		const size_t LineEnd = Report.find('\n', KeyPos);
		const std::string Line = Report.substr(LineBegin == std::string::npos ? 0 : LineBegin + 1, LineEnd - (LineBegin == std::string::npos ? 0 : LineBegin + 1));
		std::vector<std::string> vCells;
		size_t Cursor = 0;
		while(Cursor < Line.size())
		{
			const size_t Separator = Line.find('|', Cursor);
			const size_t End = Separator == std::string::npos ? Line.size() : Separator;
			const std::string Cell = Trim(Line.substr(Cursor, End - Cursor));
			if(!Cell.empty())
				vCells.push_back(Cell);
			if(Separator == std::string::npos)
				break;
			Cursor = Separator + 1;
		}
		ASSERT_GE((int)vCells.size(), MinimumCells) << Line;
		for(const std::string &Cell : vCells)
		{
			EXPECT_NE(Cell, "collecting") << Line;
			EXPECT_NE(Cell, "pending") << Line;
		}
	};

	for(const char *pCommand : {"| game-client |", "| run_cxx_tests |", "| run_rust_tests |", "| check_docs.py |", "| check_gate.py --mode default |", "| bun test.ts |", "| npx tsc --noEmit |"})
		RequireCompleteRow(pCommand, 4);
	for(const char *pOperation : {"| server_browser_scroll |", "| friends_scroll |", "| demo_browser_scroll |", "| assets_grid_scroll |", "| skins_grid_scroll |", "| flags_grid_scroll |", "| language_list_scroll |", "| dropdown_first_wheel |"})
		RequireCompleteRow(pOperation, 11);
	for(const char *pPage : {"| server_browser |", "| friends |", "| demo_browser |", "| assets |", "| skins |", "| flags |", "| language |", "| dropdown |"})
		RequireCompleteRow(pPage, 8);
	RequireCompleteRow("review-readback", 5);
	for(const char *pTrack : {"| R1 |", "| R2 |", "| R3 |"})
		RequireCompleteRow(pTrack, 3);

	const size_t GapHeading = Report.find("## Remaining visual gaps");
	ASSERT_NE(GapHeading, std::string::npos);
	const size_t GapSectionBegin = Report.find('\n', GapHeading);
	ASSERT_NE(GapSectionBegin, std::string::npos);
	const size_t GapSectionEnd = Report.find("\n## ", GapSectionBegin + 1);
	const std::string GapSection = Report.substr(
		GapSectionBegin + 1,
		GapSectionEnd == std::string::npos ? std::string::npos : GapSectionEnd - GapSectionBegin - 1);

	std::vector<std::vector<std::string>> vVisualGapRows;
	int HeaderRows = 0;
	int SeparatorRows = 0;
	for(size_t LineBegin = 0; LineBegin <= GapSection.size();)
	{
		const size_t LineEnd = GapSection.find('\n', LineBegin);
		const std::string Line = Trim(GapSection.substr(
			LineBegin,
			LineEnd == std::string::npos ? std::string::npos : LineEnd - LineBegin));
		if(Line.size() >= 2 && Line.front() == '|' && Line.back() == '|')
		{
			std::vector<std::string> vCells;
			size_t CellBegin = 1;
			while(CellBegin < Line.size())
			{
				const size_t CellEnd = Line.find('|', CellBegin);
				ASSERT_NE(CellEnd, std::string::npos) << Line;
				vCells.push_back(Trim(Line.substr(CellBegin, CellEnd - CellBegin)));
				CellBegin = CellEnd + 1;
			}
			ASSERT_EQ(vCells.size(), 4u) << "malformed visual gap row: " << Line;
			const bool Header =
				vCells[0] == "Page" &&
				vCells[1] == "Exact visual difference" &&
				vCells[2] == "Evidence" &&
				vCells[3] == "Follow-up owner";
			bool Separator = true;
			for(const std::string &Cell : vCells)
			{
				if(Cell.empty() || Cell.find_first_not_of("-:") != std::string::npos)
				{
					Separator = false;
					break;
				}
			}
			if(Header)
				++HeaderRows;
			else if(Separator)
				++SeparatorRows;
			else
				vVisualGapRows.push_back(vCells);
		}
		if(LineEnd == std::string::npos)
			break;
		LineBegin = LineEnd + 1;
	}

	ASSERT_EQ(HeaderRows, 1) << "Remaining visual gaps must have exactly one four-column header";
	ASSERT_EQ(SeparatorRows, 1) << "Remaining visual gaps must have exactly one four-column separator";
	ASSERT_FALSE(vVisualGapRows.empty())
		<< "Remaining visual gaps must contain the complete none sentinel or at least one real gap";
	int SentinelRows = 0;
	int RealGapRows = 0;
	int InvalidGapRows = 0;
	for(const std::vector<std::string> &Row : vVisualGapRows)
	{
		const bool CompleteSentinel =
			Row[0] == "none" &&
			Row[1] == "visual-gap-none" &&
			Row[2] == "no visual gap recorded" &&
			Row[3] == "closed";
		bool ContainsSentinel = false;
		for(const std::string &Cell : Row)
			ContainsSentinel = ContainsSentinel || Cell.find("visual-gap-none") != std::string::npos;
		if(CompleteSentinel)
		{
			++SentinelRows;
			continue;
		}
		if(ContainsSentinel)
		{
			++InvalidGapRows;
			continue;
		}

		bool CompleteRealGap = true;
		for(const std::string &Cell : Row)
		{
			if(Cell.empty() || Cell == "-" || Cell == "N/A" || Cell == "none" ||
				Cell == "collecting" || Cell == "pending" || Cell == "no visual gap recorded" || Cell == "closed")
			{
				CompleteRealGap = false;
				break;
			}
		}
		if(CompleteRealGap)
			++RealGapRows;
		else
			++InvalidGapRows;
	}

	const bool NoVisualGapState =
		SentinelRows == 1 && RealGapRows == 0 && InvalidGapRows == 0 && vVisualGapRows.size() == 1;
	const bool RealVisualGapState =
		SentinelRows == 0 && RealGapRows >= 1 && InvalidGapRows == 0 &&
		static_cast<size_t>(RealGapRows) == vVisualGapRows.size();
	EXPECT_TRUE(NoVisualGapState || RealVisualGapState)
		<< "visual gaps require exactly one legal state: complete none sentinel or complete real rows; sentinel_rows="
		<< SentinelRows << " real_rows=" << RealGapRows << " invalid_rows=" << InvalidGapRows;
}
```

在 `src/test/qmclient_monitoring_test.cpp` 的 include block 显式保留 `<string>` 和 `<vector>`；测试不得依赖其他 include 间接提供这些类型。

验收报告中的 automated/performance/manual 行必须使用上述稳定 key。Review 无 finding 时写 `| none | review-readback | no findings | no action | reviewer readback attached |`；有 finding 时每条填完整 resolution/recheck，并仍增加该 readback 行代表“无剩余 finding”。

`Remaining visual gaps` 必须接受且只接受两个互斥状态：

1. 没有视觉 gap：只写完整 sentinel `| none | visual-gap-none | no visual gap recorded | closed |`，该节不得出现第二条数据行。
2. 存在允许保留的真实视觉 gap：完全不写 sentinel，写一条或多条 `| page | exact visual difference | evidence | follow-up owner |`；四个字段必须都有实际内容，不能使用 `-`、`N/A`、`none`、`collecting`、`pending`、`no visual gap recorded`、`closed` 或 `visual-gap-none`，字段内容也不得包含额外的 Markdown 列分隔符 `|`。

解析器只截取 `Remaining visual gaps` 到下一条二级标题之间的 Markdown 表，因此不会把 R1–R3 当成视觉 gap。`NoVisualGapState` 与 `RealVisualGapState` 的互斥判定允许字段完整的真实视觉 gap 通过，同时拒绝空表、残缺 sentinel、残缺真实 gap、重复 sentinel，以及 sentinel 与真实 gap 共存。

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.P7AcceptanceEvidenceIsCollected
```

Expected: FAIL because evidence status is still `collecting` and all required evidence rows are still absent.

- [ ] **Step 2: Build and use Dogfood only as primitive evidence**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
cmake-build-release/DDNet.exe
```

In the client console set `dbg_qm_ui_dogfood 1`. Verify runtime theme change, InputField focus/clear/IME, `large/medium/small/horizontal`, `AUTO/HIDDEN`, Alt 3x and short/long dropdown ownership. Record screenshots under `tmp/`; adjust only existing primitive constants when both Dogfood and a real page show the same defect. Do not add controls, tabs, icons or shader demonstrations.

- [ ] **Step 3: Capture identical fixed performance scenes**

Before each scene set:

```text
qm_perf_debug 1
qm_perf_logfile 1
qm_perf_debug_threshold_ms 4
```

Use windowed `1920x1080`, renderer recorded from system info, UI scale `100%`, Simplified Chinese, VSync state recorded. For each of the eight operations: reopen the page, discard 30 warmup frames, perform 10 identical wheel gestures over 300 sampled frames, repeat three times, and retain the median run. Assets/skins must keep previews enabled. Generate reports:

```powershell
bun qmclient_scripts/perf/analyze.ts tmp/settings-ui-p7-before.log --output tmp/settings-ui-p7-before.html
bun qmclient_scripts/perf/analyze.ts tmp/settings-ui-p7-after.log --output tmp/settings-ui-p7-after.html
```

Expected: every operation meets Task 6 budget, or the responsible code is fixed and the identical scene rerun. A functional or non-visual failure cannot remain as a gap.

- [ ] **Step 4: Execute the manual interaction matrix**

Run every target page at `1920x1080/100%` and `1280x720/125%`; repeat language-sensitive rows in English and Simplified Chinese. Cover:

```text
server browser: overflow/no-overflow, refresh, search/clear, keyboard selection, wheel, Alt wheel
friends: categories expanded/collapsed, category drag, popup, first wheel, parent scroll
Demo: replay/screenshot source, search, variable preview row, keyboard, short dropdown
assets: every asset tab, local/workshop filter, thumbnail loading, selection, scroll recovery
skins: filter, sort dropdown, visible preview, dummy/custom colors, long session re-entry
flags: two-row hidden-rail filter grid, keyboard, Ctrl/GUI/Shift non-consumption
language: overflow/no-overflow, selection, active-language rebuild, UI scale change
dropdown: 2/8/9 items, parent-limited height, partial anchor, fully offscreen anchor, first wheel
```

Expected: no overlap, stale text/rect, input offset, rail flash, wheel leak or business-card drag affordance. Only visual differences with correct behavior may enter `Remaining visual gaps`.

- [ ] **Step 5: Mark the evidence collected, verify green and commit**

Only after every Task 7 matrix row has actual evidence, change the marker to `**P7 evidence status:** collected`.

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.P7AcceptanceEvidenceIsCollected
git add docs/superpowers/reports/2026-07-11-settings-ui-p7-acceptance.md src/test/qmclient_monitoring_test.cpp
git add src/game/client/components/qmclient/menus_qmclient.cpp
git diff --cached --check
git commit -m "docs(settings-ui): 记录 P7 性能与人工验收" -m "test: 固化 Dogfood primitive、八个固定场景和非卡片交互矩阵" -m "docs: 登记 R1、R2、R3 后续专项与视觉 gap"
```

Expected: focused evidence test passes；staging 时若 Dogfood 源码未调整，第二个 `git add` 无需执行；commit 只包含测试、实际调参和验收报告，不包含 `tmp/` 产物。

### Task 8: 全自动验证、独立只读 review、版本与最终提交

**Files:**
- Modify: `src/game/version.h`
- Modify: `docs/info.json`
- Modify: `docs/superpowers/reports/2026-07-11-settings-ui-p7-acceptance.md`
- Modify: `src/test/qmclient_monitoring_test.cpp`
- Modify: only files directly required to resolve returned review findings

**Interfaces:**
- Consumes: Tasks 1–7 commits and acceptance evidence.
- Produces: QmClient `2.74.24`、串行全量验证记录、独立 findings-first review 返回结果，以及仅含明确视觉 gap 的 P0–P7 handoff。

- [ ] **Step 1: Write the failing final-closure test**

```cpp
TEST(QmMonitoringHelpers, P7FinalVersionIsSynchronized)
{
	const std::string Version = ReadRepoFile("src/game/version.h");
	const std::string Info = ReadRepoFile("docs/info.json");
	EXPECT_NE(Version.find("QMCLIENT_VERSION \"2.74.24\""), std::string::npos);
	EXPECT_NE(Info.find("\"version\": \"2.74.24\""), std::string::npos);
}
```

- [ ] **Step 2: Rebuild and verify the final gate is red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.P7FinalVersionIsSynchronized
```

Expected: FAIL on version `2.74.23`; review status remains a process/readback gate and is not faked by a source-string test.

- [ ] **Step 3: Apply the single MMP version update**

Run:

```powershell
python qmclient_scripts/bump_version.py --tag v2.74.24
rg -n "2\.74\.15" src/game/version.h docs/info.json
```

Expected: both files contain `2.74.24`; no other version or release file changes.

- [ ] **Step 4: Dispatch and wait for an independent read-only review**

Create one fresh read-only review agent with scope equal to all P7 commits plus version/report diff. Instruct it to read `docs/ai-workflow/review.md`, list findings before conclusion, focus on wheel ownership ordering, cache lifetime/bounds, stale rect/text, hot-path allocation, non-card boundary, telemetry/report schema and test quality. Do not let that reviewer edit files or dispatch another agent. Wait for its report; do not cancel or claim completion while it is running.

Expected: review report returns. For every finding, apply the smallest fix and rebuild `testrunner` to rerun the focused reproduction, then send the resulting diff back to the same read-only reviewer for a follow-up pass. Repeat until the returned findings list has no unresolved correctness, ownership, lifetime, performance-contract or non-visual finding.

- [ ] **Step 5: Close the report markers and verify the TDD gate is green**

Update the acceptance report with review findings/resolutions and version `2.74.24`. Set `**Independent review status:** correct` only after the returned report has no unresolved finding; do not set the overall P7 status complete before the full serial gates finish.

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.P7AcceptanceEvidenceIsCollected:QmMonitoringHelpers.P7FinalVersionIsSynchronized
```

Expected: both focused closure tests pass; this is the green state for the Task 8 red test.

- [ ] **Step 6: Run the complete serial automated sequence**

Run one command at a time in this order:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiV2ScrollPolicy.*:UiV2WheelOwnership.*:UiV2DropdownPolicy.*:UiV2DropdownIntegration.*:QmMonitoringHelpers.NonCardMenu*:QmMonitoringHelpers.InputFieldForwardingAliasesAreDeletedAfterP7:QmMonitoringHelpers.WheelOwnershipFrameBeginsOnlyFromUiUpdate:QmMonitoringHelpers.DropdownRegistersWheelOwnerBeforeParentCanConsume:QmMonitoringHelpers.*MenuUiPerf*:QmMonitoringHelpers.*CacheBoundary*:QmMonitoringHelpers.P7*
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_rust_tests -j 14
Push-Location qmclient_scripts/perf
bun test.ts
npx tsc --noEmit
Pop-Location
python qmclient_scripts/gate/check_docs.py --sync-only --prefer agents
python qmclient_scripts/gate/check_docs.py
python qmclient_scripts/gate/check_gate.py --mode default --explain-scope --report-json-path tmp/settings-ui-p7-default-gate.json
git diff --check
```

Expected: every command exits `0`. `testrunner` is rebuilt before focused tests; standalone `run_cxx_tests` and `run_rust_tests` provide full regressions; default gate is an additional repository gate, not their substitute.

- [ ] **Step 7: Run full only as the additional concentrated-closure gate**

Run:

```powershell
python qmclient_scripts/gate/check_gate.py --mode full --explain-scope --report-json-path tmp/settings-ui-p7-full-gate.json
```

Expected: exit `0`. Full adds heavy/static checks and is never cited as a replacement for `run_cxx_tests` or `run_rust_tests`;若失败，先在 acceptance report 分类环境/基线/当前改动并修复或恢复环境后重跑，full 未返回 `0` 时 P7 不完成。

- [ ] **Step 8: Stage the exact closure files and commit once**

Write the actual Step 6/7 command results into the acceptance report. Only after every required command is `0`, set `**P7 status:** complete`; if any command is not green, keep the report non-complete and return to the owning task.

Run:

```powershell
git status --short
git diff -- CMakeLists.txt src/game/version.h docs/info.json docs/superpowers/reports/2026-07-11-settings-ui-p7-acceptance.md src/game/client/QmUi/UiForms.h src/game/client/QmUi/UiForms.cpp src/game/client/QmUi/QmUiPerf.h src/game/client/QmUi/QmUiPerf.cpp src/game/client/components/menus.h src/game/client/components/menus.cpp src/game/client/components/menus_browser.cpp src/game/client/components/menus_demo.cpp src/game/client/components/menus_ingame.cpp src/game/client/components/menus_ingame_touch_controls.cpp src/game/client/components/menus_assets_editor.cpp src/game/client/components/menus_settings.cpp src/game/client/components/menus_settings7.cpp src/game/client/components/menus_settings_assets.cpp src/game/client/components/qmclient/menus_qmclient.cpp src/test/QmAnimTest.cpp src/test/qmclient_monitoring_test.cpp src/test/settings_warmup_test.cpp src/test/skins_test.cpp qmclient_scripts/perf/analyze.ts qmclient_scripts/perf/test.ts qmclient_scripts/perf/lib/stats.ts qmclient_scripts/perf/lib/quality.ts qmclient_scripts/perf/lib/report.ts
git add -p -- CMakeLists.txt src/game/version.h docs/info.json docs/superpowers/reports/2026-07-11-settings-ui-p7-acceptance.md src/game/client/QmUi/UiForms.h src/game/client/QmUi/UiForms.cpp src/game/client/QmUi/QmUiPerf.h src/game/client/QmUi/QmUiPerf.cpp src/game/client/components/menus.h src/game/client/components/menus.cpp src/game/client/components/menus_browser.cpp src/game/client/components/menus_demo.cpp src/game/client/components/menus_ingame.cpp src/game/client/components/menus_ingame_touch_controls.cpp src/game/client/components/menus_assets_editor.cpp src/game/client/components/menus_settings.cpp src/game/client/components/menus_settings7.cpp src/game/client/components/menus_settings_assets.cpp src/game/client/components/qmclient/menus_qmclient.cpp src/test/QmAnimTest.cpp src/test/qmclient_monitoring_test.cpp src/test/settings_warmup_test.cpp src/test/skins_test.cpp qmclient_scripts/perf/analyze.ts qmclient_scripts/perf/test.ts qmclient_scripts/perf/lib/stats.ts qmclient_scripts/perf/lib/quality.ts qmclient_scripts/perf/lib/report.ts
git diff --cached --name-only
git diff --cached --check
git commit -m "feat(settings-ui): 收口非卡片菜单与性能体系" -m "fix: 统一列表网格的 theme、input、scroll 与 dropdown ownership" -m "perf: 固化有界缓存和八个菜单场景预算" -m "test: 完成 C++/Rust/perf/default/full、人工矩阵与独立审查" -m "chore: 更新 QmClient 版本至 2.74.24"
```

Expected: inspect the displayed diff before staging, accept only P7/review hunks, then verify the cached path list contains no unrelated concurrent file or hunk. The commit contains only P7 closure/review files. The acceptance report proves all automated checks, non-visual behavior and review findings are closed; remaining items are evidence-backed visual gaps and the explicitly out-of-P7 R1–R3 tracks.

---

## Self-review

- Spec coverage: 非卡片边界、runtime theme/input、四 preset、`AUTO/HIDDEN`、Alt 三倍、行步长、两行 filter grid、dropdown 八项/父 viewport/首轮 wheel、缓存与性能预算、Dogfood、结构清退、自动/人工验收和独立 review 均有对应任务。
- Completeness scan: 所有路径、接口、容量、阈值、版本、命令、expected result 和后续专项边界均为确定内容，实施内容完整。
- Type consistency: P7 只消费 `SUiTheme`/`ResolveUiTheme(...)`、`IUiContext::m_pTheme`、`ui_widget::InputField(...)`、`CQmScrollState`、`QmResolveScrollPolicy(...)`、`CUi::RegisterWheelOwner(...)`/`TryConsumeWheel(...)`，并只创建 `SQmMenuUiFramePerf`/`QmLogMenuUiFramePerf(...)` 这一层 existing-logger facade；未定义第二套 wheel/policy/logger。
- Scope boundary: 服务器、好友、Demo、资产、皮肤、国旗、语言条目保持列表/网格；R1、R2、R3 只登记；协议、格式、物理、预测、回放与 shader/资源管线未混入。
- Exit gate: standalone C++/Rust full tests、perf tests/typecheck、docs、default、附加 full、真实固定场景、人工矩阵和独立只读 review 全部有返回证据；只有视觉 gap 可以留存。
