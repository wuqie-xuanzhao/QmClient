# 设置页 Tee Work Drain 源头降温 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 post-FBO 基线 `4f0a5f8ba` 之上，把 `settings:tee` 页面的主线程 `Work Drain` 压回到受控范围，优先收紧后台 backlog 和 idle drain 的过度预取。

**Architecture:** `docs/superpowers/specs/2026-06-10-设置页性能优化总纲.md` 已经把 FBO cleanup 后的 `settings:tee / Work Drain` 定为当前第一热点。问题不是“日志不够”，而是 Tee 列表在 `all_visible_ready -> idle_drain` 之后会继续发放过大的后台请求窗口，导致历史 `BACKGROUND_REQUESTED` backlog 被放大，并在后续 `PENDING/LOADING -> finalize/upload/merge` 阶段集中回到主线程。本轮只做 source cooling：收紧 `settings_resource_jobs.*` 的背景请求预算与 backlog 上限，保持 `menus_settings.cpp -> skins.cpp` 的 admission / loading 合同不变，并用现有 perf log 字段验证行为没有失真。

**Tech Stack:** C++, GoogleTest, settings resource jobs helpers, existing perf telemetry, CMake Windows gate.

---

## Scope

本计划只做 `settings:tee / Work Drain` 主线优化，不重建 `qmclient_scripts/perf`，不重新引入 FBO，不把 `server_browser / list_frame`、Demo Browser 异步化、Assets drain 或统一 scheduler 重构塞进来。

## Spec Phase Mapping

本计划是 `docs/superpowers/specs/2026-06-10-设置页性能优化总纲.md` 的 **Phase 1 / P0-D 子计划**，不是完整总计划。

| Spec 阶段 | 本计划处理方式 |
|----------|----------------|
| Phase 0：实现前 UI Bug Audit | 本计划新增 Task 0，作为动性能代码前的前置检查。 |
| Phase 1：源头降温 | 本计划只处理 P0-D：`IDLE_DRAIN` 预算降到 8 + hard backlog cap。 |
| Phase 2：缓存 | 不做。必须等 Phase 1 源头降温完成，且 telemetry 证明仍有实时 CPU 成本后再开独立计划。 |
| Phase 3：锦上添花 | 不做。图标图集、overdraw、每帧分配审计都不进入本计划。 |

Phase 1 内其它 P0 项也不混入本计划：

- P0-A：文本缓存全覆盖，后续单独计划。
- P0-B：Section 测量延迟化 / 视口外 section 跳过 widget，后续单独计划。

## Current Baseline

- 起点提交：`4f0a5f8ba perf(settings): 清理设置页FBO缓存路径`
- 权威 spec：`docs/superpowers/specs/2026-06-10-设置页性能优化总纲.md`
- 当前 post-FBO perf baseline：`2026-06-10 14:17:59`，`p99=12.906ms`，`spike=464`
- 当前第一热点：`settings:tee / Work Drain`，单帧 merge 272 个 job 结果，`6839.642ms`
- 当前代码事实：
  - `SettingsSkinThroughputProfileForMode(IDLE_DRAIN)` 仍为 `m_BackgroundRequestBudget = 24`
  - `SettingsSkinBackgroundRequestBudgetDecision(...)` 仍允许“有 recent loaded delta”时继续扩大大型 backlog
  - `menus_settings.cpp` 已有 `request_budget_default` / `request_budget_actual` / `request_budget_block_reason` / `max_requested` / `max_real_inflight` 等验证字段
- 当前验证 gap：`python qmclient_scripts/gate/check_gate.py --mode quick` 在起点提交前已知会因两个未改动 Python 脚本的 `ruff format` 失败而退出 1：
  - `qmclient_scripts/languages_qmclient/extract_strings.py`
  - `qmclient_scripts/languages_qmclient/generate_all.py`
  执行本计划时不要把这个既有格式 gap 误判为 Work Drain 改动失败；除非本任务明确扩大范围，否则不要顺手格式化这两个无关脚本。

## Source Context

**Authoritative docs:**

- `docs/superpowers/specs/2026-06-10-设置页性能优化总纲.md`
- `docs/superpowers/explore/2026-06-10-页面性能框架阶段路径校准.md`
- `docs/superpowers/explore/2026-06-09-性能量化固定场景.md`

**Current implementation anchors:**

- `src/game/client/components/menus_settings.cpp`
- `src/game/client/components/settings_resource_jobs.cpp`
- `src/game/client/components/settings_resource_jobs.h`
- `src/game/client/components/skins.cpp`
- `src/test/settings_warmup_test.cpp`
- `src/test/skins_test.cpp`

## File Structure

- Modify `src/game/client/components/settings_resource_jobs.h`
  - Keep the budget-decision input/output contract focused on backlog gating and stop reasons.
- Modify `src/game/client/components/settings_resource_jobs.cpp`
  - Tighten idle-drain background request budget and add a hard backlog cap so healthy progress no longer authorizes unbounded prefetch backlog growth.
- Modify `src/test/settings_warmup_test.cpp`
  - Add deterministic helper-level tests for the new backlog cap and idle-drain tuning.
- Modify `src/test/skins_test.cpp`
  - Keep source-contract coverage around `menus_settings.cpp` / `skins.cpp` logging and queue-state glue.

---

### Task 0: UI Bug Audit Before Performance Changes

**Files:**
- Inspect: `src/game/client/components/menus_settings.cpp`
- Inspect: `src/game/client/components/settings_resource_jobs.cpp`
- Inspect: `src/game/client/components/skins.cpp`
- Inspect: `src/test/settings_warmup_test.cpp`
- Inspect: `src/test/skins_test.cpp`

- [ ] **Step 1: List the affected UI paths**

Record the affected paths in the implementation notes before editing production code:

```text
Affected paths:
- Settings -> Tee tab initial entry
- Settings -> Tee tab Player/Dummy switch
- Tee skin list fast scroll
- Tee skin list idle settle after scroll
- Tee skin preview loading/status indicators
```

- [ ] **Step 2: Search for known same-path bug reports and stale cleanup notes**

Run:

```powershell
rg -n "Tee|tee|skin list|Work Drain|work_drain|BACKGROUND_REQUESTED|IDLE_DRAIN|stale|错位|花屏|焦点|滚动跳动|loading|preview" docs src/test src/game/client/components
```

Expected: any relevant same-path bug or cleanup note is either out of scope and recorded, or fixed before continuing. Do not broaden this plan for unrelated Server Browser / Demo Browser / Assets findings.

- [ ] **Step 3: Inspect the current Tee request and loading contracts**

Verify these current contracts still exist before modifying budgets:

```powershell
rg -n "SettingsSkinBackgroundRequestBudgetDecision|SettingsSkinThroughputProfileForMode|BackgroundBudgetDecision|request_budget_block_reason|SetSettingsTeeVisibleSnapshot|UpdateStartLoading" src/game/client/components src/test
```

Expected:
- `menus_settings.cpp` still calls `SettingsSkinBackgroundRequestBudgetDecision(...)`
- `settings_resource_jobs.cpp` still owns `SettingsSkinThroughputProfileForMode(...)`
- `skins.cpp` still owns admission/loading behavior
- tests still cover `TeeSettingsListEmitsRequestWindowPerfLogs` and `ThroughputControllerKeepsVisibleBacklogOutOfIdleDrain`

- [ ] **Step 4: Stop or split if a real same-path UI bug is found**

If the audit finds a same-path user-visible bug that can invalidate performance measurements, stop this plan and create/fix that bug first. Examples:

```text
- Tee preview shows stale skin after scroll settles
- Visible skin falls back to loading while already displayed
- Player/Dummy switch loses selected skin state
- Fast scroll changes selection unexpectedly
```

If no such bug is found, record:

```text
Phase 0 audit result: no blocking same-path UI bug found; proceed with P0-D source cooling.
```

---

### Task 1: Lock The Source-Cooling Contract In Tests

**Files:**
- Modify: `src/test/settings_warmup_test.cpp`
- Modify: `src/test/skins_test.cpp`

- [ ] **Step 1: Write a failing helper test for healthy-progress backlog capping**

Add this test near the existing `TeeBackgroundRequestBudget*` coverage:

```cpp
TEST(SettingsResourceJobs, TeeBackgroundRequestBudgetCapsHealthyBacklogBeforeQueueInflates)
{
	SSettingsSkinBackgroundRequestBudgetInput Input;
	Input.m_DefaultBudget = 24;
	Input.m_Pending = 8;
	Input.m_Loading = 8;
	Input.m_BackgroundRequested = 256;
	Input.m_CountFuseLimit = 128;
	Input.m_VisibleReserve = 0;
	Input.m_RecentLoadedDelta = 4;
	Input.m_RecentAdmittedDelta = 4;
	Input.m_DrainActive = true;

	const auto Decision = SettingsSkinBackgroundRequestBudgetDecision(Input);
	EXPECT_EQ(Decision.m_RequestBudget, 0);
	EXPECT_EQ(Decision.m_BlockReason, ESettingsSkinBackgroundRequestBlockReason::STALL_BACKPRESSURE);
}
```

- [ ] **Step 2: Write a failing helper test for smaller idle-drain request budget**

Update the settled branch in `ThroughputControllerKeepsVisibleBacklogOutOfIdleDrain` so it expects a smaller background request budget:

```cpp
EXPECT_EQ(Settled.m_Mode, ESettingsSkinThroughputControllerMode::IDLE_DRAIN);
EXPECT_TRUE(Settled.m_BackgroundDrainActive);
EXPECT_EQ(Settled.m_BackgroundRequestBudget, 8);
```

If another test asserts the old `24`, update that test in the same edit so the suite expresses the new target consistently.

- [ ] **Step 3: Strengthen the existing source-contract test for runtime glue staying intact**

`src/test/skins_test.cpp` already checks `menus_settings.cpp` uses `BackgroundBudgetDecision` and logs request-window/work-drain fields. Add the missing explicit assertions near `TeeSettingsListEmitsRequestWindowPerfLogs` so the test fails if the runtime handoff stops exposing the budget decision reason:

```cpp
EXPECT_NE(Source.find("const auto BackgroundBudgetDecision = SettingsSkinBackgroundRequestBudgetDecision({"), std::string::npos);
EXPECT_NE(Source.find("const int BackgroundRequestBudget = BackgroundBudgetDecision.m_RequestBudget;"), std::string::npos);
EXPECT_NE(Source.find("request_budget_block_reason=%s"), std::string::npos);
EXPECT_NE(Source.find("SettingsSkinBackgroundRequestBlockReasonName(BackgroundBudgetDecision.m_BlockReason)"), std::string::npos);
```

- [ ] **Step 4: Run the targeted C++ tests and confirm RED**

Run:

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release\testrunner.exe --gtest_filter=SettingsResourceJobs.TeeBackgroundRequestBudgetCapsHealthyBacklogBeforeQueueInflates:SettingsResourceJobs.ThroughputControllerKeepsVisibleBacklogOutOfIdleDrain:Skins.TeeSettingsListEmitsRequestWindowPerfLogs
```

Expected: helper tests fail before the implementation patch because current `IDLE_DRAIN` budget is still `24` and healthy progress still allows oversized backlog growth. The source-contract test may already pass if the logging glue is intact.

---

### Task 2: Tighten Idle-Drain Budget And Backlog Cap

**Files:**
- Modify: `src/game/client/components/settings_resource_jobs.cpp`
- Modify: `src/game/client/components/settings_resource_jobs.h`

- [ ] **Step 1: Lower the idle-drain request budget in the throughput profile**

In `SettingsSkinThroughputProfileForMode(...)`, change the `IDLE_DRAIN` profile from:

```cpp
Profile.m_BackgroundRequestBudget = 24;
```

to:

```cpp
Profile.m_BackgroundRequestBudget = 8;
```

Keep `SCROLL_ACTIVE` / `POST_SCROLL_RECOVERY` at `0`, and do not change unrelated upload/finalize/window bounds in the same step.

- [ ] **Step 2: Add a hard backlog cap to the budget decision**

Inside `SettingsSkinBackgroundRequestBudgetDecision(...)`, keep the existing real-inflight and visible-reserve checks, then add a hard cap before computing the final request budget:

```cpp
const int HardBacklogLimit = maximum(CountFuseLimit, maximum(Input.m_DefaultBudget, 1) * 8);
if(Input.m_BackgroundRequested >= HardBacklogLimit)
{
	Output.m_BlockReason = ESettingsSkinBackgroundRequestBlockReason::STALL_BACKPRESSURE;
	return Output;
}
```

After that, keep the softer stall branch for “large backlog + no recent progress”, but tighten it so both loaded and admitted deltas must be dry before reopening:

```cpp
const int BacklogHighWatermark = maximum(VisibleReserve * 8, CountFuseLimit * 2);
if(Input.m_BackgroundRequested >= BacklogHighWatermark &&
	Input.m_RecentLoadedDelta <= 0 &&
	Input.m_RecentAdmittedDelta <= 0)
{
	Output.m_BlockReason = ESettingsSkinBackgroundRequestBlockReason::STALL_BACKPRESSURE;
	return Output;
}
```

This keeps the stop-reason vocabulary stable while preventing “healthy progress” from authorizing thousands of queued background requests.

- [ ] **Step 3: Keep the header contract minimal**

If the implementation needs an inline helper, add it locally in `settings_resource_jobs.cpp`. Do not expand `settings_resource_jobs.h` with new public enums or telemetry shapes unless the `.cpp` patch truly cannot stay private.

- [ ] **Step 4: Run the targeted tests and confirm GREEN**

Run:

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release\testrunner.exe --gtest_filter=SettingsResourceJobs.TeeBackgroundRequestBudgetCapsHealthyBacklogBeforeQueueInflates:SettingsResourceJobs.ThroughputControllerKeepsVisibleBacklogOutOfIdleDrain
```

Expected: both helper tests pass.

---

### Task 3: Keep `skins.cpp` And Telemetry Contracts Aligned

**Files:**
- Modify: `src/game/client/components/skins.cpp`
- Modify: `src/test/skins_test.cpp`

- [ ] **Step 1: Verify no extra production queue path is needed**

Keep `UpdateStartLoading(...)` in `skins.cpp` on the current contract:

```cpp
const auto Admission = DetermineAdmission(pSkinContainer, Priority);
if(!Admission.m_PromoteAllowed)
{
	str_copy(m_SettingsSourceAdmissionTelemetry.m_aLastWaitReason, Admission.m_pBlockReason, sizeof(m_SettingsSourceAdmissionTelemetry.m_aLastWaitReason));
	LogSettingsSkinSourceWaitEvent(...);
	return false;
}
```

The source-cooling change must come from “fewer background requests are issued”, not from introducing a second delayed queue or a new state machine branch.

- [ ] **Step 2: If a small code cleanup is needed, keep it local**

Only if the new budget cap exposes duplication or stale naming in the `menus_settings.cpp -> skins.cpp` handoff, do the smallest possible cleanup. Example acceptable shape:

```cpp
const auto BackgroundBudgetDecision = SettingsSkinBackgroundRequestBudgetDecision({...});
const int BackgroundRequestBudget = BackgroundBudgetDecision.m_RequestBudget;
```

Do not rewrite `RenderSettingsTee`, `UpdateStartLoading`, or `PrepareSettingsThroughputForFrame` into new helper layers just because this patch touches them.

- [ ] **Step 3: Re-run the source-contract test**

Run:

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release\testrunner.exe --gtest_filter=Skins.TeeSettingsListEmitsRequestWindowPerfLogs
```

Expected: PASS, proving the telemetry handoff still exposes the same key request-window / work-drain fields.

---

### Task 4: Verification And Perf Gate

**Files:**
- No new source files beyond earlier tasks.

- [ ] **Step 1: Run the full C++ test target**

Run:

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
```

Expected: `run_cxx_tests` exits with code 0.

- [ ] **Step 2: Run the quick gate**

Run:

```powershell
python qmclient_scripts/gate/check_gate.py --mode quick
```

Expected: quick gate exits with code 0. If it exits 1 only because of the known pre-existing `ruff format` findings in `qmclient_scripts/languages_qmclient/extract_strings.py` and `qmclient_scripts/languages_qmclient/generate_all.py`, record that as an unrelated verification gap and do not modify those files in this task.

- [ ] **Step 3: Capture the perf follow-up gap explicitly**

This patch is not complete until the implementation note or final report records that the next perf re-check must use:

```text
PERF-SETTINGS-TEE-SWITCH
PERF-TEE-SCROLL
```

from `docs/superpowers/explore/2026-06-09-性能量化固定场景.md`, and confirms whether the top attribution count/backlog moved down from the `2026-06-10 14:17:59` post-FBO baseline.

## Completion Criteria

The implementation is complete only when all are true:

1. `SettingsSkinBackgroundRequestBudgetDecision(...)` no longer permits unbounded `BACKGROUND_REQUESTED` backlog growth just because some loads are still completing.
2. `IDLE_DRAIN` no longer starts with the previous oversized background request budget.
3. `skins.cpp` still uses the same admission / wait-reason telemetry contract; no second queue type or scheduler rewrite was introduced.
4. Targeted helper tests and `run_cxx_tests` pass; `check_gate --mode quick` either passes or reports only the documented pre-existing `ruff format` gap.
5. Final report explicitly records that perf validation against `PERF-SETTINGS-TEE-SWITCH` / `PERF-TEE-SCROLL` is still required if a fresh client run was not performed in this session.
