# QmClient 设置页 UI 统一 P1 Theme 与 SettingsCard 基础 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立随 `qm_ui_color`/`qm_ui_opacity` 实时变化的 QmUi theme/token 链，以及唯一的设置页布局、卡片 frame 和 card motion contract，并让 Graphics 成为首个不再绘制旧 glass shell 的生产切片。

**Architecture:** 颜色由无全局依赖的 `ResolveUiTheme(ColorHSLA, float)` 纯函数解析，通过 `IUiContext::m_pTheme` 注入 primitive；几何与排版常量仍由 `UiTokens.h` 管理。`SettingsCardGeometry.cpp` 只负责 canonical frame 与 motion policy，可独立链入 `testrunner`；client-only `SettingsCard.cpp` 只从一次内容测量调用纯 owner，再绘制 shell、统一 handle 和文本。现有 `CUiV2AnimationRuntime` 负责 motion，不创建第二套动画 runtime。

**Tech Stack:** C++、QmUi、DDNet `CUi`、`CUiV2AnimationRuntime`、GoogleTest、CMake/MSVC。

## Global Constraints

- P0 merge、基线报告和 merge 后规格校准必须完成；本计划只基于 P0 merge commit 开工。
- 权威规格：`docs/superpowers/specs/2026-07-10-QmClient-设置页UI统一与滚动体系规格.md`。
- `SUiTheme` 是运行时颜色事实源；页面不得再解释 `g_Config.m_QmUiColor` 或手写同类 card/input/focus 颜色。
- `UiTokens.h` 保留无状态的 spacing、radius、typography 和可测量几何 token；运行时颜色不得继续放在静态 `ui_token::color::*` 常量中。
- `SSettingsCardFrame::m_Rect` 同时是 display、hit-test、drag item 和 proxy source rect；entry transform 仅影响绘制，不改变这些 rect。
- P1 只提供 page/card shell，不实现完整 Root Panel/L0/L1/L2；完整层级属于 R2。
- 不新建渲染或动画 runtime；复用 `CUiV2AnimationRuntime`、`ResolveTargetValue(...)` 和 P0 合入并验证的 presentation-state 能力。
- P1 复用现有 `qm_extra_animations`（不改变其默认值或既有语义），保留既有 `qm_ui_motion_level` 的 `0..2` 语义；`qm_ui_card_rainbow_titles` 只在 Task 4 有真实 card title shell 消费者时新增，页面不得自行解释这些配置。
- 本阶段不迁移 QmClient/TClient 私有 deck，不实现 Search/持久化；它们分别属于 P2/P6。
- 同一 `cmake-build-release` 目标串行；P1 不更新功能版本，版本只在 P7 最终收口时更新一次。

---

## File Structure

- Create: `src/game/client/QmUi/UiTheme.h` — 运行时 theme 数据与纯解析函数，header-only，避免为一个纯值对象修改根 `CMakeLists.txt`。
- Create: `src/game/client/QmUi/SettingsPageLayout.h` — 设置页 inset、全宽子 tab、单双列与 column frame 的纯布局解析。
- Create: `src/game/client/QmUi/SettingsCardGeometry.h/.cpp` — 不依赖 UI renderer 的 `SSettingsCardFrame`、canonical geometry 与 `SCardMotionSpec` 纯 owner。
- Create: `src/game/client/QmUi/SettingsCard.h` — client shell 的 visual options/state 与 measure/render callback contract。
- Create: `src/game/client/QmUi/SettingsCard.cpp` — client-only surface/border/title/subtitle/handle/content 绘制，不重复 frame/motion 逻辑。
- Modify: `src/game/client/QmUi/UiContext.h` — 注入 `const SUiTheme *m_pTheme`。
- Modify: `src/game/client/QmUi/UiTokens.h` — 只保留/补齐可缩放的 geometry/typography token，清退静态 theme 色。
- Modify: `src/game/client/QmUi/UiForms.cpp` — card/input/focus 从 `Ctx.m_pTheme` 取色。
- Modify: `src/game/client/components/menus.h`、`src/game/client/components/menus.cpp` — 每帧构造并持有 `SUiTheme`，统一生成 `IUiContext` 与 `SCardMotionSpec`。
- Modify: `src/game/client/components/menus_settings.cpp` — Graphics 试点使用新 page/card shell。
- Modify: `CMakeLists.txt` — client source list 登记四个 card 文件；`TESTS_EXTRA` 只登记 `SettingsCardGeometry.cpp`。
- Modify: `src/test/QmAnimTest.cpp` — 只 include/link 纯 owner，覆盖 theme/layout/frame/motion 行为。
- Modify: `src/test/qmclient_monitoring_test.cpp` — Graphics 生产路径和旧 shell 删除断言。

---

### Task 1: 建立运行时 SUiTheme 与配置入口

**Files:**
- Create: `src/game/client/QmUi/UiTheme.h`
- Modify: `src/game/client/QmUi/UiContext.h`
- Modify: `src/game/client/QmUi/UiTokens.h`
- Modify: `src/engine/shared/config_variables_qmclient.h`
- Modify: `src/game/client/components/menus.h`
- Modify: `src/game/client/components/menus.cpp`
- Test: `src/test/QmAnimTest.cpp`

**Interfaces:**
- Consumes: `ColorHSLA(g_Config.m_QmUiColor)`、`g_Config.m_QmUiOpacity / 100.0f`。
- Produces: `SUiTheme ResolveUiTheme(ColorHSLA BaseColor, float Opacity)`、`IUiContext::m_pTheme`、`IUiContext::m_UiScale`、`CMenus::SettingsUiContext(const char *pScope, float UiScale = 1.0f)`。

- [x] **Step 1: Write the failing theme tests**

```cpp
TEST(UiTheme, RuntimeThemeTracksBaseColorAndOpacity)
{
	const SUiTheme Blue = ResolveUiTheme(ColorHSLA(0.60f, 0.75f, 0.45f, 1.0f), 1.0f);
	const SUiTheme RedHalf = ResolveUiTheme(ColorHSLA(0.00f, 0.75f, 0.45f, 1.0f), 0.5f);
	EXPECT_NE(Blue.m_Accent.r, RedHalf.m_Accent.r);
	EXPECT_NE(Blue.m_Accent.b, RedHalf.m_Accent.b);
	EXPECT_LT(RedHalf.m_Surface.a, Blue.m_Surface.a);
	EXPECT_FLOAT_EQ(RedHalf.m_InputSurface.a, RedHalf.m_Surface.a);
}

TEST(UiTheme, FocusRingKeepsInputFillStable)
{
	const SUiTheme Theme = ResolveUiTheme(ColorHSLA(0.58f, 0.35f, 0.48f, 1.0f), 1.0f);
	EXPECT_FLOAT_EQ(Theme.m_InputSurface.r, Theme.m_InputSurfaceFocused.r);
	EXPECT_FLOAT_EQ(Theme.m_InputSurface.g, Theme.m_InputSurfaceFocused.g);
	EXPECT_FLOAT_EQ(Theme.m_InputSurface.b, Theme.m_InputSurfaceFocused.b);
	EXPECT_GE(Theme.m_FocusRingWidth, 2.0f);
	EXPECT_GT(Theme.m_FocusRing.a, Theme.m_Border.a);
}
```

- [x] **Step 2: Run tests to verify red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiTheme.*
```

Expected: compile FAIL because `SUiTheme` and `ResolveUiTheme(...)` do not exist.

- [x] **Step 3: Add the minimal runtime theme contract**

`src/game/client/QmUi/UiTheme.h`:

```cpp
#ifndef GAME_CLIENT_QMUI_UITHEME_H
#define GAME_CLIENT_QMUI_UITHEME_H

#include <base/color.h>

#include <algorithm>

struct SUiTheme
{
	ColorRGBA m_Surface;
	ColorRGBA m_SurfaceHovered;
	ColorRGBA m_SurfaceFocused;
	ColorRGBA m_Border;
	ColorRGBA m_BorderHovered;
	ColorRGBA m_BorderFocused;
	ColorRGBA m_InputSurface;
	ColorRGBA m_InputSurfaceFocused;
	ColorRGBA m_FocusRing;
	ColorRGBA m_Accent;
	ColorRGBA m_TextTitle;
	ColorRGBA m_TextBody;
	ColorRGBA m_TextSmall;
	float m_FocusRingWidth = 2.0f;
	float m_FocusRingInset = 1.0f;
};

inline SUiTheme ResolveUiTheme(ColorHSLA BaseColor, float Opacity)
{
	Opacity = std::clamp(Opacity, 0.0f, 1.0f);
	const ColorRGBA SurfaceBase = color_cast<ColorRGBA>(BaseColor.UnclampLighting(0.42f));
	const ColorRGBA AccentBase = color_cast<ColorRGBA>(BaseColor.UnclampLighting(0.48f));
	SUiTheme Theme;
	Theme.m_Surface = SurfaceBase.WithAlpha(std::clamp(std::max(SurfaceBase.a, 0.70f) * Opacity, 0.0f, 1.0f));
	Theme.m_SurfaceHovered = ColorRGBA(
		std::clamp(Theme.m_Surface.r * 1.06f, 0.0f, 1.0f),
		std::clamp(Theme.m_Surface.g * 1.06f, 0.0f, 1.0f),
		std::clamp(Theme.m_Surface.b * 1.06f, 0.0f, 1.0f), Theme.m_Surface.a);
	Theme.m_SurfaceFocused = Theme.m_SurfaceHovered;
	Theme.m_Border = ColorRGBA(1.0f, 1.0f, 1.0f, 0.10f * Opacity);
	Theme.m_BorderHovered = AccentBase.WithAlpha(0.45f * Opacity);
	Theme.m_BorderFocused = AccentBase.WithAlpha(0.75f * Opacity);
	Theme.m_InputSurface = Theme.m_Surface;
	Theme.m_InputSurfaceFocused = Theme.m_InputSurface;
	Theme.m_FocusRing = AccentBase.WithAlpha(0.90f * Opacity);
	Theme.m_Accent = AccentBase.WithAlpha(std::clamp(std::max(AccentBase.a, 0.85f) * Opacity, 0.0f, 1.0f));
	Theme.m_TextTitle = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	Theme.m_TextBody = ColorRGBA(0.92f, 0.92f, 0.94f, 1.0f);
	Theme.m_TextSmall = ColorRGBA(0.72f, 0.74f, 0.78f, 1.0f);
	return Theme;
}

#endif
```

在 `IUiContext` 中前置声明 `struct SUiTheme;` 并增加：

```cpp
const SUiTheme *m_pTheme = nullptr;
float m_UiScale = 1.0f;
```


- [x] **Step 4: Route menus and primitives through the theme pointer**

`CMenus` 持有本帧 theme，并只通过一个 helper 构造设置页 context：

```cpp
IUiContext CMenus::SettingsUiContext(const char *pScope, float UiScale)
{
	m_SettingsUiTheme = ResolveUiTheme(ColorHSLA(g_Config.m_QmUiColor), g_Config.m_QmUiOpacity / 100.0f);
	IUiContext Ctx;
	Ctx.m_pUi = Ui();
	Ctx.m_pAnim = &GameClient()->UiRuntimeV2()->AnimRuntime();
	Ctx.m_pTree = &GameClient()->UiRuntimeV2()->Tree();
	Ctx.m_pIconManager = GameClient()->QmIconManager();
	Ctx.m_pMenus = this;
	Ctx.m_pTooltips = &GameClient()->m_Tooltips;
	Ctx.m_pTextRender = TextRender();
	Ctx.m_pTheme = &m_SettingsUiTheme;
	Ctx.m_ScopeHash = MakeUiScopeHash(pScope);
	Ctx.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();
	Ctx.m_UiScale = UiScale;
	return Ctx;
}
```

`UiForms.cpp` 的 field fill 与 focus ring 改为读取 `Ctx.m_pTheme`；`m_pTheme == nullptr` 时只允许使用 `ResolveUiTheme(ColorHSLA(0.0f, 0.0f, 0.29f, 1.0f), 1.0f)` 这一公共 fallback，不回到静态 `CUi::ms_LightButtonColorFunction`。

- [x] **Step 5: Run tests to verify green**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiTheme.*
```

Expected: `2 tests` PASS。

- [x] **Step 6: Commit runtime theme**

```powershell
git add docs/superpowers/plans/2026-07-11-QmClient-设置页UI统一-P1-Theme与SettingsCard基础.md src/game/client/QmUi/UiTheme.h src/game/client/QmUi/UiContext.h src/game/client/QmUi/UiForms.cpp src/game/client/components/menus.h src/game/client/components/menus.cpp src/test/QmAnimTest.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "feat(settings-ui): 建立运行时主题链" -m "feat: 统一卡片、输入与焦点颜色解析" -m "test: 覆盖主题色与透明度实时派生"
```

### Task 2: 实现唯一 SettingsPageLayout

**Files:**
- Create: `src/game/client/QmUi/SettingsPageLayout.h`
- Modify: `src/game/client/QmUi/UiTokens.h`
- Test: `src/test/QmAnimTest.cpp`

**Interfaces:**
- Consumes: 页面 content rect、是否有全宽子 tab、UI scale。
- Produces: `SSettingsPageLayoutFrame ResolveSettingsPageLayout(const CUIRect &PageRect, bool HasSubTabs, float UiScale = 1.0f)`。

- [x] **Step 1: Write failing layout tests**

```cpp
TEST(SettingsPageLayout, WideViewportUsesEqualColumnsBelowFullWidthTabs)
{
	const SSettingsPageLayoutFrame Frame = ResolveSettingsPageLayout({0.0f, 0.0f, 1000.0f, 700.0f}, true, 1.0f);
	EXPECT_TRUE(Frame.m_TwoColumns);
	EXPECT_FLOAT_EQ(Frame.m_SubTabRect.w, Frame.m_PageRect.w);
	EXPECT_LT(Frame.m_SubTabRect.y, Frame.m_aColumns[0].y);
	EXPECT_FLOAT_EQ(Frame.m_aColumns[0].w, Frame.m_aColumns[1].w);
	EXPECT_GT(Frame.m_aColumns[1].x, Frame.m_aColumns[0].x + Frame.m_aColumns[0].w);
}

TEST(SettingsPageLayout, NarrowViewportUsesOneColumnWithoutPhantomRightColumn)
{
	const SSettingsPageLayoutFrame Frame = ResolveSettingsPageLayout({0.0f, 0.0f, 620.0f, 700.0f}, false, 1.0f);
	EXPECT_FALSE(Frame.m_TwoColumns);
	EXPECT_FLOAT_EQ(Frame.m_aColumns[0].w, Frame.m_ContentViewport.w);
	EXPECT_FLOAT_EQ(Frame.m_aColumns[1].w, 0.0f);
	EXPECT_FLOAT_EQ(Frame.m_SubTabRect.h, 0.0f);
}
```

- [x] **Step 2: Run tests to verify red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=SettingsPageLayout.*
```

Expected: compile FAIL because layout types and resolver do not exist.

- [x] **Step 3: Implement the exact layout contract**

实现边界：

- `m_SubTabRect` 在有子 tab 时使用 `PageRect` 全宽；卡片/滚动 viewport 在 tab 下方再应用 inset，避免 tab 被卡片列宽限制。
- 宽屏两列仅从同一份 `m_ContentViewport` 派生，两列等宽并由 `m_CardGap` 分隔；窄屏右列保持零 rect。
- `UiScale <= 0` 回退为基础比例；过小 viewport 通过非负宽高收敛。
- resolver 只读写 `CUIRect` 值字段，不调用需要 UI renderer 链接的成员函数，因此可直接由 `testrunner` 验证。
- [x] **Step 4: Run tests to verify green**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=SettingsPageLayout.*
```

Expected: `2 tests` PASS。

- [x] **Step 5: Commit page layout**

```powershell
git add src/game/client/QmUi/SettingsPageLayout.h src/game/client/QmUi/UiTokens.h src/test/QmAnimTest.cpp
git commit -m "feat(settings-ui): 统一设置页布局解析" -m "feat: 固化全宽子页签与单双列 frame" -m "test: 覆盖宽窄 viewport 几何"
```

### Task 3: 实现 canonical SettingsCard frame 与 motion contract

**Files:**
- Create: `src/game/client/QmUi/SettingsCardGeometry.h`
- Create: `src/game/client/QmUi/SettingsCardGeometry.cpp`
- Create: `src/game/client/QmUi/SettingsCard.h`
- Create: `src/game/client/QmUi/SettingsCard.cpp`
- Modify: `src/game/client/QmUi/UiTokens.h`
- Modify: `src/game/client/components/menus.h`
- Modify: `src/game/client/components/menus.cpp`
- Modify: `CMakeLists.txt`
- Test: `src/test/QmAnimTest.cpp`

**Interfaces:**
- Consumes: `IUiContext`、slot rect、`SSettingsCardSpec`、measure/render callbacks、`SSettingsCardVisualState`。
- Produces: `SSettingsCardFrame BuildSettingsCardFrame(...)`、`SSettingsCardFrame SettingsCard(...)`、`SCardMotionSpec ResolveCardMotionSpec(int MotionLevel, bool ExtraAnimations)`。

- [x] **Step 1: Write failing frame and motion tests**

```cpp
TEST(SettingsCard, CanonicalRectOwnsDisplayHitDragAndProxyGeometry)
{
	SSettingsCardSpec Spec;
	Spec.m_pStableId = "deck:graphics-display";
	Spec.m_pTitle = "Graphics display";
	Spec.m_pSubtitle = "Window and monitor";
	const SSettingsCardFrame Frame = BuildSettingsCardFrame({10.0f, 20.0f, 400.0f, 0.0f}, Spec, 180.0f, 1.0f);
	EXPECT_EQ(&Frame.DisplayRect(), &Frame.HitRect());
	EXPECT_EQ(&Frame.DisplayRect(), &Frame.DragRect());
	EXPECT_EQ(&Frame.DisplayRect(), &Frame.ProxySourceRect());
	EXPECT_GE(Frame.m_ContentRect.x, Frame.m_Rect.x);
	EXPECT_GE(Frame.m_ContentRect.y, Frame.m_Rect.y);
	EXPECT_LE(Frame.m_ContentRect.x + Frame.m_ContentRect.w, Frame.m_Rect.x + Frame.m_Rect.w);
	EXPECT_LE(Frame.m_ContentRect.y + Frame.m_ContentRect.h, Frame.m_Rect.y + Frame.m_Rect.h);
	EXPECT_GT(Frame.m_SubtitleRect.h, 0.0f);
}

TEST(SettingsCard, MotionPolicyKeepsRequiredFeedbackAtLevelZero)
{
	const SCardMotionSpec Full = ResolveCardMotionSpec(2, true);
	const SCardMotionSpec Reduced = ResolveCardMotionSpec(1, true);
	const SCardMotionSpec Off = ResolveCardMotionSpec(0, true);
	EXPECT_GT(Full.m_EntryDistance, Reduced.m_EntryDistance);
	EXPECT_FLOAT_EQ(Full.m_EntryDuration, 0.16f);
	EXPECT_FLOAT_EQ(Reduced.m_ReflowDuration, 0.12f);
	EXPECT_TRUE(Full.m_DecorativeMotion);
	EXPECT_FALSE(ResolveCardMotionSpec(2, false).m_DecorativeMotion);
	EXPECT_FLOAT_EQ(Off.m_EntryDistance, 0.0f);
	EXPECT_FLOAT_EQ(Off.m_EntryDuration, 0.0f);
	EXPECT_FLOAT_EQ(Off.m_ReflowDuration, 0.0f);
	EXPECT_GT(Off.m_DropFeedbackDuration, 0.0f);
	EXPECT_GT(Off.m_ReflowCompleteFeedbackDuration, 0.0f);
	EXPECT_FLOAT_EQ(ResolveCardMotionSpec(-1, true).m_EntryDistance, 0.0f);
	EXPECT_FLOAT_EQ(ResolveCardMotionSpec(99, true).m_EntryDistance, Full.m_EntryDistance);
	EXPECT_TRUE(Off.m_KeepDragProxy);
	EXPECT_TRUE(Off.m_KeepDropFeedback);
	EXPECT_TRUE(Off.m_KeepReflowCompleteFeedback);
}
```

- [x] **Step 2: Run tests to verify red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=SettingsCard.*
```

Expected: compile FAIL because `SettingsCardGeometry.h/.cpp` and the card contract are absent.

- [x] **Step 3: Implement exact public structs**

`SettingsCardGeometry.h` owns only renderer-free value types and pure functions:

```cpp
struct SSettingsCardSpec
{
	const char *m_pStableId = nullptr;
	const char *m_pTitle = nullptr;
	const char *m_pSubtitle = nullptr;
};

struct SSettingsCardFrame
{
	CUIRect m_Rect;
	CUIRect m_HeaderRect;
	CUIRect m_TitleRect;
	CUIRect m_SubtitleRect;
	CUIRect m_HandleRect;
	CUIRect m_ContentRect;
	const CUIRect &DisplayRect() const { return m_Rect; }
	const CUIRect &HitRect() const { return m_Rect; }
	const CUIRect &DragRect() const { return m_Rect; }
	const CUIRect &ProxySourceRect() const { return m_Rect; }
};

struct SCardMotionSpec
{
	float m_EntryDistance = 0.0f;
	float m_EntryDuration = 0.0f;
	float m_ReflowDuration = 0.0f;
	float m_DropFeedbackDuration = 0.08f;
	float m_ReflowCompleteFeedbackDuration = 0.08f;
	bool m_DecorativeMotion = false;
	bool m_KeepDragProxy = true;
	bool m_KeepDropFeedback = true;
	bool m_KeepReflowCompleteFeedback = true;
};

SSettingsCardFrame BuildSettingsCardFrame(const CUIRect &Slot, const SSettingsCardSpec &Spec, float ContentHeight, float UiScale);
SCardMotionSpec ResolveCardMotionSpec(int MotionLevel, bool ExtraAnimations);
```

`SettingsCard.h` includes `SettingsCardGeometry.h` and owns the client rendering contract:

```cpp
struct SSettingsCardDeckVisualOptions
{
	bool m_RainbowTitles = false;
};

struct SSettingsCardVisualState
{
	bool m_Hovered = false;
	bool m_Focused = false;
	bool m_Dragged = false;
	bool m_DropFeedback = false;
	bool m_ReflowCompleteFeedback = false;
	float m_DrawOffsetX = 0.0f;
	float m_DrawOffsetY = 0.0f;
	float m_DrawAlpha = 1.0f;
};

using FSettingsCardMeasure = std::function<float(float ContentWidth)>;
using FSettingsCardRender = std::function<void(CUIRect ContentRect)>;

SSettingsCardFrame SettingsCard(const IUiContext &Ctx, const CUIRect &Slot, const SSettingsCardSpec &Spec, const SSettingsCardVisualState &State, const SSettingsCardDeckVisualOptions &VisualOptions, const FSettingsCardMeasure &Measure, const FSettingsCardRender &Render);
```

`SettingsCardGeometry.cpp` 是 `BuildSettingsCardFrame(...)` 和 `ResolveCardMotionSpec(...)` 的唯一 owner，只 include `SettingsCardGeometry.h`、`UiTokens.h`与基础数学头，不 include `UiContext.h`、`UiForms.h`、`ui.h`、`graphics.h` 或任何菜单组件。`SettingsCard.cpp` 只实现 `SettingsCard(...)` 绘制。先在 `UiTokens.h` 定义 `CARD_PADDING`、`CARD_HEADER_TITLE_HEIGHT`、`CARD_HEADER_SUBTITLE_HEIGHT`、`CARD_HEADER_GAP`、`CARD_HANDLE_SIZE` 和 `CARD_RADIUS`，然后实现下面的单次测量契约：

```cpp
namespace ui_token::font
{
	inline constexpr float TITLE = 18.0f;
	inline constexpr float BODY = 12.0f;
	inline constexpr float SMALL = 10.0f;
}
```

`BODY` 是现有 token 的唯一声明，实施时在原 namespace 中补 `TITLE`/`SMALL`，不复制 `BODY`。

```cpp
SSettingsCardFrame BuildSettingsCardFrame(const CUIRect &Slot, const SSettingsCardSpec &Spec, float ContentHeight, float UiScale)
{
	const float Padding = ui_token::settings::CARD_PADDING * UiScale;
	const float TitleHeight = ui_token::settings::CARD_HEADER_TITLE_HEIGHT * UiScale;
	const float SubtitleHeight = Spec.m_pSubtitle != nullptr ? ui_token::settings::CARD_HEADER_SUBTITLE_HEIGHT * UiScale : 0.0f;
	const float HeaderGap = ui_token::settings::CARD_HEADER_GAP * UiScale;
	const float HeaderHeight = TitleHeight + SubtitleHeight;
	const float HandleSize = ui_token::settings::CARD_HANDLE_SIZE * UiScale;

	SSettingsCardFrame Frame{};
	Frame.m_Rect = {Slot.x, Slot.y, Slot.w, Padding + HeaderHeight + HeaderGap + maximum(0.0f, ContentHeight) + Padding};
	Frame.m_HeaderRect = {Frame.m_Rect.x + Padding, Frame.m_Rect.y + Padding, maximum(0.0f, Frame.m_Rect.w - Padding * 2.0f), HeaderHeight};
	Frame.m_HandleRect = {Frame.m_HeaderRect.x + maximum(0.0f, Frame.m_HeaderRect.w - HandleSize), Frame.m_HeaderRect.y, HandleSize, HandleSize};
	const float TextWidth = maximum(0.0f, Frame.m_HeaderRect.w - HandleSize - Padding);
	Frame.m_TitleRect = {Frame.m_HeaderRect.x, Frame.m_HeaderRect.y, TextWidth, TitleHeight};
	Frame.m_SubtitleRect = {Frame.m_HeaderRect.x, Frame.m_HeaderRect.y + TitleHeight, TextWidth, SubtitleHeight};
	Frame.m_ContentRect = {Frame.m_Rect.x + Padding, Frame.m_HeaderRect.y + HeaderHeight + HeaderGap, maximum(0.0f, Frame.m_Rect.w - Padding * 2.0f), maximum(0.0f, ContentHeight)};
	return Frame;
}

namespace
{
	void RenderCanonicalSettingsCardHandle(const IUiContext &Ctx, const CUIRect &HandleRect, bool Active, float DrawAlpha)
	{
		const SUiTheme Fallback = ResolveUiTheme(ColorHSLA(0.0f, 0.0f, 0.29f, 1.0f), 1.0f);
		const SUiTheme &Theme = Ctx.m_pTheme != nullptr ? *Ctx.m_pTheme : Fallback;
		const ColorRGBA Color = (Active ? Theme.m_Accent : Theme.m_TextSmall).WithAlpha((Active ? Theme.m_Accent.a : Theme.m_TextSmall.a) * DrawAlpha);
		const float Stroke = maximum(2.0f, 2.0f * Ctx.m_UiScale);
		const float Width = maximum(0.0f, HandleRect.w * 0.5f);
		for(int LineIndex = -1; LineIndex <= 1; ++LineIndex)
		{
			CUIRect Line{HandleRect.x + (HandleRect.w - Width) * 0.5f, HandleRect.y + HandleRect.h * 0.5f + LineIndex * Stroke * 2.0f - Stroke * 0.5f, Width, Stroke};
			Line.Draw(Color, IGraphics::CORNER_ALL, Stroke * 0.5f);
		}
	}
}

SSettingsCardFrame SettingsCard(const IUiContext &Ctx, const CUIRect &Slot, const SSettingsCardSpec &Spec, const SSettingsCardVisualState &State, const SSettingsCardDeckVisualOptions &VisualOptions, const FSettingsCardMeasure &Measure, const FSettingsCardRender &Render)
{
	const float UiScale = Ctx.m_UiScale;
	const float ContentWidth = maximum(0.0f, Slot.w - 2.0f * ui_token::settings::CARD_PADDING * UiScale);
	const float ContentHeight = Measure ? maximum(0.0f, Measure(ContentWidth)) : 0.0f;
	const SSettingsCardFrame Frame = BuildSettingsCardFrame(Slot, Spec, ContentHeight, UiScale);
	SSettingsCardVisualState DrawState = State;
	DrawState.m_Hovered = Ctx.m_pUi != nullptr && Ctx.m_pUi->MouseHovered(&Frame.m_Rect);
	SSettingsCardFrame DrawFrame = Frame;
	for(CUIRect *pRect : {&DrawFrame.m_Rect, &DrawFrame.m_HeaderRect, &DrawFrame.m_TitleRect, &DrawFrame.m_SubtitleRect, &DrawFrame.m_HandleRect, &DrawFrame.m_ContentRect})
	{
		pRect->x += State.m_DrawOffsetX;
		pRect->y += State.m_DrawOffsetY;
	}
	const SUiTheme Fallback = ResolveUiTheme(ColorHSLA(0.0f, 0.0f, 0.29f, 1.0f), 1.0f);
	const SUiTheme &Theme = Ctx.m_pTheme != nullptr ? *Ctx.m_pTheme : Fallback;
	const float FeedbackAlpha = DrawState.m_DropFeedback ? 0.94f : DrawState.m_ReflowCompleteFeedback ? 0.97f : 1.0f;
	const ColorRGBA Surface = (DrawState.m_Hovered ? Theme.m_SurfaceHovered : Theme.m_Surface).WithAlpha(Theme.m_Surface.a * DrawState.m_DrawAlpha * FeedbackAlpha);
	const bool InteractionComplete = DrawState.m_DropFeedback || DrawState.m_ReflowCompleteFeedback;
	const ColorRGBA BorderBase = DrawState.m_Focused || InteractionComplete ? Theme.m_BorderFocused : DrawState.m_Hovered ? Theme.m_BorderHovered : Theme.m_Border;
	const ColorRGBA Border = BorderBase.WithAlpha(BorderBase.a * DrawState.m_DrawAlpha);
	DrawFrame.m_Rect.Draw(Surface, IGraphics::CORNER_ALL, ui_token::settings::CARD_RADIUS * UiScale);
	DrawFrame.m_Rect.DrawOutline(Border);
	if(DrawState.m_Focused)
	{
		CUIRect FocusRect;
		DrawFrame.m_Rect.Margin(-Theme.m_FocusRingWidth * UiScale, &FocusRect);
		FocusRect.DrawOutline(Theme.m_FocusRing.WithAlpha(Theme.m_FocusRing.a * DrawState.m_DrawAlpha));
	}
	if(Ctx.m_pUi != nullptr && Ctx.m_pTextRender != nullptr)
	{
		ColorRGBA TitleColor = Theme.m_TextTitle;
		if(VisualOptions.m_RainbowTitles)
		{
			const float Phase = std::fmod(time_get() / (float)time_freq() * 0.08f + str_quickhash(Spec.m_pStableId) / 65535.0f, 1.0f);
			TitleColor = color_cast<ColorRGBA>(ColorHSLA(Phase, 0.75f, 0.65f, DrawState.m_DrawAlpha));
		}
		Ctx.m_pTextRender->TextColor(TitleColor.WithAlpha(TitleColor.a * DrawState.m_DrawAlpha));
		Ctx.m_pUi->DoLabel(&DrawFrame.m_TitleRect, Spec.m_pTitle, ui_token::font::TITLE * UiScale, TEXTALIGN_ML);
		if(Spec.m_pSubtitle != nullptr && (DrawState.m_Hovered || DrawState.m_Focused))
		{
			Ctx.m_pTextRender->TextColor(Theme.m_TextSmall.WithAlpha(Theme.m_TextSmall.a * DrawState.m_DrawAlpha));
			Ctx.m_pUi->DoLabel(&DrawFrame.m_SubtitleRect, Spec.m_pSubtitle, ui_token::font::SMALL * UiScale, TEXTALIGN_ML);
		}
		Ctx.m_pTextRender->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	}
	RenderCanonicalSettingsCardHandle(Ctx, DrawFrame.m_HandleRect, DrawState.m_Hovered || DrawState.m_Focused || DrawState.m_Dragged, DrawState.m_DrawAlpha);
	if(Render)
		Render(DrawFrame.m_ContentRect);
	return Frame;
}

SCardMotionSpec ResolveCardMotionSpec(int MotionLevel, bool ExtraAnimations)
{
	const int Level = maximum(0, minimum(2, MotionLevel));
	SCardMotionSpec Spec{};
	Spec.m_KeepDragProxy = true;
	Spec.m_KeepDropFeedback = true;
	Spec.m_KeepReflowCompleteFeedback = true;
	if(Level == 0)
		return Spec;

	if(Level == 1)
	{
		Spec.m_EntryDistance = 6.0f;
		Spec.m_EntryDuration = 0.10f;
		Spec.m_ReflowDuration = 0.12f;
		Spec.m_DecorativeMotion = false;
		return Spec;
	}

	Spec.m_EntryDistance = 12.0f;
	Spec.m_EntryDuration = 0.16f;
	Spec.m_ReflowDuration = 0.18f;
	Spec.m_DecorativeMotion = ExtraAnimations;
	return Spec;
}
```

`SettingsCard(...)` 用 `DrawFrame` 绘制，不修改返回的 canonical `Frame`。有 subtitle 时 `m_SubtitleRect.h` 始终参与 header/frame 高度，但 subtitle 文字只在 hover/focus 时绘制，因此显隐不引起 reflow；没有 subtitle 时高度为 `0`。`SettingsCard.cpp` 的私有 `RenderCanonicalSettingsCardHandle(...)` 始终绘制 `m_HandleRect`，页面不得再绘 6-dot/私有 handle；旧 `CMenus::RenderSettingsCardDragHandle(...)` 仅保留给 P5/P6 尚未迁移页面，页面迁完后删除。`BuildSettingsCardFrame(...)` 只消费已测得的 `ContentHeight`，`SettingsCard(...)` 每张卡恰好调用一次 `Measure(ContentWidth)`。

在根 `CMakeLists.txt` 做两处不对称登记。既有 QmUi client source list 登记：

```cmake
QmUi/SettingsCard.cpp
QmUi/SettingsCard.h
QmUi/SettingsCardGeometry.cpp
QmUi/SettingsCardGeometry.h
```

`TESTS_EXTRA` 只登记：

```cmake
src/game/client/QmUi/SettingsCardGeometry.cpp
```

`QmAnimTest.cpp` 只 include `SettingsCardGeometry.h`。禁止把 `SettingsCard.cpp`、`UiForms.cpp` 或整个 UI renderer 加入 `TESTS_EXTRA`；client shell 由 Task 4 的 production structure test 与 `game-client` build 覆盖。

- [x] **Step 4: Centralize motion configuration**

`CMenus` 只通过一个 helper 解释现有动画配置：

```cpp
SCardMotionSpec CMenus::SettingsCardMotionSpec() const
{
	return ResolveCardMotionSpec(g_Config.m_QmUiMotionLevel, g_Config.m_QmExtraAnimations != 0);
}
```

Motion level `0` 仅关闭 entry 的 Y/alpha tween 与装饰 tween；drag proxy 立即跟随，drop 和 reflow-complete 保留 `0.08s` 的必要 border/alpha 反馈，不改变 canonical rect 或持久化完成时机。`qm_ui_card_rainbow_titles` 与 `SettingsCardDeckVisualOptions()` 延后到 Task 4：届时它会有 Graphics card title shell 的真实消费者，避免新增未使用配置。

`testrunner` 不链接 `ui_rect.cpp`，因此 frame 测试使用 rect 值域比较代替 `CUIRect::Inside(...)`；geometry owner 仍不依赖 UI renderer。
- [x] **Step 5: Run tests to verify green**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=SettingsCard.*
```

Expected: `2 tests` PASS；本命令只是 release focused 证据，不声称已运行 sanitizer。

- [x] **Step 6: Commit card shell**

```powershell
git add CMakeLists.txt src/game/client/QmUi/SettingsCardGeometry.h src/game/client/QmUi/SettingsCardGeometry.cpp src/game/client/QmUi/SettingsCard.h src/game/client/QmUi/SettingsCard.cpp src/game/client/QmUi/UiTokens.h src/game/client/components/menus.h src/game/client/components/menus.cpp src/test/QmAnimTest.cpp docs/superpowers/plans/2026-07-11-QmClient-设置页UI统一-P1-Theme与SettingsCard基础.md
git commit -m "feat(settings-ui): 建立唯一设置卡片 frame" -m "feat: 统一标题、内容、命中与拖拽几何" -m "test: 覆盖 canonical rect 与 motion level"
```

### Task 4: 用 Graphics 验证完整 theme/layout/card slice

**Files:**
- Modify: `src/game/client/components/menus_settings.cpp:3026`
- Modify: `src/game/client/components/menus.h`
- Modify: `src/game/client/components/menus.cpp`
- Modify: `src/test/qmclient_monitoring_test.cpp`
- Test: `src/test/QmAnimTest.cpp`

**Interfaces:**
- Consumes: Tasks 1–3 的 `SettingsUiContext(...)`、`ResolveSettingsPageLayout(...)`、`SettingsCard(...)`；暂时复用现有 `BeginSettingsCardDeck(...)` 排序入口。
- Produces: Graphics 四张注册卡使用 canonical shell；P2 可直接替换 deck coordinator，不再迁 shell。

- [ ] **Step 1: Write failing production-path test**

```cpp
TEST(QmMonitoringHelpers, SettingsCardShellConsumesCanonicalVisualContract)
{
	const std::string Source = ReadRepoFile("src/game/client/QmUi/SettingsCard.cpp");
	EXPECT_NE(Source.find("State.m_DrawOffsetY"), std::string::npos);
	EXPECT_NE(Source.find("DrawState.m_DrawAlpha"), std::string::npos);
	EXPECT_NE(Source.find("DrawState.m_DropFeedback"), std::string::npos);
	EXPECT_NE(Source.find("DrawState.m_ReflowCompleteFeedback"), std::string::npos);
	EXPECT_NE(Source.find("DrawState.m_Hovered || DrawState.m_Focused"), std::string::npos);
	EXPECT_NE(Source.find("VisualOptions.m_RainbowTitles"), std::string::npos);
	EXPECT_NE(Source.find("RenderCanonicalSettingsCardHandle("), std::string::npos);
}

TEST(QmMonitoringHelpers, GraphicsUsesCanonicalSettingsCardShell)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsGraphics(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());
	EXPECT_NE(Body.find("ResolveSettingsPageLayout("), std::string::npos);
	EXPECT_NE(Body.find("SettingsCard("), std::string::npos);
	EXPECT_NE(Body.find("deck:graphics-display"), std::string::npos);
	EXPECT_EQ(Body.find("RenderQmSettingsGlassCard("), std::string::npos);
}
```

- [ ] **Step 2: Run test to verify red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.GraphicsUsesCanonicalSettingsCardShell
```

Expected: FAIL because Graphics still invokes the old shell/deck card rendering path.

- [ ] **Step 3: Migrate each Graphics card without changing settings behavior**

四个 stable ID 固定为：

```cpp
"deck:graphics-display"
"deck:graphics-visual"
"deck:graphics-backend"
"deck:graphics-modes"
```

每张卡遵循同一调用形状。Graphics 依现有 model order 遍历四张卡，用局部 `aColumnY[2]` 从 `Page.m_aColumns` 取下一个 slot；`SettingsCard(...)` 返回 frame 后才以 `Frame.m_Rect.h + Page.m_CardGap` 推进该列，因此 measure 仍只发生一次。`VisualState` 只由当前 hover/focus/drag 状态构造；页面不得使用 cached height 重建 rect：

```cpp
const IUiContext CardCtx = SettingsUiContext("settings_graphics");
const SSettingsPageLayoutFrame Page = ResolveSettingsPageLayout(MainView, false, 1.0f);
const int LayoutColumn = 0; // P1 桥接期由现有 placement 将 model column 1/2 映射为 0/1
CUIRect Slot = Page.m_aColumns[LayoutColumn];
Slot.y = aColumnY[LayoutColumn];
SSettingsCardVisualState VisualState{};
const SSettingsCardDeckVisualOptions VisualOptions = SettingsCardDeckVisualOptions();
const SSettingsCardSpec Spec{"deck:graphics-display", Localize("Graphics display"), Localize("Window and monitor")};
const SSettingsCardFrame Frame = SettingsCard(CardCtx, Slot, Spec, VisualState, VisualOptions,
	[&](float ContentWidth) { return MeasureGraphicsDisplayCard(ContentWidth); },
	[&](CUIRect ContentRect) { RenderGraphicsDisplayCard(ContentRect); });
aColumnY[LayoutColumn] = Frame.m_Rect.y + Frame.m_Rect.h + Page.m_CardGap;
RegisterSettingsCardDeckItemFromFrame(Frame, Spec.m_pStableId);
```

`RegisterSettingsCardDeckItemFromFrame(...)` 是 `CMenus` 的 P1 临时 bridge，在 `menus.h/.cpp` 给出唯一签名：

```cpp
void CMenus::RegisterSettingsCardDeckItemFromFrame(const SSettingsCardFrame &Frame, const char *pStableId);
```

该 helper 只把 `Frame.m_Rect`/`Frame.m_HandleRect` 原样注册给现有 drag item，不绘制 shell。P2 的第一个生产迁移提交必须删除它与 P1 局部列游标。

- [ ] **Step 4: Run focused tests and build**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiTheme.*:SettingsPageLayout.*:SettingsCard.*:QmMonitoringHelpers.SettingsCardShellConsumesCanonicalVisualContract:QmMonitoringHelpers.GraphicsUsesCanonicalSettingsCardShell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
```

Expected: focused tests PASS，`game-client` build 退出码 `0`。

- [ ] **Step 5: Manual Graphics acceptance**

使用 `cmake-build-release/DDNet.exe`，记录 1280×720/1920×1080、UI scale 75%/100%/125%、中文/英文：

```text
qm_ui_color 改变时 card、input、focus 同帧换色
qm_ui_opacity 改变时 surface alpha 同步且文字可读
宽屏双列、窄屏单列，四张卡无重叠
卡片只有单层 surface，无白雾/backdrop 叠层
hover/focus/entry 不改变鼠标命中位置
qm_ui_card_rainbow_titles=0 时标题静态且 header 高度不变
qm_ui_motion_level=0 时拖拽反馈仍可用
```

Expected: 每项有截图或文字结果；未执行项明确记为 gap，不写“视觉通过”。

- [ ] **Step 6: Commit Graphics slice**

```powershell
git add src/game/client/components/menus_settings.cpp src/game/client/components/menus.h src/game/client/components/menus.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "refactor(settings-ui): 迁移 Graphics 卡片外壳" -m "refactor: 使用统一 theme、page layout 与 canonical card frame" -m "test: 禁止 Graphics 回退旧 glass shell"
```

### Task 5: P1 全量验证与只读审查

**Files:**
- Modify: none unless review findings require a scoped fix
- Test: all P1 tests, full C++ regression, docs and default gate

**Interfaces:**
- Consumes: Tasks 1–4 commits。
- Produces: P2 可依赖的稳定 theme/layout/card API。

- [ ] **Step 1: Run serial verification**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiTheme.*:SettingsPageLayout.*:SettingsCard.*:QmMonitoringHelpers.SettingsCardShellConsumesCanonicalVisualContract:QmMonitoringHelpers.GraphicsUsesCanonicalSettingsCardShell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
python qmclient_scripts/gate/check_docs.py
python qmclient_scripts/gate/check_gate.py --mode default
git diff --check
```

Expected: 所有命令退出码 `0`；全量 `run_cxx_tests` 而非 focused filter 是最终 C++ 证据。

- [ ] **Step 2: Dispatch independent read-only review**

review 范围为 P1 全部 commit，重点检查：theme pointer 生命周期、header-only ODR、安全 fallback、canonical rect 不变量、Graphics 行为未改变、没有第二套 animation runtime、没有旧 glass 双绘制。按 `docs/ai-workflow/review.md` 先列 findings。

Expected: 子代理完整报告返回；P0/P1 finding 修复并重跑 Step 1，未收口前不进入 P2。

- [ ] **Step 3: Record manual gaps**

把 Task 4 的视觉矩阵结果写入 P1 commit body 或权威规格的验证记录；截图放 `tmp/`，不把临时截图散落仓库根目录。

Expected: 自动验证与人工验证分别陈述；未人工验证项保留为 gap。

---

## Self-review

- Spec coverage: 覆盖运行时 theme、静态 token 清退、page layout、单层 card、三档文字入口、header/subtitle、彩虹/额外动画配置、motion level 和 Graphics 试点。
- Marker scan: 未发现未决占位、未定义版本号或省略式跨任务指令。
- Link consistency: `QmAnimTest.cpp` 只链入 `SettingsCardGeometry.cpp`；`SettingsCard.cpp`/`UiForms.cpp`/UI renderer 不进 `TESTS_EXTRA`，绘制契约由结构测试与 `game-client` 覆盖。
- Type consistency: 后续计划只消费 `SUiTheme`、`IUiContext::m_pTheme`、`SSettingsPageLayoutFrame`、`ResolveSettingsPageLayout(...)`、`SSettingsCardFrame`、`SettingsCard(...)`、`SSettingsCardDeckVisualOptions`、`SCardMotionSpec`。
- Scope boundary: Search/持久化在 P2，输入在 P3，滚动在 P4，页面批量迁移在 P5/P6，R1–R3 未混入。
