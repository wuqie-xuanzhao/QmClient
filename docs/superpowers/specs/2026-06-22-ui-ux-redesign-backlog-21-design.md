# QmClient UI/UX 重构设计规格（Backlog #21）

- **状态**：active
- **日期**：2026-06-22
- **范围**：设置页 11 tab 重组、顶部导航左右分栏、可复用组件库补全、图标库 MSDF、动画系统完善
- **继承**：扩展 `2026-06-13-ui-ux-apple-redesign.html`（v4 spec），复用其主题系统/卡片布局/动画基础设施
- **硬约束**：所有改动仅限 `m_QmNewUi != 0` 新 UI 路径，旧 UI 不动；不引入运行时第三方库；不改 DDNet 上游协议/物理/预测

## 0 · 设计哲学

本设计是 QmClient 新 UI（`m_QmNewUi`）的**结构性重组**，解决三个核心问题：

1. **设置页 tab 过多且语义混乱**：当前 15 个扁平 tab + QmClient 5 子 tab，用户难以定位设置项。重组为 11 个语义清晰的分组。
2. **顶部导航栏缺乏左右分区**：当前"服务器浏览"和"工具操作"混在一起。明确左右对齐分组。
3. **组件库不完整**：缺少搜索框、下拉框、分段控件、颜色选择器等基础组件，图标库质量差。

### 0.1 与 v4 spec 的关系

- v4 spec 的**主题系统（QmThemeRuntime）、卡片布局（L0/L1/L2）、图标 MSDF、SDF 圆角**——保留，本 spec 直接引用
- ⚠️ **v4 的组件视觉重绘（Toggle knob 0.87、Button radius、Slider 重写）降级为待办**：用户认为 v4 的组件视觉不够好看，本 spec **不直接采用** v4 的组件外观。组件重绘作为独立待办事项，等 v4 视觉方案修订后另行处理。本 spec 的组件工作聚焦于**补全新组件**（SearchField/Dropdown/SegmentedControl/ColorPicker）和**动画完善**，不重写现有组件外观。
- 本 spec **新增**：设置页 tab 重组、顶部导航分栏、组件库补全（SearchField/Dropdown/SegmentedControl/ColorPicker）、动画配置项、搜索功能

> **依赖关系**：本 spec 的 P0（主题系统，即 v4 spec P0）是后续所有阶段的前置条件——新组件（Dropdown/SegmentedControl/ColorPicker）的颜色和卡片布局（L0/L1/L2）都依赖 `QmThemeRuntime` 运行时主题派生函数。若 v4 spec 的 P0 尚未实现，本 spec 的 P3~P8 必须在 P0 完成后才能落地。两份 spec 的 P0 是同一个工作单元（`QmThemeRuntime.h`），不重复实现。

### 0.2 硬约束（不可突破）

- `DoButtonLogic` / `HotItem` / `ActiveItem` **不动**——交互判定是基石
- 不引入运行时第三方库（SDF 图标走构建期烘焙，Phosphor SVG 作为构建期输入）
- 不改 DDNet 上游协议 / 物理 / 预测 / 碰撞
- 新增文件仅落 `src/game/client/QmUi/`、`src/game/client/components/qmclient/`、`data/shader/`、`data/qmclient/icons/`、`qmclient_scripts/`
- QmClient 配置项统一用 `qm_` / `Qm` 前缀
- 所有改动仅限 `g_Config.m_QmNewUi != 0` 新 UI 路径，旧 UI 路径不动

> **例外（旧 UI 导航栏字体校准）**：旧 UI 路径（`m_QmNewUi == 0`）的顶部导航栏高度被 QmClient 从原版的 24.0px 改成了 34.0px（`menus.cpp:285/2844/2948/5077`），导致字体比 BestClient/原版 DDNet 大 50%（字体 = `rect_height × ms_FontmodHeight`）。经 BestClient 仓库对比核实（BestClient `menus.cpp:1574/1619` = 24.0px），这是一个需要修复的回归。**此修复仅限 menubar 高度这一个数值**（34.0→24.0 或折中 28.0，待用户确认），不涉及旧 UI 的其他改动。详见附录 A。

---

## 1 · 设置页 11 tab 重组

### 1.1 tab 列表与内容映射

从当前 15 个扁平 tab 重组为 11 个语义分组：

| # | tab 名称 | 图标 | 包含内容 | 来源映射 |
|---|---|---|---|---|
| 1 | **语言** Languages | `FONT_ICON_LANGUAGE` | 语言选择、字体 | = `SETTINGS_LANGUAGE` |
| 2 | **社交** Social | `FONT_ICON_USERS` | 社交关系配置：好友颜色、仅好友聊天、好友/同 clan 名字牌颜色、DDNet 账号绑定（如未来新增） | ← `SETTINGS_APPEARANCE` 的社交相关配置项（`m_ClFriendsListFriendColor`、`m_ClSameClanColor`、`m_ClShowChatFriends`、`m_ClFriendColor` 等）。好友列表管理本身在独立组件（`CFriends`），不在设置 tab |
| 3 | **Tee** | `FONT_ICON_USER` | 玩家身份（名字/clan/flag/country）、皮肤选择与自定义、社区皮肤 | ← `SETTINGS_PLAYER` 全部 + `SETTINGS_ASSETS` 皮肤部分 |
| 4 | **视觉** Visual | `FONT_ICON_PALETTE` | UI 主题色、透明度、UI 缩放、名字牌/HUD 风格、warlist 颜色 | ← `SETTINGS_APPEARANCE` + `QMCLIENT_TAB_VISUAL` + TClient 视觉类 |
| 5 | **图像** Graphics | `FONT_ICON_MONITOR` | 分辨率、全屏、VSync、FPS 限制、抗锯齿、纹理质量、多线程渲染、渲染后端 | = `SETTINGS_GRAPHICS` |
| 6 | **声音** Sound | `FONT_ICON_SPEAKER` | 主音量、音乐音量、音效、语音 | = `SETTINGS_SOUND` |
| 7 | **资源** Assets | `FONT_ICON_PACKAGE` | 皮肤/实体/装饰/游戏主题资源管理、贡献者列表 | ← `SETTINGS_ASSETS`（非皮肤部分）+ `SETTINGS_CONTRIBUTORS` |
| 8 | **控制** Controls | `FONT_ICON_GAMEPAD` | 键盘/鼠标/手柄绑定 | = `SETTINGS_CONTROLS` |
| 9 | **游戏界面** HUD | `FONT_ICON_LAYOUT` | QmClient HUD 全部内容（HUD 编辑器、名字牌、聊天框样式等） | ← `QMCLIENT_TAB_HUD` |
| 10 | **小功能** Functions | `FONT_ICON_GEARS` | QmClient 功能子tab、常规设置（demo/回放/自动登录/关键词回复/翻译）、profiles/configs | ← `QMCLIENT_TAB_FUNCTION` + `SETTINGS_GENERAL` + TClient 功能类 + `SETTINGS_PROFILES` + `SETTINGS_CONFIGS` |
| 11 | **搜索** Search | `FONT_ICON_MAGNIFYING_GLASS` | **全新**：设置项搜索引擎 | 新功能 |

### 1.2 移除/合并的现有 tab

| 现有 tab | 去向 |
|---|---|
| `SETTINGS_GENERAL` | → Functions |
| `SETTINGS_PLAYER` | → Tee |
| `SETTINGS_APPEARANCE` | → Visual |
| `SETTINGS_DDNET` | → Functions（全部内容：Demo/Ghost/Gameplay/Background/Miscellaneous，无社交部分。DDNet tab 实际不含好友/API，那些在独立组件） |
| `SETTINGS_TCLIENT` | → Visual（视觉类）+ Functions（功能类），按内容拆分 |
| `SETTINGS_QMCLIENT`（含 5 子tab） | → Visual（视觉子tab）+ HUD（HUD子tab）+ Functions（功能子tab），子tab 提升为顶级 tab |
| `SETTINGS_PROFILES` | → Functions |
| `SETTINGS_CONFIGS` | → Functions |
| `SETTINGS_CONTRIBUTORS` | → Assets |

### 1.3 搜索 tab 设计（全新功能）

**交互形式**：独立搜索页 tab，点击后显示一个大搜索框 + 结果列表页面（类似 macOS System Settings 搜索结果页）。

**搜索框行为**：
- 输入关键词后，实时搜索所有 tab 中已注册的设置项
- 每个结果项显示：设置项名称（当前语言）+ 所在 tab 标签 + 简短描述
- 点击结果项：直接跳转到对应 tab，并高亮目标设置项（短暂闪烁/缩放动画）

**设置项注册表**：
- 扩展现有的 `CMenuTextPlan` / Section 注册机制，每个设置项注册一个元组：`(display_name_key, tab_enum, description_key, search_keywords)`
- 搜索时：对 `display_name_key` + `description_key` + `search_keywords` 做子串匹配（大小写不敏感）
- 支持中英文搜索（注册时同时注册中英文关键词）

**搜索结果排序**：
- 精确名称匹配优先
- 然后是名称包含
- 然后是描述/关键词包含

**空状态**：未输入时显示热门设置项推荐（主题色、UI缩放、分辨率等）。

### 1.4 设置页布局结构（复用 v4 spec L0/L1/L2 三级容器）

```
┌─────────────────────────────────────────────────────────────────┐
│  L0 Root Panel（统一 material 面板，外围 20pt 呼吸边距）        │
│  ┌──────────┬──────────────────────────────────────────────────┐│
│  │ 导航栏   │  内容区                                            ││
│  │ (左)     │                                                    ││
│  │          │  ┌─ L1 Section Card ────────────────────────────┐││
│  │ 🌐 语言  │  │  分组标题                                      │││
│  │ 👥 社交  │  │  ┌─ L2 Inset Group ───────────────────────┐  │││
│  │ 🎮 Tee   │  │  │  设置项行（label + control）           │  │││
│  │ 🎨 视觉  │  │  │  设置项行                    ────────  │  │││
│  │ 🖥️ 图像  │  │  │  设置项行                              │  │││
│  │ 🔊 声音  │  │  └────────────────────────────────────────┘  │││
│  │ 📦 资源  │  └──────────────────────────────────────────────┘││
│  │ 🎮 控制  │                                                    ││
│  │ 📊 界面  │  ┌─ L1 Section Card ────────────────────────────┐││
│  │ ⚙️ 功能  │  │  ...                                          │││
│  │ 🔍 搜索  │  └──────────────────────────────────────────────┘││
│  └──────────┴──────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
```

导航栏与内容区用分隔线（`BORDER_SUBTLE`）分离，而非两个独立卡片（v4 spec 6.5.2 方案）。

### 1.5 实现要点

**枚举重构**：
```cpp
// menus.h — 新的设置 tab 枚举
enum
{
    SETTINGS_LANGUAGE,
    SETTINGS_SOCIAL,          // 新
    SETTINGS_TEE,             // 合并 PLAYER + 皮肤
    SETTINGS_VISUAL,          // 合并 APPEARANCE + QmClient-Visual
    SETTINGS_GRAPHICS,
    SETTINGS_SOUND,
    SETTINGS_ASSETS,          // 合并 CONTRIBUTORS
    SETTINGS_CONTROLS,
    SETTINGS_HUD,             // 提升
    SETTINGS_FUNCTIONS,       // 合并 GENERAL + QmClient-Function + TClient功能
    SETTINGS_SEARCH,          // 搜索放最后（导航栏顺序与用户需求一致）

    SETTINGS_LENGTH,
};
```

**向后兼容**：旧枚举值通过映射函数兼容现有配置文件（`m_UiSettingsPage`）。

**渲染分发**：`RenderSettings()` 根据 `m_UiSettingsPage` 分发到新的 `RenderSettingsXxx()` 函数，每个函数内部用 L1 Section Card 组织内容。

---

## 2 · 顶部导航栏左右分栏

### 2.1 布局

```
┌──────────────────────────────────────────────────────────────────────┐
│  [🏠] [🌐] [🔗] [⭐] [🏘️][🏘️][🏘️]    ←→    [🗺️] [🎬] [✏️] [📦] [⚙️] [⏻] │
│   主  互联  网络  收藏  社区×N              地图  Demo 编辑 资源 设置 退出 │
│   菜单  网    服务器                       收藏夹 列表 器  编辑器     │
│   ←────── 左对齐（服务器/世界浏览）──→  ←── 右对齐（工具/系统）──→     │
└──────────────────────────────────────────────────────────────────────┘
```

### 2.2 左右分组语义

- **左边（左对齐）**：所有"进入某个游戏世界/服务器列表"的入口
  - 主菜单（🏠 Home）
  - 互联网（🌐 Internet）
  - 网络/LAN（🔗 Network）
  - 收藏服务器（⭐ Favorites）
  - 收藏社区（🏘️ Favorite Communities ×N，动态数量）

- **右边（右对齐）**：所有"工具/系统操作"入口
  - 地图收藏夹（🗺️ Favorite Maps）
  - 回放列表/Demo（🎬 Demos）
  - 地图编辑器（✏️ Editor）
  - 资源编辑器（📦 Assets Editor）—— 新增独立入口
  - 设置（⚙️ Settings）
  - 退出游戏（⏻ Quit）

### 2.3 实现改造（复用现有 `RenderMenubar` 的 `m_QmNewUi` 路径）

当前代码（`menus.cpp:1506-1794`）已部分实现左右分区：
- 右边已有：Quit、Settings、Editor（用 `VSplitRight`）
- 左边已有：Home、Internet、LAN、Favorites、FavoriteMaps、FavoriteCommunities（用 `VSplitLeft`）

**改造点**：

1. **地图收藏夹从左移到右**：当前在 `:1672` 用 `VSplitLeft`，改为 `VSplitRight`
2. **Demo 从左移到右**：当前在 `:1580` 用 `VSplitRight`（已在右边），确认位置正确
3. **新增"资源编辑器"按钮到右边**：点击设置 `g_Config.m_UiSettingsPage = SETTINGS_ASSETS` 并切换到 `PAGE_SETTINGS`
4. **右边按钮顺序调整**为：地图收藏夹 → Demo → 编辑器 → 资源编辑器 → 设置 → 退出

**离线 vs 在线状态**：
- **离线（主菜单）**：显示全部 11 项（左 5 + 右 6）
- **在线（游戏内 ESC）**：只显示主菜单 + 设置 + 退出（当前行为，保持不变）

### 2.4 资源编辑器入口

"资源编辑器"是给现有 Assets 设置页一个独立的顶部导航入口：
- 点击后：`g_Config.m_UiSettingsPage = SETTINGS_ASSETS`；`SetMenuPage(PAGE_SETTINGS)`
- 图标：`FONT_ICON_PACKAGE`
- Tooltip："Assets Editor" / 本地化"资源编辑器"

---

## 3 · 可复用 UI 组件库

### 3.0 现有公共组件完整盘点（逐文件核实，2026-06-23）

> 以下是代码库中**所有**可复用 UI 组件的完整清单，分三套。本 spec 不遗漏任何一个。

**A. QmUi 新组件库**（`src/game/client/QmUi/`）：

| 组件 | 文件 | 职责 |
|---|---|---|
| `PrimaryButton` / `SecondaryButton` / `IconButton` | `UiButtons.h/.cpp` | 按钮三变体 |
| `TextField` | `UiForms.h/.cpp` | 文本输入框 |
| `Toggle` | `UiForms.h/.cpp` | 开关（knob spring 动画） |
| `Slider`（QmUi 封装） | `UiForms.h/.cpp` | 滑块 |
| `Checkbox` | `UiForms.h/.cpp` | 复选框 |
| `DrawCard` / `InsetGroup` / `SectionHeader` | `UiContainers.h` | 卡片容器三件套 |
| `Modal` | `UiOverlays.h` | 模态弹窗（SCALE 弹入动画） |
| `Toast` | `UiOverlays.h` | 通知条（POS_Y + ALPHA 动画） |
| `Tooltip` | `UiOverlays.h` | 悬浮提示 |
| `TabBar` / `ListItem` | `UiNavigation.h/.cpp` | 标签栏 + 列表项 |
| `CUiV2LayoutEngine` / `SUiLength` | `QmLayout.h/.cpp` | 布局引擎（ROW/COLUMN, AUTO/PX/PERCENT/FLEX） |
| `QmRender` | `QmRender.h` | 渲染桥接 |
| `QmThemeRuntime`（P0 前置） | `QmTheme.h` | 运行时主题色派生 |
| `CUiV2TreeTracker` | `QmTree.h` | 树跟踪器 |
| `CUiV2AnimationRuntime` | `QmAnim.h/.cpp` | 动画引擎（9 属性 + spring） |
| `ResolveUiAnimValue*` | `QmAnimResolve.h/.cpp` | 动画消费 API |
| 曲线预设（STANDARD/EMPHASIZED 等） | `QmAnimCurves.h` | 命名缓动 + spring 预设 |
| `L0/L1/L2` 降级 | `QmMotion.h` | 三级运动减少 |
| Tokens（颜色/字体/圆角） | `UiTokens.h` | 设计 token |
| `UiContextV2` | `UiContext.h` | UI 上下文 |
| Dogfood 页 | `UiDogfood.cpp` | 组件验证/动画实验室 |

**B. DDNet 原生 + QmClient 定制菜单组件**（`menus.cpp` / `ui.cpp`）：

| 组件 | 文件:行 | 动画 |
|---|---|---|
| `DoButton_Menu` | `menus.cpp:621` | hover ✅(ALPHA+Lift 0.11s) |
| `DoButton_MenuTab` | `menus.cpp:678` | hover ✅(Animator) |
| `DoButton_Toggle` | `menus.cpp:593` | hover ✅(ALPHA 0.10s) |
| `DoButton_CheckBox` / `_Common` | `menus.cpp:909/938` | hover ✅(SCALE) + check ✅(ALPHA) |
| `DoButton_Favorite` | `menus.cpp:883` | visibility ✅ + hover ✅(SCALE) |
| `DoButton_GridHeader` | `menus.cpp:870` | ❌ 硬切 |
| `DoButton_ColorPicker` / `DoLine_ColorPicker` | `menus.cpp` | ❌ |
| `DoIngameMenuTab` | `menus.cpp:4476` | ✅(委托 MenuTab) |
| `DoButton_FontIcon` | `ui.cpp:1387` | ❌ |
| `DoButton_PopupMenu` | `ui.cpp:1415` | ❌ |
| `DoValueSelector` / `DoValueSelectorWithState` | `ui.cpp:1427/1432` | ❌ |
| `DoScrollbarH` | `ui.cpp:1655` | ❌ handle 硬切 |
| `DoScrollbarV` | `ui.cpp:1585` | ❌ |
| `DoScrollbarOption` | `ui.cpp:1758` | ❌ |
| `DoEditBox` | `ui.cpp:1104` | focus ring ✅(QmClient 定制) |
| `DoClearableEditBox` | `ui.cpp:1230` | ❌ |
| `DoEditBox_Search` | `ui.cpp:1252` | ❌ |
| `DoDropDown` | `ui.cpp:2212` | ❌ |
| `DoLabel` / `DoLabelStreamed` / `DoLabel_AutoLineSize` | `ui.cpp:969/1032/1096` | N/A |
| `DoButtonLogic` / `DoDraggableButtonLogic` / `DoDoubleClickLogic` / `DoPickerLogic` | `ui.cpp:663/703/773/791` | N/A(逻辑层) |
| `DoSmoothScrollLogic` | `ui.cpp:835` | ✅ 平滑滚动 |
| `DoPopupMenu` | `ui.cpp:1888` | N/A |
| `CUiScrollRegion` | `ui_scrollregion.cpp` | ✅ 平滑滚动 |
| `CUiListBox` | `ui_listbox.cpp` | N/A |
| `CUIRect`（Draw/Margin/VSplit/HSplit 等） | `ui_rect.h` | N/A(几何原语) |

**C. QmClient 辅助 UI 组件**（`components/qmclient/`）：

| 组件 | 文件 | 职责 |
|---|---|---|
| `CQmIconManager` | `qm_icon_manager.h/.cpp` | 图标渲染管理 |
| `CQmImeCandidatePopup` | `qm_ime_candidate_popup.h` | IME 候选词弹窗 |
| `CQmImeManager` | `qm_ime_manager.h` | IME 输入管理 |

**本 spec 的组件工作范围**：补全 A 类缺失的新组件（SearchField/Dropdown/SegmentedControl/ColorPicker），完善 B 类的动画接入。**不重写** A 类现有组件的外观（v4 视觉重绘降级为待办）。

### 3.1 组件盘点与补全计划

| # | 组件 | 现状 | 本 spec 工作 |
|---|---|---|---|
| 1 | **输入框** | ✅ `ui_widget::TextField` | 新增 `SearchField`（左搜索图标 + 右删除按钮）+ `ClearableTextField`（带删除按钮变体） |
| 2 | **滚动条** | ✅ `CUiScrollRegion` | 现有足够 |
| 3 | **卡片** | ✅ `ui_widget::DrawCard` | 现有足够；配合 v4 spec L0/L1/L2 |
| 4 | **下拉框** | ⚠️ 仅 DDNet 原生 `DoDropDown` | **新增** `ui_widget::Dropdown` |
| 5 | **弹窗** | ✅ `ui_widget::Modal` | 现有足够 |
| 6 | **通知栏 Toast** | ✅ `ui_widget::Toast` | **扩展**：支持类型图标（success/warning/error），聊天框复用 |
| 7 | **按钮** | ✅ Primary/Secondary/IconButton | **新增** `SegmentedControl`（分段控件/多按钮选择器） |
| 8 | **颜色选择器** | ⚠️ 仅 DDNet 原生 | **新增** `ui_widget::ColorPicker`（接入主题色） |
| 9 | **开关 Toggle** | ✅ `ui_widget::Toggle` | 外观重绘降级为待办（v4 knob 0.87 不采用）；本 spec 仅做 track 颜色与 knob spring 时长对齐（U2） |
| 10 | **图标库** | ⚠️ 8 个 RGBA 图标 | **新增** Phosphor Icons + MSDF 烘焙管线 |

### 3.2 新增组件规格

#### 3.2.1 SearchField（搜索框）

```cpp
// QmUi/UiForms.h
struct SSearchFieldProps
{
    const char *m_pPlaceholder = nullptr;
    float m_FontSize = ui_token::font::BODY;
    bool m_AutoFocus = false;        // 首次显示时自动聚焦
};

// 左侧 🔍 图标 + 文本输入 + 输入非空时右侧显示 ✕ 清除按钮
// 清除按钮点击：清空输入 + 保持焦点
bool SearchField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect,
                 const SSearchFieldProps &Props = {});
```

**布局**：`[🔍] [输入文本区域        ] [✕]`，图标用 `IconButton`，删除按钮用 `IconButton`，中间是 `TextField`。

#### 3.2.2 ClearableTextField（带删除按钮输入框）

```cpp
// QmUi/UiForms.h — TextField 的带删除按钮变体
bool ClearableTextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect,
                        const char *pPlaceholder = nullptr, float FontSize = ui_token::font::BODY);
// 输入非空时右侧显示 ✕，点击清空
```

#### 3.2.3 Dropdown（下拉框）

```cpp
// QmUi/UiForms.h
struct SDropdownOption
{
    const char *m_pLabel = nullptr;
    const void *m_pValue = nullptr;  // 选项值
    EQmIcon m_Icon = EQmIcon::COUNT; // 可选前置图标
};

struct SDropdownProps
{
    const SDropdownOption *m_pOptions = nullptr;
    int m_OptionCount = 0;
    int *m_pSelectedIndex = nullptr;  // 当前选中索引
    float m_FontSize = ui_token::font::BODY;
    bool m_Disabled = false;
};

// material 背景 + 选中项主题色 + 展开时高度动画（HEIGHT）+ 内容淡入（ALPHA）
// 交互复用 DoButtonLogic，展开列表用 Modal 的 backdrop 机制（点击外部关闭）
bool Dropdown(const IUiContext &Ctx, const void *pId, const CUIRect &Rect,
              const SDropdownProps &Props);
```

**展开动画**：展开时用 Modal 的 backdrop 机制（点击外部关闭）。展开列表用 `ALPHA` 0→1 淡入（DECELERATE 200ms），高度通过显式目标值计算（选项数 × 行高），不依赖动画系统的 "auto" 概念（`CUiV2AnimationRuntime` 无 auto height，按选项数预算目标高度后渲染）。选中项用 `AccentDim()` 背景。

#### 3.2.4 SegmentedControl（分段控件/多按钮选择器）

```cpp
// QmUi/UiButtons.h
struct SSegmentedProps
{
    const char *const *ppLabels = nullptr;  // 选项标签数组
    int m_Count = 0;
    int *m_pActive = nullptr;               // 当前选中索引
    bool m_Disabled = false;
};

// 一组互斥选项的水平按钮组，选中项用 AccentDim 背景 + AccentPrimary 文字
// 选中切换时用 SNAPPY spring 动画滑动高亮背景（类似 TabBar 下划线）
// 用于替代部分 DoDropDown 场景（选项少时更直观）
bool SegmentedControl(const IUiContext &Ctx, const void *pId,
                      const CUIRect &Rect, const SSegmentedProps &Props);
```

**滑动高亮**：选中项变化时，背景高亮用 POS_X spring 动画滑动（类似 TabBar 下划线机制）。

#### 3.2.5 ColorPicker（颜色选择器）

```cpp
// QmUi/UiForms.h
struct SColorPickerProps
{
    unsigned int *m_pColorValue = nullptr;  // 当前颜色（0xRRGGBBAA）
    bool m_ShowAlphaSlider = false;         // 是否显示透明度滑块
    const char *m_pLabel = nullptr;
};

// 点击色块弹出 Modal 内的颜色选择面板：
// - 色相环/色相条 + 饱和度/亮度方块
// - HEX 输入框
// - 预设色板（主题色相关）
// - 透明度滑块（可选）
bool ColorPicker(const IUiContext &Ctx, const void *pId, const CUIRect &Rect,
                 const SColorPickerProps &Props);
```

**接入主题色**：颜色选择器本身用 `qm_theme_runtime::AccentPrimary()` 作为 UI 强调色。预设色板包含 v4 spec 1.4 的预设色（琥珀/蓝/绿/红/紫/橙/青）。

#### 3.2.6 Toast 扩展（类型图标 + 聊天框复用）

```cpp
// QmUi/UiOverlays.h — 扩展 SToastProps
enum class EToastType
{
    INFO,       // 默认，无图标或 ℹ️
    SUCCESS,    // ✅ 绿色
    WARNING,    // ⚠️ 橙色
    ERROR,      // ❌ 红色
};

struct SToastProps
{
    float m_Width = 280.0f;
    float m_Height = 38.0f;
    float m_Margin = ui_token::spacing::LG;
    const char *m_pText = nullptr;
    EToastType m_Type = EToastType::INFO;  // 新增
    float m_Duration = 3.0f;               // 新增：自动消失时间（0=手动）
};
```

**类型图标**：左侧显示对应类型图标，颜色用语义色（SUCCESS/WARNING/DANGER token）。

**聊天框复用**：聊天系统的临时通知（如"已加入服务器"、"好友上线"）改用 `Toast` 组件渲染，统一视觉风格。

---

## 4 · 图标库：Phosphor Icons + MSDF

### 4.1 图标库选型：Phosphor Icons

**选型理由**（v4 spec 第 10 章已论证）：
- **6 档字重**：Thin / Light / Regular / Bold / Fill / Duotone，最接近 SF Symbols 的可变字重体系
- **9000+ 图标**，覆盖面远超 Lucide（1500+）和 Tabler（当前在用）
- **256×256 viewBox**，MSDF 烘焙质量好
- **MIT 许可**，商用友好
- **状态驱动字重映射**：Normal→Regular，Hover→Bold，Active→Fill，Disabled→Light

**对比**：
| 库 | 字重档位 | 强调态 | 图标数 | 许可 |
|---|---|---|---|---|
| **Phosphor**（选） | 6 档 | Fill 变体 | 9000+ | MIT |
| Lucide | 1 档 | 无 | 1500+ | ISC |
| Material Symbols | 4 档(变量) | Fill | 2500+ | Apache 2.0 |
| Tabler（当前） | 1 档 | 无 | 4500+ | MIT |

### 4.2 状态字重映射

| UI 状态 | Phosphor 字重 | SF Symbols 对标 | 用途 |
|---|---|---|---|
| Normal（默认） | **Regular** | Regular | 常规图标 |
| Hover | **Bold** | Medium~Semibold | 鼠标悬浮加粗 |
| Active / Selected | **Fill** | Bold + .fill | 选中/激活填充 |
| Disabled | **Light** + alpha 0.36 | Thin + 降透明度 | 弱化 |

### 4.3 MSDF 烘焙管线

**构建期**：
1. 从 Phosphor Icons 挑选全套图标（目标 40+），覆盖：导航、操作、状态、媒体、设置
2. SVG 源文件存放 `data/qmclient/icons/src/`，按字重分子目录
3. 用 `msdf-atlas-gen` 批量生成 MSDF 图集：单张 `qm_icons_msdf.png`（RGB 三通道）+ `qm_icons_msdf.json`
4. 构建脚本 `qmclient_scripts/build_sdf_icons.py` 封装
5. 废弃旧 `qm_icons_1x/2x/4x.png + .json`

**运行时**：
1. 新增 SDF 图标 Fragment Shader `data/shader/qm_icon_msdf.frag`（median 三通道 + fwidth 抗锯齿）
2. 扩展 `CQmIconManager::RenderIcon()` 绑定 MSDF shader
3. Manifest 扩展格式标识字段（`"format": "msdf"`, `"sdfRange": 4.0`）
4. `EQmIcon` 枚举扩充到 40+，增加 `EQmIconWeight` 参数

**降级**：不支持 `fwidth` 的老旧 GLES 平台，降级为固定 1px AA。

### 4.4 图标覆盖清单（40+ 目标）

| 分类 | 图标 |
|---|---|
| 导航 | caret-left/right/up/down, arrow-left/right, house, earth-americas, network-wired, star |
| 操作 | plus, minus, x, check, trash, pencil-simple, copy, share-network, download-simple |
| 状态 | star, heart, eye, eye-slash, bell, warning, info |
| 媒体 | play, pause, magnifying-glass, funnel, clapperboard |
| 设置 | gear, gears, sliders, palette, monitor, speaker, gamepad |
| 用户 | user, users, package, layout, language |

---

## 5 · 动画系统：从"能跑"到"好用"

### 5.1 审计结论：现有系统是什么状态

对 `CUiV2AnimationRuntime`（`QmAnim.h/.cpp`）、`ResolveUiAnimValue*`（`QmAnimResolve.h/.cpp`）、`QmMotion.h`、`QmAnimCurves.h` 的全量审计，以及 23 个文件中的调用点扫描，得出三个层面的诊断：

#### 5.1.1 引擎层：能力够用但有硬伤

**已有能力（不重写）**：
- tween + spring 双驱动，10 种动画属性（POS_X/Y, WIDTH/HEIGHT, ALPHA, COLOR_RGBA×4, SCALE）
- 8 种 easing（LINEAR/EASE_IN/OUT/IN_OUT/OUT_QUART/OUT_BACK/IN_OUT_CUBIC/CUBIC_BEZIER）+ spring 物理积分（240Hz 子步）
- 4 种中断策略（REPLACE/QUEUE/KEEP_HIGHER_PRIORITY/MERGE_TARGET）
- 3 级降级（`QmMotion.h`：L0 归零 / L1 ×0.45 时长 + ×1.35 阻尼 / L2 全量）
- 5 种命名曲线预设 + 3 种 spring 预设（GENTLE/SNAPPY/WOBBLY）

**引擎层硬伤（需补）**：

| # | 缺口 | 现状代码 | 后果 | 补法 |
|---|---|---|---|---|
| C1 | **无 ROTATION 属性** | `EUiAnimProperty` 枚举无旋转 | 旋转类动画（卡片翻转、图标 spin、loading 转圈）无法做 | 新增 `EUiAnimProperty::ROTATION`，在 tween/spring 都支持 |
| C2 | **spring 无 velocity 接力** | `RequestAnimation` 只接收 target，不接收初始速度（`:188`） | 连续滑动/抛掷手势无法把惯性传递给 spring，手感生硬 | `SUiAnimRequest` 增加 `m_InitialVelocity` 字段，`StartTrack` 用它初始化 `m_Velocity` |
| C3 | **无 stagger/sequence 原语** | 只有 `m_DelaySec` 单值，无列表内"第 i 个延迟 i×50ms"的编排 | 错峰揭示（搜索结果、卡片列表入场）需要每个元素手写 delay | 新增 `ResolveUiAnimValueStagger()` helper，按 index 自动计算 delay |
| C4 | **完成回调是轮询式** | `PollCompletedEvent` 需要每帧主动轮询（`:370`） | 动画完成后的副作用（如播放音效、清理状态）必须每帧检查，易遗漏 | 保留轮询（IM GUI 模式），但补充 `m_OnCompleteCallback` 选项给需要一次性副作用的场景 |
| C5 | **无显式 curve 可视化/debug** | 无预览动画曲线的 dogfood 工具 | 设计新曲线/弹簧参数时只能盲调 | dogfood 页增加 curve preview：画 easing 函数曲线 + spring 衰减包络线 |

#### 5.1.2 接入层：覆盖不均（逐函数体核实）

对每个组件的 hover/press/入场三种动画做**逐函数体读码核实**（非简略扫描），结果如下。注意：QmClient 已自行给 DDNet 原生组件（DoButton_Menu/Toggle/CheckBox/Favorite）添加了 hover 动画，这些是 QmClient 独有定制，BestClient/原版 DDNet 没有。

| 组件 | hover | press | 入场 | 证据 |
|---|---|---|---|---|
| **DoButton_Menu** | ✅ ALPHA 0.11s + Lift -1.25px | ❌ | ❌ | `menus.cpp:626-631`（BestClient 对比：`DoButton_MenuEx` 无动画） |
| **DoButton_MenuTab** | ✅ ResolveMenuTabAnimationValue | ❌ | ❌ | `menus.cpp:683`（BestClient：SUIAnimator 100ms 线性手写） |
| **DoButton_Toggle** | ✅ ALPHA 0.10s | ❌ | ❌ | `menus.cpp:595-599`（BestClient：无动画） |
| **DoButton_CheckBox** | ✅ SCALE 0.10s + check ALPHA 0.10s | ❌ | ❌ | `menus.cpp:945-951`（BestClient：无动画） |
| **DoButton_Favorite** | ✅ visibility ALPHA 0.12s + hover SCALE 0.10s | ❌ | ❌ | `menus.cpp:886-894`（BestClient：硬切无动画） |
| **DoButton_GridHeader** | ❌ | ❌ | ❌ | `menus.cpp:870-881`（硬切色） |
| **页面/tab 切换** | ✅ TriggerUiSwitchAnimation ×12 处 | — | — | menu/game/settings/browser/filter/toolbox/touch/qmclient |
| **CheckBox hover** | ✅（见上） | ❌ | ❌ | |
| **QmUi Button** | ✅ 颜色 hover | ❌ 按压 SCALE 缺失 | ❌ | `UiButtons.cpp`（spec 3.2 要求加按压 SCALE） |
| **Toggle** | ✅ knob POS_X spring + ✅ track 颜色 `ResolveUiAnimValueColor`（DECELERATE） | ❌ | ❌ | `UiForms.cpp:80,93-107`；U2：track 颜色与 knob spring 时长不同步 |
| **TextField** | ✅ focus ring ALPHA | — | ❌ | `UiForms.cpp:35` |
| **Slider handle** | 🔴 **无**（硬切 `Handle.h += 3.0f`） | 🔴 **无** | ❌ | `ui.cpp:1718-1722` 读码确认 |
| **设置项内容行** | 🔴 **无**（抽查 RenderSettingsGeneral 确认） | 🔴 **无** | 🔴 **无** | `menus_settings.cpp:709-906` |
| **DrawCard** | 🔴 **无** | 🔴 **无** | 🔴 **无** | `UiContainers.h`（待核实 .cpp 实现） |
| **ListItem** | ⚠️ hover 背景 ALPHA 有 | ❌ 选中态无动画 | ❌ | `UiNavigation.cpp:86` |
| **Toast** | — | — | ✅ POS_Y + ALPHA | `UiOverlays.h:74-75` |
| **Modal** | — | — | ✅ SCALE 弹入 | `UiOverlays.h:146` |
| **HUD** | — | — | ✅ 多处 | 录制状态/切换倒计时/媒体岛/本地时间 `hud.cpp` |
| **名字牌/聊天气泡** | — | — | ✅ chat bubble | `nameplates.cpp:117` |

**覆盖率结论（修正后）**：不能笼统说"动画大面积空白"。QmClient 在 **hover 层面**覆盖已较好（Menu/Toggle/CheckBox/Favorite/Tab 全有 hover 动画，且优于原版）。真正的空白集中在三类：
1. **所有组件的 press 反馈**（SCALE 0.96→1.0 spring，逐个确认全无）
2. **所有组件的首次入场动画**（逐个确认全无）
3. **Slider handle**（核心控件，hover/press 硬切 confirmed）、**设置行内容**（抽查 confirmed）、**DrawCard/GridHeader**（confirmed 无动画）

#### 5.1.3 UX 质量：参数粗糙、体感不一致

即使接入了动画的地方，UX 质量也有问题：

| # | 问题 | 证据 | 影响 |
|---|---|---|---|
| U1 | **hover 时长几乎都是 0.10~0.11s EASE_OUT** | `DoButton_CheckBox`（`:950` 0.10s）、`DoButton_Menu`（`:630` 0.11s）、`DoButton_Toggle`（`:599` 0.10s）、`DoButton_Favorite`（hover `:894` 0.10s / visibility `:890` 0.12s） | 各控件 hover 时长差异极小（0.10~0.11s），视觉层次感弱。曲线全用 EASE_OUT 无分层 |
| U2 | **Toggle track 颜色与 knob 时长不同步** | `UiForms.cpp:80` track 颜色已用 `ResolveUiAnimValueColor`（DECELERATE），knob 用 spring（`TOGGLE`）。两者曲线/时长不同，track 颜色过渡可能比 knob 位移先完成 | 切换 toggle 时颜色和位移节奏不一致，体感不够统一 |
| U3 | **按压反馈几乎为零** | PrimaryButton/SecondaryButton 零按压动画；checkbox 只有 hover 没有 press scale | 按钮点击无"物理重量感"，交互缺乏确认感 |
| U4 | **页面切换位移幅度小，仅水平** | `ApplyUiSwitchOffset`（`menus_settings.cpp:4882`）settings page switch 位移幅度 clamp 24~90px，且仅水平（vertical=false）。Appearance 子 tab（`:5450`）clamp 24~120px | 切换时位移感弱，空间方向性不足 |
| U5 | **无"首次出现"动画** | 卡片、设置项内容在页面首次渲染时直接显示 | 大量内容瞬间出现，缺乏编排感，像"截图"而非"活界面" |
| U6 | **spring 参数无物理直觉** | SNAPPY=(280,26) 看不出阻尼比 ζ；用户无法从参数推断手感 | 设计/调参靠试错，不可预测 |

### 5.2 目标：三层补全

#### 5.2.1 引擎层补全（C1~C5）

**新增 ROTATION 属性（C1）**：
```cpp
// QmAnim.h — EUiAnimProperty 新增
enum class EUiAnimProperty
{
    POS_X, POS_Y, WIDTH, HEIGHT, ALPHA,
    COLOR_R, COLOR_G, COLOR_B, COLOR_A,
    SCALE,
    ROTATION,  // 新增：弧度，0=无旋转
};
```
ROTATION 作为独立的 float track，消费者拿到值后在绘制前做 transform rotate。用于：卡片翻转、图标 spin（loading）、下拉箭头旋转、错误抖动。

**spring velocity 接力（C2）**：
```cpp
// QmAnim.h — SUiAnimRequest 新增字段
struct SUiAnimRequest
{
    // ... 现有字段 ...
    bool m_HasInitialVelocity = false;   // 新增
    float m_InitialVelocity = 0.0f;      // 新增：px/s 或 unit/s
};
```
`StartTrack` 中 `if(Request.m_HasInitialVelocity) Track.m_Velocity = Request.m_InitialVelocity;`。用于：滑块拖拽释放后惯性→spring、列表抛掷、Toggle 快速连击的动量保持。

**stagger 原语（C3）**：
```cpp
// QmAnimResolve.h — 新增
// 对列表中第 Index 个元素，自动计算 delay = Index * StaggerSec
float ResolveUiAnimValueStagger(CUiV2AnimationRuntime &AnimRuntime, uint64_t NodeKey,
    EUiAnimProperty Property, float Target, float DurationSec, EEasing Easing,
    int Index, float StaggerSec);
```
内部设置 `Transition.m_DelaySec = Index * StaggerSec`。用于：搜索结果错峰、卡片列表入场、设置分组错峰展开。

**完成回调（C4）** 和 **curve 可视化（C5）** 见 dogfood 扩展（spec 5.4）。

#### 5.2.2 接入层补全（覆盖空白）

逐个补齐审计中标记为 🔴 和 ⚠️ 的区域：

| 区域 | 补全内容 | 实现方式 |
|---|---|---|
| **Slider** | handle hover SCALE 放大 + 拖拽态 spring 放大（1.1×）+ track fill 颜色过渡 | 在 QmUi `Slider()`（`UiForms.cpp`）接管 `DoScrollbarH` 绘制，用 `ResolveUiAnimValue(SCALE)` 驱动 handle 尺寸 |
| **Toggle** | track on↔off 颜色 `ResolveUiAnimValueColor` 过渡 | `UiForms.cpp:79` 增加颜色 track |
| **Button 按压** | PrimaryButton/SecondaryButton 按压时 SCALE 0.96→1.0 spring | `UiButtons.cpp` 增加 SCALE track（`BuildUiAnimNodeKey(scope^0xBEEF, btn)`） |
| **设置项内容** | 每行 hover 时背景 `SURFACE_HIGHLIGHT` ALPHA 淡入；首次出现时 POS_Y +8→0 + ALPHA 0→1 | 在 L2 Inset Group 的行渲染包装器内加 hover track + 首次出现 stagger |
| **卡片容器** | `DrawCard` 首次出现 SCALE 0.98→1 + ALPHA；hover 时 POS_Y lift -1.25px（复用 `DoButton_Image` 的 HoverLift） | `UiContainers.h` DrawCard 内增加 SCALE/ALPHA track |
| **ListItem 选中** | 选中态背景 `AccentDim()` 从左→右展开（WIDTH 0→full）+ 文字颜色过渡 | `UiNavigation.cpp:72` ListItem 增加 WIDTH track |

#### 5.2.3 UX 质量补全（U1~U6）

**分层 hover 曲线（U1）**：

不再所有控件用同一个 `0.10s EASE_OUT`。按交互重要性分三层：

| 层级 | 用途 | 曲线 | 时长 |
|---|---|---|---|
| **L-fast** | 纯视觉反馈（checkbox hover、icon button hover） | EASE_OUT | 0.10s |
| **L-medium** | 功能性控件（slider handle、dropdown、list item） | EASE_OUT_QUART | 0.16s |
| **L-slow** | 大面积/强引导（卡片 hover lift、active tab 强调） | EASE_IN_OUT_CUBIC | 0.22s |

在 `QmAnimCurves.h` 新增 `HOVER_FAST`/`HOVER_MEDIUM`/`HOVER_SLOW` 三个命名预设，替换硬编码的 `0.10f, EASE_OUT`。

**Toggle 颜色时长对齐（U2）**：track on↔off 颜色已用 `ResolveUiAnimValueColor`（`UiForms.cpp:80` DECELERATE），但与 knob spring（`TOGGLE`）时长不同步。调整 track 颜色 transition 使其与 knob spring 完成时间对齐（让颜色和位移同步完成）。

**按压反馈（U3）**：所有可点击组件（Button/Toggle/Checkbox/ListItem/Card 可点击时）按压时 SCALE 0.96→1.0 spring（SNAPPY），释放回弹。这是"物理重量感"的核心来源。

**页面切换方向性（U4）**：settings page switch 增加垂直位移（POS_Y ±8px），配合 alpha，制造"从下方滑入/滑出"的方向感。已有 `ApplyUiSwitchOffset` 支持 vertical，需要启用。

**首次出现动画（U5）**：每个 L1 Section Card 和 L2 Inset Group 首次渲染时（通过 `s_FirstRendered` flag 判断），触发 SCALE 0.98→1 + ALPHA 0→1，duration 0.30s（EMPHASIZED）。同组多个卡片用 stagger 50ms 错峰。

**spring 物理直觉化（U6）**：在 `QmAnimCurves.h` 的 spring 预设旁标注阻尼比 ζ 和振荡预期：
```cpp
// ζ = damping / (2*sqrt(stiffness*mass))
// ζ≥1 无振荡（过阻尼）；0<ζ<1 有衰减振荡；ζ越小越弹
inline constexpr SUiSpringConfig SNAPPY = {1.0f, 280.0f, 26.0f};   // ζ≈0.778，1-2次微振荡后归位，"干脆"
inline constexpr SUiSpringConfig GENTLE = {1.0f, 120.0f, 14.0f};   // ζ≈0.639，3-4次振荡，"柔和"
inline constexpr SUiSpringConfig WOBBLY = {1.0f, 180.0f, 12.0f};   // ζ≈0.447，5+次振荡，"果冻"
```

### 5.3 用户可配置动画参数

```cpp
// config_variables_qmclient.h — 新增
MACRO_CONFIG_FLOAT(QmUiAnimDurationScale, qm_ui_anim_duration_scale, 1.0f, 0.5f, 2.0f, CFGFLAG_CLIENT | CFGFLAG_SAVE, "全局动画时长倍率");
MACRO_CONFIG_FLOAT(QmUiAnimSpringDamping, qm_ui_anim_spring_damping, 1.0f, 0.3f, 1.0f, CFGFLAG_CLIENT | CFGFLAG_SAVE, "全局弹簧阻尼比倍率（越小越弹）");
```

**接入点**：`CUiV2AnimationRuntime::Advance()` 中，spring track 的 damping 乘 `QmUiAnimSpringDamping`，所有 track 的 duration 乘 `QmUiAnimDurationScale`。在 `RequestAnimation` 入口处应用（与 `ApplyMotionLevel` 叠加）。

**设置页入口**："视觉"tab 的 L1 Section Card "动画偏好"，提供 3 个 Slider：`QmUiMotionLevel`（已有）、`QmUiAnimDurationScale`、`QmUiAnimSpringDamping`。

### 5.4 dogfood 动画实验室

扩展现有 dogfood 页（`UiDogfood.cpp`），增加动画专用区域，用于设计期调参和回归验证：

| 工具 | 功能 |
|---|---|
| **Easing 曲线预览** | 画 7 种 easing 函数曲线 + 2 个自定义 bezier 控制点拖拽 |
| **Spring 包络线预览** | 输入 stiffness/damping/mass，画衰减包络线 + ζ 值实时显示 |
| **Stagger 编排器** | 输入元素数 + stagger 间隔，实时预览列表错峰展开效果 |
| **属性矩阵** | 10+1 种 `EUiAnimProperty` 各一个实时 demo（ROTATION 是卡片翻转、SCALE 是弹跳、ALPHA 是呼吸等） |
| **降级对比** | 三列并排显示 L0/L1/L2 同一动画的效果差异 |
| **性能计数器** | 实时显示 `ActiveTrackCount`/`QueuedTrackCount`/`AnimAdvanceMs` |

### 5.5 UX 动画规范（场景→参数映射表）

结合 frontend-design skill 的 Motion 原则，每个场景有明确的曲线 + 时长 + 属性 + 触发条件。**这是实现时的检查清单**：

| 场景 | 属性 | 曲线 | 时长 | 触发 | 降级行为 |
|---|---|---|---|---|---|
| **页面/tab 切换** | ALPHA + POS_Y(±8) | DECELERATE | 0.25s | page enum 变化 | L0:瞬切 L1:仅 alpha |
| **卡片首次入场** | ALPHA + SCALE(0.98→1) | EMPHASIZED | 0.30s + stagger 50ms | 首次渲染 | L0:不播 L1:无 stagger |
| **Button 按压** | SCALE(0.96→1) | SNAPPY spring | ζ≈0.78 | ActiveItem==btn | L0:不缩 L1:tween 替代 |
| **Button hover** | 颜色(ACCENT_DIM→HOVER) | HOVER_MEDIUM | 0.16s | HotItem==btn | L0:瞬切 |
| **Toggle 切换** | knob POS_X + track 颜色 | SNAPPY spring | ζ≈0.78 | 点击 | L0:瞬切 L1:tween |
| **Slider handle hover** | SCALE(1→1.08) | HOVER_MEDIUM | 0.16s | HotItem==handle | L0:不缩 |
| **Slider handle 拖拽** | SCALE(1.08→1.15) | SNAPPY spring | ζ≈0.78 | ActiveItem==handle | L0:不缩 L1:tween |
| **Checkbox 勾选** | ALPHA(X mark) + 颜色 | HOVER_FAST | 0.10s | Checked 变化 | L0:瞬切 |
| **设置行 hover** | ALPHA(SURFACE_HIGHLIGHT bg) | HOVER_MEDIUM | 0.16s | HotItem==row | L0:瞬切 |
| **Dropdown 展开** | ALPHA(内容) | DECELERATE | 0.20s | 点击触发器 | L0:瞬切 L1:0.10s |
| **SegmentedControl** | POS_X(高亮背景) | SNAPPY spring | ζ≈0.78 | active 变化 | L0:瞬切 L1:tween |
| **Modal 打开** | SCALE(0.92→1) + ALPHA | EMPHASIZED | 0.50s | *pOpen=true | L0:瞬显 L1:0.25s |
| **Toast 出现** | POS_Y + ALPHA | EMPHASIZED | 0.40s | Visible=true | L0:瞬显 L1:0.20s |
| **搜索结果** | ALPHA + POS_Y stagger | DECELERATE | 0.20s + stagger 30ms | 结果变化 | L0:瞬显 L1:无 stagger |
| **导航 active 下划线** | POS_X | SNAPPY spring | ζ≈0.78 | ActivePage 变化 | L0:瞬切 L1:tween |
| **收藏社区出现** | ALPHA + WIDTH(2→full) | DECELERATE | 0.18s | 社区列表变化 | L0:瞬显 |
| **设置项跳转高亮** | ALPHA 脉冲 | BOUNCE_OUT | 0.30s ×2 | 搜索点击跳转 | L0:不播 |
| **图标 spin（loading）** | ROTATION(0→2π) loop | LINEAR | 1.0s loop | 异步操作 | L0:不转 L1:2.0s |
| **错误抖动** | POS_X(±4px 衰减) | WOBBLY spring | ζ≈0.45 | 验证失败 | L0:不抖 L1:单次 |

### 5.6 核心原则

- **编排优于散乱**：一个精心编排的页面加载（错峰揭示 + 首次入场）比零散的 hover 微交互更让人愉悦
- **聚焦高影响力时刻**：页面切换、首次出现、操作确认是重点；不要每个像素都动
- **功能性克制、装饰性允许**：切换/定位类动画要快而准（0.10~0.25s）；入场/强调类可以慢而有意（0.30~0.50s）
- **物理一致性**：所有"按压→回弹"用同一个 spring（SNAPPY），让整个 app 的触感统一
- **三层 hover 分明**：fast/medium/slow 对应三种视觉重要性，不混用
- **降级是一等公民**：每个动画必须定义 L0/L1 行为，`QmUiMotionLevel=0` 时 UI 仍然完全可用
- **性能可见**：dogfood 页实时显示 track 数和 advance 耗时，动画增多时第一时间发现

---

## 6 · 配置项汇总

### 6.1 新增配置项

| 配置项 | 类型 | 默认 | 范围 | 用途 |
|---|---|---|---|---|
| `qm_ui_anim_duration_scale` | float | 1.0 | 0.5~2.0 | 全局动画时长倍率 |
| `qm_ui_anim_spring_damping` | float | 1.0 | 0.3~1.0 | 全局弹簧阻尼比倍率 |

### 6.2 修改的配置项

| 配置项 | 变化 |
|---|---|
| `m_UiSettingsPage` | 枚举值重新映射（11 个新 tab），通过映射函数兼容旧值 |

---

## 7 · 文件影响清单

### 7.1 新增文件

| 文件 | 内容 |
|---|---|
| `src/game/client/QmUi/QmThemeRuntime.h` | 运行时主题色派生（v4 spec 1.2，AccentPrimary/MaterialSurface 等） |
| `src/game/client/QmUi/UiSearch.cpp/.h` | 设置项搜索引擎 + 注册表 |
| `data/shader/qm_icon_msdf.frag` | MSDF 图标 Fragment Shader |
| `data/shader/ui_sdf_rect.frag` | SDF 抗锯齿圆角 Shader（v4 spec 第 9 章） |
| `data/qmclient/icons/src/**/*.svg` | Phosphor Icons SVG 源文件 |
| `data/qmclient/icons/qm_icons_msdf.png` | MSDF 烘焙图集（构建产物） |
| `data/qmclient/icons/qm_icons_msdf.json` | MSDF manifest（构建产物） |
| `qmclient_scripts/build_sdf_icons.py` | MSDF 烘焙脚本 |

### 7.2 修改文件

| 文件 | 改动 |
|---|---|
| `src/game/client/QmUi/QmAnim.h` | 新增 `EUiAnimProperty::ROTATION`、`SUiAnimRequest::m_InitialVelocity`（C1/C2） |
| `src/game/client/QmUi/QmAnim.cpp` | ROTATION track 处理、velocity 接力、duration/damping scale 接入 |
| `src/game/client/QmUi/QmAnimResolve.h/.cpp` | 新增 `ResolveUiAnimValueStagger()`（C3） |
| `src/game/client/QmUi/QmAnimCurves.h` | 新增 `HOVER_FAST/MEDIUM/SLOW` 预设、spring ζ 注释（U1/U6） |
| `src/game/client/QmUi/UiForms.cpp` | Slider/Toggle 接入动画（覆盖空白）、新增组件 |
| `src/game/client/QmUi/UiButtons.cpp` | Button 按压 SCALE 动画（U3） |
| `src/game/client/QmUi/UiContainers.h` | DrawCard 入场/hover/press 动画（覆盖空白） |
| `src/game/client/QmUi/UiNavigation.cpp` | ListItem 选中动画、stagger（覆盖空白） |
| `src/game/client/QmUi/UiDogfood.cpp` | 动画实验室（curve/spring/stagger/属性矩阵/降级对比/性能） |
| `src/game/client/components/menus.h` | 设置 tab 枚举重组（11 个）、导航栏布局调整 |
| `src/game/client/components/menus.cpp` | `RenderMenubar()` 左右分栏调整、新增资源编辑器入口、设置行 hover 动画接入 |
| `src/game/client/components/menus_settings.cpp` | `RenderSettings()` 分发到新 tab、各 `RenderSettingsXxx()` 用 L1 Section Card 重写、设置项内容首次入场动画 |
| `src/game/client/QmUi/UiForms.h/.cpp` | 新增 SearchField/ClearableTextField/Dropdown/ColorPicker |
| `src/game/client/QmUi/UiButtons.h/.cpp` | 新增 SegmentedControl |
| `src/game/client/QmUi/UiOverlays.h` | Toast 扩展（EToastType + 图标） |
| `src/game/client/QmUi/UiTokens.h` | 新增 SWITCH_OFF_LIGHT/DARK、SURFACE_BASE、radius::POPOVER/BUTTON（v4 spec 7） |
| `src/game/client/qm_icon_manager.h/.cpp` | MSDF 渲染、EQmIcon 扩充 40+、EQmIconWeight |
| `src/engine/shared/config_variables_qmclient*.h` | 新增 qm_ui_anim_duration_scale/damping |
| `src/game/client/components/menus_ingame.cpp` | ESC 菜单卡片化（v4 spec 6.6.3） |
| `src/game/client/components/menus_start.cpp` | 主菜单按钮图标支持（v4 spec 6.6.1） |

---

## 8 · 落地路径

### 8.1 分阶段交付（P0~PD，继承 v4 spec 12.1 + 新增）
| **P0 — 主题系统** | `QmThemeRuntime.h` + accent 迁移 | 改 `QmUiColor` 后 accent 组件即时变色 |
| ~~**P1 — 组件重绘**~~ | ~~Toggle knob 0.87 + Button radius/按压 + Slider 重写（v4 spec 2-4）~~ | **降级为待办**：用户认为 v4 组件视觉不够好看，等 v4 视觉方案修订后另行处理 |
| **P2 — 卡片布局** | L0 Root Panel + L1 Section Card + L2 Inset Group（v4 spec 6.5） | 设置内容有三级卡片包裹 |
| **P3 — 设置页 11 tab 重组** | 枚举重组 + RenderSettings 分发 + 各 tab 内容迁移 | 11 个 tab 语义清晰，无设置项丢失 |
| **P4 — 顶部导航左右分栏** | 地图收藏夹移右 + 资源编辑器入口 + 顺序调整 | 左右对齐分组正确 |
| **P5 — 新增组件** | SearchField + Dropdown + SegmentedControl + ColorPicker | Dogfood 页全可交互 |
| **P6 — 搜索功能** | 设置项注册表 + 搜索 tab + 跳转高亮 | 搜索能找到并跳转任意设置项 |
| **P7 — 图标 MSDF** | Phosphor SVG + MSDF 烘焙 + shader + 枚举 40+ | cl_ui_scale 0.5~3.0 图标锐利 |
| **PA — 动画引擎补全** | ROTATION + velocity 接力 + stagger + curve 可视化（C1~C5） | dogfood 动画实验室可调参，10+1 种属性全 demo |
| **PB — 动画接入补全** | Slider/Toggle/Button/Card/ListItem/设置项内容 全部接入动画（覆盖空白） | 审计表 🔴/⚠️ 区域全清零 |
| **PC — 动画 UX 质量** | 三层 hover 曲线 + 按压 SCALE + 首次入场 stagger + spring ζ 可读（U1~U6） | 所有 hover 有层次、按压有重量感、入场有编排 |
| **PD — 动画配置 + ESC/主菜单** | duration_scale/spring_damping 配置项 + ESC 菜单卡片化 + 主菜单图标 | 动画可调 + 三面覆盖完成 |
| **P-oldui — 旧 UI 导航栏字体校准** | menubar 高度 34.0→24.0（或折中 28.0，待用户确认），`menus.cpp:285/2844/2948/5077` | 旧 UI 导航栏字体和 BestClient/原版对齐 |

### 8.2 验收标准

| 指标 | 标准 | 测量 |
|---|---|---|
| 设置 tab 数量 | 11 个 | 代码量测 |
| 设置项无丢失 | 所有现有设置项都能在新 tab 或搜索中找到 | 逐项核对 |
| 顶部导航左右分区 | 左 5 项 + 右 6 项 | 目视 |
| 资源编辑器入口 | 点击跳转 Assets 设置页 | 点击验证 |
| 搜索功能 | 输入关键词能找到设置项并跳转 | 搜索测试 |
| 组件库完整 | 10 类组件全可交互 | Dogfood 页 |
| 图标锐利度 | MSDF 图标在 cl_ui_scale 0.5~3.0 无锯齿 | 截图 400% |
| 图标改色 | shader uniform 改色即时 | 主题色切换联动 |
| 动画可配置 | duration_scale + spring_damping 可调 | 设置页调节 |
| 主题色联动 | 改 QmUiColor 后 accent 即时变色 | 设置页改色 |
| 降级 | MotionLevel 0 动画归零 | 切换 L0/L1/L2 |
| FPS 无回退 | Dogfood FPS 无波动 | 帧率计数器 |
| 动画覆盖率 | Slider/Toggle/Button/Card/ListItem/设置项内容 全有动画 | dogfood + 设置页目视 |
| 按压反馈 | 所有可点击组件按压有 SCALE spring | 快速连点 10 次 |
| hover 分层 | fast/medium/slow 三层曲线各用在正确场景 | dogfood 三层对比 |
| 首次入场 | 卡片/设置项首次出现有 ALPHA+SCALE stagger | 切换 tab 目视 |
| ROTATION | 卡片翻转/图标 spin/下拉箭头旋转可用 | dogfood 属性矩阵 |
| velocity 接力 | slider 拖拽释放有惯性 spring | 拖拽后松手 |
| dogfood 动画实验室 | curve/spring/stagger/属性矩阵/降级对比/性能全可用 | 运行 dogfood |
| 旧 UI 导航栏字体 | menubar 高度 ≤28px，字体与 BestClient 对齐 | 切换旧 UI 目视对比 |

### 8.3 不在范围

- 旧 UI 路径（`m_QmNewUi == 0`）——不改动，**唯一例外**是导航栏字体高度校准（P-oldui，详见附录 A）
- v4 spec 的组件视觉重绘（Toggle/Button/Slider 外观重写）——降级为待办，等 v4 视觉方案修订
- backdrop-filter 实时模糊（DDNet 管线不支持）
- HUD 拖拽磁吸/碰撞（独立 feature）
- DDNet 原生组件交互逻辑（只接管绘制）
- 设置项的增删（本 spec 只重组分组，不增删设置项内容）

---

## 9 · 风险与调研项

### 9.1 已知风险

| 风险 | 影响 | 缓解 |
|---|---|---|
| 设置 tab 重组导致用户配置丢失 | `m_UiSettingsPage` 旧值失效 | 映射函数兼容旧枚举值 |
| MSDF shader 需要上游引擎改动 | 新 program ID 注册 | 调研 IGraphics 现有接口是否支持，或走纹理标志 |
| `fwidth` 在部分 GLES 平台不支持 | MSDF 抗锯齿失败 | 降级为固定 1px AA |
| 搜索功能依赖设置项注册 | 未注册的设置项搜不到 | 扩展 Section 注册机制，确保全覆盖 |
| 设置页内容迁移工作量大 | 每个 tab 的 RenderSettings 重写 | 分阶段迁移，先用现有渲染 + 新导航，再逐步卡片化 |

### 9.2 需调研项

| 项 | 说明 |
|---|---|
| MSDF shader 注册路径 | DDNet 图形后端如何注册新 shader program（`graphics_threaded.cpp` / OpenGL backend），是否属于上游改动 |
| `fwidth` 平台覆盖 | 确认 QmClient 目标平台（Win/Linux/macOS/Android）的 GLES/GLSL 版本支持 |
| Phosphor 图标许可证细节 | 确认 MIT 许可证条款，记录到 docs |
| msdf-atlas-gen 可用性 | 确认构建工具链可用性，是否需要预编译二进制 |

---

## 10 · 设计决策记录

| 决策 | 选择 | 理由 |
|---|---|---|
| 搜索 tab 语义 | 设置项搜索（非过滤/非全局） | 类似 macOS System Settings 搜索 |
| 声音 tab | 独立保留（11 个 tab） | 声音设置自成体系 |
| Social tab 范围 | 社交关系 + 玩家身份并入 Tee | 用户确认 |
| 视觉 vs 图像 | UI 外观 vs 引擎渲染 | UI 主题/缩放 vs 分辨率/VSync |
| TClient 拆分 | 按内容拆到 Visual/Functions | 不保留 TClient tab |
| 搜索交互 | 独立搜索页 tab | 类似 macOS System Settings |
| 资源编辑器 | 现有 Assets 设置页的独立入口 | 非全新功能 |
| 图标库 | Phosphor Icons (MSDF) | 6 字重 + 9000+ 图标 + MIT |
| 动画引擎补全 | 新增 ROTATION + velocity 接力 + stagger（C1~C3） | 现有引擎缺旋转、惯性、错峰三个能力 |
| 动画接入补全 | press 反馈 + 入场动画 + Slider/DrawCard/GridHeader/设置行 补齐 | 逐函数核实：hover 层面已覆盖（QmClient 定制），空白集中在 press/入场/Slider/内容行 |
| hover 三层曲线 | fast/medium/slow 按交互重要性分层 | 替代所有控件用同一个 0.10s 的粗糙做法 |
| 动画实验室 | dogfood 页加 curve/spring/stagger 预览 | 设计期调参和回归验证需要可视化工具 |
| 实现范围 | 一次性全做（P0~PD） | 用户确认 |

---

*本 spec 继承并扩展 `2026-06-13-ui-ux-apple-redesign.html`（v4）。v4 spec 的主题系统、卡片布局、SDF 圆角等设计保留并引用。v4 的组件视觉重绘（Toggle/Button/Slider 外观）降级为待办，等 v4 视觉方案修订。动画审计基于逐函数体读码核实（2026-06-23），BestClient 对比基于仓库实际源码。*

---

## 附录 A · 旧 UI 导航栏字体校准（P-oldui）

### A.1 问题

QmClient 旧 UI 路径（`m_QmNewUi == 0` 的 else 分支）顶部导航栏高度被从原版 24.0px 改成 34.0px，导致字体放大 50%。

### A.2 代码级证据

| 参数 | BestClient (≈原版) | QmClient 旧 UI | QmClient 新 UI |
|---|---|---|---|
| Menubar 高度 | **24.0px** (`BestClient/menus.cpp:1574,1619`) | **34.0px** (`menus.cpp:285,2844,2948,5077`) | 24.0px（已正确） |
| 字体大小 | `(24-4) × ms_FontmodHeight` | `(34-4) × ms_FontmodHeight` | 正确 |
| 偏差 | 基准 | 大 **41.7%**（高度）/ 大 **50%**（字体） | 无偏差 |

### A.3 修复方案（待用户确认）

- **方案 A（推荐）**：旧 UI menubar 高度改回 24.0px，与新 UI 和原版完全一致
- **方案 B**：折中 28.0px，比原版稍大但不过分

修改点：`menus.cpp` 第 285、2844、2948、5077 行的 `MenubarHeight = UseNewUi ? 24.0f : 34.0f` 改为统一 24.0f（或 `: 28.0f`）。

---

## 附录 B · 设置项迁移映射表

> 本附录是设置页 11 tab 重组的**逐项迁移映射**，供用户逐条审查。每一条设置项都标注了旧位置 → 新位置 + 理由。完整的迁移映射（200+ 项）详见独立核实文档。

### B.1 当前 Tab 结构

**当前右侧栏 Tab 顺序**（`menus_settings.cpp:4774-4785`）：GENERAL → TEE → APPEARANCE → CONTROLS → GRAPHICS → SOUND → ASSETS → DDNET → TCLIENT → QMCLIENT

**子 Tab**：
- Appearance（6 子 tab）：HUD / Chat / Name Plate / Hook Collisions / Info Messages / Laser
- TClient（6 子 tab）：Settings / BindWheel / WarList / BindChat / StatusBar / Info
- QmClient（5 子 tab）：Visual / Function / HUD / Contributors / Config

### B.2 目标 11 Tab

语言 → 社交 → Tee → 视觉 → 图像 → 声音 → 资源 → 控制 → HUD → 功能 → 搜索

### B.3 迁移映射摘要（按旧 tab 分组）

| 旧 Tab | 条目数 | 去向 | 关键迁移 |
|---|---|---|---|
| **GENERAL** | ~19 项 | 控制/图像/功能 | Dynamic Camera→控制；Refresh Rate→图像；Auto-record→功能；文件操作→功能 |
| **TEE** | ~13 项 | Tee | 全部保留 + 合并 Player 身份 + Assets 皮肤选择 |
| **APPEARANCE→HUD** | ~25 项 | HUD/社交 | HUD 显示项→HUD；好友/氏族颜色→社交 |
| **APPEARANCE→Chat** | ~12 项 | HUD/社交 | chat 显示行为→HUD；好友/团队过滤→社交 |
| **APPEARANCE→Name Plate** | ~14 项 | HUD | 全部→HUD |
| **APPEARANCE→Hook Coll** | ~7 项 | HUD | 全部→HUD |
| **APPEARANCE→Info Msg** | ~2 项 | HUD | 全部→HUD |
| **APPEARANCE→Laser** | ~若干 | HUD | 全部→HUD |
| **CONTROLS** | 键位+鼠标 | 控制 | 全部保留 |
| **GRAPHICS** | ~15 项 | 图像/视觉 | 渲染设置→图像；UI 颜色/透明度→视觉 |
| **SOUND** | ~12 项 | 声音 | 全部保留 |
| **ASSETS** | ~5+资源 | 资源/Tee | 皮肤→Tee；其余资源→资源 |
| **DDNET** | ~27 项 | 功能 | 全部→功能 |
| **TCLIENT→Settings** | ~49 项 | 视觉/功能 | 视觉效果类→视觉；预测/冻结类→功能 |
| **TCLIENT→BindWheel** | 绑定轮盘 | 功能 | 全部→功能 |
| **TCLIENT→WarList** | ~8 项 | 社交 | 全部→社交 |
| **TCLIENT→BindChat** | 绑定聊天 | 功能 | 全部→功能 |
| **TCLIENT→StatusBar** | ~10 项 | HUD | 全部→HUD |
| **TCLIENT→Info** | 信息 | 功能 | →功能（关于） |
| **QMCLIENT→Visual** | Qm 视觉 | 视觉 | 全部→视觉 |
| **QMCLIENT→Function** | Qm 功能 | 功能 | 全部→功能 |
| **QMCLIENT→HUD** | Qm HUD | HUD | 全部→HUD |
| **QMCLIENT→Contributors** | 贡献者 | 功能 | →功能（关于） |
| **QMCLIENT→Config** | 配置管理 | 功能 | →功能 |

### B.4 待用户确认的分类边界

1. **旧 UI 导航栏高度**：24px 还是 28px？
2. **文件操作按钮**（Settings file / Saves file / Config directory / Themes directory）：放"功能"还是单独"关于/文件"区？
3. **Chat 显示行为 vs 社交过滤**：显示行为（Show chat / Always show / Font size / Width）→HUD，社交过滤（Friends only / Team only / Team colors）→社交——这个拆法 OK？
4. **StatusBar**（TClient）：放 HUD 还是功能？
5. **WarList**：放社交还是功能？
6. **TClient→Settings 的视觉 vs 功能分类**：视觉效果（Cursor scale / Wheel animate / Tiny Tee / Jelly / Pet / Trail / Rainbow / Outline）→视觉，功能参数（Prediction Margin / Frozen Margin / Uncertainty / Minimum time）→功能——这个边界对吗？

> **注**：完整的逐条映射（每一条设置项的 file:line + 旧 tab + 新 tab）在独立核实文档中，本文档仅提供按 tab 分组的摘要。用户确认分类边界后，再展开为完整的逐条映射表。
