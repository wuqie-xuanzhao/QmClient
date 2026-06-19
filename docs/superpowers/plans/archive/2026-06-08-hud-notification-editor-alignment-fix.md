# HUD Notification Editor Alignment Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make HUD notifications report their real visible bounds to the HUD editor and render with complete left/right anchoring semantics.

**Architecture:** Keep the existing notification route/settings pipeline intact, but split notification rendering into measurable geometry and actual draw steps. Add a HUD-editor preview transform path so notification layout can choose the correct horizontal growth direction before the real transform registration happens.

**Tech Stack:** C++17, DDNet/QmClient HUD editor, HUD notification renderer, gtest, Windows `qmclient_scripts/cmake-windows.cmd`

---

## File Structure

- Modify: `src/game/client/components/qmclient/hud_notifications.h`
  - Add pure geometry helpers for visible rect and left/right placement.
- Modify: `src/game/client/components/qmclient/hud_notifications.cpp`
  - Split render preparation from draw, compute real visible rect, and wire left/right growth logic.
- Modify: `src/game/client/components/hud_editor.h`
  - Expose a preview transform API without visible-element registration side effects.
- Modify: `src/game/client/components/hud_editor.cpp`
  - Refactor shared transform computation so preview and real begin-transform stay consistent.
- Modify: `src/test/qm_hud_notifications_test.cpp`
  - Add geometry regression tests for visible bounds and left/right placement.

## Task 1: Lock Notification Geometry With Failing Tests

**Files:**
- Modify: `src/test/qm_hud_notifications_test.cpp`
- Modify: `src/game/client/components/qmclient/hud_notifications.h`

- [ ] **Step 1: Write failing geometry tests**

Add tests for:

- content-driven visible rect width;
- left-growth box placement;
- right-growth box placement.

- [ ] **Step 2: Run focused tests to verify failure**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
E:\Coding\DDNet\QmClient\cmake-build-release\testrunner.exe --gtest_filter="*QmHudNotifications*Geometry*"
```

Expected: compile or assertion failure because helpers do not exist yet.

- [ ] **Step 3: Add minimal pure geometry helpers**

Implement helper API in `hud_notifications.h` only after the tests are in place.

- [ ] **Step 4: Re-run focused tests**

Run the same command and expect PASS.

## Task 2: Add HUD Editor Preview Transform

**Files:**
- Modify: `src/game/client/components/hud_editor.h`
- Modify: `src/game/client/components/hud_editor.cpp`

- [ ] **Step 1: Add preview transform API**

Expose a side-effect-free transform preview method returning the same placement metadata as `BeginTransform(...)`.

- [ ] **Step 2: Refactor shared transform math**

Make preview and real begin-transform share one computation path so anchor detection cannot drift.

- [ ] **Step 3: Build and run notification tests**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
E:\Coding\DDNet\QmClient\cmake-build-release\testrunner.exe --gtest_filter="*QmHudNotifications*:*QmHudNotificationRules*"
```

Expected: PASS.

## Task 3: Rework Notification Layout To Use Real Visible Bounds

**Files:**
- Modify: `src/game/client/components/qmclient/hud_notifications.cpp`
- Modify: `src/game/client/components/qmclient/hud_notifications.h`

- [ ] **Step 1: Prepare notification layout before real transform registration**

Use HUD-editor preview placement to choose left/right growth direction, then compute:

- actual visible width;
- actual used height;
- actual visible rect offset inside the transform rect.

- [ ] **Step 2: Render with left/right-aware placement**

Make each notification box position derive from the chosen horizontal growth direction instead of unconditional right anchoring.

- [ ] **Step 3: Register the real visible rect with HUD editor**

Use the measured visible rect in the real `BeginTransform(...)` path and keep `UpdateVisibleRect(...)` consistent with the same bounds.

- [ ] **Step 4: Run focused regression tests**

Run:

```powershell
E:\Coding\DDNet\QmClient\cmake-build-release\testrunner.exe --gtest_filter="*QmHudNotifications*Geometry*:*QmHudNotifications*HandleServerChat*:*QmHudNotificationRules*"
```

Expected: PASS.

## Task 4: Full Verification And Review Gate

**Files:**
- No new file changes required unless verification finds issues

- [ ] **Step 1: Run doc gate**

```powershell
python qmclient_scripts/gate/check_docs.py
```

- [ ] **Step 2: Run build and relevant tests**

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
E:\Coding\DDNet\QmClient\cmake-build-release\testrunner.exe --gtest_filter="*SwapCountdownMessage*:*QmHudNotifications*:*QmHudNotificationRules*:*RaceHelper*:*Score*"
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
```

- [ ] **Step 3: Dispatch read-only review**

Run a fresh read-only review focused on:

- notification geometry and anchoring;
- HUD editor integration;
- regressions in notification routing.

- [ ] **Step 4: Commit**

Use a focused commit after review findings are resolved.
