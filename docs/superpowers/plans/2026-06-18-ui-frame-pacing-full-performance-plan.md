# UI Frame Pacing Full Performance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把上一阶段文本渲染稳定化留下的 runtime FAIL 收口为可验收的 UI/FPS 性能优化，重点降低 ingame Esc、settings tab switch 和 assets tab switch 的首帧/切换卡顿。

**Architecture:** 保留上一阶段 `CUi` cached label 语义、`CMenus` stable text pool 和 perf report 可信口径，把下一阶段优化重心从 text container/glyph 转到 `ui_layout_or_render_total` 的细分归因、首帧工作削峰、QmUI/layout 缓存和 FPS 基线验收。先让 report 能指出具体 UI section，再用分帧 scheduler / shell-first render / stable layout cache 减少首帧同步工作，最后用 fresh client log 验收。

**Tech Stack:** C++, DDNet/QmClient CUI/QmUI, `perf/fps`, `perf/ui_budget`, `perf/section`, `perf/assets`, `perf/settings-text`, `qmclient_scripts/perf` TypeScript report, GoogleTest source-contract tests, Windows CMake gate.

---

## Starting Point And Scope

- 起点计划：`docs/superpowers/plans/archive/2026-06-18-text-rendering-stabilization-observability.md`
- 起点 runtime log：`C:/Users/11054/AppData/Roaming/DDNet/dumps/QmClient_Perf/qm_perf_2026-06-18_20-43-39.log`
- 起点 report：`C:/Users/11054/AppData/Roaming/DDNet/dumps/QmClient_Perf/Perf_Report/qm_perf_2026-06-18_20-43-39_report.html`
- 起点结论：
  - 文本渲染语义稳定化和可观测性已基本落地，但通用文本渲染优化不能宣称完成。
  - 旧计划核心验收未完成，runtime verdict 为 `FAIL`。
  - `ingame_esc_open` 出现 `frameMsMax=481.193ms`、`menuMsMax=324.659ms`。
  - 多个 `settings_assets_tab_switch` 窗口仍有约 `25-28ms` p99/max 峰值。
  - report 指向 `ui_layout_or_render_total`，不是 `text_container_create`、`glyph_rasterize` 或 `glyph_upload`。

## User Questions To Preserve In This Plan

1. 旧 plan 核心目标未完全达成；Task 1-10 的架构基础落地，Task 11 runtime/gate 验收未完成。
2. 通用文本渲染优化不能说完成；只能说第一阶段“语义稳定化和可观测性”落地。
3. 性能优化方向应转向 UI/layout/render 首帧工作量、QmUI/layout 缓存、shell-first 和分帧 hydration。
4. 图形后端优化尚未做；除非新归因证明 backend/GPU upload 是瓶颈，否则本计划不优先动 OpenGL/Vulkan backend。
5. FPS 基线优化需要单独方案：减少首帧渲染需求、优化 QmUI/layout、拆分 `ui_layout_or_render_total`、建立窗口级 p95/p99/1% low 验收。

## Success Criteria

- Fresh client log 中 `ingame_esc_open` 目标窗口：
  - `frameMsP99 <= 16.7ms`
  - `frameMsMax <= 33.4ms`
  - `menuMsMax <= 12.0ms`
  - `fpsOnePctLow >= 60`
- Fresh client log 中 `settings_assets_tab_switch` 目标窗口：
  - first switch after cold entry `frameMsP99 <= 16.7ms`
  - repeated switch after warmup `frameMsP99 <= 8.333ms`
  - `menuMsMax <= 8.0ms`
- Fresh client log 中 `settings_tab_switch` for TClient/QmClient/DDNet:
  - no window with `ui_layout_or_render_total_ms > 16.7`
  - report names the top concrete UI section, not only aggregate `ui_layout_or_render_total`
- Stable text acceptance:
  - `miss=0`, `stale=0`, `unplanned=0`, `fallback_immediate=0`, `build_queued=0` in target windows after warmup.
- Report quality:
  - `qmclient_scripts/perf` report must show concrete UI section attribution under every target FPS failure.
  - Report must keep backend/GPU fields separate from UI/layout fields so backend work is not blamed without evidence.

## Non-Goals

- Do not change protocol, physics, prediction, snapshots, demo/map/skin formats, rank semantics, or server gameplay.
- Do not rewrite graphics backend or introduce a new renderer in this plan.
- Do not add threads or background GPU upload architecture unless a later plan is explicitly approved.
- Do not remove visual content from QmClient pages; reduce first-frame demand with shell-first, caching, and scheduling.
- Do not use old perf logs as acceptance. Old logs may be used only as baseline context.

## File Structure

- Modify: `qmclient_scripts/perf/lib/stats.ts`
  - Extend `BudgetCorrelationWindow` and `budgetCorrelationSummary(entries: PerfEntry[])` so target FPS windows rank concrete `perf/menu`, `perf/section`, `perf/assets`, `perf/ui_budget`, and `perf/settings-ui` section costs before aggregate UI buckets.
- Modify: `qmclient_scripts/perf/lib/report.ts`
  - Show target-window UI section attribution, FPS baseline tables, and backend-vs-UI separation.
- Modify: `qmclient_scripts/perf/lib/quality.ts`
  - Fail when target FPS windows only have aggregate attribution.
- Modify: `qmclient_scripts/perf/test.ts`
  - Cover UI attribution, FPS baseline thresholds, and backend separation.
- Modify: `src/game/client/components/menus.cpp`
  - Emit finer section telemetry around ingame Esc open, ingame tabs, settings tab switch, and QmUI page regions.
- Modify: `src/game/client/components/menus.h`
  - Keep the existing `PrebuildIngameEscTextPoolBeforeOpen(int Budget)` declaration as the single Esc text prewarm entry point.
- Modify: `src/game/client/components/menus_settings_assets.cpp`
  - Use the existing `s_AssetsTabSwitchFirstFrame`, `s_AssetsTabSwitchCooldownFrames`, `SSettingsAssetsCardHydrationScheduler::m_TabSwitchShellOnlyFrame`, and `LogAssetsFramePerfStage` hooks to make tab switches shell-first and measurable.
- Modify: `src/test/qmclient_monitoring_test.cpp`
  - Source-contract tests for new telemetry names, no aggregate-only report, and shell-first behavior.
- Modify: `src/test/settings_warmup_test.cpp`
  - Scheduler tests for first-frame conservative budgets and warm repeated tab behavior.

---

### Task 1: Make Report Attribute Target FPS Failures To Concrete UI Sections

**Files:**
- Modify: `qmclient_scripts/perf/lib/stats.ts`
- Modify: `qmclient_scripts/perf/lib/report.ts`
- Modify: `qmclient_scripts/perf/lib/quality.ts`
- Test: `qmclient_scripts/perf/test.ts`

- [ ] **Step 1: Write failing perf test for concrete UI attribution**

Add this test to `qmclient_scripts/perf/test.ts` near existing FPS/window attribution tests:

```ts
test('target fps failure requires concrete ui section attribution', () => {
  const { entries } = parseLog([
    '[2026-06-18 10:00:00][perf/fps]: event=fps_summary operation=ingame_esc_open context=online page=game tab=none sample_frames=30 sample_seconds=0.500 fps_avg=60 fps_min=2 fps_1pct_low=2 fps_1pct_source=real_sampled fps_max=1200 frame_ms_avg=16.0 frame_ms_p95=20.0 frame_ms_p99=120.0 frame_ms_max=120.0 menu_ms_max=110.0 window_start_frame=100 window_end_frame=130 cap_limited=0',
    '[2026-06-18 10:00:00][perf/menu]: page=game operation=ingame_esc_open frame=112 stage=ingame_esc_menu_shell duration_ms=9.5',
    '[2026-06-18 10:00:00][perf/menu]: page=game operation=ingame_esc_open frame=112 stage=ingame_server_info_layout duration_ms=77.0',
    '[2026-06-18 10:00:00][perf/menu]: page=game operation=ingame_esc_open frame=112 stage=ingame_tabbar duration_ms=3.0',
  ].join('\n'));
  const summary = summarizeForBundle(entries, 'test.log', { totalLines: entries.length, invalidLines: 0 });
  const sample = JSON.stringify(summary);
  assert.match(sample, /ingame_server_info_layout/);
  assert.doesNotMatch(sample, /top_culprit=ui_layout_or_render_total/);
});
```

- [ ] **Step 2: Run the failing perf test**

Run:

```pwsh
cd qmclient_scripts/perf
bun test.ts
```

Expected: FAIL because current attribution can still stop at `ui_layout_or_render_total`.

- [ ] **Step 3: Add concrete UI section aggregation**

In `qmclient_scripts/perf/lib/stats.ts`, add a helper that scans entries inside each FPS window for `perf/menu`, `perf/section`, `perf/assets`, and `perf/ui_budget` section events:

```ts
type UiSectionCost = {
  stage: string;
  page: string;
  operation: string;
  frame: number;
  durationMs: number;
};

const UI_SECTION_SYSTEMS: ReadonlySet<string> = new Set([
  PERF_SYSTEM.MENU,
  PERF_SYSTEM.SECTION,
  'perf/assets',
  'perf/ui_budget',
  'perf/settings-ui',
]);

function targetWindowUiSections(entries: PerfEntry[], startFrame: number, endFrame: number, operation: string): UiSectionCost[] {
  return entries
    .map(e => {
      const frame = numberField(e, 'frame', -1);
      const entryOperation = field(e, 'operation', operation);
      return {
        stage: field(e, 'stage', e.stage || field(e, 'event', 'unknown')),
        page: field(e, 'page', 'unknown'),
        operation: entryOperation,
        frame,
        durationMs: entryDurationMs(e) ?? e.durationMs,
        system: e.system,
      };
    })
    .filter(e => e.frame >= startFrame && e.frame <= endFrame)
    .filter(e => e.operation === operation || e.operation === 'unknown' || e.operation === '')
    .filter(e => UI_SECTION_SYSTEMS.has(e.system))
    .filter(e => e.durationMs > 0 && e.stage !== 'menus_render_total' && e.stage !== 'ingame_page_content')
    .sort((a, b) => b.durationMs - a.durationMs);
}
```

- [ ] **Step 4: Prefer concrete UI section over aggregate culprit**

In `BudgetCorrelationWindow`, add:

```ts
topUiSectionStage: string;
topUiSectionPage: string;
topUiSectionFrame: number;
topUiSectionMs: number;
```

Initialize these fields in the `Windows` object inside `budgetCorrelationSummary`:

```ts
topUiSectionStage: '',
topUiSectionPage: '',
topUiSectionFrame: 0,
topUiSectionMs: 0,
```

After the existing loop that fills budget windows and before `rankedBudgetCulprits(Window)`, compute concrete sections:

```ts
const uiSections = targetWindowUiSections(entries, Window.windowStartFrame, Window.windowEndFrame, Window.operation);
const topUiSection = uiSections[0];
if (topUiSection) {
  Window.topUiSectionStage = topUiSection.stage;
  Window.topUiSectionPage = topUiSection.page;
  Window.topUiSectionFrame = topUiSection.frame;
  Window.topUiSectionMs = topUiSection.durationMs;
}
```

Then prepend the concrete section to `culpritRank` inside `CorrelatedWindows`:

```ts
const baseCulpritRank = rankedBudgetCulprits(Window);
const uiSectionCulprit = Window.topUiSectionMs > 0
  ? [{
      kind: `ui_section:${Window.topUiSectionStage}`,
      score: Window.topUiSectionMs,
      details: summaryKv(
        ['page', Window.topUiSectionPage],
        ['frame', String(Window.topUiSectionFrame)],
        ['duration_ms', Window.topUiSectionMs.toFixed(3)],
      ),
    }]
  : [];
const culpritRank = [...uiSectionCulprit, ...baseCulpritRank];
const dominantAttribution = culpritRank[0]?.kind ?? 'none';
```

- [ ] **Step 5: Update report UI**

In `qmclient_scripts/perf/lib/report.ts`, add a target-window section table with these columns:

```text
Operation | Page | Window Frames | P99 | Top UI Section | Section ms | Backend ms | Text create ms
```

The `Top UI Section` cell must show `ui_section:<stage>` when available.

- [ ] **Step 6: Fail aggregate-only target attribution**

In `qmclient_scripts/perf/lib/quality.ts`, add a warning/failure when a target FPS failure has `dominantAttribution === 'ui_layout_or_render_total'` and no `ui_section:` culprit:

```ts
warnings.push('target fps failure has only aggregate ui attribution');
failed = true;
```

- [ ] **Step 7: Verify perf tests and typecheck**

Run:

```pwsh
cd qmclient_scripts/perf
bun test.ts
npx tsc --noEmit
```

Expected: PASS.

### Task 2: Instrument Ingame Esc First Frame With Fine-Grained Sections

**Files:**
- Modify: `src/game/client/components/menus.cpp`
- Modify: `src/test/qmclient_monitoring_test.cpp`

- [ ] **Step 1: Write failing source-contract test for ingame Esc section telemetry**

Add this test to `src/test/qmclient_monitoring_test.cpp`:

```cpp
TEST(QmMonitoringHelpers, IngameEscOpenHasConcreteSectionTelemetry)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	EXPECT_NE(Source.find("ingame_esc_menu_shell"), std::string::npos);
	EXPECT_NE(Source.find("ingame_esc_button_column"), std::string::npos);
	EXPECT_NE(Source.find("ingame_esc_tab_content"), std::string::npos);
	EXPECT_NE(Source.find("ingame_server_info_layout"), std::string::npos);
}
```

- [ ] **Step 2: Run the failing focused test**

Run:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.IngameEscOpenHasConcreteSectionTelemetry
```

Expected: FAIL until telemetry labels exist.

- [ ] **Step 3: Add scoped perf sections around ingame Esc render regions**

In `src/game/client/components/menus.cpp`, add local `CPerfScope` or the existing QmClient perf scope helper around:

```cpp
char aEscPerfExtra[128];
str_format(aEscPerfExtra, sizeof(aEscPerfExtra), "operation=ingame_esc_open page=game frame=%" PRIu64, Client()->PerfFrame());

CPerfTimer MenuShellTimer;
// Existing Esc menu shell/background/button column draw calls stay here.
LogPerfStage(Client(), "ingame_esc_menu_shell", MenuShellTimer.ElapsedMs(), true, aEscPerfExtra);

CPerfTimer ButtonColumnTimer;
// Existing disconnect/settings/quit button column draw calls stay here.
LogPerfStage(Client(), "ingame_esc_button_column", ButtonColumnTimer.ElapsedMs(), true, aEscPerfExtra);

CPerfTimer TabContentTimer;
// Existing active ingame tab content draw calls stay here.
LogPerfStage(Client(), "ingame_esc_tab_content", TabContentTimer.ElapsedMs(), true, aEscPerfExtra);

CPerfTimer ServerInfoTimer;
// Existing server info layout/draw calls stay here.
LogPerfStage(Client(), "ingame_server_info_layout", ServerInfoTimer.ElapsedMs(), true, "operation=ingame_esc_open page=game");
```

Use the existing `LogPerfStage` helper in `menus.cpp`; it writes `perf/menu` through `QmPerfLogStage`. Keep the measured blocks around existing code, not around newly duplicated rendering.

- [ ] **Step 4: Verify focused test**

Run:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.IngameEscOpenHasConcreteSectionTelemetry
```

Expected: PASS.

### Task 3: Remove Ingame Esc Visible Text Builds From The Open Frame

**Files:**
- Modify: `src/game/client/components/menus.cpp`
- Modify: `src/game/client/components/menus.h`
- Test: `src/test/qmclient_monitoring_test.cpp`

- [ ] **Step 1: Write failing source-contract test for Esc prewarm before visible render**

Add:

```cpp
TEST(QmMonitoringHelpers, IngameEscPrewarmsStableTextBeforeVisibleFrame)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	EXPECT_NE(Header.find("PrebuildIngameEscTextPoolBeforeOpen"), std::string::npos);
	EXPECT_NE(Source.find("BuildIngameMenuTextPlan(vVisibleItems, Screen)"), std::string::npos);
	EXPECT_NE(Source.find("PrebuildIngameEscTextPoolBeforeOpen"), std::string::npos);
	EXPECT_NE(Source.find("operation=ingame_esc_open"), std::string::npos);
	EXPECT_NE(Source.find("fallback_immediate=0"), std::string::npos);
}
```

- [ ] **Step 2: Run failing test**

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.IngameEscPrewarmsStableTextBeforeVisibleFrame
```

Expected: FAIL until the existing ingame Esc prebuild path is proven to use `BuildIngameMenuTextPlan` before the visible `ingame_esc_open` frame.

- [ ] **Step 3: Keep one Esc prewarm entry point**

Verify `src/game/client/components/menus.h` keeps this existing declaration and does not add a second Esc-specific prewarm state machine:

```cpp
	void PrebuildIngameEscTextPoolBeforeOpen(int Budget);
```

- [ ] **Step 4: Trigger existing prebuild before the first visible Esc frame**

In the ingame menu open path in `menus.cpp`, call:

```cpp
PrebuildIngameEscTextPoolBeforeOpen(3);
```

before the first visible render frame of the ingame menu. Do not add `m_IngameEscTextPrewarmPending` or `PrewarmIngameEscStableText`; those would duplicate the existing prebuild path and risk producing different text keys.

- [ ] **Step 5: Ensure the prebuild path uses the real ingame menu plan**

In `menus.cpp`, keep `PrebuildIngameEscTextPoolBeforeOpen(int Budget)` as the only helper and ensure it drains the plan generated from `BuildIngameMenuTextPlan(...)`:

```cpp
void CMenus::PrebuildIngameEscTextPoolBeforeOpen(int Budget)
{
	if(Budget <= 0)
		return;
	// Existing adaptive-budget input setup stays here.
	PrebuildSettingsMenuTextPool(minimum(Budget, maximum(1, AdaptiveBudget.m_TextPrebuildTokens)), "target_settings", "ingame_esc_open");
}
```

Do not hard-code ids such as `"disconnect"`, `"settings"`, or `"quit"` in this helper. The real keys must come from `BuildIngameMenuTextPlan` through `DoIngameMenuButton`, `DoIngameMenuTab`, `DoIngameMenuLabel`, and `CollectMenuTextPlanItem`, so prebuilt keys match visible render keys such as the existing `ingame-*` ids.

- [ ] **Step 6: Verify focused test and runtime log**

Run:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.IngameEscPrewarmsStableTextBeforeVisibleFrame
```

Expected: PASS.

Then run a fresh client and verify `ingame_esc_open` no longer reports target-window `fallback_immediate > 0` after warmup.

### Task 4: Make Assets Tab Switch Shell-First And Defer Heavy Card Work

**Files:**
- Modify: `src/game/client/components/menus_settings_assets.cpp`
- Modify: `src/test/qmclient_monitoring_test.cpp`

- [ ] **Step 1: Write failing test for shell-first tab switch behavior**

Add:

```cpp
TEST(QmMonitoringHelpers, AssetsTabSwitchUsesShellFirstFrame)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const size_t HelperPos = Source.find("BeginAssetsCardHydrationFrame");
	EXPECT_NE(Source.find("AssetsTabSwitchCooldownFrames"), std::string::npos);
	ASSERT_NE(HelperPos, std::string::npos);
	const std::string HelperBody = Source.substr(HelperPos, 900);
	EXPECT_NE(HelperBody.find("Scheduler.m_TabSwitchShellOnlyFrame = AssetsTabSwitchFirstFrame"), std::string::npos);
	EXPECT_NE(HelperBody.find("AssetsTabSwitchFirstFrame ? maximum(1, minimum(VisibleCardCount, MetadataLayoutTokens))"), std::string::npos);
	EXPECT_NE(HelperBody.find("Scheduler.m_PreviewBudget = AssetsTabSwitchCooldownActive ? 0"), std::string::npos);
	EXPECT_NE(Source.find("LogAssetsFramePerfStage(\"assets_tab_switch_shell_first\""), std::string::npos);
	EXPECT_NE(Source.find("CardHydrationScheduler.m_TabSwitchShellOnlyFrame ? 1 : 0"), std::string::npos);
}
```

- [ ] **Step 2: Run failing test**

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.AssetsTabSwitchUsesShellFirstFrame
```

Expected: FAIL until shell-first telemetry and behavior exist.

- [ ] **Step 3: Add shell-first telemetry**

In `menus_settings_assets.cpp`, when `AssetsTabSwitchFirstFrame` is true, log:

```text
system=perf/assets stage=assets_tab_switch_shell_first operation=settings_assets_tab_switch tab=<tab> duration_ms=<ms> tab_switch_shell_only=1
```

- [ ] **Step 4: Defer preview/artifact/card-heavy work on first switch frame**

In `BeginAssetsCardHydrationFrame`, enforce:

```cpp
if(AssetsTabSwitchFirstFrame)
{
	Scheduler.m_TabSwitchShellOnlyFrame = true;
	Scheduler.m_MetadataBudget = maximum(1, minimum(VisibleCardCount, MetadataLayoutTokens));
	Scheduler.m_PreviewBudget = 0;
	return Scheduler;
}
```

Ensure the render path draws card shell, title/status, and stable placeholder on shell-only frames, and verify the first switch frame does not start preview/artifact-heavy work. The final runtime report must show reduced first-frame `preview_draw_ms`, texture upload, and card draw contribution; the source-contract test alone is not acceptance evidence.

- [ ] **Step 5: Verify focused test**

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.AssetsTabSwitchUsesShellFirstFrame
```

Expected: PASS.

### Task 5: Add FPS Baseline Contract To Perf Report

**Files:**
- Modify: `qmclient_scripts/perf/lib/quality.ts`
- Modify: `qmclient_scripts/perf/lib/report.ts`
- Test: `qmclient_scripts/perf/test.ts`

- [ ] **Step 1: Write failing test for FPS baseline thresholds**

Add:

```ts
test('fps baseline fails ingame esc and assets tab switch windows independently', () => {
  const { entries } = parseLog([
    '[2026-06-18 10:00:00][perf/fps]: event=fps_summary operation=ingame_esc_open context=online page=game tab=none sample_frames=30 sample_seconds=0.5 fps_avg=55 fps_min=2 fps_1pct_low=2 fps_1pct_source=real_sampled fps_max=1200 frame_ms_avg=18 frame_ms_p95=15 frame_ms_p99=481 frame_ms_max=481 menu_ms_max=324 window_start_frame=10 window_end_frame=40 cap_limited=0',
    '[2026-06-18 10:00:01][perf/fps]: event=fps_summary operation=settings_assets_tab_switch context=offline page=settings:assets tab=1 sample_frames=30 sample_seconds=0.1 fps_avg=320 fps_min=35 fps_1pct_low=35 fps_1pct_source=real_sampled fps_max=1300 frame_ms_avg=3 frame_ms_p95=17 frame_ms_p99=28 frame_ms_max=28 menu_ms_max=25 window_start_frame=50 window_end_frame=80 cap_limited=0',
  ].join('\n'));
  const summary = summarizeForBundle(entries, 'test.log', { totalLines: entries.length, invalidLines: 0 });
  assert.equal(summary.quality.failed, true);
  assert.match(JSON.stringify(summary.quality.warnings), /ingame_esc_open/);
  assert.match(JSON.stringify(summary.quality.warnings), /settings_assets_tab_switch/);
});
```

- [ ] **Step 2: Run failing perf test**

```pwsh
cd qmclient_scripts/perf
bun test.ts
```

Expected: FAIL until operation-specific FPS baseline warnings exist.

- [ ] **Step 3: Add operation-specific FPS thresholds**

In `quality.ts`, define:

```ts
const FPS_BASELINES = {
  ingame_esc_open: { p99: 16.7, max: 33.4, menuMax: 12.0, onePctLow: 60 },
  settings_assets_tab_switch: { p99: 16.7, max: 33.4, menuMax: 8.0, onePctLow: 60 },
  settings_tab_switch: { p99: 16.7, max: 33.4, menuMax: 8.0, onePctLow: 60 },
} as const;
```

Use the existing `fpsSummaries(entries: PerfEntry[])` list and the `BudgetCorrelationWindow` summary produced by `budgetCorrelationSummary(entries)` to fail each matching FPS window independently when it exceeds its baseline.

- [ ] **Step 4: Render baseline table**

In `report.ts`, add a table under the FPS section:

```text
Operation | p99 target | max target | menu target | 1% low target | current | verdict
```

- [ ] **Step 5: Verify perf tests**

```pwsh
cd qmclient_scripts/perf
bun test.ts
npx tsc --noEmit
```

Expected: PASS.

### Task 6: Add Graphics Backend Separation Without Backend Optimization

**Files:**
- Modify: `qmclient_scripts/perf/lib/stats.ts`
- Modify: `qmclient_scripts/perf/lib/report.ts`
- Test: `qmclient_scripts/perf/test.ts`

- [ ] **Step 1: Write failing test that backend is not blamed when upload is zero**

Add:

```ts
test('backend remains separate when ui layout dominates and uploads are zero', () => {
  const { entries } = parseLog([
    '[2026-06-18 10:00:00][perf/fps]: event=fps_summary operation=ingame_esc_open context=online page=game tab=none sample_frames=30 sample_seconds=0.5 fps_avg=55 fps_min=2 fps_1pct_low=2 fps_1pct_source=real_sampled fps_max=1200 frame_ms_avg=18 frame_ms_p95=15 frame_ms_p99=481 frame_ms_max=481 menu_ms_max=324 window_start_frame=10 window_end_frame=40 cap_limited=0',
    '[2026-06-18 10:00:00][perf/menu]: page=game operation=ingame_esc_open frame=20 stage=ingame_server_info_layout duration_ms=300',
    '[2026-06-18 10:00:00][perf/assets]: stage=assets_preview_gpu_upload_batch operation=ingame_esc_open frame=20 duration_ms=0 uploads_this_frame=0 bytes=0',
  ].join('\n'));
  const summary = summarizeForBundle(entries, 'test.log', { totalLines: entries.length, invalidLines: 0 });
  const text = JSON.stringify(summary);
  assert.match(text, /ui_section:ingame_server_info_layout/);
  assert.doesNotMatch(text, /dominantAttribution":"texture_upload/);
});
```

- [ ] **Step 2: Run failing test**

```pwsh
cd qmclient_scripts/perf
bun test.ts
```

Expected: FAIL until backend separation is explicit.

- [ ] **Step 3: Add backend bucket fields**

In `BudgetCorrelationWindow` and `budgetCorrelationSummary(entries)`, ensure each target window summary separately carries:

```ts
backendUploadMs
textureUploadMs
glyphUploadMs
uiLayoutOrRenderMs
topUiSection
```

- [ ] **Step 4: Update report wording**

In `generateReport(entries, sourceFile, comparison, diagnostics, generationDurationMs)`, add this note near the budget-correlation section:

```text
Graphics backend optimization is not implicated unless backend upload/render buckets dominate the target window. This report keeps backend, glyph/text, and UI/layout buckets separate.
```

- [ ] **Step 5: Verify perf tests**

```pwsh
cd qmclient_scripts/perf
bun test.ts
npx tsc --noEmit
```

Expected: PASS.

### Task 7: Full Runtime Verification And Gate

**Files:**
- Verify only.
- Update: `docs/superpowers/plans/2026-06-18-ui-frame-pacing-full-performance-plan.md`

- [ ] **Step 1: Run docs check**

```pwsh
python qmclient_scripts/gate/check_docs.py
```

Expected: PASS.

- [ ] **Step 2: Run perf tests**

```pwsh
cd qmclient_scripts/perf
bun test.ts
npx tsc --noEmit
```

Expected: PASS.

- [ ] **Step 3: Run C++ focused tests**

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.IngameEscOpenHasConcreteSectionTelemetry:QmMonitoringHelpers.IngameEscPrewarmsStableTextBeforeVisibleFrame:QmMonitoringHelpers.AssetsTabSwitchUsesShellFirstFrame
```

Expected: PASS.

- [ ] **Step 4: Build client**

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
```

Expected: PASS.

- [ ] **Step 5: Fresh runtime session**

Start a new client with perf logging:

```pwsh
Start-Process .\cmake-build-release\DDNet.exe -WorkingDirectory .\cmake-build-release -ArgumentList @('qm_perf_debug 1','qm_perf_logfile 1','qm_perf_debug_threshold_ms 4','gfx_fullscreen 0','snd_enable 0','snd_enable_music 0')
```

In that new client session cover:

- ingame Esc open twice,
- ingame server info tab,
- settings assets tab switch cold and repeated warm switches,
- settings TClient/QmClient/DDNet tab switch,
- assets/resource page scroll and post-scroll settled interval.

- [ ] **Step 6: Generate report from the fresh latest log**

```pwsh
cd qmclient_scripts/perf
bun analyze.ts
```

Expected:

- report verdict not blocked by aggregate-only attribution,
- report names concrete top UI sections for any FPS failure,
- `ingame_esc_open` and repeated assets tab switch meet the FPS baselines in `## Success Criteria`.

If the report names exact concrete sections but any FPS baseline still fails, this step is `FAIL` with a useful diagnosis, not acceptance. Record the blocking section and continue optimizing before running the final gate.

- [ ] **Step 7: Run quick gate**

```pwsh
python qmclient_scripts/gate/check_gate.py --mode quick
```

Expected: PASS.

- [ ] **Step 8: Record evidence in this plan**

Append a `## Final Evidence` section to this plan with:

```text
Command: python qmclient_scripts/gate/check_docs.py
Result: PASS or FAIL, copied from the command outcome.
Scope: Documentation governance and links.
Gaps: None, or the exact skipped/failed checks.

Command: cd qmclient_scripts/perf && bun test.ts && npx tsc --noEmit
Result: PASS or FAIL, copied from the command outcome.
Scope: Perf parser, report, quality gates, and TypeScript type safety.
Gaps: None, or the exact skipped/failed checks.

Command: qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
Result: PASS or FAIL, copied from the command outcome.
Scope: C++ test binary build.
Gaps: None, or the exact skipped/failed checks.

Command: qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
Result: PASS or FAIL, copied from the command outcome.
Scope: Client build.
Gaps: None, or the exact skipped/failed checks.

Command: cd qmclient_scripts/perf && bun analyze.ts
Result: PASS or FAIL, copied from the fresh runtime log report.
Scope: Fresh client runtime performance report.
Gaps: None, or exact target windows still failing.

Command: python qmclient_scripts/gate/check_gate.py --mode quick
Result: PASS or FAIL, copied from the command outcome.
Scope: Repository quick gate.
Gaps: None, or the exact skipped/failed checks.
```

## Rollout Order

1. Task 1 first. No optimization should proceed while target FPS failures only say `ui_layout_or_render_total`.
2. Task 2 next. Ingame Esc is the worst observed spike and needs concrete section data.
3. Task 3 after Task 2. Remove visible text/fallback work from the open frame.
4. Task 4. Apply shell-first and deferred work to assets tab switch.
5. Task 5 and Task 6. Lock FPS baselines and backend separation in the report.
6. Task 7. Run fresh runtime verification; old logs are not acceptance evidence.

## Risk Notes

- `ui_layout_or_render_total` may include work from multiple systems. Do not optimize blindly until Task 1/2 produce concrete section attribution.
- If backend upload/render becomes dominant in a fresh report, stop and write a separate backend-specific plan. Do not mix backend rewrite work into this plan.
- If concrete attribution proves `src/game/client/QmUi/` runtime/layout is the bottleneck, stop after recording the evidence and write a focused QmUI/layout follow-up plan with exact files and tests. Do not broaden this plan to the whole QmUI directory.
- If `menus_settings_assets.cpp` cannot express the required shell-first behavior through `SSettingsAssetsCardHydrationScheduler`, write a focused scheduler/resource-jobs follow-up plan before modifying `settings_resource_jobs.*`.
- Shell-first rendering must preserve visible layout stability. Do not hide controls or change gameplay/menu semantics to make numbers look better.
- FPS baselines are acceptance thresholds for target windows, not global FPS guarantees across all pages.
- Repeated warm switch and cold first switch should be reported separately; otherwise caching effects can hide cold-start problems.

## Self-Review Checklist

- [ ] The old five user questions are represented in this plan.
- [ ] The plan does not claim the previous text-rendering plan is complete.
- [ ] Every task has concrete files, test, implementation steps, commands, and expected results.
- [ ] Graphics backend optimization is explicitly out of scope unless new evidence proves it dominates.
- [ ] Fresh runtime logs are required for acceptance.
- [ ] No task changes protocol, physics, prediction, snapshot, demo, map, or file formats.

## Final Evidence

Command: `python qmclient_scripts/gate/check_docs.py`
Result: PASS. Output ended with `治理文档入口一致，未发现断链。`
Scope: Documentation governance and links.
Gaps: None.

Command: `cd qmclient_scripts/perf && bun test.ts && npx tsc --noEmit`
Result: PASS. Output included `qmclient perf tests passed`; TypeScript typecheck exited 0.
Scope: Perf parser, report, quality gates, FPS baselines, backend/UI attribution separation, and TypeScript type safety.
Gaps: None for static perf tooling.

Command: `qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14`
Result: PASS. Latest run reported `ninja: no work to do.`
Scope: C++ test binary build.
Gaps: None for the focused test binary build.

Command: `cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.IngameEscOpenHasConcreteSectionTelemetry:QmMonitoringHelpers.IngameEscPrewarmsStableTextBeforeVisibleFrame:QmMonitoringHelpers.AssetsTabSwitchUsesShellFirstFrame:QmMonitoringHelpers.AssetsTabSwitchFirstFrameShellOnly:QmMonitoringHelpers.AssetsFirstVisibleFrameHasMetadataWarmupBudget:QmMonitoringHelpers.AssetsCardHydrationSchedulerDefersContentAfterTabSwitch`
Result: PASS. 6 tests ran and passed.
Scope: Task 2, Task 3, and Task 4 source-contract coverage.
Gaps: Source-contract tests do not replace fresh runtime FPS verification.

Command: `qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14`
Result: PASS. `DDNet.exe` linked successfully.
Scope: Client build.
Gaps: None for build.

Command: `cd qmclient_scripts/perf && bun analyze.ts`
Result: NOT RUN for final acceptance.
Scope: Fresh client runtime performance report.
Gaps: Fresh client runtime session is still required. Old logs were not used as acceptance evidence. The remaining runtime gap is to launch the newly built client, exercise ingame Esc open, ingame server info, settings assets cold and warm tab switches, settings TClient/QmClient/DDNet tab switches, and assets scroll/recovery, then generate a report from the latest fresh log.

Command: `python qmclient_scripts/gate/check_gate.py --mode quick`
Result: PASS. Output summary: 10 passed, 0 warnings, 0 failures.
Scope: Repository quick gate.
Gaps: Quick gate does not include full runtime FPS acceptance and does not replace fresh perf report validation.
