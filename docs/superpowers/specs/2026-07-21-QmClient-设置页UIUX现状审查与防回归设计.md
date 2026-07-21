---
type: design-review-and-specification
date: 2026-07-21
updated: 2026-07-22
status: active
scope:
  - 新设置页壳层、全部设置卡片页面及其子 Tab
  - Dropdown、Popup、列表、网格、输入、数值、颜色、Tab、预览和滚动组件
  - 设置卡片布局、折叠、拖拽、持久化、入场与动态高度动画
authority: 用户在 2026-07-14 至 2026-07-21 的最新反馈、当前生产代码、当前测试与 gate
relationship:
  - 本文是 2026-07-10 设置页 UI 统一规格的现状审查和后续收口设计
  - 与旧规格冲突时，以本文记录的用户后续决定为准
  - 历史 implementation plan 只作为迁移记录，不作为“已经完成”的证明
---

# QmClient 设置页 UI/UX 现状审查与防回归设计

## 0. 2026-07-22 实现收口状态

本节记录当前代码状态，优先于正文中仍用于解释历史根因的旧现状描述。

| 收口项 | 当前实现 | 自动验证 |
|---|---|---|
| Dropdown 完整行 | popup 被 viewport 限高时按 `row height + spacing` 向下取整，只显示完整行 | `UiV2DropdownGeometry.ClipsToCompleteRowsInsteadOfShowingAHalfRow` |
| Dropdown 上下翻转 | 优先完整容纳；两侧都不足时选择完整行更多的一侧；一行也放不下时不显示 | `UiV2DropdownGeometry.ChoosesTheSideWithMoreCompleteRowsWhenBothSidesAreShort` |
| Dropdown viewport | 上下候选高度同时限制在所属 viewport 总可用高度内，trigger 滚出时不会把 popup 撑出容器 | `UiV2DropdownGeometry.ClampsOversizedPopupInsideViewportMargins` |
| Popup 行高接线 | `CUi::ShowPopupSelection` 显式传入 entry height、spacing、固定 chrome/message 高度 | game-client 构建、全量 C++ tests |
| Popup 无消息高度 | 无提示文本时固定区只包含 popup chrome，不再为零长度文本预留一行 | `UiV2DropdownGeometry.EmptyMessageDoesNotReservePhantomTextHeight` |
| 卡片边框/背景 | border alpha 先限制到可补偿范围；外层 border 与内层 Surface 使用同一 effective border，背景目标色不跟随边框色 | `SettingsCardDeck.EffectiveBorderAlphaCannotPolluteATranslucentSurface` 及 Surface 合成测试 |
| 折叠唯一入口 | `SSettingsCardDefinition` 只声明 `m_DefaultCollapsed` 与 `m_OnCollapseChanged`；按钮命中、绘制、stable-ID session 状态和高度动画全部由 Deck 管理 | `SettingsCardDeck.EveryCardUsesTheSharedCollapseControl`、源码接线契约、全量 C++ tests |
| Controls 兼容 | bind group 的 expanded 状态通过 Deck callback 写回；搜索 reveal 先按 stable ID 强制展开目标卡片 | `SettingsCardDeck.ExplicitCollapseStateOverridesCachedStableIdState`、生产接线契约 |
| QmClient 兼容 | HUD/Function/Visual 通过 Deck callback 写回旧 `qm_sidebar_card_collapsed` 字符串；外部配置变化进入 definitions revision，并作为权威快照覆盖 session cache | game-client 构建、迁移 gate、源码接线契约 |

2026-07-22 本地证据：

- `game-client` Release 编译和链接成功。
- `testrunner.exe` 全量运行：`2326/2326` 通过。
- `check_settings_ui_migration.py --all`：23 个设置页面/子页面全部 `clean`。
- Dropdown/Card Deck 针对性行为测试：`58/58` 通过。
- 相关生产接线与 RenderOnly 契约测试：`5/5` 通过。
- 未制作截图；最终视觉和手感仍由用户在客户端验收。
- 独立 Sol 复审发现的无消息 popup 空白、高 alpha 边框合成、Controls 搜索展开和 QmClient 外部折叠配置同步问题均已按代码修复并补测试；最终提交前仍需基于最终 diff 再执行只读复审。

Mac 交接说明：`mac-origin` 指向 `https://github.com/wuqie-xuanzhao/QmClient.git`。开始收尾时远端 `master` 和 `dyl_dev` 都停在 `b35b19b8a7`；最终提交与远端同步结果记录在本文末尾的“Mac 交接验收”中。

## 1. 文档目的

本文不是又一份按截图罗列局部常量的修补清单，而是对当前设置页 UI/UX 的一次代码级审查，并定义后续唯一允许的组件行为。它解决两个长期问题：

1. 同一类控件已经多次修到可用，但因为仍存在旧入口、隐式默认值和页面私有接线，后续改动又把旧 bug 带回来。
2. 当前自动测试数量很多，但纯函数测试、源码字符串断言、真实组件行为测试和客户端视觉验收经常被混为一谈，导致“测试通过”被误写成“用户看到的行为已经正确”。

本文的目标是让后续实现者不需要从历史截图重新猜设计，并使已经确认过的行为进入公共 API、行为测试和迁移 gate，而不是继续依赖某个页面的偶然实现。

### 1.1 最新用户决定优先级

以下决定覆盖旧规格中的冲突描述：

- 设置页外层滚动条使用稳定逻辑尺寸：布局槽位 `20`，两侧各 `5`，实际可见轨道约 `10`；不随页面宽度缩放。
- 页面内容宽屏最大 `1000`，内容面板与右侧主 Tab 作为整体居中；窄屏流式占满。
- 国旗、皮肤和 Tee7 部件网格在 overflow 时必须显示滚动条；不再使用 hidden rail。
- Dropdown 最多显示完整的 8 行；不得多出半行。项目数 `<= 8` 时无 overflow 就不显示滚动条，项目数 `> 8` 或 viewport 实际限高时显示滚动条；滚轮一次移动 3 行。
- Dropdown popup 不得越过所属设置页面 viewport；锚点不再完整位于其所属容器时关闭，而不是把 popup 留在错误位置或超出容器。
- Dropdown 当前项使用整行背景高亮，不使用左侧竖条。
- 所有普通设置卡片都应使用同一折叠按钮和真实动态高度；不是只有栖梦页面能折叠，也不能出现“按钮存在但不改变高度”。
- 卡片入场和高度动画只允许改变位置或高度，不允许改变 Surface 的颜色或 alpha；展开/折叠时同列后续卡片不能重叠。
- 字号、行距、输入高度、按钮高度、列表行高以统一 metrics 为准；特殊预览、图标和网格字号必须集中登记例外。
- 边框是否显示、边框颜色与卡片背景是三个独立概念。改变边框颜色不得联动卡片背景。
- 视觉确认由用户完成；自动化验收必须准确说明它证明了什么，不能把未运行的客户端视觉检查写成已通过。

## 2. 审查范围、证据和限制

### 2.1 本次读取的主要生产路径

- 页面壳层与 metrics：`SettingsPageLayout.h`、`UiTokens.h`、`menus_settings.cpp`、`menus.cpp`
- 卡片：`SettingsCard.*`、`SettingsCardDeck.*`、`SettingsCardDeckLogic.*`
- Dropdown：`QmDropdown.*`、`ui.cpp`、`ui.h`、`CMenus::DoSettingsDropDown`
- 滚动：`QmScroll.*`、`CScrollRegion`、`CListBox` 的 profile/ownership 接线
- 设置页面：General、Player/Tee、Appearance、Controls、Graphics、Sound、DDNet、QmClient、TClient、Assets、Contributors 和 Global Search
- 公共表单：`UiForms.*`、设置 checkbox/radio/color picker/menu helper
- 防回归：`QmAnimTest.cpp`、`settings_card_deck_logic_test.cpp`、`qm_new_ui_menu_branch_test.cpp`、`check_settings_ui_migration.py`

### 2.2 历史证据

从近期提交和会话反馈可以确认，同一组问题在多个提交中被反复修复：动态卡片高度、卡片背景亮闪、Dropdown 定位与滚动、8 行列表、折叠按钮、字号/行距、嵌套滚动归属。仅最近几次就包括：

- `d8f000fcd1 fix(settings): 修复动态卡片与下拉框交互回归`
- `43c511c910 fix(settings): 收口卡片交互与设置页回归`
- `377ca8225a fix(settings): 修复设置列表与控制器高度回归`
- `fedf78c71f fix(settings): 修复列表高度与下拉框布局回归`
- `91f67959ae fix(settings): 统一卡片布局并修复动画交互`
- `e376d412bb fix(settings): 修复卡片背景闪烁并统一内部布局`

提交标题反复出现“回归”本身就是架构信号：问题不是缺少修复，而是修复没有完全改变可用入口和责任边界。

### 2.3 本文不声称的内容

- 本次没有制作或使用新的截图作为验收证据。
- 本次不声称所有页面的最终视觉已经由自动测试证明。
- 文档初稿只做审查；随后已按本文 P1 收口 Dropdown 与 Settings Card 公共层。协议、玩法、旧设置壳层和非设置菜单业务仍不在范围内。
- Vulkan/其他渲染后端造成的 FPS 差异不再归因于设置页空闲限帧或预热；本文只保留 UI 稳定帧不得重复测量/分配的性能契约。

## 3. 当前总体架构

```text
RenderSettings
  └─ ResolveSettingsShellLayout
       ├─ Content panel（最大内容宽度 1000）
       ├─ Right main Tab bar
       ├─ Restart bar
       └─ shared content metrics
            └─ page renderer
                 ├─ optional shared sub Tab
                 ├─ SETTINGS_OUTER CScrollRegion
                 └─ CSettingsCardDeck
                      ├─ registry / order model / persistence
                      ├─ measure cache / height animation / reflow / drag
                      └─ SettingsCard shell
                           └─ page content controls

Dropdown trigger
  └─ CMenus::DoSettingsDropDown
       └─ CUi::DoDropDown
            ├─ CQmDropdownState
            ├─ QmComputeDropdownPopupGeometry
            ├─ QmResolveDropdownPopupPolicy
            └─ PopupSelection
                 └─ CScrollRegion + POPUP_LIST
```

整体方向是正确的：壳层、metrics、Card shell/Deck、scroll policy 和 Dropdown policy 都已经存在。当前主要风险不在“没有公共组件”，而在公共组件旁边仍有可绕过路径，且部分公共层同时承载了业务页面的手写高度公式。

## 4. 页面壳层、宽度与响应式设计

### 4.1 当前实现

`ResolveSettingsShellLayout` 当前已经实现以下核心规则：

- 右侧主 Tab 宽度按可用宽度的 `16%` 计算，并限制在 `132–168`。
- 内容面板与 Tab 之间间隔 `10`。
- 内容面板左右内边距各 `10`。
- 内容最大宽度为 `1000`，面板最大宽度为 `1000 + 2 × 10`。
- “内容面板 + 间隔 + Tab”作为一个整体在可用区域居中。
- 页面内 inset 使用 `PAGE_INSET = 16`。
- 外层滚动槽位固定扣除 `OUTER_SCROLLBAR_SLOT = 20`。
- 基于扣除滚动槽位后的有效 viewport 再判断双列；当前双列阈值 `760`。
- 双列等宽，列间距使用缩放后的 `CARD_GAP = 16`。
- `ResolveSettingsUiScale` 在窄宽度使用 `0.78–1.0` 的连续缩放，宽屏不超过 `1.0`。

这部分与“窄屏流式、宽屏最大 1000、整体居中、滚动槽位后重算列宽”的目标基本一致。

### 4.2 仍然割裂的地方

1. `SettingsPageLayout.h` 同时存放壳层、通用 metrics、radio 几何，以及 Appearance、DDNet、QmClient HUD 等页面专用高度公式。公共布局层正在变成业务高度公式的集中堆放处，而不是纯基础设施。
2. 页面虽然都能取得统一 `ContentView`，但部分子 Tab、预览区、内部列表和复合工具栏仍自行切割固定宽度或固定高度。
3. 主 Tab 和子 Tab 已有同高 token，但部分页面仍用页面私有调用方式渲染；“尺寸相同”尚未等于“入口唯一”。
4. `SETTINGS_PAGE` 和 `SETTINGS_OUTER` 当前解析为相同 scroll profile，保留了两个同义枚举，增加未来误用概率。

### 4.3 目标设计

壳层只负责以下输出，不承载任何页面业务行数：

```cpp
struct SSettingsShellFrame
{
    CUIRect m_ShellRect;
    CUIRect m_ContentPanelRect;
    CUIRect m_PageRect;
    CUIRect m_MainTabRect;
    CUIRect m_RestartRect;
    CUIRect m_OuterScrollViewport;
    SSettingsContentMetrics m_Metrics;
};
```

页面级 layout resolver 只负责：是否有子 Tab、全宽卡片、默认列 placement 和页面特殊 preview/list 区域。动态卡片的业务 geometry 放在对应页面的纯布局文件中，不能继续扩充 `SettingsPageLayout.h`。

响应式契约：

| 条件 | 行为 |
|---|---|
| 有效内容宽度 `< 760` | 单列，按 canonical reading order 展开 |
| 有效内容宽度 `>= 760` | 双列等宽，尊重用户持久化 column/order |
| 可用内容宽度 `> 1000` | 内容保持 1000，壳层整体居中 |
| 外层滚动条出现/消失 | 始终保留 20 槽位，列宽不跳变 |
| 本地化文本过长 | 优先调整 label/control 分配或切为纵向行；最后才缩小/省略 |

## 5. 统一 Typography、行布局和内容密度

### 5.1 当前 metrics

当前 `SSettingsContentMetrics` 已提供：

- `BodySize`
- `SmallSize`
- `HeadlineSize`
- `LineHeight`
- `LineSpacing`
- `RowStep`
- `InputHeight`
- `ButtonHeight`
- `SectionGap`
- `BadgeHeight`
- `ListRowHeight`
- `LabelWidth`
- `CardGap`

标准宽度下的基准是：Body `12`、Small `10`、Headline `14`、LineHeight `20`、LineSpacing `5`、RowStep `25`。紧凑宽度按统一比例缩放，但 Body 不低于 `10`、Small 不低于 `9`、Headline 不低于 `12`。

迁移 gate 已能检查一部分裸 `9/10/12/13/14/16/20/24/25` 字号、从 rect 高度推导字号、旧 color picker scalar geometry 和设置网格误用通用 `GRID` profile。这是有效的结构门禁，但它不能证明最终文本没有越界、换行或视觉大小一致。

### 5.2 当前割裂

- `menus.h/.cpp` 仍保留多组 checkbox、radio、color picker 和 InputField overload；调用者仍可绕过显式 metrics。
- 旧 `DoLine_ColorPicker(float LineSize, float LabelSize, float BottomMargin, ...)` 仍存在，虽然 gate 试图阻止设置页调用。
- `UiForms` 保留多种简写 InputField overload；它们适合兼容非设置页，但不应继续作为设置页生产 API。
- 特殊视觉字号 allowlist 目前按源码 token/调用片段豁免，粒度偏粗；同一片段未来可能容纳新的业务文本而被误放行。
- 页面专用高度函数仍大量以“行数 × RowStep + 若干 gap”手写，测量和绘制并不天然共享同一组 row rect。

### 5.3 唯一行布局契约

所有普通设置内容必须先生成 geometry，再测量和绘制：

```text
row 0: LineHeight
gap:   LineSpacing
row 1: LineHeight
...
last row: LineHeight（末尾不再追加 LineSpacing）
```

统一语义：

| 元素 | 字号 | 高度 | 说明 |
|---|---|---|---|
| 普通标签、checkbox、radio、按钮、输入文本、placeholder | Body | LineHeight / InputHeight / ButtonHeight | 不从本地 rect 二次缩放 |
| 帮助文本、副标题、badge、次要状态 | Small | 内容测量高度 / BadgeHeight | 不替代普通业务标签 |
| 卡片内分区标题 | Headline | LineHeight | 只用于明确分组 |
| 卡片标题 | Title token | Card header | 不属于内容 metrics 三档 |
| preview/grid/icon | 例外 token | 专用 geometry | 必须按 stable semantic ID 登记 |

focus ring 不改变布局尺寸，但相邻输入行必须保留标准 `LineSpacing`，保证外框不会压住下一行。

## 6. Settings Card shell 与 Card Deck

### 6.1 当前实现已经具备的能力

`SSettingsCardDefinition` 当前支持可见性、测量、绘制、基于测量 rect 绘制、动态 revision、默认折叠状态和折叠变更回调。Deck 已具备：

- stable ID 与全局 order model；
- 双列/单列投影；
- 拖拽、跨列放置、自动滚动和持久化；
- 内容高度缓存与动态高度动画；
- 同列后续卡片基于前一张卡片动画底边排布；
- 整个 Deck 的统一入场位移；
- reflow 和 drop feedback；
- 可见卡片裁剪；
- display cycle 与 diagnostics；
- definitions revision 缓存。

`SettingsCard` 当前在 RenderOnly 阶段不绘制 chrome，Surface alpha 不随入场/重排变化，普通高度变化不触发完成高亮；这些都是正确的防闪烁方向。

### 6.2 当前设计割裂

#### A. 折叠双接线（已于 2026-07-22 收口）

Controls/QmClient 已删除 definition 级 `m_IsCollapsed`、`m_PreLayoutHeaderInput` 和 `m_HeaderAction` 接线。所有卡片由 Deck 统一绘制和处理按钮；需要兼容旧业务状态的页面只通过 `m_DefaultCollapsed` 初始化，并通过 `m_OnCollapseChanged(bool)` 写回，不再拥有第二套 hit-test 或视觉实现。

#### B. 动态高度仍可绕过 revision

`m_MeasureEachFrame` 仍是公开字段。只要页面把它设为 true，就能重新引入每帧测量、动态高度抖动和性能回归。当前它应被视为迁移逃生口，而不是正式能力。

#### C. measure 只返回 float，render 另行切 rect

大量卡片测量返回手写高度，绘制时再执行另一套 `HSplitTop`/`VSplit`。两者即使使用同一 metrics，也可能因为条件分支、末尾 gap、说明文字高度或列表 viewport 不一致而漂移。通知栏文本越界、坐标卡片过高、动态视野收起仍留空白、控制器展开后内容重叠都属于这一类风险。

#### D. 背景与边框耦合（已于本轮收口）

`SettingsCard` 现在直接从 `Theme.m_Surface` 解析 Surface；边框颜色只参与 border ring，不再混入背景。内部 Surface 的 alpha 补偿只抵消半透明边框的合成影响，不改变目标 Surface 颜色。

#### E. 副标题没有内容质量契约

副标题当前只在 hover/focus/运动锁存时显示，但 registry/页面没有禁止通用 fallback 或重复描述。多个卡片出现相同副标题不会触发 gate，也削弱 Search 的可区分性。

#### F. 稳定卡片永久裁剪（已于本轮收口）

`SettingsCardDeckShouldClipContent` 现在同时接收“有可绘制内容”和“内容高度动画仍活动”两个状态。稳定展开卡片不再施加 card-content clip；只有高度动画期间裁剪到当前 ContentRect。

#### G. definitions 缓存身份缺少 Tab（已于本轮收口）

`RenderCached()` 的缓存键现在包含 `{pTab, DefinitionsRevision}`。即使两个 Tab 恰好使用相同 revision，切换后也会重新执行目标 Tab 的 definitions builder。

### 6.3 目标 CardDefinition

卡片必须把结构状态和视觉状态分开：

```cpp
struct SSettingsCardDefinition
{
    SCardIdentity m_Identity;          // stable id, page, tab, title, unique subtitle
    SCardPlacement m_DefaultPlacement; // default only; user persistence wins
    SCardLayoutSnapshot (*m_ResolveLayout)(const SCardLayoutInput &);
    void (*m_Render)(const SCardLayoutSnapshot &, const SCardRenderContext &);
    SCardCollapsePolicy m_Collapse;    // Deck-owned state and input
    uint64_t m_LayoutRevision;
};
```

`SCardLayoutSnapshot` 至少包含：content height、每个普通 row rect、section rect、list viewport、preview rect、toolbar rect 和可见位。测量阶段返回 snapshot；绘制阶段只能消费 snapshot，不得再独立推导高度。

### 6.4 折叠、拖拽和持久化

- 折叠状态和默认折叠按钮完全由 Deck 管理。
- 页面可以声明“不可折叠”或提供业务状态，但不能自行绘制同样式按钮。
- 点击区域是统一 header action rect；拖拽热区排除折叠按钮和其他 header action。
- 收起高度只包含 card header，不保留隐藏内容高度。
- 展开/收起改变 `layout revision`，由同一 snapshot 驱动高度动画。
- 用户持久化的 tab/column/order 永远优先于 registry 默认 placement；默认 placement 只用于新卡片、缺失卡片和显式重置布局。
- 单列模式只做视觉 projection，不覆写用户的双列持久化 column。

### 6.5 卡片视觉

- Surface、Border、HoverBorder、FocusBorder、DropBorder 分别取独立 token。
- `AlwaysShowBorders=false` 表示普通态无边框；hover/focus 是否显示交互边框由交互 token 决定，不影响 Surface。
- `AlwaysShowBorders=true` 表示普通态持续绘制用户选择的 BorderColor。
- BorderColor 不得混入 SurfaceColor。
- 边框采用“完整外层圆角 + 内缩 Surface”的单一 ring 几何；focus/hover 只换颜色，不改变内缩量和 radius。
- 圆角细分不作为用户设置项。渲染层使用固定质量或按物理半径自适应并缓存 geometry；不能用提高每帧细分数换取肉眼不可见的差异。
- 稳定展开卡片不施加 card-content clip，只受页面外层 viewport 裁剪；折叠、展开/折叠动画和动态高度动画期间才裁剪到当前动画 ContentRect。

## 7. 卡片动画、闪烁与稳定帧

### 7.1 正确状态流

```text
收集稳定状态
  → 解析 layout revision
  → 生成目标 layout snapshot
  → 解析当前动画高度
  → 用动画底边排布同列后续卡片
  → 整个 Deck 统一 entry transform
  → 每张可见卡片只绘一次 chrome 和内容
```

### 7.2 动效规则

| 场景 | 允许变化 | 禁止变化 |
|---|---|---|
| 首次进入/page 切换 | Deck 统一短距离 Y 位移 | Surface alpha、Surface 色、逐卡错峰 |
| 展开/折叠 | content height，后续卡片位置 | 两张半透明卡片重叠、旧高度先绘 |
| 显示模式切换导致列表 1→8 行 | content height | 瞬间裁剪、列表只剩一行 |
| 拖拽 | proxy 位置、drop indicator | 源卡片和 proxy 使用不同 geometry |
| drop 完成 | 被拖卡片短边框反馈 | 普通 reflow 全卡片背景亮一下 |
| hover/focus | 边框颜色 | Surface geometry/alpha |

`RenderOnly` 只允许遍历稳定内容以建立文本/布局缓存，不绘制 card chrome，不改变折叠、配置、输入、资源或动画状态。

Deck definitions cache 的身份必须是 `{tab stable key, definitions revision}`。同一个 Deck、同一个数值 revision 切换到另一个 Tab 时仍必须调用该 Tab 的 builder。

### 7.3 稳定帧性能契约

稳定页面在 revision 未变化且动画结束后：

- definitions rebuild = 0
- card measure = 0
- height/reflow/entry animation resolve = 0
- 不重新生成页面 card vector/lambda/std::function
- 只处理 viewport 可见卡片和列表项
- 不执行预热、offscreen skin drain 或隐藏页面绘制（当 `qm_settings_prewarm=0`）

该性能契约用于防止 UI 自己产生持续开销，但不再把不同图形后端的 FPS 差异错误归因于设置页限帧。

## 8. 滚动体系

### 8.1 当前 profile 矩阵

| Profile | 当前尺寸/行为 | 目标用途 |
|---|---|---|
| `SETTINGS_OUTER` | MEDIUM 固定 1.0：槽位 20、margin 5、可见轨道约 10；始终预留；wheel scale 120 | 设置页最右侧外层滚动 |
| `SETTINGS_INNER` | SMALL：宽 8–10、margin 1–2；按 row extent × 默认 2 行滚动 | 卡片内部普通列表 |
| `SETTINGS_GRID` | SMALL、可见 rail、禁止 content drag；按网格行滚动 | 国旗、皮肤、Tee7 部件网格 |
| `POPUP_LIST` | SMALL；最多 8 项；按 row extent × 3 行滚动 | Dropdown popup |
| `MENU_LIST` | MEDIUM；按行滚动 | 非设置普通菜单列表 |
| `FILTER_GRID` | SMALL + hidden rail | 明确设计为隐藏轨道的筛选网格，不得替代 SETTINGS_GRID |

### 8.2 当前现状评价

- 外层 20/5/10 和固定逻辑尺寸已经落实。
- `SETTINGS_GRID` 当前不是 hidden rail，符合最新反馈；旧 active spec 中的 hidden-rail 描述已经过时。
- `CQmScrollState` 已承载 offset/动画/drag 状态，`QmResolveScrollPolicy` 已集中 profile 规则。
- 仍存在 `CScrollRegion` 与 `CQmScrollController` 两种生命周期 adapter；它们可以共享 state 类型，但 ownership、clip、注册顺序和渲染 rail 的入口尚未统一为一个组件接口。
- `SETTINGS_PAGE` 与 `SETTINGS_OUTER` 是同义 profile，应删除前者或仅保留编译期 deprecated alias。
- `SQmSettingsCardStyle` 仍携带 scrollbar width/margin，而 Card shell 本身不应拥有页面滚动视觉。这是职责泄漏，应拆除。

### 8.3 overflow 与槽位规则

- 外层设置页：始终保留 20，避免卡片列宽随 overflow 跳变；无 overflow 时 rail 可不绘制。
- 内部列表/网格/popup：无 overflow 时不绘制也不预留 rail；有 overflow 时才从内容 viewport 扣除轨道空间。
- 任何列表 viewport 高度只包含完整行；8 行高度为 `8 × RowHeight + 7 × RowSpacing`（如果 row extent 已包含 spacing，则直接为 `8 × RowExtent`），不得重复追加 spacing 形成半行。
- 内部组件获得 wheel ownership 后，外层页面同帧不得消费同一份 raw wheel。打开的 popup 在指针位于其区域内时始终阻止背后页面响应滚轮；是否可滚动、是否显示 rail、是否阻止底层交互是三个独立状态。

## 9. Dropdown 完整设计

Dropdown 是本轮最重要的防回归对象。以后不能把 trigger、popup、scroll 和父页面分别修补。

### 9.1 当前已确认存在且可复用的实现

- 生产链路已有 `DoSettingsDropDown → CUi::DoDropDown → ShowPopupSelection → PopupSelection`。
- `QmResolveDropdownPopupPolicy` 使用最多 8 个可见项目。
- `POPUP_LIST` 按 3 行滚动。
- active item 使用整行背景高亮。
- 只在打开时或键盘 active index 真正改变时请求 ScrollHere；普通重绘、滚轮和 rail 拖动不会每帧把 offset 拉回 active item。
- popup 可根据下方空间在 anchor 上方翻转，并限制在 popup viewport 内。
- 长列表或 viewport 限高造成 overflow 时，popup 会注册更高优先级 wheel owner。
- trigger 和 popup 共享同一个解析后的设置 Body 字号和视觉 style。

### 9.2 当前结构性风险与本轮收口状态

#### P1：设置页入口仍不是唯一（已于本轮收口）

Controls joystick 已迁移到 `CMenus::DoSettingsDropDown`；迁移 gate 会扫描设置业务源并拒绝新的 `Ui()->DoDropDown(...)` 直连。

#### P1：viewport 有隐式和显式两套来源（公共入口已收口）

设置 wrapper 现在同时补齐 AnchorViewport 和 PopupViewport：anchor 使用当前内容 clip，popup 使用最外层设置页 clip；调用方显式传入时仍保留覆盖能力。后续仍应把这两个 viewport 收进类型化 context，避免通用非设置调用误用。

#### P1：打开帧与后续帧的 anchor 判定不一致（已于本轮收口）

popup geometry 与后续生命周期现在都使用 `QmDropdownAnchorFullyVisible`。部分离开所属容器的 trigger 不会先显示一帧错误 popup。

#### P1：popup 生命周期依赖源控件继续渲染（已于本轮收口）

打开的 Dropdown 每帧把当前 `PerfFrame` 写入 popup properties；popup manager 在正式绘制前检查 source frame。源卡片滚出、折叠、切 Tab 或当帧不再渲染时，旧 popup 会主动关闭，不再停留在上一帧位置。

#### P1：默认 CScrollRegion 被多个 Dropdown 共享（已于本轮收口）

函数静态默认滚动状态已删除。每个 `SDropDownState` 延迟拥有独立 `CScrollRegion`，并稳定记录调用方注入的外部 region；context reset 后再次打开仍恢复同一 region。

#### P1：短 popup 的 wheel 隔离语义错误（已于本轮收口）

`Scrollable` 与 `BlockUnderlying` 已拆分。`<=8` 项且无 overflow 时不显示 rail，但指针位于 popup 内仍由 popup 拥有 wheel，页面外层不会滚动；`>8` 项或 viewport 限高时才显示 rail。

#### P2：popup context reset 仍需维护字段白名单

打开时仍会在 `Reset()` 前后保留 scroll region 和 special-font flag。滚动 region 已由 `SDropDownState` 稳定绑定，修复了重开丢失；但新增需要跨 reset 保留的字段时仍须更新这条白名单，后续可用专用 presentation state 替代。

#### P2：测试层次仍不完整

现有测试覆盖 policy、geometry、state machine、wheel router 和 dragged offset 的纯逻辑，但部分 `qm_new_ui_menu_branch_test` 仍是源码字符串断言。缺少真实连续帧 CUi/CScrollRegion 组件测试来证明所有调用点行为相同。

### 9.3 目标公共接口

设置页只能调用一个类型化入口：

```cpp
struct SSettingsDropDownContext
{
    CUIRect m_TriggerRect;
    CUIRect m_AnchorViewport; // trigger 必须完整位于其中
    CUIRect m_PopupViewport;  // popup 必须完整限制于其中
    SSettingsDropDownState &m_State; // 独占 scroll state
    std::span<const SDropDownEntry> m_Entries;
    int m_CurrentIndex;
    SSettingsContentMetrics m_Metrics;
    SSettingsDropDownPolicy m_Policy;
    uint64_t m_PageGeneration;
};

int DoSettingsDropDown(const SSettingsDropDownContext &Context);
```

约束：

- `AnchorViewport` 和 `PopupViewport` 必填，不能从 clip stack 隐式猜。
- 每个 `SDropDownState` 自己拥有 `CQmScrollState/CScrollRegion` 所需状态，不共享函数静态对象。
- state 记录 anchor 的 last-seen frame/page generation；popup manager 每帧验证 liveness，不能依赖源 renderer 一定执行。
- popup policy 固定定义 max rows、row extent、spacing、chrome、wheel step 和 rail profile；页面不能覆盖其中一部分。
- 设置页禁止直接调用 `Ui()->DoDropDown`；非设置页保留通用 API。
- `Reset()` 由 state 自己完整重置 transient fields，不能由调用者保存/恢复内部指针。

### 9.4 Dropdown 状态机

```text
CLOSED
  ├─ trigger click + entries > 0 + anchor fully visible → OPENING
  └─ otherwise → CLOSED

OPENING
  ├─ build policy/geometry
  ├─ initialize active index
  ├─ scroll active item into view once
  └─ → OPEN

OPEN
  ├─ wheel/rail drag → keep user offset
  ├─ Up/Down → update active + scroll active into view
  ├─ Enter/click item → SELECT + CLOSE
  ├─ Escape/trigger click/click outside/disabled → CLOSE
  ├─ anchor not fully inside AnchorViewport → CLOSE
  ├─ anchor not refreshed in current frame → CLOSE
  ├─ page/tab generation changed → CLOSE
  └─ popup viewport unusable → CLOSE

CLOSE
  ├─ release wheel owner
  ├─ release popup capture
  ├─ clear transient active/selection/drag
  └─ → CLOSED
```

### 9.5 Dropdown geometry 和 UX

- trigger 与 popup 宽度默认一致；特殊 picker 若需更宽必须使用显式 policy，不得偷偷修改 X。
- 默认先放在 trigger 下方，Y = `trigger.bottom + gap`；空间不足才放上方。
- popup 四周保留统一 viewport margin。
- popup 在上方时只保留下侧相邻圆角，在下方时只保留上侧相邻圆角；trigger 与 popup 视觉连接但不能重叠。
- popup 不以 trigger 中线定位。
- `<=8` 项：显示实际项数对应完整行，无 overflow 时无 rail；指针位于 popup 内的 wheel 被吞掉，不移动父页面。
- `>8` 项：精确显示 8 个完整行，显示 rail。
- viewport 限高不足 8 行：只显示能容纳的完整行，显示 rail；不能显示半行。
- 当前项、hover 项使用统一的整行背景层级；当前项不得额外画左 bar。
- 鼠标拖 rail 后 offset 保持；不得因 active item 每帧 ScrollHere 自动回顶部。

### 9.6 Dropdown 必须新增的行为测试

1. 8 项打开：8 个完整 row、无 rail；指针在 popup 内时父页面不能消费 wheel，指针离开后可以。
2. 9 项打开：8 个完整 row、有 rail、popup 消费首个 wheel、父页面 offset 不变。
3. popup viewport 只能容纳 5 行：5 个完整 row、有 rail、无半行。
4. 下方不足、上方足够：popup 翻到上方且不与 trigger 重叠。
5. 左右屏幕边缘：X 被限制在 viewport，entry hit rect 与 draw rect 相同。
6. 滚轮滚到底部后连续 3 帧：offset 不回跳。
7. 拖 rail 到底部后连续 3 帧：offset 不回跳。
8. 键盘切换 active：只在 active 改变时自动滚动。
9. anchor 部分离开所属 card clip：同一帧关闭并释放 wheel owner。
10. popup 打开后父页面因动画移动：popup 跟随最新 trigger rect；trigger 不完整可见时关闭。
11. 两个不同 Dropdown 依次打开：scroll offset、active、drag 状态互不继承。
12. Controls、Graphics、TClient Status Bar 通过同一个设置 wrapper 执行上述共享测试。
13. popup 打开后源卡片完全滚出、renderer 被跳过：popup manager 在同一帧因 anchor liveness 失效而关闭。

## 10. 列表、网格和复合滚动组件

### 10.1 普通列表

- 设置卡片内部列表使用 `SETTINGS_INNER`，显式 row extent 和 wheel owner priority。
- 标准 viewport 最多 8 行；业务明确需要固定 8 行（如音频包）时即使项目较少也可保留空列表区域，但必须由该组件语义说明，不得由错误高度公式偶然产生。
- 动态列表（如 Graphics 显示模式）按实际项目数 1–8 行变化，并驱动卡片高度动画。
- 列表选择项整行高亮；文本使用 Body；badge 使用 Small/BadgeHeight。
- 两组同等重要且短的列表可在宽卡片内两列排列；窄宽度降级为上下排列。

### 10.2 国旗、皮肤和 Tee7 网格

- 使用 `SETTINGS_GRID`，overflow 时显示 rail。
- grid 获得 wheel owner 后阻止外层页面滚动。
- 列数由可用宽度、最小 cell 宽度和 gap 解析，不手写固定列数覆盖极窄窗口。
- viewport 高度只包含完整 grid row。
- 搜索/排序工具栏与 grid 之间保留 SectionGap。
- 国旗 picker 是 popup 语义时，wheel owner priority 为 POPUP；嵌入 Tee 页面时为 COMPOSITE_CONTROL。

### 10.3 CListBox 与 CScrollRegion

二者可以暂时保留为 adapter，但必须消费同一个 resolved policy 和 wheel ownership API。禁止组件内部再根据 `MENU_LIST/MEDIUM` 默认值猜设置页语义。

## 11. 输入、数值、选择与颜色组件

### 11.1 InputField

设置页只允许显式 `SInputFieldOptions` 入口：

- Body 字号；placeholder 与输入文本相同字号。
- 固定 InputHeight。
- 搜索图标、清除 X 占独立正方形 slot。
- focus 只画外框，不改变背景或内容 rect。
- 相邻输入行保留 LineSpacing。
- 无效输入、IME、selection、readonly 和 commit policy 由组件管理。

简写 overload 仅保留给非设置兼容调用；migration gate 禁止设置页使用。

### 11.2 NumericField

统一布局：

```text
[optional checkbox] [label] [slider] [input] [optional unit]
```

宽度不足时通过公共 resolver 降级为两行：第一行开关/标签，第二行 slider/input；页面不能自行把 input 放到 slider 上方。slider 轨道缩短时必须仍满足最小可操作宽度。

### 11.3 Checkbox 与 Radio

- checkbox icon 与 Body 文本垂直居中。
- radio row 使用 `ResolveSettingsRadioRowLayout`；宽时标签和按钮一行，窄时选项整体下一行。
- “界面动效强度”和它的多选按钮按同一 radio row；“额外动画”是下一独立行，不强行并行。
- 动态开关显示更多内容时必须更新 layout revision 和 card snapshot。

### 11.4 ColorPicker

- 行高、label、preview swatch、reset 按钮、alpha 由同一个 metrics-aware API 管理。
- 带 alpha 的颜色直接使用颜色选择器 alpha 通道，不再额外增加重复 opacity 行。
- reset 按钮统一 ButtonHeight；颜色 swatch 与按钮同高。
- BorderColor、UiColor、BrowserColor、ScoreboardColor 等必须绑定各自独立配置，点击 hit rect 与 swatch rect 同源。
- 旧 scalar geometry overload 从设置页禁用，并在迁移完成后移出公共头文件。

## 12. Tab、按钮、badge 与预览组件

### 12.1 Tab

- 主 Tab 和子 Tab 使用同一高度 `26`、同一字体 token 和同一内边距。
- 主 Tab 可以竖排，子 Tab 可以横排，但 active/hover/focus 状态样式同源。
- 页面不能私自缩放 Tab 高度或字号。
- Tab 切换只开始一次 display cycle；不能先渲染旧 Tab 再更新 active。

### 12.2 Button 与 Reset

- 普通按钮统一 ButtonHeight 和 Body 字号。
- Reset 按钮不得存在“大/小两种标准”；紧凑只允许通过同一个 scale 解析。
- destructive 操作颜色独立，不通过尺寸区分。
- 预览下方按钮使用标准行距和等高布局。

### 12.3 Badge

- 使用 Small 和 BadgeHeight。
- badge 高度不撑满列表 row；上下相邻 row 之间仍保留行距。
- 数量 badge 与文本右对齐，但必须给 rail 留出安全宽度。

### 12.4 Preview

预览不是普通设置行，必须有明确 semantic geometry：

- 预览内容在 preview rect 中居中。
- 每个预览条目可使用全宽、轻量背景，以形成稳定层次。
- Tee preview 为名字、Tee 本体、参数、按钮保留独立区域；不能把 Tee 压缩进普通一行。
- preview 高度、行数、字体或资源尺寸改变时进入 layout revision。
- preview 和 card content 使用同一 snapshot，不能只调绘制而不调测量。

## 13. 页面级布局规则

### 13.1 General / Graphics / Sound

- Graphics Display 卡片包含窗口模式和显示模式列表；列表按模式实际数量 1–8 行，卡片平滑改变高度。
- 图形后端与 GPU 选择按单行 `label + dropdown` 合并到合理分组，不为一个选择器单独制造过宽卡片。
- Sound 音频包列表使用固定 8 行 viewport；数量 badge 不占满行高。
- General 语言/主题列表使用精确完整行；不能出现第 8 行后多半行。

### 13.2 Tee

- Player preview 左列、Skin options 右列、Skin search/list 全宽是默认宽屏 placement。
- 上方两张卡片目标等高应由 placement/group layout 明确表达，不能靠填充无意义空白。
- Skin search 与数据库操作区之间保留 SectionGap。
- Skin queue 的 toolbar/预设栏使用标准列表行和 Body 字号；功能区可放在皮肤列表右侧，窄屏降级到下方。
- country Dropdown、skin list、queue list 各自拥有滚动状态，不把 wheel 泄漏到外层。

### 13.3 Appearance

- Messages、Nameplate、Hook、Laser 按功能拆卡，但每张卡使用 snapshot。
- 消息颜色/渐变行使用统一 Body 和 color row；删除冗余重复控制。
- 输入 focus ring、说明和下一行必须有安全间距。
- 预览卡片内容居中、全宽背景、下方按钮标准间距。

### 13.4 Controls

- controller disabled/enabled/device missing/relative/absolute/axis count 都是 layout state。
- 开启手柄后卡片必须按真实行数增加高度。
- Controls 禁止直接调用通用 Dropdown；统一设置 wrapper。
- 页面不得同时出现私有折叠按钮和 Deck 默认按钮。

### 13.5 TClient

- 敌对列表保持一个完整功能卡片，不按内部三个列表拆成互不关联卡片。
- Profiles 的 Actions、Options、Saved Profiles 保持独立卡片，窄屏纵向排列。
- Status Bar 默认 placement：Settings 左 0、Items/Codes 右 0、Preview 左 1；用户持久化优先。
- Status Bar popup 必须使用独立 scroll state，能滚到底且不自动回顶；状态码注册表与 popup entries 同源。
- 所有 TClient 行使用相同 metrics，禁止私有 12/20/24/25 字号或固定 300/320 宽度切分。

### 13.6 QmClient HUD/Visual/Function

- Notifications、Coords、PlayerStats、InputOverlay、DummyMiniView、Voice、Lyrics、Background3D 等动态卡片必须各自有 layout state + snapshot。
- 诊断/运行时错误文本不能改变卡片高度；预留稳定状态区并显示中性“状态正常”。
- QmClient 现有自定义 collapse header action 迁回 Deck。
- 栖梦页面作为交互密度和动态高度行为参考，但不保留私有实现入口。

### 13.7 Assets / Contributors / Search

- Assets 显式使用 SETTINGS_OUTER，不依赖 CListBox 默认 profile 充当页面标准。
- Contributors 和 Search 使用同一 shell metrics、外层滚动和 Tab 尺寸。
- Search 只索引 registry 的唯一标题/副标题/关键词，不复制卡片视觉和布局。

## 14. 当前 Findings（按严重度）

### P1

1. **动态卡片 measure/render 不是同一 geometry snapshot。** 手写 float 高度和独立 rect 切分仍能造成内容越界、留白和展开不增高。
2. **真实组件级回归测试仍未覆盖最终像素和完整客户端事件循环。** 当前行为测试覆盖 Dropdown policy/geometry、anchor liveness、wheel owner、拖动 offset、Card clip/cache/collapse；客户端实际 clip stack、popup 输入顺序和最终视觉仍由人工验收确认。

本轮已经关闭的原 P1：设置页 Dropdown 直连、打开帧 anchor 判定、source liveness、短 popup wheel 泄漏、静态默认 CScrollRegion、完整行限高、上下空间选择、stable viewport clamp、稳定卡片永久 clip、Deck 缓存缺 Tab、边框颜色污染 Surface、Controls/QmClient 自定义折叠接线。

### P2

1. `m_MeasureEachFrame` 仍是公开正式字段，允许重新引入稳定帧测量。
2. `SettingsPageLayout.h` 混入大量页面业务高度公式，公共层责任过宽。
3. `SETTINGS_PAGE/SETTINGS_OUTER`、多组 InputField/ColorPicker/Checkbox overload 和 Card header action 仍保留同义路径。
4. 卡片 style 仍携带 scrollbar width/margin，卡片视觉和页面滚动职责未完全解耦。
5. 副标题没有唯一性/语义 gate，重复 fallback 不会失败。
6. 普通卡片折叠状态按 stable ID 保存在 Deck session map；QmClient/Controls 仅保留旧业务状态写回 callback，不再拥有自定义 header action。
7. 网格/list/popup 的 helper 测试较多，但缺少 renderer/adapter 实际 viewport 与 hit rect 一致性测试。
8. 旧 active spec 与后续反馈冲突，特别是 grid hidden rail 和 anchor 关闭规则；若不由本文覆盖，后续代理会继续按旧文档改回去。

### 已排除或不应误报

- 外层设置滚动条宽度当前已经统一为 20/5/约10，不应再通过修改全局 SMALL/MEDIUM 解决。
- `SETTINGS_GRID` 当前会在 overflow 时显示 rail；hidden rail 只属于 `FILTER_GRID`。
- active Dropdown 项当前已是整行背景，不是左侧 bar。
- Dropdown 当前已避免普通重绘/rail drag 每帧 ScrollHere；“仍会自动回顶”若在客户端复现，应优先检查调用点 state 被重建或 context 被 Reset，而不是再次修改 active-item policy。
- RenderOnly 重复绘制 card chrome 的旧首帧亮闪根因已有保护；新的闪烁若复现，应检查重复 shell、Surface/Border 耦合、display cycle 或旧/新 frame 同帧绘制，不能笼统归因于 hover。
- 8/9 项高度 policy、popup 从 anchor 底部定位、空间不足向上翻转、Graphics 显式 popup viewport 当前未发现计算回归。

## 15. 历史回归根因分类

| 根因 | 典型症状 | 防回归措施 |
|---|---|---|
| 公共 wrapper 旁保留直接入口 | 某页面 Dropdown、字体、滚动行为不同 | 设置页编译/gate 禁止直接旧入口 |
| 隐式 clip/default profile | popup 位置随页面不同、wheel 给外层 | context 必填 viewport/profile/owner |
| float measure + 独立 render | 卡片留白、内容溢出、展开不增高 | geometry snapshot 同源 |
| 共享/静态 transient state | 一个下拉框继承另一个的 offset | 每实例拥有 state |
| 源码字符串测试替代行为测试 | 代码含某行但实际连续帧仍坏 | headless component harness |
| 旧规格未标出被覆盖条款 | 后续代理按旧 hidden rail/anchor 规则改回 | 新文档明确 authority 和冲突表 |
| 页面私有视觉复制公共样式 | 看似一样，后续一处改动另一处不跟 | 删除私有接线，不只统一颜色值 |
| 视觉反馈改变 Surface/geometry | 动效开启时背景亮闪、边框缺角 | 状态只改变 border color/transform |

## 16. 目标架构与修改建议

### P0：冻结行为契约并修正文档权威关系

- 将本文作为设置 UI/UX 最新行为来源。
- 在旧 2026-07-10 active spec 顶部增加“滚动网格、Dropdown anchor、栖梦参考标准已被本文覆盖”的 banner。
- 建立组件验收矩阵，不再从截图文字临时恢复需求。

### P1：Dropdown 收口（本轮公共回归已完成，类型化 context 待后续）

- 已完成：设置页全部调用 `DoSettingsDropDown`，Controls 直连已迁移，gate 禁止新直连。
- 已完成：wrapper 同时解析 AnchorViewport 和 PopupViewport。
- 已完成：每个 state 独立拥有默认 scroll region，并稳定保留调用方注入的外部 region。
- 已完成：popup manager 使用 source frame liveness，源卡片不再渲染时主动关闭。
- 已完成：拆分 Scrollable 与 BlockUnderlying；短 popup 也阻止底层 wheel。
- 已完成：打开帧和后续帧统一 fully-visible 判断。
- 待后续：把现有 properties 参数收紧为类型化 `SSettingsDropDownContext`，并增加完整客户端事件循环 harness。

### P2：Card layout snapshot 与折叠收口

- 引入 `SCardLayoutSnapshot` 和无副作用 resolver。
- 已完成：`RenderCached` 缓存键纳入 Tab stable key。
- 先迁所有动态卡片，再迁固定行卡片。
- 删除设置页 `m_MeasureEachFrame` 用法；字段改为 debug assert/迁移期内部开关。
- 已完成：折叠输入、状态、按钮完全进入 Deck；页面只提供默认值和状态写回 callback，不再自绘相同按钮。
- 已完成：Surface/Border token 分离，删除 linked surface color。
- 已完成：稳定展开卡片关闭 content clip；只在高度动画期间裁剪。
- 已完成：默认 collapse session state 以 stable ID 保存，页面/Tab 切换后保持一致。

### P3：表单与列表 API 收口

- 设置页只允许显式 metrics options 的 InputField、NumericField、ColorPicker、Radio、Checkbox。
- 内部列表显式 SETTINGS_INNER/GRID/POPUP_LIST。
- 统一完整 8 行 viewport helper，并让生产组件直接使用其 geometry。
- 副标题、preview 字号例外改为 stable semantic allowlist。

### P4：逐页迁移与视觉债清理

- 按 General/Graphics/Sound → Tee → Appearance/Controls → TClient → QmClient 的顺序迁移。
- 每页删除旧入口后再标完成；不能“公共 wrapper + 旧实现”并存。
- 逐页记录用户视觉验收结果，但不让视觉 gap 阻止行为测试先落地。

### P5：性能、gate 与最终 review

- 稳定帧 diagnostics 断言 measure/animation/definition rebuild 为 0。
- 扩展 migration gate。
- 全量构建/测试/gate 后由独立只读 review 检查实际 diff。

## 17. 测试策略

### 17.1 测试分层

| 层级 | 能证明 | 不能证明 |
|---|---|---|
| 纯函数 | 尺寸公式、policy、状态机、projection | 实际 clip、输入顺序、绘制重叠 |
| 组件 headless harness | 连续帧输入、wheel owner、scroll offset、chrome 次数、geometry | 最终像素观感 |
| 源码/gate | 旧入口没有重新出现、必需 API 被调用 | API 被正确调用、最终行为正确 |
| game-client 构建 | 编译和链接完整 | 交互正确 |
| 用户人工验收 | 最终视觉和手感 | 自动化长期防回归 |

### 17.2 必须保留/补齐的测试

#### 页面与 metrics

- 640×480、800×600：单列、无负宽度、无横向 overflow。
- 1280×720：双列 placement 和全宽卡片规则。
- 1920×1080、2560×1440、3840×2160：内容不超过 1000，壳层整体居中。
- 外层槽位恒为 20，实际 rail geometry 为约 10，所有页面一致。
- Tab/SubTab 高度、字体和 gap 相同。

#### Card

- 每个动态状态 snapshot consumed height == card content height。
- 展开/折叠全过程同列 card rect 不重叠。
- display/hit/drag/proxy rect 同源。
- 首帧 RenderOnly chrome count = 0；正式可见帧每卡 chrome count = 1。
- entry/height/reflow 中 Surface RGBA 不变。
- hover/focus/border on/off 中 Surface geometry 不变。
- 稳定展开卡片 ClipContent=false；高度动画期间为 true；focus ring 不被 card content rect 截断。
- 同一 Deck、同 revision 从 Tab A 切到 Tab B 时 B 的 builder 必须执行且不含 A stable ID。
- 动态高度动画结束后 measure/animation resolve 归零。
- collapse button 对所有普通卡片生效；不可折叠卡片不显示按钮。
- 写文件后用全新 order model 加载，column/order/stable ID 保持。

#### Dropdown

- 使用 9.6 节的 13 项真实行为矩阵。
- 特别覆盖“drag rail 后不回顶”“第一个 wheel 不泄漏”“anchor 部分离开同帧关闭”“两个 Dropdown 状态隔离”。

#### Scroll/List/Grid

- 8 行无半行；9 项 overflow 显示 rail。
- Grid overflow 显示 rail并消费 wheel；无 overflow 不占槽。
- 内部 owner 优先于外层 owner，raw wheel 只消费一次。
- 动态列表 1↔8 行触发 card height animation。

#### Forms

- placeholder/Body 字号受 InputHeight 限制且不越界。
- focus ring 不覆盖下一行。
- reset/color swatch 同高。
- alpha color picker 不生成额外 opacity 行。
- responsive radio 和 numeric field 只在宽度不足时换行。

### 17.3 migration gate 新规则

- 设置页面禁止 `Ui()->DoDropDown`。
- 设置 Dropdown 必须显式提供 anchor/popup viewport。
- 禁止设置页面使用简写 InputField 和 scalar ColorPicker overload。
- 禁止页面自绘 `RenderSettingsCardCollapseButton`。
- 禁止普通业务卡片设置 `m_MeasureEachFrame=true`。
- 禁止 `SETTINGS_PAGE` 新调用，只允许 SETTINGS_OUTER。
- 禁止 Card style 携带 scrollbar width/margin。
- 每个 subtitle 为空或唯一；禁止通用 fallback 重复超过一次。
- 裸字号例外按 stable semantic ID + 注释登记，不按宽泛源码片段放行。

## 18. 人工 UI/UX 验收矩阵

人工验收由用户执行，结果应记录为 pass/fail/gap，不用截图作为本代理的自动证据。

| 场景 | 检查点 |
|---|---|
| 每个设置主页面首次进入 | 无背景亮闪；Tab 大小一致；外层 rail 宽度一致 |
| 动效关闭/精简/完整 | Surface 不闪；差异只在允许的位移/高度时长 |
| 展开/折叠 | 按钮统一且有作用；下方卡片不重叠；无收起留白 |
| Graphics 模式切换 | 1–8 行高度平滑变化；8 行完整；rail 正确 |
| Tee country/skin/queue | wheel 属于内部组件；外层不动；搜索和功能区有间距 |
| TClient Status Bar Dropdown | 能滚到底；拖 rail 不回顶；当前项整行高亮 |
| 控制器开启/关闭 | 卡片高度跟随真实内容；无文字重叠 |
| ColorPicker | 点击有效；alpha 内置；reset 尺寸一致；边框色不改背景 |
| 极窄窗口 | 单列、控件纵向降级、无负宽度/遮挡 |
| 4K | 内容不无限变宽；字号/轨道仍是逻辑尺寸；整体居中 |

## 19. 完成定义

本设计只有在以下条件全部满足时才能称为实现完成：

1. 设置页 Dropdown、Collapse、Input/Color、Scroll profile 各只有一个正式入口。
2. 所有动态卡片使用 layout snapshot，measure/render 不再分叉。
3. 本文列出的 P1/P2 结构 gap 已删除或有明确非设置页豁免。
4. 真实组件行为测试覆盖 Dropdown、Card animation、wheel ownership 和动态高度，不只依赖源码字符串。
5. migration gate 能阻止旧入口和私有复制重新进入。
6. `game-client`、`testrunner`、全量 C++ tests、settings migration gate、quick/default gate 按仓库规范串行通过。
7. 独立只读 review 无未解决 P0/P1 finding。
8. 用户完成客户端视觉/交互验收；未验收项明确写为视觉 gap，不能写“已通过”。

## 20. 非目标

- 不修改协议、物理、预测、Demo/地图/配置文件格式。
- 不统一聊天/控制台文本滚动。
- 不为了 UI 收口重写整个 DDNet 通用 UI。
- 不通过全局修改 `MENU_LIST/SMALL/MEDIUM` 影响其他页面。
- 不增加后台预热作为视觉或性能修复。
- 不用提高圆角细分、重复绘制或隐藏 offscreen pass 掩盖布局问题。

## 21. 代码证据索引

下表用于后续实现和 review 快速定位，不代表仅修改这些行即可完成设计：

| 主题 | 当前代码证据 |
|---|---|
| 最大内容宽度、外层槽位、Tab/Row/Card token | `src/game/client/QmUi/UiTokens.h:71-88` |
| 内容 metrics 与 UiScale | `src/game/client/QmUi/SettingsPageLayout.h:140-172` |
| 页面 inset、双列与有效 scroll viewport | `src/game/client/QmUi/SettingsPageLayout.h:674-728` |
| 壳层整体居中、最大 1000、扣 20 槽位 | `src/game/client/QmUi/SettingsPageLayout.h:731-762` |
| 外层/内部/网格/popup profile | `src/game/client/QmUi/QmScroll.cpp:154-202` |
| Dropdown policy、geometry、anchor helper | `src/game/client/QmUi/QmDropdown.cpp:15-94` |
| Dropdown 生产状态、每实例 scroll region 与 source liveness | `src/game/client/ui.cpp:2273-2338`、`src/game/client/ui.cpp:2653-2805` |
| Popup list、wheel owner 与 active row | `src/game/client/ui.cpp:2501-2642` |
| 设置 wrapper | `src/game/client/components/menus.cpp:547-556` |
| Controls 通过设置 wrapper | `src/game/client/components/menus_settings_controls.cpp:708-711` |
| Card Surface/Border/RenderOnly/collapse 绘制 | `src/game/client/QmUi/SettingsCard.cpp:41-137` |
| Deck display cycle、measure cache、height/reflow/entry | `src/game/client/QmUi/SettingsCardDeck.cpp:115-697` |
| definitions 缓存入口 | `src/game/client/QmUi/SettingsCardDeck.h:66-78` |
| 高度动画期间 content clip 决策 | `src/game/client/QmUi/SettingsCardDeckLogic.cpp:275`、`SettingsCardDeck.cpp:570` |
| 纯布局/Dropdown/scroll policy tests | `src/test/QmAnimTest.cpp` |
| Card 动画/顺序/折叠逻辑 tests | `src/test/settings_card_deck_logic_test.cpp` |
| 页面生产接线源码契约 tests | `src/test/qm_new_ui_menu_branch_test.cpp` |
| 设置迁移 gate | `qmclient_scripts/gate/check_settings_ui_migration.py` |

## 22. 最终结论

当前代码已经具备一套相当完整的统一设置 UI 基础设施，页面最大宽度、外层滚动条、metrics、Card Deck、scroll profile 和 Dropdown policy 都不是从零开始。真正未收口的是“唯一入口、唯一状态 owner、唯一 geometry snapshot 和真实行为测试”。

后续最优先的工作不是继续调某张卡片的 `+5/-10` 高度，而是先把 Dropdown 的 viewport/state 生命周期和 Card 的 measure/render/collapse 双路径收成单一组件契约。只要这些逃生口仍存在，历史上已经满意的 Dropdown、折叠、行距和动态高度仍可能被下一轮页面修改重新带回最初的 bug。

## 21. Mac 交接验收

本节是 Windows 收尾后的可执行交接清单。最终提交和推送完成后必须补齐 commit hash 与远端一致性结果，未填写时不得声称 Mac 已可直接接手。

- 分支：`dyl_dev`
- 目标远端：`mac-origin` -> `https://github.com/wuqie-xuanzhao/QmClient.git`
- 最终提交：以包含本文的 `dyl_dev` 提交为准；在交接机器上使用 `git rev-parse mac-origin/dyl_dev` 取得不可歧义的 hash。
- 远端一致性：推送后执行 `git rev-list --left-right --count HEAD...mac-origin/dyl_dev`，必须得到 `0 0`；`mac-origin/master` 同步指向同一提交，保证默认克隆可直接取得本轮代码。
- Mac 拉取：`git fetch mac-origin && git switch dyl_dev && git reset --keep mac-origin/dyl_dev`
- 首次构建：使用独立的 macOS build 目录，按 `.agents/skills/qmclient-verification-gate/SKILL.md` 的 Linux/macOS 命令配置 Ninja Release，不复用 Windows `cmake-build-release`。
- 人工验收：Dropdown 8/9 项、popup 上下翻转与滚轮归属；Controls 搜索展开；QmClient 折叠持久化；卡片边框颜色与背景解耦；展开/折叠和入场无背景亮闪。
