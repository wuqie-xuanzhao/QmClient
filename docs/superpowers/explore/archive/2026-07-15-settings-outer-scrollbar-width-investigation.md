---
type: question
date: 2026-07-15
status: archived
confidence: high
scope:
  - src/game/client/QmUi/QmScroll.cpp
  - src/game/client/ui_listbox.cpp
  - src/game/client/ui_listbox.h
  - src/game/client/ui_scrollregion.cpp
  - src/game/client/components/menus_settings.cpp
  - src/game/client/components/menus_settings_assets.cpp
  - src/game/client/components/menus_settings7.cpp
  - src/game/client/components/menus_settings_controls.cpp
  - src/game/client/components/tclient/menus_tclient.cpp
  - src/game/client/components/qmclient/menus_qmclient.cpp
commit: 1ad8b84048
related:
  - file: 2026-07-15-settings-scrollbar-width-investigation.md
    relation: supersedes
notes: |
  本报告只调查每个设置页面最右侧、承担页面内容滚动的外层竖向滚动条。
  工作树存在其他未提交改动，本报告没有修改或回退这些改动。
updated: 2026-07-15
---
> **已归档，禁止作为实现依据。** 本文保留为历史记录；当前实现请以活动 spec/plan/skill 为准。


# 设置页外层滚动条宽度调查

## Quick Answer

“外层”在同一页面实现内是固定 profile，但当前代码还不是所有页面共用同一个尺寸输入。标准卡片化页面使用 `SETTINGS_PAGE -> LARGE`，Assets 使用 `MENU_LIST -> MEDIUM`；此外 Contributors 和 Global Search 虽然也使用 `LARGE`，却各自使用了不同的旧 `UiScale` 公式。因此非 Assets 页面确实也会出现不同的可见宽度。

这里必须区分两种宽度：`CScrollRegion` 先用 `m_ScrollbarThickness` 从右侧切出整个槽位，再用 `m_ScrollbarMargin` 缩进后绘制可见轨道。标准 `LARGE` 的槽宽是 `clamp(28 * UiScale, 24, 28)`，可见轨道宽是 `槽宽 - 2 * clamp(8 * UiScale, 6, 8)`；Assets 的 `MEDIUM` 在默认 scale 下槽宽是 `20`、margin 是 `5`、可见轨道是 `10`。

按当前新设置壳层完整代码计算，若 `Screen.w = 1000`：菜单先左右各缩 `10`，新设置壳层再减去 `clamp(0.16 * 980, 132, 168) = 156.8`、间隔 `10` 和内容 margin `20`，所以各页面收到的 `MainView.w = 793.2`。此时最右侧外层可见轨道精确值为：标准页面 `10.463467`，Contributors `11.308800`，Global Search `10.400000`，Assets `10.000000`。标准页面包括 General、Player、普通 Tee、Appearance、Controls、Graphics、Sound、DDNet、TClient，以及 QmClient 的 Visuals、Functions、HUD；它们在同一个 `MainView.w` 下相同。

## Key Evidence

| # | 结论 | 证据 | 位置 |
|---|---|---|---|
| 1 | `SETTINGS_PAGE` 固定解析为 `LARGE`，`MENU_LIST` 固定解析为 `MEDIUM` | `QmResolveScrollPolicy` 对两个 profile 分别选择 `LARGE` 和 `MEDIUM` | `src/game/client/QmUi/QmScroll.cpp:147-164` |
| 2 | 两个 profile 的实际尺寸不同 | `LARGE` 为 `28 * UiScale`、margin `8 * UiScale`；`MEDIUM` 为 `20 * UiScale`、margin `5 * UiScale`，并分别 clamp | `src/game/client/QmUi/QmScroll.cpp:104-123` |
| 3 | 可见轨道不是 `m_ScrollbarThickness` 本身 | `SplitContentArea` 先从右侧切出 thickness，随后 `Margin` 生成 `m_RailRect`；因此可见宽是 thickness 减去两侧 margin | `src/game/client/ui_scrollregion.cpp:219-244` |
| 4 | 标准页面统一使用平滑的 `ResolveSettingsUiScale` | 标准页面入口都按 `MainView.w` 调用该函数；函数的 compact baseline 从 `0.78` 平滑到 `0.85`，再与 `ContentWidth / 1000` 取较大值 | `src/game/client/QmUi/SettingsPageLayout.h:32-36`, `src/game/client/components/menus_settings.cpp:727-728`, `src/game/client/components/menus_settings.cpp:5993-6000` |
| 5 | Contributors 使用不同的旧 scale | Contributors 直接使用 `clamp(MainView.w / 1000, 0.78, 1.0)`，再用 `SETTINGS_PAGE` 生成外层条 | `src/game/client/components/qmclient/menus_qmclient.cpp:1353-1361`, `src/game/client/components/qmclient/menus_qmclient.cpp:1534-1548` |
| 6 | Global Search 也使用另一套旧 scale | Global Search 在 `MainView.w >= 680` 时将 baseline 固定为 `0.85`，与标准页的平滑 baseline 不同 | `src/game/client/components/qmclient/menus_qmclient.cpp:4835-4843`, `src/game/client/components/qmclient/menus_qmclient.cpp:4935-4957` |
| 7 | Settings 壳层给各页面相同的 ContentView 宽度，但没有统一滚动容器 | `RenderSettings` 经过同一 shell 后把 `ContentView` 分发给各页面；新 shell 的 tabbar、间隔和 margin 在这里扣除 | `src/game/client/components/menus_settings.cpp:5257-5265`, `src/game/client/components/menus_settings.cpp:5418-5427` |
| 8 | Assets 页面最右侧的页面滚动条是默认 `CListBox` 的 medium 条 | `RenderSettingsCustom` 对页面的 `CustomList` 调用 `CListBox::DoStart`；该实例没有切换 profile，而 `CListBox` 默认是 `MENU_LIST` | `src/game/client/components/menus_settings_assets.cpp:4484-4491`, `src/game/client/components/menus_settings_assets.cpp:5780-5790`, `src/game/client/ui_listbox.cpp:19-31` |
| 9 | `CListBox` 的滚动条会占用传入页面矩形的最右侧 | `DoStart` 将按 profile 得到的 thickness 写入 `CScrollRegionParams`；`CScrollRegion::SplitContentArea` 对竖向条调用 `VSplitRight` | `src/game/client/ui_listbox.cpp:110-124`, `src/game/client/ui_scrollregion.cpp:219-225` |

## Details

### 页面级路径的实际分类

- **标准卡片化页面**：页面函数自己生成 `SETTINGS_PAGE` 参数，再交给 `CSettingsCardDeck` 或同等页面级 `CScrollRegion`，因此外层条是 `LARGE`。
- **Assets**：没有公共 deck 的页面级 scroll region。资产网格自身就是整个 `CustomList` 的滚动容器，默认 `CListBox` 产生 `MEDIUM` 条。
- **Sixup Tee 0.7**：没有公共 deck 的页面级 scroll region。基础皮肤或皮肤部件列表自身占用 `MainView`，默认 `CListBox` 产生 `MEDIUM` 条。
- **语言列表、皮肤网格、卡片内列表和 popup**：这些是页面内部的嵌套滚动条，不属于本次比较的 outer 条；它们使用的 profile 可能不同，但不能拿来解释标准 outer 条之间的差异。

### “外层固定”应如何理解

当前代码的准确表述是：**同一页面实现、同一 `MainView.w` 和同一 scale 公式下固定，不是所有设置页面全局固定**。

标准页面的 outer profile 是统一的 `SETTINGS_PAGE/LARGE`。但是 `RenderSettings` 没有在壳层先创建一个所有页面共享的 `CScrollRegion`，而是让每个页面自行决定页面内容的滚动实现。因此只要某个页面仍使用 `CListBox` 直接承载完整页面内容，它就会继承 `CListBox` 的 `MENU_LIST/MEDIUM` 默认值。

### 不是主要原因的方向

标准页面普遍通过 `ResolveSettingsUiScale(MainView.w)` 取得 scale，但 Contributors 和 Global Search 没有复用该函数。它们的 profile 都是 `LARGE`，所以仅看 profile 会漏掉非 Assets 页面的差异；实际可见宽还受到 scale 和 margin 的影响。

### 精确计算表

下表是“可见轨道宽度”，单位为逻辑 UI 单位；括号内的 `MainView.w` 是页面函数实际收到的内容宽度。标准列覆盖所有复用 `ResolveSettingsUiScale` 的页面。

| `MainView.w` | 标准页面 | Contributors | Global Search | Assets |
|---:|---:|---:|---:|---:|
| 726.0 | 11.090667 | 11.520000 | 10.400000 | 10.000000 |
| 760.0 | 10.773333 | 11.520000 | 10.400000 | 10.000000 |
| 793.2 (`Screen.w=1000`, 新壳层) | 10.463467 | 11.308800 | 10.400000 | 10.000000 |
| 800.0 | 10.400000 | 11.200000 | 10.400000 | 10.000000 |
| 810.0 | 10.400000 | 11.040000 | 10.400000 | 10.000000 |
| 900.0 | 10.800000 | 10.800000 | 10.800000 | 10.000000 |
| 1000.0 | 12.000000 | 12.000000 | 12.000000 | 10.000000 |

## Exploration Scope

- 重点检查：设置页壳层分发、`QmScroll` profile 尺寸、`CListBox` 默认 profile、`CScrollRegion` 右侧分割，以及 Assets/Sixup 的页面入口。
- 未修改 C++、测试或其他未提交文件；只将上一版调查标记为过时，并新增本 outer-only 调查记录。
- 未运行客户端截图或像素测量；结论来自当前磁盘源码，宽度数值为逻辑 UI 单位。

## Confidence Notes

**confidence: high**

- 已覆盖 profile 定义、页面分发、标准页面 outer 参数、Assets 页面列表入口和 Sixup Tee 入口。
- `CListBox` 的默认 profile 和 `CScrollRegion` 的 `VSplitRight` 直接连接了“页面最右侧条”和 `MEDIUM` 参数。
- 工作树存在后台改动；本报告只记录读取到的当前代码，没有回退任何未计划改动。

## Open Questions

- 若要把当前运行环境的单个像素值代入，只需取得该帧页面函数的 `MainView.w`；新壳层下它由屏幕宽、左右 10 margin、tabbar 宽、10 间隔和 10 内容 margin 决定。
- 若目标是所有设置页面外层统一同宽，需要同时统一 Assets/Sixup 的 profile，以及 Contributors/Global Search 的 scale 公式；本调查不做实现决策。

## Related Documents

- `2026-07-15-settings-scrollbar-width-investigation.md` — 初版调查，已标记为 outdated；其中讨论的是嵌套列表/popup，不作为本问题结论依据。

## Next Steps

如果要修复统一宽度，修改点应优先放在 Assets/Sixup 的页面级滚动容器选择，而不是调整 `QmScrollContainerStyleForSize` 的全局数值。
