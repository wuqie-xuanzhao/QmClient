> **已归档，禁止作为实现依据。** 本文保留为历史记录；当前实现请以活动 spec/plan/skill 为准。

# Gores Map Progress Budgeted Build Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Avoid a single long main-thread frame when `qm_player_stats_map_progress` first builds the Gores distance field.

**Architecture:** Keep the current distance-field algorithm and result semantics, but execute the build through a small per-tick state machine. The HUD continues to show no progress until the field is ready.

**Tech Stack:** C++, existing DDNet/QmClient component code, GoogleTest.

---

### Task 1: Add Budget Helper Tests

**Files:**
- Modify: `src/game/client/components/qmclient/modes.h`
- Modify: `src/game/client/components/qmclient/modes.cpp`
- Modify: `src/test/qm_modes_test.cpp`

- [ ] Add a pure helper that consumes a bounded number of work units and reports whether more work remains.
- [ ] Add tests proving the helper stops at the budget and handles zero/negative budget safely.

### Task 2: Convert Gores Distance Field Build To Incremental State

**Files:**
- Modify: `src/game/client/components/tclient/tclient.h`
- Modify: `src/game/client/components/tclient/tclient.cpp`

- [ ] Add CTClient build state for scan, semantic layer pass, queue init, Dijkstra, and reachable-start check.
- [ ] Replace synchronous `BuildGoresDistanceField()` calls from `EnsureGoresDistanceField()` with per-tick progress.
- [ ] Keep final `m_vGoresCMap`, `m_vvGoresDirectTeleOuts`, and `m_vGoresDistanceToFinish` semantics unchanged after completion.

### Task 3: Verify

**Files:**
- None.

- [ ] Run focused C++ tests for `QmGoresMode`.
- [ ] Run full C++ tests if available.
- [ ] Run `python qmclient_scripts/gate/check_gate.py --mode quick`.
