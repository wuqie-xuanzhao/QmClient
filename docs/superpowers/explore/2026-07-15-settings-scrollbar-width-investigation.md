---
type: question
date: 2026-07-15
status: outdated
confidence: high
scope:
  - src/game/client/QmUi/QmScroll.cpp
  - src/game/client/QmUi/QmScroll.h
  - src/game/client/QmUi/SettingsCardDeck.cpp
  - src/game/client/QmUi/SettingsPageLayout.h
  - src/game/client/ui_scrollregion.h
  - src/game/client/ui_scrollregion.cpp
  - src/game/client/ui_listbox.cpp
  - src/game/client/components/menus.cpp
  - src/game/client/components/menus_settings.cpp
  - src/game/client/components/menus_settings_controls.cpp
  - src/game/client/components/qmclient/menus_qmclient.cpp
  - src/game/client/components/tclient/menus_tclient.cpp
commit: 1ad8b84048
superseded-by: 2026-07-15-settings-outer-scrollbar-width-investigation.md
notes: |
  基于当前工作树调查；工作树存在其他未提交改动，本报告没有修改或回退这些改动。
---

> 本报告已过时：初版把卡片内列表和 popup 滚动条混入了用户所指的“设置页面最右侧外层滚动条”。请以 `2026-07-15-settings-outer-scrollbar-width-investigation.md` 为准。

# 设置页竖向滚动条宽度调查

## Quick Answer

滚动条宽度不是运行时随机变化，而是由滚动容器收到的 profile、UiScale、margin 和是否使用旧 helper 共同决定。当前设置页主滚动区域走 `SETTINGS_PAGE` 的 `LARGE` preset；但语言页和卡片内大多数 `CListBox` 走 `MENU_LIST` 的 `MEDIUM` preset，TClient Config 卡片内的列表又直接调用 `QmSettingsScrollRegionParams(1.0f)` 使用未缩放的 `LARGE` preset。因此用户在设置页内看到的竖向滚动条确实可能分别是 medium、large，popup 还可能是 small；这就是宽度不一致的直接原因。

当前工作树中，Controls/TClient 等页面原先固定 `UiScale = 1.0f` 的改动已经变为 `ResolveSettingsUiScale(MainView.w)`，所以“不同主设置页因为页面缩放公式不同而宽度不同”目前不是首要原因。剩余主路径的主要差异是“页面滚动条”和“嵌套列表滚动条”消费了不同 profile；另有 TClient Config 内嵌列表绕过了页面当前 UiScale。

## Key Evidence

| # | 结论 | 证据 | 位置 |
|---|---|---|---|
| 1 | 设置页主滚动条使用 large，列表使用 medium，popup 使用 small | `SETTINGS_PAGE -> LARGE`、`MENU_LIST -> MEDIUM`、`POPUP_LIST -> SMALL`；large/medium/small 的 thickness 分别为 `28/20/10 * UiScale` 并带 clamp | `src/game/client/QmUi/QmScroll.cpp:104-123`, `src/game/client/QmUi/QmScroll.cpp:147-174` |
| 2 | preset 的宽度和 margin 会传入旧 `CScrollRegion`，不是只用于新容器 | `QmScrollRegionParamsFromPolicy` 将 `m_Style.m_ScrollbarWidth`/`m_ScrollbarMargin` 写入 `m_ScrollbarThickness`/`m_ScrollbarMargin` | `src/game/client/ui_scrollregion.h:59-68` |
| 3 | 标准设置卡片主滚动区确实使用调用方传入的 params | `CSettingsCardDeck::Render` 在绘制卡片前调用 `pScrollRegion->Begin(..., Input.m_pScrollParams)`，结束时调用 `End()` | `src/game/client/QmUi/SettingsCardDeck.cpp:97-102`, `src/game/client/QmUi/SettingsCardDeck.cpp:366-367` |
| 4 | 语言页不是 settings page profile，而是独立的 medium 列表滚动区 | 语言页显式设置 `m_Profile = MENU_LIST`，再通过 `BeginSettingsScrollRegion` 开始滚动；预布局还手工按 `LANGUAGE_SCROLLBAR_WIDTH = 20` 预留宽度 | `src/game/client/components/menus_settings.cpp:571-606`, `src/game/client/components/menus_settings.cpp:5112-5119` |
| 5 | 卡片内大多数列表默认也是 medium | `CListBox::Reset` 默认 `MENU_LIST`，`DoStart` 每帧按该 profile 解析参数；设置页中的 Graphics mode、音频包、皮肤队列、TClient Profiles 等实例没有切换到 `SETTINGS_PAGE` | `src/game/client/ui_listbox.cpp:19-31`, `src/game/client/ui_listbox.cpp:109-123`, `src/game/client/components/menus_settings.cpp:3445-3462`, `src/game/client/components/menus_settings.cpp:4849-4917`, `src/game/client/components/tclient/menus_tclient.cpp:5176-5180` |
| 6 | 皮肤/国旗网格是 small profile，但当前 profile 隐藏轨道 | `FILTER_GRID` 使用 small preset，并将 `m_RailVisibility` 设为 `HIDDEN`；设置页国旗和 Tee skin 列表显式选择该 profile | `src/game/client/QmUi/QmScroll.cpp:171-176`, `src/game/client/components/menus_settings.cpp:1165-1168`, `src/game/client/components/menus_settings.cpp:2388-2392` |
| 7 | TClient Config 内嵌列表绕过当前页面 UiScale，固定使用 large 的 `1.0f` 参数 | `QmSettingsScrollRegionParams(1.0f)` 返回 large 参数；该参数被 `s_ConfigListScrollRegion` 直接用于 `BeginSettingsScrollRegion` | `src/game/client/components/menus.cpp:4631-4635`, `src/game/client/components/tclient/menus_tclient.cpp:5762-5767` |
| 8 | 视觉上看到的 track 宽度还会被 margin 再缩小 | 新容器的 track 宽度为 `ScrollbarRect.w - 2 * ScrollbarMargin`；旧 `CScrollRegion` 也会用 margin 收缩 rail。故 large/medium 不仅预留厚度不同，最终可见轨道也不同 | `src/game/client/QmUi/QmScroll.cpp:65-77`, `src/game/client/ui_scrollregion.cpp:219-245` |

## Details

### 当前宽度档位

在 `UiScale = 1.0` 时，当前代码的逻辑值是：

| 语义 | preset | 预留 thickness | margin | 新容器可见 track 宽度 |
|---|---|---:|---:|---:|
| 页面 | large | 28 | 8 | 12 |
| 普通列表 | medium | 20 | 5 | 10 |
| popup / 小列表 | small | 10 | 2 | 6 |

在较窄页面上 preset 会 clamp，例如 large 最小为 24、medium 最小为 18、small 最小为 8；因此即使页面 UiScale 相同，profile 不同也仍然会有差异。

### 当前路径归类

- **页面外层**：标准 General/Player/Tee/Graphics/Sound/DDNet、Appearance、QmClient deck、Controls 和 TClient deck 都通过 `SETTINGS_PAGE` 生成 `CScrollRegionParams`，再交给 `CSettingsCardDeck` 或对应页面的 `CScrollRegion`。
- **页面型但非 deck 的语言列表**：使用 `MENU_LIST`，所以它不是页面 large 滚动条。
- **卡片内嵌列表**：`CListBox` 默认 `MENU_LIST`，因此普通列表一般是 medium。`FILTER_GRID` 是 small 但隐藏轨道，不应产生可见滚动条。
- **TClient Config 内嵌列表**：使用旧的 settings helper，固定 `UiScale = 1.0` 的 large；在页面缩放低于 1 时，它可能比外层 large 还粗。
- **下拉 popup**：通用 `Ui()->DoDropDown` 走 `POPUP_LIST` small；这是 popup 语义上的独立滚动条，不应与页面外层条比较宽度。

### 这说明什么

如果用户比较的是每个设置页面最右侧的**外层滚动条**，当前代码理论上应当由同一个 `SETTINGS_PAGE` large preset 控制；需要重点检查实际运行时各页面传入的 `MainView.w` 是否一致，以及是否有页面仍保留未迁移的旧滚动调用。

如果用户比较的是设置卡片或语言页内部出现的**嵌套滚动条**，宽度不一致是当前实现的确定行为，不是绘制误差：页面条是 large，普通 `CListBox`/语言列表是 medium，popup 是 small，TClient Config 又是固定 scale 的 large。

## Exploration Scope

- 重点检查了 `QmScroll` preset/policy、`CScrollRegion` 几何、`CListBox` 适配器、SettingsCardDeck，以及标准/QmClient/Controls/TClient 设置页调用点。
- 未运行客户端截图或像素测量，因此本报告解释的是代码层面的宽度来源，不判断某一张实际截图中的具体 scrollbar 实例。
- 未修改任何 C++、测试或现有未提交文件；只新增本调查记录。

## Confidence Notes

**confidence: high**

- preset 定义、legacy adapter、SettingsCardDeck 调用路径和主要设置页实例均已读取当前工作树源码。
- 工作树在调查期间存在其他后台改动；本报告以读取到的当前磁盘内容为准，没有把后台改动当作本轮修改。
- “外层主页面是否在某个具体分辨率出现差异”仍需客户端运行时或截图确认，因为它依赖实际 `MainView` 几何和 overflow 是否成立。

## Open Questions

- 用户反馈中所指的是页面最右侧 outer scrollbar、语言列表、卡片内列表，还是 TClient Config 内嵌列表，尚未通过截图确认。
- `BeginSettingsQmScrollContainer` 及其 `SQmSettingsCardStyle` 仍保留在声明/实现中，但当前生产调用搜索未显示它参与主设置 deck；是否应删除属于后续清理问题。

## Related Documents

- `docs/superpowers/explore/2026-07-14-QmClient-设置页UI与布局现状.md` — 当前设置页 UI 现状汇总，本报告补充其中未展开的滚动条宽度分叉。
- `docs/superpowers/plans/2026-07-11-QmClient-设置页UI统一-P4-Scroll与Dropdown.md` — 滚动 preset 与 profile 的迁移约束。

## Next Steps

若要统一视觉，应先确定“页面 outer”和“卡片内嵌 list”是否允许不同语义档位；若要求设置页内所有可见竖向滚动条完全同宽，则需要让语言页、普通 `CListBox` 和 TClient Config 内嵌列表统一消费 settings-page 的 width/margin，而不是只改绘制函数。
