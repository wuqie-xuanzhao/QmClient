# QmClient 设置页 UI 统一 P4 Scroll 与 Dropdown Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 `CScrollRegion`、QmUi container、`CListBox`、设置页和 popup 共享一个 `CQmScrollState` 与 `QmResolveScrollPolicy(...)`，并用同帧 wheel owner 保证长 dropdown 的首个滚轮不会泄漏给父页面。

**Architecture:** `CQmScrollState` 吸收 offset、velocity、native animation、thumb/content drag 等全部可变滚动状态；`CQmScrollController` 变为无状态的 frame/geometry adapter，`CScrollRegion` 只保留测量、clip、style params 和 UI item ID 等非滚动交互状态。`CUi` 每帧维护一个按 priority 解析的 wheel owner；dropdown 在父 `CScrollRegion::End()` 消费输入前、创建 popup anchor 时登记，父/popup 都只能通过 `TryConsumeWheel(...)` 获取同一份 raw wheel。

**Tech Stack:** C++、QmUi `QmScroll`/`QmDropdown`、DDNet `CUi`/`CScrollRegion`/`CListBox`、GoogleTest、CMake/MSVC。

## Global Constraints

- 保留已落地的 `QmResolveScrollPolicy(...)`、`AUTO/HIDDEN`、ListBox profile、Alt `3.0x` 与 dropdown viewport geometry；本计划收口状态和真实生产调用，不重写已验证 policy。
- offset、velocity、target animation、thumb drag 和 content drag 只存在于 `CQmScrollState`；adapter 不保存同义字段。未分派的 raw wheel 只存在 `CScrollWheelOwnership`，不是 scroll offset state。
- rail 只有 `AUTO`/`HIDDEN`；无 overflow 时不绘制、不预留宽度、不扩大 hot rect，hidden rail 仍允许 wheel/keyboard/clip。
- preset 固定为：settings `LARGE`、menu list `MEDIUM`、popup `SMALL`、filter grid `SMALL + HIDDEN`、numeric `HORIZONTAL`。
- list step 来自 row extent/rows per step；filter grid 固定两行，popup 固定一行且最多八项可见。
- Alt 只把已解析 wheel delta 乘 `3.0`；Ctrl/GUI/Shift 本身不得让 raw wheel 消失。若 `NumericField`/其他复合控件登记了更高优先级 owner，wheel 由该控件消费，而不是被 modifier 隐式吞掉。
- 短 dropdown 无 overflow 时以 `Eligible=false` 登记 popup owner 候选，不参与竞争，保持打开并让父页面滚动；长/受限 dropdown 以 `Eligible=true` 登记 popup owner 并显示 small rail。
- popup owner 必须在父 scroll region 消费前登记；不接受“下一帧 hot region”或“滚轮关闭 popup”的规避。
- 聊天/控制台文本滚动语义不在本计划；P7 才迁全部非卡片菜单 adapter。
- 同一 `cmake-build-release` 目标串行；版本更新留给 P7。

## Progress

- [x] 2026-07-12：CScrollRegion 已接入 CQmScrollState，清退本地 offset、动画和 thumb-grab 状态；保留稳定 rail ID、内容临时不溢出时的拖拽连续性，以及 ScrollHere/热键/边缘滚动语义。
- [ ] CQmScrollController 与 UiContainers 的 state 注入、同帧 wheel owner、dropdown 迁移仍按下列任务继续。

---

## File Structure

- Modify: `src/game/client/QmUi/QmScroll.h/.cpp` — 单一 state、programmatic/smooth-target API、无状态 controller、profile/preset 解析。
- Modify: `src/game/client/ui_scrollregion.h/.cpp` — 嵌入 `CQmScrollState`，删除重复 offset/animation/input state。
- Modify: `src/game/client/QmUi/UiContainers.h` — scroll container 接收外部 `CQmScrollState &`。
- Modify: `src/game/client/ui_listbox.h/.cpp` — 只通过 menu-list policy/scroll-region adapter。
- Modify: `src/game/client/ui.h/.cpp` — `ui.h` header-inline wheel router/production seam，`ui.cpp` 的 lifecycle wrapper 和 dropdown 生产接线。
- Modify: `src/game/client/QmUi/QmDropdown.h/.cpp` — 八项 cap、父 viewport、owner eligibility。
- Modify: `src/game/client/components/menus.h/.cpp` — 设置页 scroll helper 传 `SETTINGS_PAGE` request，不重写参数。
- Modify: `src/test/QmAnimTest.cpp` — state/policy/owner/dropdown-parent integration。
- Modify: `src/test/qmclient_monitoring_test.cpp` — 生产顺序与重复状态删除断言。

---

### Task 1: 把全部滚动可变状态收口到 CQmScrollState

**Files:**
- Modify: `src/game/client/QmUi/QmScroll.h`
- Modify: `src/game/client/QmUi/QmScroll.cpp`
- Modify: `src/game/client/ui_scrollregion.h`
- Modify: `src/game/client/ui_scrollregion.cpp`
- Modify: `src/game/client/QmUi/UiContainers.h`
- Test: `src/test/QmAnimTest.cpp`
- Test: `src/test/qmclient_monitoring_test.cpp`

**Interfaces:**
- Consumes: 现有 `SQmScrollMetrics`、`SQmScrollContainerInput`、`SQmResolvedScrollPolicy`。
- Produces: `CQmScrollState::ScrollTo(...)`/`ScrollBy(...)`/`Advance(...)`、无状态 `CQmScrollController::Update(...)`、`CScrollRegion::State()`。

- [ ] **Step 1: Write failing shared-state behavior tests**

```cpp
TEST(UiV2ScrollState, RegionAndContainerAdaptersAdvanceTheSameState)
{
	CQmScrollState State;
	SQmScrollMetrics Metrics{200.0f, 800.0f};
	SQmScrollConfig Config = QmSettingsScrollConfig(1.0f, 0.18f);
	State.AddWheelImpulse(-120.0f, Metrics, Config);
	State.Advance(1.0f / 60.0f, Metrics, Config);
	const float AfterWheel = State.Offset();
	EXPECT_GT(AfterWheel, 0.0f);
	State.SetOffset(300.0f, Metrics, Config);
	EXPECT_FLOAT_EQ(State.Offset(), 300.0f);
	EXPECT_NE(State.Offset(), AfterWheel);
}

TEST(UiV2ScrollState, ContentShrinkClampsOffsetWithoutAdapterReset)
{
	CQmScrollState State;
	SQmScrollConfig Config = QmNativeWheelScrollConfig(1.0f, 0.5f);
	State.ScrollTo(500.0f, {200.0f, 800.0f}, Config, EQmScrollMotion::SMOOTH);
	State.Advance(0.1f, {200.0f, 800.0f}, Config);
	ASSERT_GT(State.Offset(), 0.0f);
	ASSERT_LT(State.Offset(), 500.0f);
	State.Advance(0.0f, {200.0f, 260.0f}, Config);
	EXPECT_GE(State.Offset(), 0.0f);
	EXPECT_LE(State.Offset(), 60.0f);
	EXPECT_FLOAT_EQ(State.TargetOffset(), 60.0f);
	for(int Frame = 0; Frame < 60; ++Frame)
		State.Advance(1.0f / 60.0f, {200.0f, 260.0f}, Config);
	EXPECT_FLOAT_EQ(State.Offset(), 60.0f);
	EXPECT_FALSE(State.Animating());
}

TEST(UiV2ScrollState, ProgrammaticInstantScrollUsesTheSharedState)
{
	CQmScrollState State;
	const SQmScrollMetrics Metrics{200.0f, 800.0f};
	const SQmScrollConfig Config = QmNativeWheelScrollConfig(1.0f, 0.5f);
	State.SetOffset(100.0f, Metrics, Config);
	State.ScrollBy(40.0f, Metrics, Config, EQmScrollMotion::INSTANT);
	EXPECT_FLOAT_EQ(State.Offset(), 140.0f);
	EXPECT_FLOAT_EQ(State.TargetOffset(), 140.0f);
	EXPECT_FALSE(State.Animating());
}

TEST(UiV2ScrollState, RestoreWaitsForValidMetricsAndRemainsSingleSource)
{
	CQmScrollState State;
	const SQmScrollConfig Config = QmNativeWheelScrollConfig(1.0f, 0.0f);
	State.RestoreOffset(220.0f, {0.0f, 0.0f}, Config);
	EXPECT_FLOAT_EQ(State.Offset(), 0.0f);
	State.Advance(0.0f, {200.0f, 800.0f}, Config);
	EXPECT_FLOAT_EQ(State.Offset(), 220.0f);
	EXPECT_FLOAT_EQ(State.TargetOffset(), 220.0f);
}

TEST(UiV2ScrollState, SmoothProgrammaticScrollAccumulatesFromThePendingTarget)
{
	CQmScrollState State;
	const SQmScrollMetrics Metrics{200.0f, 800.0f};
	const SQmScrollConfig Config = QmNativeWheelScrollConfig(1.0f, 0.5f);
	State.ScrollTo(300.0f, Metrics, Config, EQmScrollMotion::SMOOTH);
	EXPECT_FLOAT_EQ(State.Offset(), 0.0f);
	EXPECT_FLOAT_EQ(State.TargetOffset(), 300.0f);
	State.Advance(0.1f, Metrics, Config);
	ASSERT_GT(State.Offset(), 0.0f);
	ASSERT_LT(State.Offset(), 300.0f);
	State.ScrollBy(50.0f, Metrics, Config, EQmScrollMotion::SMOOTH);
	EXPECT_FLOAT_EQ(State.TargetOffset(), 350.0f);
	for(int Frame = 0; Frame < 60; ++Frame)
		State.Advance(1.0f / 60.0f, Metrics, Config);
	EXPECT_FLOAT_EQ(State.Offset(), 350.0f);
}
```

Source deletion assertion:

```cpp
TEST(QmMonitoringHelpers, ScrollRegionOwnsOnlySharedQmScrollState)
{
	const std::string Header = ReadRepoFile("src/game/client/ui_scrollregion.h");
	EXPECT_NE(Header.find("CQmScrollState m_State;"), std::string::npos);
	EXPECT_EQ(Header.find("float m_ScrollPos;"), std::string::npos);
	EXPECT_EQ(Header.find("float m_AnimTime;"), std::string::npos);
	EXPECT_EQ(Header.find("float m_AnimTargetScrollPos;"), std::string::npos);
	EXPECT_EQ(Header.find("m_RequestScrollPos"), std::string::npos);
	EXPECT_EQ(Header.find("m_ScrollDirection"), std::string::npos);
	EXPECT_EQ(Header.find("m_ScrollSpeedMultiplier"), std::string::npos);
	const std::string QmHeader = ReadRepoFile("src/game/client/QmUi/QmScroll.h");
	EXPECT_EQ(QmHeader.find("CQmScrollState m_State;"), std::string::npos);
}
```

- [ ] **Step 2: Run tests to verify red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiV2ScrollState.*:QmMonitoringHelpers.ScrollRegionOwnsOnlySharedQmScrollState
```

Expected: source deletion test FAIL because both adapters still own duplicate state fields.

- [ ] **Step 3: Move interaction state into CQmScrollState**

`CQmScrollState` 最终公开/私有 contract：

```cpp
class CQmScrollState
{
public:
	void Reset();
	void SetOffset(float Offset, const SQmScrollMetrics &Metrics, const SQmScrollConfig &Config = {}, bool AllowOverscroll = false);
	void RestoreOffset(float Offset, const SQmScrollMetrics &Metrics, const SQmScrollConfig &Config = {});
	void ScrollTo(float TargetOffset, const SQmScrollMetrics &Metrics, const SQmScrollConfig &Config, EQmScrollMotion Motion);
	void ScrollBy(float Delta, const SQmScrollMetrics &Metrics, const SQmScrollConfig &Config, EQmScrollMotion Motion);
	void AddWheelImpulse(float WheelDelta, const SQmScrollMetrics &Metrics, const SQmScrollConfig &Config = {});
	void Advance(float Dt, const SQmScrollMetrics &Metrics, const SQmScrollConfig &Config = {}, bool PauseNativeWheelAnimation = false);
	SQmScrollContainerFrame Update(const CUIRect &ViewRect, float ContentSize, float Dt, const SQmScrollContainerInput &Input, const SQmResolvedScrollPolicy &Policy);
	float Offset() const { return m_Offset; }
	float TargetOffset() const { return m_AnimTime > 0.0f ? m_AnimTargetOffset : m_Offset; }
	float Velocity() const { return m_Velocity; }
	bool Animating() const { return m_AnimTime > 0.0f; }
	bool ScrollbarDragActive() const { return m_ScrollbarDragActive; }
	bool ContentDragActive() const { return m_ContentDragActive; }

private:
	float m_Offset = 0.0f;
	float m_Velocity = 0.0f;
	float m_LastMaxOffset = 0.0f;
	float m_AnimTime = 0.0f;
	float m_AnimTimeMax = 0.0f;
	float m_AnimStartOffset = 0.0f;
	float m_AnimTargetOffset = 0.0f;
	bool m_ScrollbarDragActive = false;
	float m_ScrollbarGrabPosition = 0.0f;
	bool m_ContentDragActive = false;
	bool m_ContentDragCandidate = false;
	float m_ContentDragPressPosition = 0.0f;
	float m_ContentDragPressOffset = 0.0f;
	float m_ContentDragLastPosition = 0.0f;
	float m_PendingRestoreOffset = 0.0f;
	bool m_HasPendingRestore = false;
};
```

`EQmScrollMotion` 是精确的 two-state contract：

```cpp
enum class EQmScrollMotion
{
	INSTANT,
	SMOOTH,
};
```

`ScrollTo(...)` 先把 target clamp 到 `[0, Metrics.MaxOffset()]`；`INSTANT` 立即设置 offset 并清空 velocity/animation，`SMOOTH` 以当前 offset 为 start，以 `Config.m_NativeWheelAnimationTime` 为时长。`ScrollBy(...)` 在已有 smooth animation 时必须以 `TargetOffset()` 为 base，否则以 `Offset()` 为 base，再唯一委托 `ScrollTo(...)`。`AddWheelImpulse(...)` 的 native-step 分支也只计算 `-WheelDelta / 120.0f * Config.m_WheelScale`，然后委托 `ScrollBy(..., SMOOTH)`；因此 router 解析后的 `-360` 会精确变成三倍步长，不再被 sign-only 逻辑降为一步。

`RestoreOffset(...)` 用于 `SetScrollOffsetY(...)` 在 `Begin(...)` 前的现有恢复调用：它把非负 offset 存入 `m_PendingRestoreOffset`；若 `Metrics.m_ViewportSize > 0.0f` 当场用 `SetOffset(...)` 应用并清除 pending，否则由下一次 `Advance(...)` 在有效 viewport 下应用。有效 metrics 即使 `MaxOffset() == 0.0f` 也必须把恢复值 clamp 为零并清 pending，避免在以后页面意外复活。`Reset()` 同时清除 pending restore 和所有 drag state。

`Advance(...)` 每帧都同时 clamp `m_Offset`/`m_AnimStartOffset`/`m_AnimTargetOffset`；content shrink 后 target 若与 offset 相同就立即结束 animation，不允许下一帧回弹到旧 target。

- [ ] **Step 4: Make adapters state-free**

`CQmScrollController` 只保留：

```cpp
class CQmScrollController
{
public:
	static SQmScrollContainerFrame PreviewFrame(const CQmScrollState &State, const CUIRect &ViewRect, float ContentSize, const SQmResolvedScrollPolicy &Policy);
	static SQmScrollContainerFrame Update(CQmScrollState &State, const CUIRect &ViewRect, float ContentSize, float Dt, const SQmScrollContainerInput &Input, const SQmResolvedScrollPolicy &Policy);
};
```

`CScrollRegion` 用 `m_State.Offset()` 产生 `m_ContentScrollOff`；`SetScrollOffsetY` 和 thumb drag 调 `SetOffset(...)`，`ScrollRelativeDirect` 调 `ScrollBy(..., INSTANT)`，`ScrollRelative`/`ScrollHere` 调 `ScrollBy`/`ScrollTo(..., SMOOTH)`。删除 `m_RequestScrollPos`、`m_ScrollDirection`、`m_ScrollSpeedMultiplier`、`StartScrollAnimation(...)` 和 `AdvanceAnimation()`；region 不得保留等待下一帧的可变滚动请求。`UiContainers::ScrollContainer(...)` 参数从 `CQmScrollContainer &State` 改为 `CQmScrollState &State`。

programmatic adapter 方法完整替换为：

```cpp
void CScrollRegion::ScrollRelative(EScrollRelative Direction, float SpeedMultiplier)
{
	if(Direction == SCROLLRELATIVE_NONE)
		return;
	const float Delta = static_cast<int>(Direction) * m_Params.m_ScrollUnit * maximum(0.0f, SpeedMultiplier);
	m_State.ScrollBy(Delta, ScrollMetrics(), ScrollConfig(), EQmScrollMotion::SMOOTH);
}

void CScrollRegion::ScrollRelativeDirect(vec2 ScrollAmount)
{
	const float Delta = m_Params.m_ScrollHorizontal ? ScrollAmount.x : ScrollAmount.y;
	m_State.ScrollBy(Delta, ScrollMetrics(), ScrollConfig(), EQmScrollMotion::INSTANT);
	SyncContentOffsetFromState();
}

void CScrollRegion::ScrollRelativeDirect(float ScrollAmount)
{
	ScrollRelativeDirect(vec2(0.0f, ScrollAmount));
}

void CScrollRegion::SetScrollOffsetY(float OffsetY)
{
	m_State.RestoreOffset(maximum(0.0f, -OffsetY), ScrollMetrics(), ScrollConfig());
	SyncContentOffsetFromState();
}
```

`ScrollHere(...)` 保留现有 `TopScroll`/`ClipSize`/`MinHeight` 几何计算和三个 option 分支，但每个原先写 `m_RequestScrollPos = Target` 的分支必须当场调 `m_State.ScrollTo(Target, ScrollMetrics(), ScrollConfig(), EQmScrollMotion::SMOOTH)`；`KEEP_IN_VIEW` 未越界时不调用 state。`SyncContentOffsetFromState()` 是无状态 helper：

```cpp
void CScrollRegion::SyncContentOffsetFromState()
{
	if(m_Params.m_ScrollHorizontal)
		m_ContentScrollOff = vec2(-m_State.Offset(), 0.0f);
	else
		m_ContentScrollOff = vec2(0.0f, -m_State.Offset());
}
```

`CScrollRegion` 只暴露同一 state 的引用，不复制数值：

```cpp
CQmScrollState &State() { return m_State; }
const CQmScrollState &State() const { return m_State; }
```

`QmScroll.cpp` 已同时位于 `GAME_CLIENT` 和 `TESTS_EXTRA`；`ScrollTo(...)`/`ScrollBy(...)` 实现保持在该文件，不新建未登记的 helper `.cpp`，因此 `game-client` 与 `testrunner` 链接同一实现。

- [ ] **Step 5: Run tests to verify green**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiV2ScrollPhysics.*:UiV2ScrollState.*:QmMonitoringHelpers.ScrollRegionOwnsOnlySharedQmScrollState
```

Expected: existing physics + new state tests PASS；source test confirms duplicate offset/animation/programmatic-request fields gone，instant/smooth/content-shrink 都只走 `CQmScrollState`。

- [ ] **Step 6: Commit shared state**

```powershell
git add src/game/client/QmUi/QmScroll.h src/game/client/QmUi/QmScroll.cpp src/game/client/ui_scrollregion.h src/game/client/ui_scrollregion.cpp src/game/client/QmUi/UiContainers.h src/test/QmAnimTest.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "refactor(ui-scroll): 统一滚动状态内核" -m "refactor: ScrollRegion 与 QmUi container 共用 CQmScrollState" -m "test: 覆盖 content shrink 与重复状态删除"
```

### Task 2: 固化五类 scroll profile 与 adapter 语义

**Files:**
- Modify: `src/game/client/QmUi/QmScroll.h`
- Modify: `src/game/client/QmUi/QmScroll.cpp`
- Modify: `src/game/client/ui_scrollregion.h`
- Modify: `src/game/client/ui_listbox.cpp`
- Modify: `src/game/client/components/menus.cpp`
- Test: `src/test/QmAnimTest.cpp`

**Interfaces:**
- Consumes: `SQmScrollRequest`。
- Produces: `SQmResolvedScrollPolicy` for settings/menu/popup/filter/numeric；adapter 只由 policy 生成参数。

- [ ] **Step 1: Write failing profile matrix test**

```cpp
TEST(UiV2ScrollPolicy, ResolvesAllMenuProfilesWithoutPrivateOverrides)
{
	const auto Settings = QmResolveScrollPolicy({EQmScrollProfile::SETTINGS_PAGE, EQmScrollAxis::VERTICAL, 0.0f, 0}, 1.0f, 0.18f);
	const auto List = QmResolveScrollPolicy({EQmScrollProfile::MENU_LIST, EQmScrollAxis::VERTICAL, 24.0f, 3}, 1.0f, 0.0f);
	const auto Popup = QmResolveScrollPolicy({EQmScrollProfile::POPUP_LIST, EQmScrollAxis::VERTICAL, 20.0f, 1}, 1.0f, 0.0f);
	const auto Grid = QmResolveScrollPolicy({EQmScrollProfile::FILTER_GRID, EQmScrollAxis::VERTICAL, 28.0f, 2}, 1.0f, 0.0f);
	const auto Numeric = QmResolveScrollPolicy({EQmScrollProfile::NUMERIC_FIELD, EQmScrollAxis::HORIZONTAL, 0.0f, 0}, 1.0f, 0.0f);
	EXPECT_GT(Settings.m_Style.m_ScrollbarWidth, List.m_Style.m_ScrollbarWidth);
	EXPECT_GT(List.m_Style.m_ScrollbarWidth, Popup.m_Style.m_ScrollbarWidth);
	EXPECT_FLOAT_EQ(List.m_Config.m_WheelScale, 72.0f);
	EXPECT_EQ(Popup.m_MaxVisibleItems, 8);
	EXPECT_EQ(Grid.m_RailVisibility, EQmScrollRailVisibility::HIDDEN);
	EXPECT_FLOAT_EQ(Grid.m_Config.m_WheelScale, 56.0f);
	EXPECT_EQ(Numeric.m_Style.m_Axis, EQmScrollAxis::HORIZONTAL);
	EXPECT_FLOAT_EQ(Settings.m_AltMultiplier, 3.0f);
}
```

- [ ] **Step 2: Run test to verify red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiV2ScrollPolicy.ResolvesAllMenuProfilesWithoutPrivateOverrides
```

Expected: compile FAIL on missing `NUMERIC_FIELD` or behavior FAIL on exact profile outputs.

- [ ] **Step 3: Add the numeric profile and exact resolver table**

```cpp
enum class EQmScrollProfile
{
	SETTINGS_PAGE,
	MENU_LIST,
	POPUP_LIST,
	FILTER_GRID,
	NUMERIC_FIELD,
};
```

Resolver rules：`RowsPerStep > 0 && RowExtent > 0` 时 `WheelScale = RowsPerStep * RowExtent`；popup `m_MaxVisibleItems=8`；filter `HIDDEN`；numeric 强制 horizontal；其余 rail `AUTO`。Alt multiplier 总是 `QmScrollAltMultiplier()`。

- [ ] **Step 4: Remove adapter-owned visual/step choices**

`CListBox::DoStart(...)` 构造 `MENU_LIST` request；`CMenus::BeginSettingsScrollRegion(...)` 构造 `SETTINGS_PAGE` request；`CScrollRegionParamsFromPolicy(...)` 是唯一从 policy 到 legacy adapter 的转换。删除相同调用点手写 thickness、margin、scroll unit 和 hide scrollbar 的代码。

- [ ] **Step 5: Run tests and commit**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiV2ScrollPolicy.*:UiV2ScrollPhysics.*
```

Expected: policy/physics tests PASS。

```powershell
git add src/game/client/QmUi/QmScroll.h src/game/client/QmUi/QmScroll.cpp src/game/client/ui_scrollregion.h src/game/client/ui_listbox.cpp src/game/client/components/menus.cpp src/test/QmAnimTest.cpp
git commit -m "refactor(ui-scroll): 固化菜单滚动 profile" -m "refactor: 统一 rail、行步长、Alt 加速与横向数值轨道" -m "test: 覆盖五类 policy 矩阵"
```

### Task 3: 建立同帧 wheel owner router

**Files:**
- Modify: `src/game/client/ui.h`
- Modify: `src/game/client/ui.cpp`
- Modify: `src/game/client/ui_scrollregion.h`
- Modify: `src/game/client/ui_scrollregion.cpp`
- Test: `src/test/QmAnimTest.cpp`
- Test: `src/test/qmclient_monitoring_test.cpp`

**Interfaces:**
- Consumes: `IClient::PerfFrame()`、raw wheel key events、owner hot rect、priority、overflow eligibility。
- Produces: `CScrollWheelOwnership`、production-owned `QmRegisterWheelOwnerCandidate(...)`/`QmTryConsumeWheel(...)` seam、`CUi::BeginWheelOwnershipFrame()`、`RegisterWheelOwner(...)`、`TryConsumeWheel(...)`。

- [ ] **Step 1: Write failing ownership tests**

```cpp
TEST(UiV2WheelOwnership, HighestEligibleOwnerConsumesRawWheelOnce)
{
	CScrollWheelOwnership Router;
	ASSERT_TRUE(Router.BeginFrame(41, -120.0f, false));
	Router.Register(reinterpret_cast<void *>(1), EUiWheelOwnerPriority::PAGE, true);
	Router.Register(reinterpret_cast<void *>(2), EUiWheelOwnerPriority::POPUP, true);
	float Delta = 0.0f;
	EXPECT_FALSE(Router.TryConsume(reinterpret_cast<void *>(1), &Delta));
	EXPECT_TRUE(Router.TryConsume(reinterpret_cast<void *>(2), &Delta));
	EXPECT_FLOAT_EQ(Delta, -120.0f);
	EXPECT_FALSE(Router.TryConsume(reinterpret_cast<void *>(2), &Delta));
}

TEST(UiV2WheelOwnership, AltAcceleratesButOtherModifiersDoNotDiscardWheel)
{
	CScrollWheelOwnership Router;
	ASSERT_TRUE(Router.BeginFrame(41, -120.0f, true));
	Router.Register(reinterpret_cast<void *>(1), EUiWheelOwnerPriority::PAGE, true);
	float Delta = 0.0f;
	ASSERT_TRUE(Router.TryConsume(reinterpret_cast<void *>(1), &Delta));
	EXPECT_FLOAT_EQ(Delta, -360.0f);
}

TEST(UiV2WheelOwnership, LaterEqualPriorityOwnerWinsAndIneligibleOwnerCannotWin)
{
	CScrollWheelOwnership Router;
	int Outer = 0;
	int Inner = 0;
	int Disabled = 0;
	ASSERT_TRUE(Router.BeginFrame(41, 120.0f, false));
	Router.Register(&Outer, EUiWheelOwnerPriority::COMPOSITE_CONTROL, true);
	Router.Register(&Disabled, EUiWheelOwnerPriority::POPUP, false);
	Router.Register(&Inner, EUiWheelOwnerPriority::COMPOSITE_CONTROL, true);
	float Delta = 0.0f;
	EXPECT_FALSE(Router.TryConsume(&Outer, &Delta));
	EXPECT_FALSE(Router.TryConsume(&Disabled, &Delta));
	EXPECT_TRUE(Router.TryConsume(&Inner, &Delta));
}

TEST(UiV2WheelOwnership, BeginFrameIsIdempotentForOneProductionFrame)
{
	CScrollWheelOwnership Router;
	int Popup = 0;
	ASSERT_TRUE(Router.BeginFrame(41, -120.0f, false));
	Router.Register(&Popup, EUiWheelOwnerPriority::POPUP, true);
	EXPECT_FALSE(Router.BeginFrame(41, 120.0f, true));
	float Delta = 0.0f;
	ASSERT_TRUE(Router.TryConsume(&Popup, &Delta));
	EXPECT_FLOAT_EQ(Delta, -120.0f);
	EXPECT_TRUE(Router.BeginFrame(42, 120.0f, false));
	EXPECT_FALSE(Router.TryConsume(&Popup, &Delta));
}
```

Add the exact production-call-point structure test:

```cpp
TEST(QmMonitoringHelpers, WheelOwnershipFrameBeginsOnlyFromUiUpdate)
{
	const std::string Ui = ReadRepoFile("src/game/client/ui.cpp");
	const std::string Update = ExtractSourceFunctionBody(Ui, "void CUi::Update()");
	const std::string Begin = ExtractSourceFunctionBody(Ui, "void CUi::BeginWheelOwnershipFrame()");
	ASSERT_FALSE(Update.empty());
	ASSERT_FALSE(Begin.empty());
	EXPECT_EQ(CountSubstring(Ui, "BeginWheelOwnershipFrame();"), 1u);
	EXPECT_LT(Update.find("BeginWheelOwnershipFrame();"), Update.find("const vec2 WindowSize"));
	EXPECT_NE(Begin.find("const uint64_t FrameId = Client()->PerfFrame();"), std::string::npos);
	EXPECT_NE(Begin.find("m_WheelOwnership.FrameStarted(FrameId)"), std::string::npos);
	EXPECT_NE(Begin.find("m_WheelOwnership.BeginFrame(FrameId, RawDelta"), std::string::npos);
}
```

- [ ] **Step 2: Run tests to verify red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiV2WheelOwnership.*:QmMonitoringHelpers.WheelOwnershipFrameBeginsOnlyFromUiUpdate
```

Expected: compile FAIL because the router/API does not exist.

- [ ] **Step 3: Define exact CUi ownership API**

```cpp
enum class EUiWheelOwnerPriority
{
	PAGE = 0,
	COMPOSITE_CONTROL = 1,
	POPUP = 2,
};

class CScrollWheelOwnership
{
public:
	bool BeginFrame(uint64_t FrameId, float RawDelta, bool AltPressed);
	bool FrameStarted(uint64_t FrameId) const { return m_HasFrame && m_FrameId == FrameId; }
	void Register(const void *pOwnerId, EUiWheelOwnerPriority Priority, bool Eligible);
	bool TryConsume(const void *pOwnerId, float *pDelta);

private:
	struct SOwner
	{
		const void *m_pId = nullptr;
		EUiWheelOwnerPriority m_Priority = EUiWheelOwnerPriority::PAGE;
		uint64_t m_Order = 0;
		bool m_Eligible = false;
	};
	std::vector<SOwner> m_vOwners;
	uint64_t m_FrameId = 0;
	float m_RawDelta = 0.0f;
	uint64_t m_NextOrder = 0;
	bool m_HasFrame = false;
	bool m_Consumed = false;
};

struct SQmWheelOwnerCandidate
{
	const void *m_pOwnerId = nullptr;
	EUiWheelOwnerPriority m_Priority = EUiWheelOwnerPriority::PAGE;
	CUIRect m_HotRect{};
	bool m_Eligible = false;
};

inline void QmRegisterWheelOwnerCandidate(CScrollWheelOwnership &Ownership, const SQmWheelOwnerCandidate &Candidate, const vec2 &PointerPosition, bool UiEnabled);
inline bool QmTryConsumeWheel(CScrollWheelOwnership &Ownership, const void *pOwnerId, float *pDelta);

void CUi::BeginWheelOwnershipFrame();
void CUi::RegisterWheelOwner(const void *pOwnerId, EUiWheelOwnerPriority Priority, const CUIRect &HotRect, bool Eligible);
bool CUi::TryConsumeWheel(const void *pOwnerId, float *pDelta);
```

router 和 helper 完整定义在 `ui.h`，复用该 header 已显式 include 的 `<algorithm>`、`<cstdint>` 和 `<vector>`；不新增 `.cpp` 链接单元。`CUi` wrapper 定义在 `ui.cpp`，是真实 dropdown/region 路径的 production-owned integration seam。下面是完整实现：

```cpp
inline bool CScrollWheelOwnership::BeginFrame(uint64_t FrameId, float RawDelta, bool AltPressed)
{
	if(FrameStarted(FrameId))
		return false;
	m_vOwners.clear();
	m_FrameId = FrameId;
	m_RawDelta = RawDelta * (AltPressed ? 3.0f : 1.0f);
	m_NextOrder = 0;
	m_HasFrame = true;
	m_Consumed = false;
	return true;
}

inline void CScrollWheelOwnership::Register(const void *pOwnerId, EUiWheelOwnerPriority Priority, bool Eligible)
{
	if(pOwnerId == nullptr)
		return;
	const auto It = std::find_if(m_vOwners.begin(), m_vOwners.end(), [pOwnerId](const SOwner &Owner) { return Owner.m_pId == pOwnerId; });
	if(It != m_vOwners.end())
	{
		It->m_Priority = Priority;
		It->m_Eligible = Eligible;
		It->m_Order = ++m_NextOrder;
		return;
	}
	m_vOwners.push_back({pOwnerId, Priority, ++m_NextOrder, Eligible});
}

inline bool CScrollWheelOwnership::TryConsume(const void *pOwnerId, float *pDelta)
{
	if(pOwnerId == nullptr || pDelta == nullptr || m_Consumed || m_RawDelta == 0.0f)
		return false;
	const SOwner *pWinner = nullptr;
	for(const SOwner &Owner : m_vOwners)
	{
		if(!Owner.m_Eligible)
			continue;
		if(pWinner == nullptr || static_cast<int>(Owner.m_Priority) > static_cast<int>(pWinner->m_Priority) ||
			(Owner.m_Priority == pWinner->m_Priority && Owner.m_Order > pWinner->m_Order))
			pWinner = &Owner;
	}
	if(pWinner == nullptr || pWinner->m_pId != pOwnerId)
		return false;
	*pDelta = m_RawDelta;
	m_Consumed = true;
	return true;
}

inline void QmRegisterWheelOwnerCandidate(CScrollWheelOwnership &Ownership, const SQmWheelOwnerCandidate &Candidate, const vec2 &PointerPosition, bool UiEnabled)
{
	const bool PointerInside =
		PointerPosition.x >= Candidate.m_HotRect.x &&
		PointerPosition.x <= Candidate.m_HotRect.x + Candidate.m_HotRect.w &&
		PointerPosition.y >= Candidate.m_HotRect.y &&
		PointerPosition.y <= Candidate.m_HotRect.y + Candidate.m_HotRect.h;
	Ownership.Register(Candidate.m_pOwnerId, Candidate.m_Priority,
		Candidate.m_Eligible && UiEnabled && PointerInside);
}

inline bool QmTryConsumeWheel(CScrollWheelOwnership &Ownership, const void *pOwnerId, float *pDelta)
{
	return Ownership.TryConsume(pOwnerId, pDelta);
}

void CUi::BeginWheelOwnershipFrame()
{
	const uint64_t FrameId = Client()->PerfFrame();
	if(m_WheelOwnership.FrameStarted(FrameId))
		return;
	float RawDelta = 0.0f;
	if(ConsumeHotkey(HOTKEY_SCROLL_UP))
		RawDelta += 120.0f;
	if(ConsumeHotkey(HOTKEY_SCROLL_DOWN))
		RawDelta -= 120.0f;
	m_WheelOwnership.BeginFrame(FrameId, RawDelta, Input() != nullptr && Input()->AltIsPressed());
}

void CUi::RegisterWheelOwner(const void *pOwnerId, EUiWheelOwnerPriority Priority, const CUIRect &HotRect, bool Eligible)
{
	QmRegisterWheelOwnerCandidate(m_WheelOwnership, {pOwnerId, Priority, HotRect, Eligible}, MousePos(), Enabled());
}

bool CUi::TryConsumeWheel(const void *pOwnerId, float *pDelta)
{
	return QmTryConsumeWheel(m_WheelOwnership, pOwnerId, pDelta);
}
```

`CUi` 增加唯一成员 `CScrollWheelOwnership m_WheelOwnership`。同 priority 时后登记的更内层 owner 胜出；同一 owner 在本帧重新登记会更新而不重复。`Eligible=false` 不参与。唯一 production call point 锁在 `CUi::Update()` 的第一条业务语句：

```cpp
void CUi::Update()
{
	BeginWheelOwnershipFrame();
	const vec2 WindowSize = vec2(Graphics()->WindowWidth(), Graphics()->WindowHeight());
	// 下方保留现有 Update 函数体。
}
```

同一 `CUi` 会被 menus、chat、console、HUD editor 和 scoreboard 调用 `Update()`，因此禁止在每次调用时无条件 reset。`Client()->PerfFrame()` 是精确 frame token：本渲染帧第一次 `Update()` 消费/合并 `HOTKEY_SCROLL_*`，同帧后续 `Update()` 立即返回，不清空已登记 owner 或 raw delta；只有新 `PerfFrame()` 重置 router。

`QmAnimTest.cpp` 已通过 `ui_scrollregion.h -> ui.h` 编译同一份 inline router/helper，不需修改 `CMakeLists.txt`；禁止把整个 `ui.cpp` 加入 `testrunner`，否则会拉入完整 UI/render 依赖并产生 unresolved symbols。行为测试直接执行该 inline production logic；source-order 测试同时证明 `ui.cpp` 的真实 wrappers 只委托它，不存在 test-only 或重复 ownership 逻辑。

- [ ] **Step 4: Route CScrollRegion input exclusively through the owner**

wheel delta 符号锁定为 DDNet/QmScroll 既有约定：`+120` 表示 wheel-up/offset 减小，`-120` 表示 wheel-down/offset 增大；Alt 只保留符号并把幅度乘三。`CScrollRegion::DoScrollInput()` 整个替换为下列实现。程序化 `ScrollRelative(...)` 不再经过该函数：它按 Task 1 锁定的 contract 直接调 `m_State.ScrollBy(...)`。

```cpp
void CScrollRegion::DoScrollInput()
{
	const bool HotFromPreviousFrame = Ui()->HotScrollRegion() == this;
	const bool HotInPopupThisFrame = Ui()->RenderingPopupMenus() && Ui()->NextHotScrollRegion() == this;
	const bool WheelEligible = QmScrollRegionCanConsumeWheel(HotFromPreviousFrame, HotInPopupThisFrame, Ui()->UnderlyingScrollBlocked(), Ui()->RenderingPopupMenus());
	const void *pWheelOwnerId = m_Params.m_pWheelOwnerId != nullptr ? m_Params.m_pWheelOwnerId : this;
	if(!m_Params.m_WheelOwnerPreRegistered)
		Ui()->RegisterWheelOwner(pWheelOwnerId, EUiWheelOwnerPriority::PAGE, m_ClipRect, ContentOverflows() && WheelEligible);

	float WheelDelta = 0.0f;
	if(!Ui()->TryConsumeWheel(pWheelOwnerId, &WheelDelta))
		return;

	m_State.AddWheelImpulse(WheelDelta, ScrollMetrics(), ScrollConfig());
}
```

`CScrollRegion` 私有 helper 只从当前 geometry/params 构造值，不保存 state：

```cpp
SQmScrollMetrics CScrollRegion::ScrollMetrics() const
{
	return {ContentAreaSize(), m_ContentSize};
}

SQmScrollConfig CScrollRegion::ScrollConfig() const
{
	SQmScrollConfig Config = QmNativeWheelScrollConfig(1.0f, g_Config.m_UiSmoothScrollTime / 1000.0f);
	Config.m_WheelScale = m_Params.m_ScrollUnit;
	return Config;
}
```

`CScrollRegionParams` 同时新增 `const void *m_pWheelOwnerId = nullptr` 和 `bool m_WheelOwnerPreRegistered = false`。普通页面保持默认值，由 region 以 `this/PAGE` 登记；已经用 popup 优先级登记的 region 传入 popup context ID 并设置 pre-registered，region 只用同一 ID 消费，不会把其重新降级为 `PAGE`。`CScrollRegion::End()` 按 `DoScrollInput()` -> `m_State.Advance(Client()->RenderFrameTime(), ScrollMetrics(), ScrollConfig())` -> 从 `m_State.Offset()` 生成 `m_ContentScrollOff` -> slider 渲染的顺序执行。`DoSlider()` 的 thumb/track 更改只调 `SetOffset(...)`，不直接赋值 offset/animation 字段。Alt 三倍已在 router 中恰好应用一次，`CScrollRegion` 不再二次读 Alt。Ctrl/GUI/Shift 不改变登记，只有更高优先级控件 owner 能接管。

- [ ] **Step 5: Run tests and commit**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiV2WheelOwnership.*:UiV2ScrollPhysics.*:UiV2ScrollState.*:QmMonitoringHelpers.WheelOwnershipFrameBeginsOnlyFromUiUpdate
```

Expected: ownership and existing scroll tests PASS。

```powershell
git add src/game/client/ui.h src/game/client/ui.cpp src/game/client/ui_scrollregion.h src/game/client/ui_scrollregion.cpp src/test/QmAnimTest.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "feat(ui-scroll): 增加同帧滚轮所有权" -m "feat: popup、复合控件与页面竞争同一 raw wheel" -m "test: 覆盖优先级、单次消费与 Alt 三倍"
```

### Task 4: 接通真实 dropdown + 父 scroll region 场景

**Files:**
- Modify: `src/game/client/QmUi/QmDropdown.h`
- Modify: `src/game/client/QmUi/QmDropdown.cpp`
- Modify: `src/game/client/ui.h`
- Modify: `src/game/client/ui.cpp:2359-2571`
- Modify: `src/game/client/ui_scrollregion.cpp`
- Modify: `src/test/QmAnimTest.cpp`
- Modify: `src/test/qmclient_monitoring_test.cpp`

**Interfaces:**
- Consumes: Task 3 owner router、现有 `SQmDropdownPopupPolicy`/geometry、`CUi::SDropDownState`。
- Produces: long popup 首轮 wheel 只更新 popup state；short popup 保持打开且 parent 滚动。

- [ ] **Step 1: Write failing nested-scroll integration test**

```cpp
TEST(UiV2DropdownIntegration, FirstWheelStaysWithLongPopupAndDoesNotMoveParent)
{
	CScrollWheelOwnership Ownership;
	int ParentRegion = 0;
	int SelectionPopupContext = 0;
	CQmScrollState ParentState;
	CQmScrollState PopupState;
	const SQmScrollMetrics ParentMetrics{300.0f, 900.0f};
	const SQmScrollMetrics PopupMetrics{160.0f, 400.0f};
	const SQmScrollConfig Config = QmNativeWheelScrollConfig(1.0f, 0.0f);
	const CUIRect ParentRect{0.0f, 0.0f, 300.0f, 300.0f};
	const CUIRect PopupRect{20.0f, 40.0f, 160.0f, 160.0f};
	const vec2 Pointer{50.0f, 80.0f};
	ASSERT_TRUE(Ownership.BeginFrame(41, -120.0f, false));

	// ShowPopupSelection：在 DoPopupMenu 和父 region End 前登记 pContext。
	QmRegisterWheelOwnerCandidate(Ownership,
		{&SelectionPopupContext, EUiWheelOwnerPriority::POPUP, PopupRect, true}, Pointer, true);
	// 父 CScrollRegion::End：普通 region 登记自身，但不能获胜。
	QmRegisterWheelOwnerCandidate(Ownership,
		{&ParentRegion, EUiWheelOwnerPriority::PAGE, ParentRect, true}, Pointer, true);
	float Delta = 0.0f;
	EXPECT_FALSE(QmTryConsumeWheel(Ownership, &ParentRegion, &Delta));
	// PopupSelection/CScrollRegion：预登记的 pSelectionPopup 使用同一 ID 消费。
	ASSERT_TRUE(QmTryConsumeWheel(Ownership, &SelectionPopupContext, &Delta));
	PopupState.AddWheelImpulse(Delta, PopupMetrics, Config);
	ParentState.Advance(1.0f / 60.0f, ParentMetrics, Config);
	PopupState.Advance(1.0f / 60.0f, PopupMetrics, Config);
	EXPECT_FLOAT_EQ(ParentState.Offset(), 0.0f);
	EXPECT_GT(PopupState.Offset(), 0.0f);
}

TEST(UiV2DropdownIntegration, ShortPopupKeepsOpenAndLetsParentConsumeWheel)
{
	const SQmDropdownPopupPolicy Policy = QmResolveDropdownPopupPolicy(4, 20.0f, 5.0f, false, 0.0f, 10.0f);
	EXPECT_FALSE(QmDropdownPopupOwnsWheel(Policy, Policy.m_PreferredHeight));
	CScrollWheelOwnership Ownership;
	int ParentRegion = 0;
	int SelectionPopupContext = 0;
	const CUIRect ParentRect{0.0f, 0.0f, 300.0f, 300.0f};
	const CUIRect PopupRect{20.0f, 40.0f, 160.0f, Policy.m_PreferredHeight};
	const vec2 Pointer{50.0f, 80.0f};
	ASSERT_TRUE(Ownership.BeginFrame(41, -120.0f, false));
	QmRegisterWheelOwnerCandidate(Ownership,
		{&SelectionPopupContext, EUiWheelOwnerPriority::POPUP, PopupRect, false}, Pointer, true);
	QmRegisterWheelOwnerCandidate(Ownership,
		{&ParentRegion, EUiWheelOwnerPriority::PAGE, ParentRect, true}, Pointer, true);
	float Delta = 0.0f;
	EXPECT_TRUE(QmTryConsumeWheel(Ownership, &ParentRegion, &Delta));
	EXPECT_FLOAT_EQ(Delta, -120.0f);
}
```

These are harness-level integration tests, not a second router model: `QmRegisterWheelOwnerCandidate(...)` and `QmTryConsumeWheel(...)` are the production-owned seams called by `CUi::RegisterWheelOwner(...)` and `CUi::TryConsumeWheel(...)`; the source-order test below locks `ShowPopupSelection -> PopupSelection -> CScrollRegion::DoScrollInput` to those wrappers. The test therefore executes the same pointer-hit filtering, priority resolution, single-consumption and state advance used by the actual path without constructing graphics/text/input backends.

- [ ] **Step 2: Write failing production-order test**

```cpp
TEST(QmMonitoringHelpers, DropdownRegistersWheelOwnerBeforeParentCanConsume)
{
	const std::string Ui = ReadRepoFile("src/game/client/ui.cpp");
	const std::string ShowPopup = ExtractSourceFunctionBody(Ui, "void CUi::ShowPopupSelection(float X, float Y, SSelectionPopupContext *pContext)");
	ASSERT_FALSE(ShowPopup.empty());
	EXPECT_NE(ShowPopup.find("RegisterWheelOwner(pContext"), std::string::npos);
	EXPECT_LT(ShowPopup.find("RegisterWheelOwner(pContext"), ShowPopup.find("DoPopupMenu("));
	const std::string Register = ExtractSourceFunctionBody(Ui, "void CUi::RegisterWheelOwner(const void *pOwnerId, EUiWheelOwnerPriority Priority, const CUIRect &HotRect, bool Eligible)");
	const std::string Consume = ExtractSourceFunctionBody(Ui, "bool CUi::TryConsumeWheel(const void *pOwnerId, float *pDelta)");
	EXPECT_NE(Register.find("QmRegisterWheelOwnerCandidate(m_WheelOwnership"), std::string::npos);
	EXPECT_NE(Consume.find("QmTryConsumeWheel(m_WheelOwnership, pOwnerId, pDelta)"), std::string::npos);
	const std::string Popup = ExtractSourceFunctionBody(Ui, "CUi::EPopupMenuFunctionResult CUi::PopupSelection(void *pContext, CUIRect View, bool Active)");
	ASSERT_FALSE(Popup.empty());
	EXPECT_NE(Popup.find("ScrollParams.m_pWheelOwnerId = pSelectionPopup"), std::string::npos);
	EXPECT_NE(Popup.find("ScrollParams.m_WheelOwnerPreRegistered = true"), std::string::npos);
	const std::string Region = ReadRepoFile("src/game/client/ui_scrollregion.cpp");
	const std::string Input = ExtractSourceFunctionBody(Region, "void CScrollRegion::DoScrollInput()");
	EXPECT_NE(Input.find("TryConsumeWheel(pWheelOwnerId"), std::string::npos);
	EXPECT_EQ(Input.find("KEY_MOUSE_WHEEL_UP"), std::string::npos);
	EXPECT_EQ(Input.find("m_ScrollPos"), std::string::npos);
	EXPECT_EQ(Input.find("m_AnimTime"), std::string::npos);
	EXPECT_EQ(Input.find("m_AnimTargetScrollPos"), std::string::npos);
}
```

- [ ] **Step 3: Run tests to verify red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiV2DropdownIntegration.*:QmMonitoringHelpers.DropdownRegistersWheelOwnerBeforeParentCanConsume
```

Expected: production-order test FAIL because dropdown still relies on `m_BlockUnderlyingScroll`/previous hot state rather than registering an owner.

- [ ] **Step 4: Register popup owner from actual resolved popup state**

owner 不在 `DoDropDown(...)` 里使用尚未存在的 `State.m_PopupRect`/`State.m_Open`。`CUi::ShowPopupSelection(...)` 已经同时持有 `SSelectionPopupContext`、最终 `X/Y/PopupWidth/PopupHeightResolved` 和 `PopupPolicy`；它在调用 `DoPopupMenu(...)` 前执行完整登记：

```cpp
const CUIRect PopupRect{X, Y, PopupWidth, PopupHeightResolved};
const bool OwnsWheel = pContext->m_PopupVisible && QmDropdownPopupOwnsWheel(pContext->m_PopupPolicy, PopupHeightResolved);
RegisterWheelOwner(pContext, EUiWheelOwnerPriority::POPUP, PopupRect, OwnsWheel);
pContext->m_BlockUnderlyingScroll = OwnsWheel;
pContext->m_Props.m_BlockUnderlyingScroll = OwnsWheel;
```

`CUi::PopupSelection(...)` 在 `pScrollRegion->Begin(...)` 前把同一 context ID 传给 region：

```cpp
CScrollRegionParams ScrollParams = QmScrollRegionParamsFromPolicy(ScrollPolicy);
ScrollParams.m_ScrollbarNoOuterMargin = true;
ScrollParams.m_pWheelOwnerId = pSelectionPopup;
ScrollParams.m_WheelOwnerPreRegistered = true;
pScrollRegion->Begin(&View, &ScrollOffset, &ScrollParams);
```

这样 popup region 以 `pSelectionPopup` 调 `TryConsumeWheel(...)`，父页面用自身 ID，同一 delta 不会双消费。短 popup 以 `Eligible=false` 登记，其 region 不 overflow，不会消费；父页面可消费且 popup 保持打开。anchor 完全离开父 viewport 才关闭；部分相交保持打开。

- [ ] **Step 5: Keep eight-item cap and rail semantics**

`QmResolveDropdownPopupPolicy(...)` 的 visible items 为 `min(NumItems, 8)`；最终 viewport 限高导致真实 overflow 时即使 `NumItems <= 8` 也 owning + small AUTO rail。无 overflow 时 rail 不绘制、不预留。

- [ ] **Step 6: Run integration tests and build**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiV2DropdownGeometry.*:UiV2DropdownPolicy.*:UiV2DropdownState.*:UiV2DropdownIntegration.*:QmMonitoringHelpers.DropdownRegistersWheelOwnerBeforeParentCanConsume
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
```

Expected: all dropdown tests PASS，`game-client` build 退出码 `0`。

- [ ] **Step 7: Commit dropdown ownership**

```powershell
git add src/game/client/QmUi/QmDropdown.h src/game/client/QmUi/QmDropdown.cpp src/game/client/ui.h src/game/client/ui.cpp src/game/client/ui_scrollregion.cpp src/test/QmAnimTest.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "fix(ui-scroll): 阻止下拉框首轮滚轮泄漏" -m "fix: 长 popup 同帧登记 owner，短 popup 交给父页面" -m "test: 覆盖真实 parent-popup 状态与生产调用顺序"
```

### Task 5: P4 全量验证、人工矩阵与只读审查

**Files:**
- Modify: none unless review findings require a scoped fix
- Test: scroll/dropdown tests, full C++ regression, docs and default gate

**Interfaces:**
- Consumes: Tasks 1–4。
- Produces: P5–P7 唯一 scroll state/policy/owner contract。

- [ ] **Step 1: Run serial automated verification**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiV2ScrollPhysics.*:UiV2ScrollState.*:UiV2ScrollPolicy.*:UiV2WheelOwnership.*:UiV2DropdownGeometry.*:UiV2DropdownPolicy.*:UiV2DropdownState.*:UiV2DropdownIntegration.*:QmMonitoringHelpers.ScrollRegionOwnsOnlySharedQmScrollState:QmMonitoringHelpers.DropdownRegistersWheelOwnerBeforeParentCanConsume
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
python qmclient_scripts/gate/check_docs.py
python qmclient_scripts/gate/check_gate.py --mode default
git diff --check
```

Expected: all commands exit `0`。

- [ ] **Step 2: Execute manual scroll/dropdown matrix**

```text
Settings large：Controls/Graphics 长页，普通滚轮与 Alt 三倍，rail 仅 overflow 显示
Menu medium：服务器/好友列表按行滚动，Ctrl/GUI/Shift 不静默吞 wheel
Filter hidden：国旗筛选网格每次两行，rail 隐藏但 clip/keyboard/wheel 有效
Short dropdown：4/8 项，popup 保持打开，父页面滚动，无 rail
Long dropdown：9/20 项与 viewport 限高，首个 wheel 只滚 popup，small rail 无闪烁
Scrolled anchor：部分可见保持，完全离开 viewport 关闭，hit/clip 不越父 viewport
Content shrink：切 tab/filter 后 offset clamp，无空白页、跳顶或 active thumb 残留
```

Expected: 每项记录页面、viewport、UI scale、输入 modifier 和结果；未执行项保留 gap。

- [ ] **Step 3: Dispatch independent read-only review**

review 重点：状态是否仍双轨、offset 符号/单位、content shrink、owner ID 生命周期、popup 注册时序、raw wheel 单次消费、modifier 冲突、AUTO/HIDDEN geometry、首轮 wheel 集成测试是否走生产接口。等待完整 findings-first 报告。

Expected: P0/P1 finding 全部修复并重跑 Step 1；报告返回前 P4 不完成。

---

## Self-review

- Spec coverage: 覆盖单一 scroll state/policy、五类 preset、AUTO/HIDDEN、Alt 三倍、列表/网格步长、dropdown 八项/viewport/首轮 owner 与 content shrink。
- Marker scan: 未发现未决占位、未定义版本或省略代码步骤。
- Type consistency: P5–P7 只使用 `CQmScrollState`、无状态 `CQmScrollController`、`QmResolveScrollPolicy(...)`、`CUi::RegisterWheelOwner(...)` 和 `CUi::TryConsumeWheel(...)`。
- Scope boundary: 聊天/控制台不改；页面批量 adapter 迁移留给 P5–P7；R1–R3 不实施。
