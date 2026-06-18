# Text Rendering Stabilization and Observability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把当前分散的通用文本渲染优化收口成语义稳定、命中率可信、预算自适应的文本渲染管线，避免按钮/标签居中、边距、fallback 和滚动期间卡顿反复回归。

**Architecture:** 先建立 `CUi` 级 canonical cached label 渲染语义，让 cached path 与 immediate `DoLabel` 在对齐、颜色、尺寸和换行上保持一致；再让 `CMenus` 的 text pool 只负责 stable identity、预建和预算，不再绕过 UI 对齐语义。Telemetry 拆分 pool hit、render-ready hit、queued build、fallback immediate 和 recreated work，并接入现有 `SSettingsAdaptiveBudget*`，用帧余量和滚动状态调整预算，同时保留滚动硬上限。

**Tech Stack:** C++, DDNet/QmClient CUI, `CUIElement` text containers, existing `SSettingsAdaptiveBudgetInput/State/Output`, `perf/text` and `perf/settings-text` telemetry, GoogleTest source-contract tests, TypeScript perf report tests, Windows CMake gate.

---

## Status And Baseline

- 计划日期：2026-06-18
- 起点提交：`acdb1e3e7`
- 调查约束：本计划来自 codegraph-backed 调查，不以旧文档结论为依据。
- 关键现状：
  - `CUi::DoLabelStreamed` 位于 `src/game/client/ui.cpp`，已有部分缓存失效条件和对齐重算。
  - `CMenus::MenuTextElement` / `DoMenuLabelStreamed` 位于 `src/game/client/components/menus.cpp`，实现了 menu text pool、visible guard、prebuild plan 和 budget queue。
  - `CTextRender::FlushQmTextRuntimeBudgetLog` 位于 `src/engine/client/text.cpp`，记录 glyph/container 创建和上传成本。
  - `qmclient_scripts/perf/lib/stats.ts` 和 `report.ts` 已解析 `settings_text_*` 与 `perf/text`，但命中率仍可能把 pool-level hit 误当作 render-ready no-work hit。

## Problem Statement

当前“通用文本渲染优化”不是一个完整系统，而是多个局部层叠：

- backend 层：`CreateTextContainer` / `UploadTextContainer` / `RenderTextContainer` 统计创建上传成本。
- UI 层：`CUi::DoLabelStreamed` 缓存 `CUIElement::SUIElementRect` 内的文本容器。
- 菜单层：`CMenus` 维护 `m_MenuTextPool`、计划收集、预建、可见区统计和预算队列。
- Ingame snapshot/MOTD/Chat/Browser 等路径各有局部缓存或 immediate text。

因此出现三个工程问题：

- 语义不稳定：cached path 和 immediate `DoLabel` 不完全等价，尤其是 `DoMenuLabelStreamed` 直接 `RenderTextContainer(..., pRect->x, pRect->y)` 会绕过 `TEXTALIGN_MC` 等对齐语义。
- 指标不可信：`hits` 可表示 pool entry 命中，但不必然表示本帧无需创建、不排队、不 fallback、且最终用正确位置渲染。
- 预算不可迁移：硬编码上限如 16 不能代表所有机器；滚动时又必须有低硬上限，否则高端机以外的环境容易卡顿。

## Success Criteria

本计划完成后必须满足：

- cached label 与 immediate label 在同一 rect、text、font size、align、label props、color 下渲染位置一致。
- 菜单层不再直接用 raw `pRect->x/y` 渲染 cached label，除非显式用于已预对齐容器且有测试证明。
- `settings_text_usage` 拆分出可信字段：`pool_hit`、`render_ready_hit`、`build_queued`、`fallback_immediate`、`text_recreated`。
- 报表中“真实命中率”只使用 `render_ready_hit / candidates`，不再用 pool hit 证明性能收益。
- target settings / ingame Esc 验收窗口要求：`miss=0`、`stale=0`、`unplanned=0`、`key_mismatch=0`、`fallback_immediate=0`、`text_recreated=0`。
- 自适应预算有两层限制：设备/帧余量驱动的 soft target，以及滚动/帧压下的 hard cap。
- 滚动期间 text container build、glyph rasterize、glyph upload 均有硬上限，默认不超过低预算；静止且帧稳定时可逐步增加。
- `qmclient_scripts/perf` report 能明确写出“命中率是否足以验收”，而不是只展示好看的百分比。

## Non-Goals

- 不改网络协议、demo/skin/map 格式、物理、预测、snapshot 语义。
- 不重写 FreeType 或 graphics backend。
- 不在第一轮迁移所有 `TextRender()->Text(...)` 调用。
- 不把所有动态文本强制纳入 menu text pool。
- 不新增线程或后台 GPU 上传模型。
- 不追求第一轮得到最高命中率；第一目标是语义正确和指标可信。

## File Structure

- Modify: `src/game/client/ui.h`
  - 增加 cached label render helper 的声明，暴露 `NeedsRecreate` / render-ready 状态给菜单层。
- Modify: `src/game/client/ui.cpp`
  - 抽出 `RenderLabelTextContainerAligned`，统一 cached label 的对齐和 flush 语义。
  - 扩展 `CUIElement::SUIElementRect` 缓存字段，补齐 style signature。
- Modify: `src/game/client/components/menus.h`
  - 增加 menu text usage counters 和 render-ready 结果结构。
  - 扩展 `SMenuTextStyleKey`，让 key 覆盖真实 UI scale、HiDPI、color/style 相关字段。
- Modify: `src/game/client/components/menus.cpp`
  - 让 `DoMenuLabelStreamed` 通过 `CUi` helper 渲染，不直接 raw xy 渲染。
  - 拆分 pool hit 与 render-ready hit。
  - 接入自适应预算 hard cap 和 telemetry。
- Modify: `src/game/client/components/settings_resource_jobs.h`
  - 扩展 `SSettingsAdaptiveBudgetInput/Output`，加入文本成本 EWMA、设备等级、滚动 hard cap 字段。
- Modify: `src/game/client/components/settings_resource_jobs.cpp`
  - 调整 `SettingsAdaptiveBudgetStep`，让 text budgets 从硬编码上限变成 frame slack + mode hard cap。
- Modify: `src/engine/textrender.h`
  - 如需暴露上一帧文本 runtime 成本快照，在接口上新增轻量只读 method。
- Modify: `src/engine/client/text.cpp`
  - 将 `FlushQmTextRuntimeBudgetLog` 中的 counters 同步到可查询 snapshot，供 adaptive scheduler 使用。
- Modify: `qmclient_scripts/perf/lib/stats.ts`
  - 解析新增 usage 字段，真实命中率使用 `render_ready_hit`。
- Modify: `qmclient_scripts/perf/lib/report.ts`
  - 报表显示 pool hit 与 render-ready hit 的差异，避免误读。
- Modify: `qmclient_scripts/perf/lib/quality.ts`
  - 将文本命中率可信度缺失、fallback、visible recreate 纳入 warning / failure。
- Modify: `qmclient_scripts/perf/test.ts`
  - 增加字段解析、真实命中率和 acceptance-blocking 测试。
- Modify: `src/test/qmclient_monitoring_test.cpp`
  - 增加 C++ 源码合同测试，防止 raw xy render、漏字段和错误验收口径回归。
- Modify: `src/test/settings_warmup_test.cpp`
  - 增加 adaptive budget 的滚动 hard cap、稳定帧增长、低端设备限制测试。

---

### Task 1: Establish Canonical Cached Label Rendering Semantics

**Files:**
- Modify: `src/game/client/ui.h`
- Modify: `src/game/client/ui.cpp`
- Test: `src/test/qmclient_monitoring_test.cpp`

- [ ] **Step 1: Write failing source-contract test for aligned cached rendering**

Add this test to `src/test/qmclient_monitoring_test.cpp` near existing `DoLabelStreamed` tests:

```cpp
TEST(QmMonitoringHelpers, StreamedLabelRenderUsesCanonicalAlignmentHelper)
{
	const std::string Source = ReadTextFile("src/game/client/ui.cpp");
	const std::string Header = ReadTextFile("src/game/client/ui.h");
	const std::string StreamedBody = ExtractSourceFunctionBody(Source, "void CUi::DoLabelStreamed(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps, int StrLen, const CTextCursor *pReadCursor, bool Render, bool *pTextContainerRecreated) const");

	EXPECT_NE(Header.find("RenderLabelTextContainerAligned"), std::string::npos);
	EXPECT_NE(Source.find("void CUi::RenderLabelTextContainerAligned"), std::string::npos);
	EXPECT_NE(StreamedBody.find("RenderLabelTextContainerAligned(RectEl, pRect, Align)"), std::string::npos);
	EXPECT_EQ(StreamedBody.find("TextRender()->RenderTextContainer(RectEl.m_UITextContainer"), std::string::npos);
	EXPECT_EQ(StreamedBody.find("pRect->x, pRect->y"), std::string::npos);
}
```

- [ ] **Step 2: Run the focused failing test**

Run:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.StreamedLabelRenderUsesCanonicalAlignmentHelper
```

Expected: FAIL because `RenderLabelTextContainerAligned` does not exist or `DoLabelStreamed` still renders directly.

- [ ] **Step 3: Add the canonical helper declaration**

In `src/game/client/ui.h`, add this public const helper next to `DoLabelStreamed`:

```cpp
	void RenderLabelTextContainerAligned(const CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, int Align) const;
```

- [ ] **Step 4: Implement the helper and use it in `DoLabelStreamed`**

In `src/game/client/ui.cpp`, add this method immediately before `CUi::DoLabelStreamed`:

```cpp
void CUi::RenderLabelTextContainerAligned(const CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, int Align) const
{
	if(pRect == nullptr || !RectEl.m_UITextContainer.Valid())
		return;

	const float *pBiggestCharHeight = RectEl.m_LineCount == 1 ? &RectEl.m_BiggestCharacterHeight : nullptr;
	const vec2 CursorPos = CalcAlignedCursorPos(pRect, vec2(RectEl.m_Cursor.m_LongestLineWidth, RectEl.m_Cursor.Height()), Align, pBiggestCharHeight);
	FlushQuadBatch();
	TextRender()->RenderTextContainer(RectEl.m_UITextContainer, RectEl.m_TextColor, RectEl.m_TextOutlineColor, CursorPos.x, CursorPos.y);
}
```

Replace the render block inside `CUi::DoLabelStreamed` with:

```cpp
	if(Render)
	{
		RenderLabelTextContainerAligned(RectEl, pRect, Align);
	}
```

- [ ] **Step 5: Verify focused test passes**

Run:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.StreamedLabelRenderUsesCanonicalAlignmentHelper
```

Expected: PASS.

- [ ] **Step 6: Commit checkpoint if implementing in a branch**

```bash
git add src/game/client/ui.h src/game/client/ui.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "fix(ui): 统一缓存文本对齐渲染入口"
```

### Task 2: Route Menu Cached Text Rendering Through The Canonical Helper

**Files:**
- Modify: `src/game/client/components/menus.cpp`
- Test: `src/test/qmclient_monitoring_test.cpp`

- [ ] **Step 1: Write failing test blocking raw xy menu cached rendering**

Add this test to `src/test/qmclient_monitoring_test.cpp`:

```cpp
TEST(QmMonitoringHelpers, MenuLabelStreamedDoesNotBypassCachedLabelAlignment)
{
	const std::string Menus = ReadTextFile("src/game/client/components/menus.cpp");
	const std::string Body = ExtractSourceFunctionBody(Menus, "void CMenus::DoMenuLabelStreamed(EMenuTextScope Scope, CUIElement &Element, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps, int StrLen, const CTextCursor *pReadCursor, bool Render)");

	EXPECT_NE(Body.find("Ui()->RenderLabelTextContainerAligned(*pElementRect, pRect, Align);"), std::string::npos);
	EXPECT_EQ(Body.find("TextRender()->RenderTextContainer(pElementRect->m_UITextContainer"), std::string::npos);
	EXPECT_EQ(Body.find("pRect->x, pRect->y"), std::string::npos);
}
```

- [ ] **Step 2: Run the focused failing test**

Run:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.MenuLabelStreamedDoesNotBypassCachedLabelAlignment
```

Expected: FAIL while `DoMenuLabelStreamed` still calls `TextRender()->RenderTextContainer(..., pRect->x, pRect->y)`.

- [ ] **Step 3: Replace direct render calls in `DoMenuLabelStreamed`**

In `src/game/client/components/menus.cpp`, replace both direct ready-container render branches with:

```cpp
	if(Render && pElementRect->m_UITextContainer.Valid() && pRect != nullptr)
		Ui()->RenderLabelTextContainerAligned(*pElementRect, pRect, Align);
```

The first replacement is in the `NeedsBuild && m_pSettingsTextPrebuildBudget == nullptr` queued-build branch. The second replacement is in the final ready render branch.

- [ ] **Step 4: Verify focused test passes**

Run:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.MenuLabelStreamedDoesNotBypassCachedLabelAlignment
```

Expected: PASS.

- [ ] **Step 5: Manually inspect high-risk call sites**

Use codegraph, not grep, to list the relevant callers:

```pwsh
codegraph callers "CMenus::DoMenuLabelStreamed"
```

Expected: menu buttons, settings labels, ingame menu labels, and assets metadata call through the same helper. No separate call site should directly render cached menu text with raw `pRect->x/y`.

- [ ] **Step 6: Commit checkpoint if implementing in a branch**

```bash
git add src/game/client/components/menus.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "fix(menus): 复用缓存文本对齐渲染语义"
```

### Task 3: Make Text Style Keys Reflect Real Render Semantics

**Files:**
- Modify: `src/game/client/components/menus.h`
- Modify: `src/game/client/components/menus.cpp`
- Test: `src/test/qmclient_monitoring_test.cpp`

- [ ] **Step 1: Write failing test for non-placeholder style key fields**

Add this test:

```cpp
TEST(QmMonitoringHelpers, MenuTextStyleKeyIncludesRealScaleAndColorState)
{
	const std::string Header = ReadTextFile("src/game/client/components/menus.h");
	const std::string Source = ReadTextFile("src/game/client/components/menus.cpp");
	const std::string BuildBody = ExtractSourceFunctionBody(Source, "CMenus::SMenuTextStyleKey BuildMenuTextStyleKey(const CUIRect *pRect, float FontSize, int Align, const SLabelProperties &LabelProps)");

	EXPECT_NE(Header.find("int m_HiDpiScaleBucket"), std::string::npos);
	EXPECT_NE(Header.find("int m_TextColorHash"), std::string::npos);
	EXPECT_NE(Header.find("int m_OutlineColorHash"), std::string::npos);
	EXPECT_NE(BuildBody.find("Graphics()->ScreenHiDPIScale()"), std::string::npos);
	EXPECT_NE(BuildBody.find("TextRender()->GetTextColor()"), std::string::npos);
	EXPECT_NE(BuildBody.find("TextRender()->GetTextOutlineColor()"), std::string::npos);
	EXPECT_EQ(BuildBody.find("StyleKey.m_UiScaleBucket = 100"), std::string::npos);
	EXPECT_EQ(BuildBody.find("str_quickhash(\"default-text-style\")"), std::string::npos);
}
```

- [ ] **Step 2: Run the focused failing test**

Run:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.MenuTextStyleKeyIncludesRealScaleAndColorState
```

Expected: FAIL because current key uses fixed `m_UiScaleBucket = 100` and default color hash.

- [ ] **Step 3: Extend `SMenuTextStyleKey`**

In `src/game/client/components/menus.h`, update the struct to:

```cpp
	struct SMenuTextStyleKey
	{
		float m_FontSize = 0.0f;
		int m_Align = TEXTALIGN_ML;
		int m_MaxWidthBucket = -1;
		int m_UiScaleBucket = 0;
		int m_HiDpiScaleBucket = 0;
		int m_TextColorHash = 0;
		int m_OutlineColorHash = 0;
		int m_CompactMode = 0;
	};
```

- [ ] **Step 4: Add local color hash helpers**

In the anonymous namespace of `src/game/client/components/menus.cpp`, near `MenuTextBucket`, add:

```cpp
	int MenuTextColorBucket(float Value)
	{
		return std::clamp(round_to_int(Value * 255.0f), 0, 255);
	}

	int MenuTextColorHash(const ColorRGBA &Color)
	{
		const int R = MenuTextColorBucket(Color.r);
		const int G = MenuTextColorBucket(Color.g);
		const int B = MenuTextColorBucket(Color.b);
		const int A = MenuTextColorBucket(Color.a);
		return (R << 24) ^ (G << 16) ^ (B << 8) ^ A;
	}
```

- [ ] **Step 5: Update `MenuTextCacheKey` formatting**

Replace the cache key format with:

```cpp
		str_format(aKey, sizeof(aKey), "%s:%d:%d:%d:%s:fs%d:al%d:mw%d:us%d:hd%d:tc%d:oc%d:cm%d",
			MenuTextScopeName(Scope), Page, Tab, Subtab, pTextId != nullptr ? pTextId : "",
			MenuTextBucket(StyleKey.m_FontSize), StyleKey.m_Align, StyleKey.m_MaxWidthBucket,
			StyleKey.m_UiScaleBucket, StyleKey.m_HiDpiScaleBucket, StyleKey.m_TextColorHash,
			StyleKey.m_OutlineColorHash, StyleKey.m_CompactMode);
```

- [ ] **Step 6: Update `BuildMenuTextStyleKey`**

Replace the fixed scale/color assignments with:

```cpp
		StyleKey.m_UiScaleBucket = MenuTextBucket(CUi::ms_FontmodHeight);
		StyleKey.m_HiDpiScaleBucket = Graphics() != nullptr ? round_to_int(Graphics()->ScreenHiDPIScale() * 100.0f) : 100;
		StyleKey.m_TextColorHash = MenuTextColorHash(TextRender()->GetTextColor());
		StyleKey.m_OutlineColorHash = MenuTextColorHash(TextRender()->GetTextOutlineColor());
```

If `BuildMenuTextStyleKey` remains a free function without access to `Graphics()` / `TextRender()`, convert it into a `CMenus` method:

```cpp
CMenus::SMenuTextStyleKey CMenus::BuildMenuTextStyleKey(const CUIRect *pRect, float FontSize, int Align, const SLabelProperties &LabelProps) const
```

Then update all current callers in `menus.cpp` to call the method directly. Do not update unrelated files in this task.

- [ ] **Step 7: Verify focused test passes**

Run:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.MenuTextStyleKeyIncludesRealScaleAndColorState
```

Expected: PASS.

- [ ] **Step 8: Commit checkpoint if implementing in a branch**

```bash
git add src/game/client/components/menus.h src/game/client/components/menus.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "fix(menus): 完整编码文本缓存样式键"
```

### Task 4: Split Pool Hit From Render-Ready Hit

**Files:**
- Modify: `src/game/client/components/menus.h`
- Modify: `src/game/client/components/menus.cpp`
- Test: `src/test/qmclient_monitoring_test.cpp`

- [ ] **Step 1: Write failing test for separate usage counters**

Add this test:

```cpp
TEST(QmMonitoringHelpers, SettingsTextUsageSeparatesPoolHitFromRenderReadyHit)
{
	const std::string Header = ReadTextFile("src/game/client/components/menus.h");
	const std::string Source = ReadTextFile("src/game/client/components/menus.cpp");

	EXPECT_NE(Header.find("m_MenuTextStablePoolHitsThisFrame"), std::string::npos);
	EXPECT_NE(Header.find("m_MenuTextStableRenderReadyHitsThisFrame"), std::string::npos);
	EXPECT_NE(Header.find("m_MenuTextStableBuildQueuedThisFrame"), std::string::npos);
	EXPECT_NE(Header.find("m_MenuTextStableFallbackImmediateThisFrame"), std::string::npos);
	EXPECT_NE(Source.find("pool_hit=%d render_ready_hit=%d"), std::string::npos);
	EXPECT_NE(Source.find("build_queued=%d fallback_immediate=%d"), std::string::npos);
}
```

- [ ] **Step 2: Run focused failing test**

Run:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.SettingsTextUsageSeparatesPoolHitFromRenderReadyHit
```

Expected: FAIL because current telemetry only has `hits` and `reused`.

- [ ] **Step 3: Add per-frame counters**

In `src/game/client/components/menus.h`, next to existing `m_MenuTextStable*ThisFrame` fields, add:

```cpp
	int m_MenuTextStablePoolHitsThisFrame = 0;
	int m_MenuTextStableRenderReadyHitsThisFrame = 0;
	int m_MenuTextStableBuildQueuedThisFrame = 0;
	int m_MenuTextStableFallbackImmediateThisFrame = 0;
```

- [ ] **Step 4: Reset counters in visible guard constructor**

In `CMenus::CScopedMenuTextVisibleGuard::CScopedMenuTextVisibleGuard`, add:

```cpp
	m_pMenus->m_MenuTextStablePoolHitsThisFrame = 0;
	m_pMenus->m_MenuTextStableRenderReadyHitsThisFrame = 0;
	m_pMenus->m_MenuTextStableBuildQueuedThisFrame = 0;
	m_pMenus->m_MenuTextStableFallbackImmediateThisFrame = 0;
```

- [ ] **Step 5: Count pool hits only in `MenuTextElement`**

In `CMenus::MenuTextElement`, change the existing hit increment to:

```cpp
		if(It != m_MenuTextPool.end() && It->second.m_Generation == m_MenuTextPoolGeneration)
			++m_MenuTextStablePoolHitsThisFrame;
```

Do not use this counter as render-ready evidence.

- [ ] **Step 6: Count render-ready, queued build, and fallback in `DoMenuLabelStreamed`**

Inside `CMenus::DoMenuLabelStreamed`:

```cpp
	if(&Element == &m_MenuTextFallbackElement)
	{
		if(m_MenuTextPoolVisibleGuard)
			++m_MenuTextStableFallbackImmediateThisFrame;
		if(Render)
			Ui()->DoLabel(pRect, pText, Size, Align, LabelProps);
		return;
	}
```

In the queued build branch:

```cpp
	if(NeedsBuild && m_pSettingsTextPrebuildBudget == nullptr)
	{
		QueueMenuTextContainerBuild(Element, pRect, pText, Size, Align, LabelProps, StrLen, pReadCursor);
		if(m_MenuTextPoolVisibleGuard)
			++m_MenuTextStableBuildQueuedThisFrame;
		CUIElement::SUIElementRect *pElementRect = Element.Rect(0);
		if(Render && pElementRect->m_UITextContainer.Valid() && pRect != nullptr)
		{
			if(m_MenuTextPoolVisibleGuard)
				++m_MenuTextStableRenderReadyHitsThisFrame;
			Ui()->RenderLabelTextContainerAligned(*pElementRect, pRect, Align);
		}
		return;
	}
```

In the final render-ready branch:

```cpp
	if(Render && pElementRect->m_UITextContainer.Valid() && pRect != nullptr)
	{
		if(m_MenuTextPoolVisibleGuard)
			++m_MenuTextStableRenderReadyHitsThisFrame;
		Ui()->RenderLabelTextContainerAligned(*pElementRect, pRect, Align);
	}
```

- [ ] **Step 7: Extend usage log format**

Update `LogSettingsTextPoolUsage` signature and call site to include the four new counters. The payload must contain:

```cpp
"pool_hit=%d render_ready_hit=%d build_queued=%d fallback_immediate=%d"
```

Keep existing `hits` temporarily as an alias for `render_ready_hit` to avoid breaking old reports in the same commit:

```cpp
const int Hits = RenderReadyHits;
```

- [ ] **Step 8: Verify focused test passes**

Run:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.SettingsTextUsageSeparatesPoolHitFromRenderReadyHit
```

Expected: PASS.

- [ ] **Step 9: Commit checkpoint if implementing in a branch**

```bash
git add src/game/client/components/menus.h src/game/client/components/menus.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "feat(perf): 拆分文本池命中与真实可渲染命中"
```

### Task 5: Update Perf Parser And Report To Use Real Hit Rate

**Files:**
- Modify: `qmclient_scripts/perf/lib/stats.ts`
- Modify: `qmclient_scripts/perf/lib/report.ts`
- Modify: `qmclient_scripts/perf/lib/quality.ts`
- Modify: `qmclient_scripts/perf/test.ts`

- [ ] **Step 1: Write failing TypeScript tests**

Add these tests to `qmclient_scripts/perf/test.ts`:

```ts
test('stable text hit rate uses render_ready_hit instead of pool_hit', () => {
  const entries = parseLog([
    '[2026-06-18 10:00:00][perf/settings-text]: event=settings_text_usage scope=target_settings page=settings tab=0 subtab=-1 operation=settings_open frame=10 text_class=static_stable candidates=10 hits=10 reused=10 miss=0 stale=0 text_new=0 text_reused=10 planned=10 unplanned=0 pool_hit=10 render_ready_hit=6 build_queued=4 fallback_immediate=0',
    '[2026-06-18 10:00:00][perf/fps]: event=fps_summary operation=settings_open context=online page=settings tab=none sample_frames=30 sample_seconds=0.125 fps_avg=240 fps_min=240 fps_1pct_low=240 fps_1pct_source=real_sampled fps_max=240 frame_ms_avg=4.1 frame_ms_p95=4.2 frame_ms_p99=4.3 frame_ms_max=4.4 menu_ms_max=1.0 window_start_frame=1 window_end_frame=30 cap_limited=0',
  ].join('\n')).entries;

  const snapshot = targetSettingsSnapshot(entries);
  assert.equal(snapshot.stableTextCoverage.visibleCandidateCount, 10);
  assert.equal(snapshot.stableTextCoverage.renderReadyHitCount, 6);
  assert.equal(snapshot.stableTextCoverage.staticHitRate, 60);
  assert.equal(snapshot.stableTextCoverage.acceptanceBlocked, true);
});

test('stable text fallback immediate blocks acceptance even when pool hit is perfect', () => {
  const entries = parseLog([
    '[2026-06-18 10:00:00][perf/settings-text]: event=settings_text_usage scope=target_settings page=settings tab=0 subtab=-1 operation=settings_open frame=10 text_class=static_stable candidates=3 hits=3 reused=3 miss=0 stale=0 text_new=0 text_reused=3 planned=3 unplanned=0 pool_hit=3 render_ready_hit=3 build_queued=0 fallback_immediate=1',
    '[2026-06-18 10:00:00][perf/fps]: event=fps_summary operation=settings_open context=online page=settings tab=none sample_frames=30 sample_seconds=0.125 fps_avg=240 fps_min=240 fps_1pct_low=240 fps_1pct_source=real_sampled fps_max=240 frame_ms_avg=4.1 frame_ms_p95=4.2 frame_ms_p99=4.3 frame_ms_max=4.4 menu_ms_max=1.0 window_start_frame=1 window_end_frame=30 cap_limited=0',
  ].join('\n')).entries;

  const snapshot = targetSettingsSnapshot(entries);
  assert.equal(snapshot.stableTextCoverage.fallbackImmediate, 1);
  assert.equal(snapshot.stableTextCoverage.acceptanceBlocked, true);
});
```

If `parseLog`, `targetSettingsSnapshot`, or `test` are named differently in the current file, use the existing imports and local test style from `qmclient_scripts/perf/test.ts`.

- [ ] **Step 2: Run failing TS tests**

Run:

```pwsh
cd qmclient_scripts/perf
bun test.ts
```

Expected: FAIL because `renderReadyHitCount` and `fallbackImmediate` are not parsed or not used.

- [ ] **Step 3: Extend stable text event parsing**

In `qmclient_scripts/perf/lib/stats.ts`, extend the stable text event type with:

```ts
  poolHit: number;
  renderReadyHit: number;
  buildQueued: number;
  fallbackImmediate: number;
```

When parsing `settings_text_usage`, use:

```ts
const poolHit = numberField(e, 'pool_hit', hits);
const renderReadyHit = numberField(e, 'render_ready_hit', hits);
const buildQueued = numberField(e, 'build_queued');
const fallbackImmediate = numberField(e, 'fallback_immediate');
```

Include these fields in the returned event and sample string.

- [ ] **Step 4: Compute real hit rate**

In `targetSettingsSnapshot`, replace static hit count/rate calculation with:

```ts
const renderReadyHitCount = staticRelevantUsage.reduce((sum, event) => sum + event.renderReadyHit, 0);
const poolHitCount = staticRelevantUsage.reduce((sum, event) => sum + event.poolHit, 0);
const buildQueued = staticRelevantUsage.reduce((sum, event) => sum + event.buildQueued, 0);
const fallbackImmediate = staticRelevantUsage.reduce((sum, event) => sum + event.fallbackImmediate, 0);
const staticHitCount = renderReadyHitCount;
const staticHitRate = candidateTotal > 0 ? (renderReadyHitCount / candidateTotal) * 100 : 0;
```

Extend the returned `stableTextCoverage` object with:

```ts
poolHitCount,
renderReadyHitCount,
buildQueued,
fallbackImmediate,
```

Update acceptance blocking:

```ts
acceptanceBlocked: missCount > 0 || staleCount > 0 || prebuildRemainingBeforeTarget > 0 || !planCollectionAvailable || !planCollectionComplete || !utilizationAvailable || !planCoverageAvailable || unplannedVisibleCount > 0 || keyMismatchCount > 0 || textNew > 0 || buildQueued > 0 || fallbackImmediate > 0,
```

- [ ] **Step 5: Update report narrative**

In `qmclient_scripts/perf/lib/report.ts`, update stable text narrative to include:

```ts
`pool hit=${targetSettings.stableTextCoverage.poolHitCount}，render-ready hit=${targetSettings.stableTextCoverage.renderReadyHitCount}，build queued=${targetSettings.stableTextCoverage.buildQueued}，fallback immediate=${targetSettings.stableTextCoverage.fallbackImmediate}`
```

The report label for hit rate must say `Render-Ready Hit Rate`, not just `Hit Rate`, in the target settings coverage panel.

- [ ] **Step 6: Update quality warnings**

In `qmclient_scripts/perf/lib/quality.ts`, add warnings:

```ts
if (targetSettings.stableTextCoverage.buildQueued > 0) {
  warnings.push('stable text queued visible builds during target window');
}
if (targetSettings.stableTextCoverage.fallbackImmediate > 0) {
  warnings.push('stable text used immediate fallback during target window');
}
```

- [ ] **Step 7: Run perf tests**

Run:

```pwsh
cd qmclient_scripts/perf
bun test.ts
npx tsc --noEmit
```

Expected: PASS.

- [ ] **Step 8: Commit checkpoint if implementing in a branch**

```bash
git add qmclient_scripts/perf/lib/stats.ts qmclient_scripts/perf/lib/report.ts qmclient_scripts/perf/lib/quality.ts qmclient_scripts/perf/test.ts
git commit -m "fix(perf): 使用真实可渲染命中率验收文本缓存"
```

### Task 6: Expose Text Runtime Cost Snapshot To Adaptive Budget

**Files:**
- Modify: `src/engine/textrender.h`
- Modify: `src/engine/client/text.cpp`
- Modify: `src/test/qmclient_monitoring_test.cpp`

- [ ] **Step 1: Write failing test for text runtime snapshot API**

Add this test:

```cpp
TEST(QmMonitoringHelpers, TextRenderExposesRuntimeBudgetSnapshotForScheduler)
{
	const std::string Header = ReadTextFile("src/engine/textrender.h");
	const std::string Text = ReadTextFile("src/engine/client/text.cpp");

	EXPECT_NE(Header.find("struct SQmTextRuntimeBudgetSnapshot"), std::string::npos);
	EXPECT_NE(Header.find("virtual SQmTextRuntimeBudgetSnapshot QmTextRuntimeBudgetSnapshot() const"), std::string::npos);
	EXPECT_NE(Text.find("SQmTextRuntimeBudgetSnapshot m_QmLastTextRuntimeBudgetSnapshot"), std::string::npos);
	EXPECT_NE(Text.find("m_QmLastTextRuntimeBudgetSnapshot.m_TextContainerCreateMs"), std::string::npos);
}
```

- [ ] **Step 2: Run focused failing test**

Run:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.TextRenderExposesRuntimeBudgetSnapshotForScheduler
```

Expected: FAIL because the snapshot API does not exist.

- [ ] **Step 3: Add snapshot type to `textrender.h`**

Add near other text render structs:

```cpp
struct SQmTextRuntimeBudgetSnapshot
{
	int m_GlyphNew = 0;
	int m_GlyphUploads = 0;
	double m_GlyphRasterizeMs = 0.0;
	double m_GlyphUploadMs = 0.0;
	int m_TextContainerNew = 0;
	int m_TextContainerUploads = 0;
	double m_TextContainerCreateMs = 0.0;
	double m_TextContainerUploadMs = 0.0;
	uint64_t m_Frame = 0;
};
```

Add to `ITextRender`:

```cpp
	virtual SQmTextRuntimeBudgetSnapshot QmTextRuntimeBudgetSnapshot() const { return {}; }
```

- [ ] **Step 4: Store last snapshot in `CTextRender`**

In `src/engine/client/text.cpp`, add member:

```cpp
	SQmTextRuntimeBudgetSnapshot m_QmLastTextRuntimeBudgetSnapshot;
```

Before `ResetQmTextRuntimeBudgetCounters(false)` in `FlushQmTextRuntimeBudgetLog`, assign:

```cpp
		m_QmLastTextRuntimeBudgetSnapshot.m_GlyphNew = GlyphNew;
		m_QmLastTextRuntimeBudgetSnapshot.m_GlyphUploads = GlyphUploads;
		m_QmLastTextRuntimeBudgetSnapshot.m_GlyphRasterizeMs = GlyphRasterizeMs;
		m_QmLastTextRuntimeBudgetSnapshot.m_GlyphUploadMs = GlyphUploadMs;
		m_QmLastTextRuntimeBudgetSnapshot.m_TextContainerNew = m_QmPerfTextContainerNew;
		m_QmLastTextRuntimeBudgetSnapshot.m_TextContainerUploads = m_QmPerfTextContainerUploads;
		m_QmLastTextRuntimeBudgetSnapshot.m_TextContainerCreateMs = m_QmPerfTextContainerCreateMs;
		m_QmLastTextRuntimeBudgetSnapshot.m_TextContainerUploadMs = m_QmPerfTextContainerUploadMs;
```

Implement:

```cpp
	SQmTextRuntimeBudgetSnapshot QmTextRuntimeBudgetSnapshot() const override
	{
		return m_QmLastTextRuntimeBudgetSnapshot;
	}
```

If `CTextRender` has access to `Client()->PerfFrame()` unavailable, keep `m_Frame = 0` in this task. The scheduler can still use recent cost values.

- [ ] **Step 5: Verify focused test passes**

Run:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.TextRenderExposesRuntimeBudgetSnapshotForScheduler
```

Expected: PASS.

- [ ] **Step 6: Commit checkpoint if implementing in a branch**

```bash
git add src/engine/textrender.h src/engine/client/text.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "feat(text): 暴露文本运行时成本快照"
```

### Task 7: Add Adaptive Text Budget Inputs And Hard Caps

**Files:**
- Modify: `src/game/client/components/settings_resource_jobs.h`
- Modify: `src/game/client/components/settings_resource_jobs.cpp`
- Test: `src/test/settings_warmup_test.cpp`

- [ ] **Step 1: Write failing tests for hard caps and stable-frame growth**

Add these tests to `src/test/settings_warmup_test.cpp` near existing adaptive budget tests:

```cpp
TEST(SettingsWarmup, AdaptiveTextBudgetKeepsLowHardCapWhileScrolling)
{
	SSettingsAdaptiveBudgetState State;
	SSettingsAdaptiveBudgetInput Input;
	Input.m_WindowActive = true;
	Input.m_ScrollActive = true;
	Input.m_TargetFrameMs = 8.333f;
	Input.m_FrameMsAverage = 4.0f;
	Input.m_FrameMsP95 = 5.0f;
	Input.m_BackgroundBacklog = 100;
	Input.m_TextScrollHardCap = 2;
	Input.m_TextIdleHardCap = 64;

	const SSettingsAdaptiveBudgetOutput Output = SettingsAdaptiveBudgetStep(Input, State);
	EXPECT_EQ(Output.m_Mode, ESettingsAdaptiveBudgetMode::SCROLL_ACTIVE);
	EXPECT_LE(Output.m_TextContainerTokens, 2);
	EXPECT_LE(Output.m_GlyphRasterizeTokens, 1);
	EXPECT_LE(Output.m_GlyphUploadTokens, 1);
}

TEST(SettingsWarmup, AdaptiveTextBudgetCanGrowBeyondSixteenOnStableHighHeadroomFrames)
{
	SSettingsAdaptiveBudgetState State;
	SSettingsAdaptiveBudgetInput Input;
	Input.m_WindowActive = true;
	Input.m_TargetFrameMs = 8.333f;
	Input.m_FrameMsAverage = 3.0f;
	Input.m_FrameMsP95 = 4.0f;
	Input.m_BackgroundBacklog = 100;
	Input.m_TextIdleHardCap = 64;
	Input.m_TextScrollHardCap = 2;

	SSettingsAdaptiveBudgetOutput Output;
	for(int i = 0; i < 40; ++i)
		Output = SettingsAdaptiveBudgetStep(Input, State);

	EXPECT_GT(Output.m_TextContainerTokens, 16);
	EXPECT_LE(Output.m_TextContainerTokens, 64);
}

TEST(SettingsWarmup, AdaptiveTextBudgetShrinksWhenRecentTextWorkIsExpensive)
{
	SSettingsAdaptiveBudgetState State;
	SSettingsAdaptiveBudgetInput Input;
	Input.m_WindowActive = true;
	Input.m_TargetFrameMs = 8.333f;
	Input.m_FrameMsAverage = 4.0f;
	Input.m_FrameMsP95 = 5.0f;
	Input.m_BackgroundBacklog = 100;
	Input.m_TextIdleHardCap = 64;
	Input.m_TextContainerCreateMsEwma = 3.0f;
	Input.m_GlyphUploadMsEwma = 2.0f;

	const SSettingsAdaptiveBudgetOutput Output = SettingsAdaptiveBudgetStep(Input, State);
	EXPECT_EQ(Output.m_Reason, ESettingsAdaptiveBudgetReason::FRAME_PRESSURE);
	EXPECT_LE(Output.m_TextContainerTokens, 2);
}
```

- [ ] **Step 2: Run focused failing tests**

Run:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=SettingsWarmup.AdaptiveTextBudget*
```

Expected: FAIL because input fields and logic do not exist or text budget is capped at 16.

- [ ] **Step 3: Extend adaptive input**

In `src/game/client/components/settings_resource_jobs.h`, add to `SSettingsAdaptiveBudgetInput`:

```cpp
	float m_TextContainerCreateMsEwma = 0.0f;
	float m_TextContainerUploadMsEwma = 0.0f;
	float m_GlyphRasterizeMsEwma = 0.0f;
	float m_GlyphUploadMsEwma = 0.0f;
	int m_TextScrollHardCap = 2;
	int m_TextPressureHardCap = 2;
	int m_TextIdleHardCap = 64;
	int m_GlyphScrollHardCap = 1;
	int m_GlyphPressureHardCap = 1;
	int m_GlyphIdleHardCap = 8;
```

- [ ] **Step 4: Add text cost pressure**

In `SettingsAdaptiveBudgetStep`, extend `FramePressure`:

```cpp
	const bool TextCostPressure =
		Input.m_TextContainerCreateMsEwma > TargetMs * 0.25f ||
		Input.m_TextContainerUploadMsEwma > TargetMs * 0.20f ||
		Input.m_GlyphRasterizeMsEwma > TargetMs * 0.20f ||
		Input.m_GlyphUploadMsEwma > TargetMs * 0.20f;
```

Then include `TextCostPressure` in `FramePressure`.

- [ ] **Step 5: Replace fixed text max of 16 with hard-cap clamp**

In stable-frame growth, replace:

```cpp
State.m_TextPrebuildWindow = std::min(16, State.m_TextPrebuildWindow + 1);
```

with:

```cpp
State.m_TextPrebuildWindow = std::min(maximum(1, Input.m_TextIdleHardCap), State.m_TextPrebuildWindow + 1);
```

After mode selection and before output assignment, compute:

```cpp
	const int TextHardCap =
		Output.m_Mode == ESettingsAdaptiveBudgetMode::SCROLL_ACTIVE ? Input.m_TextScrollHardCap :
		Output.m_Mode == ESettingsAdaptiveBudgetMode::FRAME_PRESSURE ? Input.m_TextPressureHardCap :
		Input.m_TextIdleHardCap;
	const int GlyphHardCap =
		Output.m_Mode == ESettingsAdaptiveBudgetMode::SCROLL_ACTIVE ? Input.m_GlyphScrollHardCap :
		Output.m_Mode == ESettingsAdaptiveBudgetMode::FRAME_PRESSURE ? Input.m_GlyphPressureHardCap :
		Input.m_GlyphIdleHardCap;
```

Update text outputs:

```cpp
	Output.m_TextPrebuildTokens = std::clamp(State.m_TextPrebuildWindow, Output.m_Mode == ESettingsAdaptiveBudgetMode::WINDOW_INACTIVE ? 0 : 1, maximum(0, TextHardCap));
	Output.m_TextContainerTokens = Output.m_TextPrebuildTokens;
	Output.m_GlyphRasterizeTokens = std::clamp(minimum(Output.m_TextPrebuildTokens, 2), Output.m_Mode == ESettingsAdaptiveBudgetMode::WINDOW_INACTIVE ? 0 : 1, maximum(0, GlyphHardCap));
	Output.m_GlyphUploadTokens = std::clamp(minimum(Output.m_TextPrebuildTokens, 2), Output.m_Mode == ESettingsAdaptiveBudgetMode::WINDOW_INACTIVE ? 0 : 1, maximum(0, GlyphHardCap));
```

- [ ] **Step 6: Verify focused tests pass**

Run:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=SettingsWarmup.AdaptiveTextBudget*
```

Expected: PASS.

- [ ] **Step 7: Commit checkpoint if implementing in a branch**

```bash
git add src/game/client/components/settings_resource_jobs.h src/game/client/components/settings_resource_jobs.cpp src/test/settings_warmup_test.cpp
git commit -m "feat(settings): 让文本预算按帧余量自适应"
```

### Task 8: Feed Real Text Runtime Costs Into Menu Scheduler

**Files:**
- Modify: `src/game/client/components/menus.cpp`
- Modify: `src/game/client/components/menus.h`
- Test: `src/test/qmclient_monitoring_test.cpp`

- [ ] **Step 1: Write failing test for scheduler input wiring**

Add this test:

```cpp
TEST(QmMonitoringHelpers, SettingsSchedulerFeedsTextRuntimeCostIntoAdaptiveBudget)
{
	const std::string Header = ReadTextFile("src/game/client/components/menus.h");
	const std::string Source = ReadTextFile("src/game/client/components/menus.cpp");
	const std::string PrepareBody = ExtractSourceFunctionBody(Source, "void CMenus::PrepareSettingsAdaptiveBudgetInput(SSettingsAdaptiveBudgetInput &Input)");
	const std::string LogBody = ExtractSourceFunctionBody(Source, "void CMenus::LogSettingsAdaptiveBudget(const char *pSource, const SSettingsAdaptiveBudgetInput &Input, const SSettingsAdaptiveBudgetOutput &Output) const");

	EXPECT_NE(Header.find("m_TextContainerCreateMsEwma"), std::string::npos);
	EXPECT_NE(PrepareBody.find("TextRender()->QmTextRuntimeBudgetSnapshot()"), std::string::npos);
	EXPECT_NE(PrepareBody.find("Input.m_TextContainerCreateMsEwma"), std::string::npos);
	EXPECT_NE(LogBody.find("text_create_ewma_ms=%.3f"), std::string::npos);
	EXPECT_NE(LogBody.find("text_scroll_cap=%d"), std::string::npos);
}
```

- [ ] **Step 2: Run focused failing test**

Run:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.SettingsSchedulerFeedsTextRuntimeCostIntoAdaptiveBudget
```

Expected: FAIL because the scheduler does not consume text runtime snapshots.

- [ ] **Step 3: Add EWMA state fields to `CMenus`**

In `src/game/client/components/menus.h`, add private members:

```cpp
	float m_TextContainerCreateMsEwma = 0.0f;
	float m_TextContainerUploadMsEwma = 0.0f;
	float m_GlyphRasterizeMsEwma = 0.0f;
	float m_GlyphUploadMsEwma = 0.0f;
```

- [ ] **Step 4: Add EWMA helper in `menus.cpp`**

In anonymous namespace:

```cpp
	float QmPerfEwma(float Previous, float Sample, float Alpha)
	{
		if(Sample <= 0.0f)
			return Previous * (1.0f - Alpha);
		if(Previous <= 0.0f)
			return Sample;
		return Previous * (1.0f - Alpha) + Sample * Alpha;
	}
```

- [ ] **Step 5: Feed snapshot into `PrepareSettingsAdaptiveBudgetInput`**

At the end of `CMenus::PrepareSettingsAdaptiveBudgetInput`:

```cpp
	const SQmTextRuntimeBudgetSnapshot TextSnapshot = TextRender()->QmTextRuntimeBudgetSnapshot();
	m_TextContainerCreateMsEwma = QmPerfEwma(m_TextContainerCreateMsEwma, (float)TextSnapshot.m_TextContainerCreateMs, 0.20f);
	m_TextContainerUploadMsEwma = QmPerfEwma(m_TextContainerUploadMsEwma, (float)TextSnapshot.m_TextContainerUploadMs, 0.20f);
	m_GlyphRasterizeMsEwma = QmPerfEwma(m_GlyphRasterizeMsEwma, (float)TextSnapshot.m_GlyphRasterizeMs, 0.20f);
	m_GlyphUploadMsEwma = QmPerfEwma(m_GlyphUploadMsEwma, (float)TextSnapshot.m_GlyphUploadMs, 0.20f);
	Input.m_TextContainerCreateMsEwma = m_TextContainerCreateMsEwma;
	Input.m_TextContainerUploadMsEwma = m_TextContainerUploadMsEwma;
	Input.m_GlyphRasterizeMsEwma = m_GlyphRasterizeMsEwma;
	Input.m_GlyphUploadMsEwma = m_GlyphUploadMsEwma;
	Input.m_TextScrollHardCap = 2;
	Input.m_TextPressureHardCap = 2;
	Input.m_TextIdleHardCap = 64;
	Input.m_GlyphScrollHardCap = 1;
	Input.m_GlyphPressureHardCap = 1;
	Input.m_GlyphIdleHardCap = 8;
```

- [ ] **Step 6: Extend adaptive budget telemetry**

In `CMenus::LogSettingsAdaptiveBudget`, add payload fields:

```cpp
" text_create_ewma_ms=%.3f text_upload_ewma_ms=%.3f glyph_raster_ewma_ms=%.3f glyph_upload_ewma_ms=%.3f text_scroll_cap=%d text_idle_cap=%d"
```

Pass corresponding `Input` fields.

- [ ] **Step 7: Verify focused test passes**

Run:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.SettingsSchedulerFeedsTextRuntimeCostIntoAdaptiveBudget
```

Expected: PASS.

- [ ] **Step 8: Commit checkpoint if implementing in a branch**

```bash
git add src/game/client/components/menus.h src/game/client/components/menus.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "feat(perf): 将文本成本接入自适应预算调度"
```

### Task 9: Add Runtime Acceptance Log Contract

**Files:**
- Modify: `qmclient_scripts/perf/lib/stats.ts`
- Modify: `qmclient_scripts/perf/lib/report.ts`
- Modify: `qmclient_scripts/perf/test.ts`
- Test: generated report from latest perf log

- [ ] **Step 1: Add report test for explicit trust wording**

Add this test to `qmclient_scripts/perf/test.ts`:

```ts
test('report distinguishes pool hit rate from render-ready hit rate', () => {
  const entries = parseLog([
    '[2026-06-18 10:00:00][perf/settings-text]: event=settings_text_usage scope=target_settings page=settings tab=0 subtab=-1 operation=settings_open frame=10 text_class=static_stable candidates=10 hits=6 reused=6 miss=0 stale=0 text_new=0 text_reused=6 planned=10 unplanned=0 pool_hit=10 render_ready_hit=6 build_queued=4 fallback_immediate=0',
    '[2026-06-18 10:00:00][perf/fps]: event=fps_summary operation=settings_open context=online page=settings tab=none sample_frames=30 sample_seconds=0.125 fps_avg=240 fps_min=240 fps_1pct_low=240 fps_1pct_source=real_sampled fps_max=240 frame_ms_avg=4.1 frame_ms_p95=4.2 frame_ms_p99=4.3 frame_ms_max=4.4 menu_ms_max=1.0 window_start_frame=1 window_end_frame=30 cap_limited=0',
  ].join('\n')).entries;

  const html = renderReport(entries, { title: 'test' });
  assert.match(html, /Render-Ready Hit Rate/);
  assert.match(html, /pool hit/);
  assert.match(html, /build queued/);
  assert.doesNotMatch(html, /static stable text coverage 已覆盖/);
});
```

Use the existing report test helper names in `test.ts`; if `renderReport` has a different signature, adapt to the current helper but keep the assertions.

- [ ] **Step 2: Run failing test**

Run:

```pwsh
cd qmclient_scripts/perf
bun test.ts
```

Expected: FAIL until report wording is updated.

- [ ] **Step 3: Update report labels and narrative**

In `qmclient_scripts/perf/lib/report.ts`:

- Rename the target stable text hit-rate card to `Render-Ready Hit Rate`.
- Add a separate small card for `Pool Hit`.
- Add a separate small card for `Queued Builds`.
- Add a separate small card for `Immediate Fallback`.
- Keep text explaining: `pool hit 只说明 key 命中，render-ready hit 才说明本帧无需构建且可直接绘制。`

- [ ] **Step 4: Run perf tests and typecheck**

Run:

```pwsh
cd qmclient_scripts/perf
bun test.ts
npx tsc --noEmit
```

Expected: PASS.

- [ ] **Step 5: Generate report from latest log**

Run:

```pwsh
cd qmclient_scripts/perf
bun analyze.ts --latest
```

Expected: report is generated and target stable text section shows `Render-Ready Hit Rate`, pool hit, queued builds, and immediate fallback separately. If no log exists, run the client with perf debug enabled first and repeat; do not mark runtime acceptance complete without a real log.

- [ ] **Step 6: Commit checkpoint if implementing in a branch**

```bash
git add qmclient_scripts/perf/lib/stats.ts qmclient_scripts/perf/lib/report.ts qmclient_scripts/perf/test.ts
git commit -m "docs(perf): 明确文本命中率可信口径"
```

### Task 10: Add Visual Regression Checklist For Cached Labels

**Files:**
- Modify: `src/test/qmclient_monitoring_test.cpp`
- Create: `docs/superpowers/plans/2026-06-18-text-rendering-stabilization-observability-visual-checklist.md`

- [ ] **Step 1: Create visual checklist document**

Create the checklist file with this content:

```markdown
# Text Rendering Stabilization Visual Checklist

Use after implementing `2026-06-18-text-rendering-stabilization-observability.md`.

## Required Screens

- Settings: General tab, TClient tab, QmClient tab.
- Ingame Esc menu.
- Assets/resource page including small cards, right-side tags/buttons, map/video fallback cards.
- Server browser list.
- Chat translate button and popup labels.

## Required States

- UI scale 100%.
- One non-default UI scale.
- Normal DPI and HiDPI if available.
- English and Simplified Chinese language.
- First open after cache invalidation.
- Scroll active.
- Post-scroll settled.
- Hover/pressed/selected button state.

## Pass Criteria

- Button text is centered inside button rect.
- Placeholder text such as "TODO" is centered inside its card.
- Small-card right tags/buttons keep fixed priority; left ID/title text shrinks first.
- No visible blank text on first frame unless explicitly documented as deferred dynamic content.
- No visible stale text after language/font/UI-scale change.
- Perf report says target stable text coverage is OK only when render-ready hit coverage is complete.
```

- [ ] **Step 2: Add source-contract pointer test**

Add this test:

```cpp
TEST(QmMonitoringHelpers, TextRenderingStabilizationHasVisualChecklist)
{
	const std::string Checklist = ReadTextFile("docs/superpowers/plans/2026-06-18-text-rendering-stabilization-observability-visual-checklist.md");
	EXPECT_NE(Checklist.find("Button text is centered"), std::string::npos);
	EXPECT_NE(Checklist.find("render-ready hit coverage"), std::string::npos);
	EXPECT_NE(Checklist.find("Small-card right tags/buttons keep fixed priority"), std::string::npos);
}
```

- [ ] **Step 3: Run focused test**

Run:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.TextRenderingStabilizationHasVisualChecklist
```

Expected: PASS after creating the checklist.

- [ ] **Step 4: Commit checkpoint if implementing in a branch**

```bash
git add docs/superpowers/plans/2026-06-18-text-rendering-stabilization-observability-visual-checklist.md src/test/qmclient_monitoring_test.cpp
git commit -m "test(ui): 增加文本渲染视觉验收清单"
```

### Task 11: Run Full Verification Gate

**Files:**
- Verify only; no source edits unless a failure is directly caused by this plan's implementation.

- [ ] **Step 1: Run documentation check**

Run:

```pwsh
python qmclient_scripts/gate/check_docs.py
```

Expected: PASS.

- [ ] **Step 2: Run C++ tests**

Run:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
```

Expected: PASS.

- [ ] **Step 3: Run perf script tests**

Run:

```pwsh
cd qmclient_scripts/perf
bun test.ts
npx tsc --noEmit
```

Expected: PASS.

- [ ] **Step 4: Run quick gate**

Run:

```pwsh
python qmclient_scripts/gate/check_gate.py --mode quick
```

Expected: PASS. If unrelated existing failures appear, record exact files and command output in final handoff; do not silently call the implementation complete.

- [ ] **Step 5: Produce runtime perf report**

Run a client session that covers:

- settings open target window,
- ingame Esc open target window,
- assets/resource page scroll,
- one post-scroll settled interval.

Then run:

```pwsh
cd qmclient_scripts/perf
bun analyze.ts --latest
```

Expected:

- report includes `Render-Ready Hit Rate`;
- target stable text acceptance is blocked if `build_queued > 0`, `fallback_immediate > 0`, `text_new > 0`, miss, stale, unplanned, or key mismatch exists;
- `perf/text` shows text container and glyph costs;
- adaptive budget telemetry includes text EWMA and hard caps.

- [ ] **Step 6: Record final evidence**

In final handoff, include this exact evidence format:

```text
Command: python qmclient_scripts/gate/check_docs.py
Result: pass/fail with key output
Scope: docs plan/check links
Gaps: none or exact skipped checks

Command: qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
Result: pass/fail with key output
Scope: C++ source-contract and unit tests
Gaps: none or exact skipped checks

Command: cd qmclient_scripts/perf && bun test.ts && npx tsc --noEmit
Result: pass/fail with key output
Scope: perf parser/report/quality tests
Gaps: none or exact skipped checks

Command: cd qmclient_scripts/perf && bun analyze.ts --latest
Result: pass/fail with report path
Scope: runtime telemetry acceptance
Gaps: if no real log was available, state that runtime acceptance is incomplete
```

## Rollout Order

Implement in this order:

1. Task 1 and Task 2 first. These eliminate the highest-risk layout regression source.
2. Task 3 next. This prevents stale containers after style/scale/color changes.
3. Task 4 and Task 5 together. These make hit rate trustworthy before using it for acceptance.
4. Task 6 through Task 8. These add the adaptive budget scheduling and remove hardcoded global ceiling assumptions.
5. Task 9 and Task 10. These make runtime reports and visual checks explicit.
6. Task 11 last. Do not claim completion before runtime report and gate evidence exist.

## Risk Notes

- Converting `BuildMenuTextStyleKey` from free function to `CMenus` method may touch many call sites in `menus.cpp`. Keep it in the same file unless a compile error requires declarations elsewhere.
- `CUIElement::SUIElementRect` stores cached cursor metrics. Any new style field that changes text geometry must either enter `DoLabelStreamed` invalidation or be represented in the menu style key.
- `settings_text_usage` must remain backward-compatible while reports transition. Keep old `hits` field as alias for one release window, but report must prefer `render_ready_hit`.
- Adaptive idle hard cap can exceed 16, but scroll and frame-pressure hard caps must stay low. This is the answer to “高端机器怎么知道上限”：do not guess a global upper bound; grow only when measured frames have slack and shrink immediately on pressure.
- Runtime logs sampled by threshold can under-report quiet success. Acceptance must require target-window coverage events, not infer success from absence of miss logs.

## Self-Review Checklist

- [ ] Plan covers text rendering semantics, hit-rate truthfulness, and adaptive frame budget scheduling.
- [ ] Every task has concrete files, test, implementation steps, commands, and expected results.
- [ ] No task asks workers to rely on old docs as truth; implementation starts from current code and codegraph.
- [ ] No task changes protocol, physics, snapshot, demo, map, or file formats.
- [ ] Runtime acceptance cannot pass from pool hit alone.
- [ ] Visual regressions raised by resource page / buttons / small cards are explicitly represented in checklist and test contract.
