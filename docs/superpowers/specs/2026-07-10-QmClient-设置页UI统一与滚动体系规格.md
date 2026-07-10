---
type: design-specification
date: 2026-07-10
status: draft
scope:
  - 全部设置页、QmClient、TClient 和设置搜索
  - 所有菜单对公共输入、滚动、dropdown、动效和性能基础设施的复用
authority: 本规格、当前代码审计、GitHub 合并前审查、人工验收
---

# QmClient 设置页卡片平台与菜单 UI/UX 重新设计规格

## 1. 文档地位与完成口径

本文件重新设计 QmClient 菜单 UI/UX，不继承历史文档中的视觉、组件或“已完成”结论。`docs/superpowers/specs/archive/` 中的资料仅保留决策历史，不可作为实现依据。

当前栖梦页面不是最终视觉基准。它只提供已经确认值得继承的内容密度、标题层次、卡片化信息组织、持续彩虹标题和拖拽使用场景。其现有 glass、输入框、滚动条、布局、动效和拖拽实现均须经本规格重新定义后才能保留。

一项工作只有同时满足下列条件才可标为完成：

1. 生产代码只使用本规格定义的公共 primitive；同一职责不存在新旧双路径。
2. 所有对应旧 helper、页面私有样式和兼容 wrapper 已删除，或存在明确的非设置页豁免。
3. 行为测试、结构测试、`game-client` 构建和相应 gate 通过。
4. 使用 `cmake-build-release/DDNet.exe` 完成人工视觉与交互验收，未验收项目明确保留为 gap。

构建、单页截图接近预期、或结构字符串测试通过，均不构成组件体系完成的证据。

## 2. 范围与边界

### 2.1 设置卡片平台

卡片是全部设置页的基本信息和交互单位，覆盖 General、Graphics、Sound、DDNet、Appearance、Controls、QmClient、TClient 及其子页。每张设置卡片均可拖拽、跨列移动、重新排序和持久化；Search 通过同一张全局卡片注册表索引和跳转。

宽屏默认双列，窄屏自动单列。拖拽可跨列，持久化顺序以最终视觉阅读顺序为准。页面顶部子 tab 始终占内容区全宽，不属于卡片网格，也不可拖拽。

### 2.2 非卡片菜单页

服务器、好友、Demo、资产、皮肤、国旗和语言等页面保留列表或网格语义，不把业务条目伪装成可拖拽设置卡片。它们必须复用公共 token、输入框、滚动、dropdown、动效 runtime 和性能规则，但不接入设置卡片排序模型。

### 2.3 不在范围内

本规格不改变协议、Demo/地图/配置文件格式、物理、预测、回放、编辑器业务行为或聊天/控制台文本滚动语义。远端合并带来的非 UI 功能必须独立审查，不因本规格自动纳入重构。

## 3. 设计原则

1. **一张卡片，一个真相源。** 卡片的可见 rect、内容 rect、命中 rect、拖拽源 rect、proxy 和 drop indicator 来自同一次最终布局。
2. **单一视觉外壳。** 设置页禁止多层 glass、backdrop blur、白雾覆盖或页面私有 card wrapper。
3. **先布局，后保底。** 文本缩放、单列展开和两行数值控件是安全兜底，不是用来掩盖错误列宽、label 宽度或间距的常态布局。
4. **交互优先于装饰。** 拖拽、释放、焦点和 popup 反馈始终可用；装饰性运动可按用户配置减少或关闭。
5. **公共接口优先于视觉相似。** 两页看起来一样但改一处不能同步，仍视为未统一。
6. **滚动与文本是热路径。** 设计必须说明缓存、失效和预算，不能仅通过减少视觉效果回避性能问题。

## 4. 视觉语言与 token

### 4.1 Card surface

所有设置卡片使用单层、稳定的深灰/灰色 surface。常驻细边框提供结构；hover、focus 和 drag 仅增加边框可见性与克制的表面亮度。禁止旧液态玻璃、多层透明壳、额外 blur 和白雾叠加。

卡片标题、正文、控件、状态标签、按钮和滚动条均从统一 token 链路取色、圆角、边框和透明度。页面不得手写同类颜色、半径、padding 或高亮。

### 4.2 Typography 与间距

卡片内文本仅允许三档：

| 档位 | 用途 | 规则 |
|---|---|---|
| `Title` | 卡片标题、少量分组标题 | 单独字重、行高、标题到内容间距 |
| `Body` | 正常设置行、数值、选择项 | 默认正文档，所有业务页一致 |
| `Small` | 副标题、说明、辅助状态 | 不取代正文，不与 `Body` 混用 |

每档统一字号、行高、字重和颜色。row height、row gap、section gap、label/value 对齐、标题顶部距离、header 高度、卡片 padding 和列 gap 全部是 token；业务页禁止硬编码正文大小与行间距。

文本可在同一档的安全范围内自适应缩小，作为长本地化、极端 UI scale 或最长单词的保底。触发缩放、截断或非预期换行即表明布局设计存在问题，必须记录到验收台账，并优先调整卡片宽度、双列阈值、label/value 分配或内容分组。不得把“缩小后能放下”视作完成。

### 4.3 Header

全部设置卡片具有统一 header：标题、可选副标题和统一 drag handle。副标题只在 hover/focus 时出现；没有副标题时不预留空白。标题默认持续彩虹流动，`qm_ui_card_rainbow_titles` 关闭后使用静态高可读颜色，不改变 header 尺寸或层级。

## 5. Settings Card Platform

### 5.1 `SettingsPageLayout`

唯一页面布局入口负责 top inset、side inset、scroll viewport、内容 top inset、全宽子 tab、双列阈值、列宽、列 gap 与 card gap。业务页只接收内容 viewport 和卡片 slot，不自行计算设置页卡片宽度、顶部间距或滚动区域。

### 5.2 `SettingsCard`

`SettingsCard` 是唯一 card shell。输入为 stable ID、标题、副标题、内容测量回调、页面 slot、drag model 和 token；输出不可分裂的 `SSettingsCardFrame`：

```text
display rect == hit-test rect == drag item rect == proxy source rect
frame -> header rect + title rect + subtitle rect + handle rect + content rect
```

shell 负责 surface、border、hover/focus、标题、副标题、handle、entry transform、drag proxy 和 drop indicator。业务页面只能在 `content rect` 中绘制实际内容。卡片高度由最终内容测量结果产生，不允许使用上一帧缓存高度先绘一层 card、再用另一层内容覆盖。

### 5.3 `SettingsCardDeck` 与搜索

`SettingsCardDeck` 是唯一排序、跨列拖拽、自动滚动、让位和顺序持久化协调器。它使用全局 stable ID 和现有卡片顺序模型，但必须消除 QmClient、TClient 与页面私有的并行拖拽状态。

Search 仅索引 `SettingsCard` 注册的 stable ID、标题、描述和导航目标；Search 不拥有第二套 card 视觉、尺寸、拖拽或排序模型。

### 5.4 必须清退的路径

下列符号反映当前已被截图证伪的分叉实现，迁移到对应页面时必须删除：

- `RenderQmSettingsGlassCard` 及其旧 glass/backdrop 行为。
- `TClientCacheSectionBoxRect`、`InsetTClientCacheSectionContent`、`DrawTClientCacheSectionBox`。
- `m_TClientSettingsCardDragState` 和页面私有 drop indicator / 6-dot handle。
- 页面私有 card padding、title offset、card 宽度、缓存高度或私有 box rect。

## 6. 动效与 UI 配置

### 6.1 统一状态模型

每张 `SettingsCard` 均使用同一 `SCardMotionSpec`，只能引用公共参数，不得自行创建渲染或动画 runtime。公共状态为：`hover`、`focus`、`entry`、`drag start`、`displaced`、`drop`、`reflow complete`。

拖拽、让位和高度变化必须支持目标重定向，保留速度连续性，防止卡片跳变、重叠、慢速漂移或释放后回弹错误。GitHub 远端 `d161bd10adffdff42a9b85d09f7336a64a708f60` 中的 `ResolveUiPresentationStateValue(...)` 是候选运行时能力；合入后须独立测试，不能将其现有调用效果直接视为规格。

### 6.2 必要与装饰动效

必要动效始终保留：hover/focus 边框反馈、drag proxy、让位重排、drop/释放、dropdown 状态变化。即使全局动效减弱，也不能改变 hit-test、布局、持久化或交互完成时机。

首次进入设置页、切换子 tab 或 card 首次插入当前 deck 时，可播放统一的短距离漂移加透明度 entry 动效。entry 必须在最终布局完成后仅作为绘制 transform/alpha 应用；它不改变命中或拖拽 rect。同一 `page/tab/card stable ID` 在一次展示周期只播放一次，滚动、重排、文本刷新不得重复触发。

持续彩虹标题和表面微光属于装饰动效，由 `qm_extra_animations` 控制。`qm_ui_motion_level` 统一缩短或关闭非必要 tween/spring。公开 UI 配置至少包含：额外动画、卡片标题彩虹流动和动效强度。页面不得自行读取这些配置并产生不同解释。

## 7. 公共输入与数值控件

### 7.1 `InputField`

设置页文本输入统一使用一个可组合 `InputField` shell。底层 `CUi::DoEditBox` 只负责文字、光标、选择、IME 和编辑状态，不绘制独立背景或 focus 填充。所有普通、清除、搜索、整数和 slider+input 的文本区域都必须通过 shell。

输入 shell 的参数分为四层：

1. **外观**：`Small`/`Body` 文字档位、灰色 plate、圆角、统一 content inset、粗外框 focus ring。
2. **前后 affordance**：可选左 icon、可选右侧 `X` 清除；无图标时不预留 slot。
3. **格式**：文本、整数、小数、整数加 `∞`、小数加 `∞`；数值格式定义范围、精度、单位与 `∞` 的存储/显示映射。
4. **交互**：placeholder、搜索 hotkey、提交/失焦提交、只读、IME、选区、光标和无效输入反馈。

非激活与激活背景保持相同灰色。激活态只绘制与 field 外壳同圆角的粗外框高亮；禁止浅蓝实心背景、过细描边、内缩短框、不同圆角和 placeholder 重绘。focus ring 的颜色、宽度、inset 和 radius 是单一 token，不得分控件重画。

左图标与右 `X` 均占用随 field height 计算的正方形 icon slot，图标在其中居中；slot、图标尺寸和安全边距由 token 与 UI scale 计算。文本 content rect 从 slot 外边界开始。不得通过页面手写 margin 调整搜索图标位置。

Controls 的垃圾桶是删除绑定的 destructive action，不属于输入框的 `X` 清除 affordance。它作为独立 action button，与按键识别组件共享行高和间距 token，但不进入 `InputField` API。

### 7.2 `NumericField` 与 slider+input

除颜色选择器的专用颜色轨道外，设置页数值调节统一为 `NumericField`：

```text
label + horizontal slider + InputField(integer/decimal/infinite) + optional unit
```

延迟提交是 `NumericField` 的 commit policy，不能再用 `SCROLLBAR_OPTION_DELAYUPDATE` 绕回 `CUi::DoScrollbarOption` 或旧 `DoScrollbarH` 绘制路径。`∞` 可被显示和输入；仅有滑块时最右端显示 `∞`，带输入框时输入框接受数字和 `∞`。

横向 slider 使用统一的横向短条视觉。轨道被压缩成球状或近似方块不是 thumb 造型问题，而是卡片内部可用宽度错误。应先纠正 label 长度、label/value 分配、无效中间空隙、card 宽度或内容分组；最小轨道宽度、单列占位和两行布局只是安全保底。触发保底时必须登记为布局反馈，不可作为常态设计。

## 8. 公共菜单滚动与 Dropdown

### 8.1 唯一滚动内核

所有菜单滚动通过统一 controller/policy 解析 overflow、clip、wheel owner、目标平滑、rail geometry 与 keyboard navigation。`CScrollRegion`、`CQmScrollController`、`CListBox`、设置页容器和 popup 只能作为适配层，不得继续拥有页面私有 wheel、thickness、margin、动画或 `ForceShowScrollbar` 行为。

视觉 preset 仅有 `large`、`medium`、`small`、`horizontal`：

| 用途 | preset | 交互 |
|---|---|---|
| 设置页主滚动 | `large` | DDNet cubic smooth，以 Controls 手感为参照 |
| 菜单列表 | `medium` | 由行高与每次行数解析步长 |
| popup 与复合控件内滚 | `small` | 至多八项可见，按行滚动 |
| 数值 slider | `horizontal` | 由 `NumericField` 统一管理 |
| 国旗等筛选网格 | `small` + hidden rail | 两行步长，仍保留 wheel/键盘/clip |

rail 只有 `AUTO` 与 `HIDDEN`。`AUTO` 仅真实 overflow 时显示；没有 overflow 时不得绘制、预留宽度或扩大热区。`HIDDEN` 不绘制也不预留 rail，但仍可滚动，热区只能为实际 clip。

`Alt + 滚轮` 为 3 倍加速。Ctrl/GUI/Shift 不得吞掉滚轮。视觉宽度、颜色、亮度与滚轮速度是独立 token，不能互相绑定。

### 8.2 Dropdown

短列表最多显示八项，popup 保持打开，wheel 交给父页面；长列表或受父 viewport 限高时，popup 消费 wheel 并显示 `small` rail。几何、裁剪、命中与 wheel ownership 都以 anchor 所在父 viewport 为边界；anchor 完全离开才关闭，部分可见时保持。

popup 若接管滚轮，必须在同一帧、底层页面 scroll region 消费前登记 owner。不得依赖下一帧 hot-region，更不能通过“滚轮关闭 popup”规避问题。该规则直接覆盖 Graphics dropdown 滚动 rail 闪烁和首个滚轮泄漏问题。

## 9. 性能与观测

性能规则对全菜单生效，不只覆盖设置页：

- page layout、card measure、文本 layout、搜索索引和动画 track 按明确失效条件缓存。
- 滚动、拖拽和 popup 展开帧不得堆分配、重复文本 layout、全量 card 扫描或重复排序。
- 文本缓存、资源预览、列表可见性、动画 target cache 都必须有容量和清理边界，避免长期会话无界增长。
- 公共 runtime 提供可计数指标与固定场景预算；真实滚动、拖拽、长列表、文本本地化和 UI scale 是性能验收输入。
- 任何因性能跳过渲染、延迟布局或缓存复用造成的重叠、旧文本、错误 rect 或输入错位都视为功能 bug，不可被“性能优化”豁免。

## 10. GitHub 远端整合前置条件

实现本规格前，必须先整合 GitHub `wxj881027/QmClient` 的 `master` 相对 `dyl_dev` 的 10 个未合并提交：`4f76dcb4e7b5bc86bba9a271b563a8b6a34d53f3..8ee4fa22ba0172a66605b2c5033f0736d66ced34`。

流程为：更新远端引用，逐提交审查，建立当前工作区的可验证检查点，然后显式 merge 到 `dyl_dev`。禁止 rebase。冲突按本规格解决，不能为了通过 merge 同时保留远端能力和本地旧 UI wrapper。版本号文件需独立检查，避免倒退或重复。`d161bd1` 的动画、输入、token、菜单和配置改动必须按实际 diff 审查；`8f2446b` 的编辑器术语重命名不属于本规格功能范围。

## 11. 当前代码基线与阻断项

以下为 CodeGraph 与 GitHub 代码审计得到的现状，不是目标实现：

| 编号 | 当前事实 | 阻断要求 |
|---|---|---|
| B1 | `RenderQmSettingsGlassCard`、旧 deck 和 6-dot handle 仍在生产路径。 | 迁移页面时删除旧 shell/handle。 |
| B2 | TClient 仍有 cache box、私有 inset、私有 section rect 与 drag state。 | 使用 `SettingsCard` 最终 frame，删除双层绘制。 |
| B3 | `SCROLLBAR_OPTION_DELAYUPDATE` 会绕过公共 slider+input，导致轨道存在但输入消失。 | commit policy 下沉到 `NumericField`。 |
| B4 | 输入 shell/legacy/icon/numeric 路径多套，focus、placeholder、光标和 inset 不同源。 | 统一 `InputField` 与 content rect。 |
| B5 | `CScrollRegion` 与 `CQmScrollController` 两套状态机并存，`CListBox` 仍有静默忽略的 `ForceShowScrollbar`。 | 统一 policy，删除无效 API。 |
| B6 | popup 在页面滚动输入之后渲染，长 popup 首个 wheel 可泄漏给父页面。 | 增加同帧 wheel owner。 |
| B7 | 卡片文本、间距、宽度、TClient 高度与拖拽在各页割裂。 | layout/token/card shell 同源后逐页验收。 |
| B8 | 历史文档与结构测试曾把局部 wrapper 当成完成。 | 测试必须断言生产路径删除和真实交互。 |

## 12. 迁移与删除顺序

1. 合并并审查远端 10 个提交，恢复可构建的单一基线。
2. 建立 token、`SettingsPageLayout`、card motion runtime 与公共输入/scroll API；先写红灯结构和行为测试。
3. 实现唯一 `SettingsCard` / `SettingsCardDeck` / Search 注册，迁移 TClient 并删除其 private cache box、drag 和旧 glass。
4. 收口 `InputField`、`NumericField`、`∞`、delay commit 和 Controls action row；清点所有设置页直接 `DoEditBox` / `DoScrollbarH`。
5. 收口滚动内核、popup 同帧所有权、overflow 和 preset；迁移设置页、列表页和 filter grid 适配器。
6. 逐页迁移全部设置页，完成每页截图验收后删除该页旧 layout/card/drag/input/scroll 路径。
7. 最后复审性能、Search、跨列持久化、入口动效和全菜单公共组件使用情况，删除临时兼容别名。

不允许以“先套公共 wrapper，旧路径以后再删”结束任一页面切片。

## 13. 验证与人工验收

### 13.1 自动验证

每个迁移切片先增加失败测试，再实现最小路径。测试至少覆盖：

- card display/hit-test/drag/proxy rect 同源、跨列拖拽与持久化、entry 一次性触发。
- 三档字体、长本地化/UI scale 的保底触发和布局反馈记录。
- 灰色输入背景、粗外框 focus、icon slot、placeholder、IME、光标、清除 `X`。
- 整数/小数/`∞`、单位、delay commit、slider 最小轨道与两行安全降级。
- overflow rail 显隐、Alt 3 倍、hidden rail、列表行步长、popup 八项上限、首轮 wheel ownership 和父 viewport clip。
- 禁止旧 glass/TClient cache/private drag/`ForceShowScrollbar`/设置页直接旧 input 与 slider 路径的结构检查。

最终验证使用仓库规定的 `cmake-windows.cmd` 构建入口，串行运行全量 C++ tests、docs check、`git diff --check` 和适用的 default gate。核心逻辑改动完成后必须有独立只读 review；review finding 收口前不得提交。

### 13.2 人工矩阵

| 页面/场景 | 验收重点 |
|---|---|
| QmClient、Appearance | 单层 card、持续彩虹标题、三档字体、header、副标题和跨列拖拽。 |
| TClient 及子页 | 无双层 card、透明竖块、文本逃逸或错误高度；让位/释放一致。 |
| Graphics、Controls、Sound | slider+input、`∞`、单位、focus ring、无球状横向轨道。 |
| Search | 搜索结果与设置卡同源，跳转正确，无重复/英文占位文本。 |
| 短/长 dropdown | 父 clip、rail、wheel ownership、无闪烁。 |
| 国旗、语言、服务器、好友 | list/filter profile、hidden rail、Alt 加速和性能。 |
| 非默认 UI scale、本地化长文本 | typography 保底、卡片宽度、行高与控件不重叠。 |

每项记录页面、viewport/UI scale、操作、截图或结果，以及未验证项。未人工验收的项目必须以 gap 交接。

## 14. 规格自审

- 无 `TODO`、`TBD` 或“以后决定”的实施占位。
- 卡片仅限设置页与设置搜索；列表页不被错误卡片化。
- 栖梦当前实现只作内容密度参考，不被定义为最终视觉真相。
- 远端合并、动效、输入、滚动和性能均有明确所有权与删除门槛。
- 设计与当前发现的旧 glass、TClient 双路径、delay-update、scroll 双状态机和 popup wheel 泄漏相一致。
