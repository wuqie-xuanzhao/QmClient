# QmClient 设置页 UI 统一 Execution Index

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this index stage-by-stage. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 active 设置页 UI 规格拆成 P0–P7 八个可独立验证、按依赖串行交付的实施阶段，并给执行者一份唯一入口、公共接口锁和验收路由。

**Architecture:** P0 在当前 `dyl_dev` checkout 合并并验证远端基线；P1–P4 依次稳定 theme/card、deck/registry、input/numeric、scroll/dropdown 四组公共契约；P5/P6 只做页面迁移；P7 才扩展到非卡片菜单、性能和最终删除门槛。阶段之间通过本索引列出的 exact interfaces 交接，不允许下游自造兼容 wrapper。

**Tech Stack:** C++、QmUi、DDNet menu/UI、CMake/MSVC、GoogleTest、Python gate、Git。

## Global Constraints

- 唯一权威规格：`docs/superpowers/specs/2026-07-10-QmClient-设置页UI统一与滚动体系规格.md`，当前 `status: active`。
- P0–P7 严格串行；只有同一阶段内明确独立的文档/测试工作可并行，P5/P6 不得早于 P1–P4 全部 exit gate。
- P0 的远端范围固定为 `4f76dcb4e7b5bc86bba9a271b563a8b6a34d53f3^..8ee4fa22ba0172a66605b2c5033f0736d66ced34`：11 个普通提交逐项审查，1 个 merge commit 仅记录路由与覆盖关系。当前 checkout 的用户图标改动及其他既有改动只作边界记录，绝不暂存、回退或混入 P0 提交。
- 每个阶段都采用 TDD：先增加会因缺失目标行为而失败的测试，再最小实现，再 focused green，最后全量验证。
- 生产路径同一职责只保留一个实现；页面切片不得以“公共 wrapper + 旧实现”作为完成状态。
- 当前工作区可能有用户/其他 AI 并行改动；每阶段只暂存其计划明确列出的文件，不 stash、回退或覆盖无关文件。
- 同一 `cmake-build-release` 中 `game-client`、`testrunner`、`run_cxx_tests`、`run_rust_tests`、`package_default` 串行执行。
- 协议、物理、预测、snapshot、Demo/地图/skin/配置格式、回放、rank 和服务端玩法不在本路线。
- 完整组件覆盖 R1、设置 11 tab/Root Panel/L0–L2 R2、Phosphor/MSDF/SDF shader R3 只保留后续专项状态，不属于 P0–P7 完成条件。
- P0–P6 不更新功能版本；P7 在生产迁移、自动回归、独立 review 和自动 gate 收口后执行一次 MMP 版本更新。用户 runtime/视觉验收独立记录为 pending 或 complete，版本更新不代表该矩阵已完成。

---

## 1. 当前接手结论

本规格的生产迁移和自动验证已完成。当前 checkout 已按顺序完成 P0-P7，最后两个架构收口提交为 `5cc856dfa1`（公共布局/排序与旧路径清退）和 `5f62c823e5`（TClient 主页 card height/scroll 双路径清退）。剩余工作只允许是用户执行的 `DDNet.exe` 视觉、交互和真实性能采样反馈；自动证据不得代替或虚报该矩阵。

| 区域 | 当前代码起点 | 第一责任阶段 |
|---|---|---|
| Theme/token | 菜单有 `MenuUiColorSurface`/`MenuUiColorAccent`，QmUi color token 仍静态 | P1：已落地，待最终验收 |
| Card shell | 标准页、QmClient 和 TClient 均由 shared Deck/SettingsCard shell 产生 canonical frame | P1/P5/P6：生产迁移完成 |
| Registry/order | `QmCardRegistry`、`qm_card_order::CModel` 与 `SettingsCardOrderModel()` 是唯一注册/排序事实源 | P2/P6：完成 |
| Search | 全局搜索消费 registry result/navigation target，并通过公共 Deck reveal | P2/P6：完成 |
| Input | 设置页及 P7 目标非卡片菜单均调用 `InputField`；NumericField 持有 slider/input/commit policy | P3/P7：完成 |
| Scroll | `CQmScrollState` 是 offset/target/thumb 唯一状态；adapter 消费 policy 和统一 wheel owner | P4/P7：完成 |
| 标准页 | General、Player、Tee、Graphics、Sound、DDNet、Appearance、Controls 迁移 checker 全量 `clean` | P5：自动完成，用户矩阵待验收 |
| QmClient/TClient | Overview、Contributors、Visual、Functions/HUD、Global Search、TClient 主页和复杂子页均迁公共 Deck | P6：自动完成，用户矩阵待验收 |
| 非卡片菜单/性能 | 服务器、好友、Demo、资产、皮肤、国旗、语言保留列表/网格语义并接入公共 adapter/telemetry | P7：自动完成，用户真实性能采样待验收 |

P0 merge commit 为 `01948ba392`，最终 MMP 版本为 `2.74.24`。当前自动证据记录在 `docs/superpowers/reports/2026-07-11-settings-ui-p7-acceptance.md`；其中 percentile 和游戏内 Actual 字段保持为用户 runtime 待采样，不再用 contract test 冒充实际数据。

### 当前下一步

1. 用户使用 `cmake-build-release/DDNet.exe` 完成报告中的视觉/交互矩阵。
2. 用户按 P7 Task 7 固定场景采样 p50/p95/p99/max/1% low/menu max，并把结果反馈给实现侧。
3. 若用户反馈任何非视觉行为或性能失败，重新进入对应 owner 修复并补自动回归；R1-R3 仍不属于本规格。

### 阶段状态口径

| 阶段 | 当前状态 | 可继续动作 | 不能宣称 |
|---|---|---|---|
| P0 | 显式 merge 与版本基线完成 | 仅保留历史证据 | “用户 runtime 已验收” |
| P1–P4 | 公共 theme/card/deck/input/numeric/scroll/dropdown owner 已完成并被生产消费 | 修复用户反馈时只能改公共 owner | “用户 runtime 已验收” |
| P5 | 八页生产迁移和结构审计完成 | 等用户矩阵 | “视觉/交互已通过” |
| P6 | QmClient/TClient 生产迁移、旧路径删除和高度/滚动 owner 收口完成 | 等用户矩阵 | “视觉/交互已通过” |
| P7 | 非卡片 adapter、telemetry、缓存边界、版本和自动证据完成 | 等用户 fixed-scene 采样 | “真实性能 percentile 已采集” |

## 2. 阶段依赖与计划文件

| 阶段 | 计划 | 进入条件 | 退出条件摘要 |
|---|---|---|---|
| P0 | `docs/superpowers/plans/2026-07-11-QmClient-设置页UI统一-P0-基线与规格收敛.md` | active spec | 11 个普通提交逐项审查、1 个 merge 路由记录、显式 merge、基线报告、default gate、独立 review |
| P1 | `docs/superpowers/plans/2026-07-11-QmClient-设置页UI统一-P1-Theme与SettingsCard基础.md` | P0 exit gate | runtime theme/page/card frame 稳定，Graphics 无旧 shell |
| P2 | `docs/superpowers/plans/2026-07-11-QmClient-设置页UI统一-P2-Deck注册表Search与持久化.md` | P1 exit gate | 公共 registry/model/deck/Search 稳定，Graphics 跨列重启持久化 |
| P3 | `docs/superpowers/plans/2026-07-11-QmClient-设置页UI统一-P3-InputField与NumericField.md` | P2 exit gate | 设置页无 legacy/direct input，delay 不回退旧 slider |
| P4 | `docs/superpowers/plans/2026-07-11-QmClient-设置页UI统一-P4-Scroll与Dropdown.md` | P3 exit gate | 单一 scroll state/policy，真实 popup 首轮 wheel 不泄漏 |
| P5 | `docs/superpowers/plans/2026-07-11-QmClient-设置页UI统一-P5-标准设置页迁移.md` | P1–P4 全部 exit gate | 八个标准页逐页迁移并删除旧路径 |
| P6 | `docs/superpowers/plans/2026-07-11-QmClient-设置页UI统一-P6-QmClient与TClient迁移.md` | P5 exit gate | QmClient/TClient Deck 接入，私有 coordinator/shell/cache/height/Search 清退 |
| P7 | `docs/superpowers/plans/2026-07-11-QmClient-设置页UI统一-P7-非卡片菜单性能与最终收口.md` | P6 exit gate | 非卡片 adapter、性能、全局删除门槛、版本与最终证据收口 |

顺序固定为：

```text
P0 -> P1 -> P2 -> P3 -> P4 -> P5 -> P6 -> P7
```

P5 与 P6 虽然页面文件不同，但不能并行：P6 必须继承 P5 已验证的页面迁移模式与结构 gate，避免在 QmClient/TClient 重新发明 wrapper。

## 3. 公共接口锁

下表是跨计划名称与所有权的唯一解释。若 P0 merge 证明某个签名无法兼容，必须先更新 active spec、本索引和所有消费计划，再开始实现；不得只在代码中改名。

| Owner | Exact interface | 下游用法 |
|---|---|---|
| P1 | `SUiTheme ResolveUiTheme(ColorHSLA BaseColor, float Opacity)` | 所有 primitive 从 `IUiContext::m_pTheme` 取运行时颜色 |
| P1 | `SSettingsPageLayoutFrame ResolveSettingsPageLayout(const CUIRect &PageRect, bool HasSubTabs, float UiScale = 1.0f)` | 页面只消费 scroll/content/subtab/column frame |
| P1 | `SSettingsCardFrame BuildSettingsCardFrame(const CUIRect &, const SSettingsCardSpec &, float, float)` + `SCardMotionSpec ResolveCardMotionSpec(int, bool)` | `SettingsCardGeometry.cpp` 纯 owner；`QmAnimTest` 只链入该 owner |
| P1 | `SSettingsCardFrame SettingsCard(const IUiContext &, const CUIRect &, const SSettingsCardSpec &, const SSettingsCardVisualState &, const SSettingsCardDeckVisualOptions &, const FSettingsCardMeasure &, const FSettingsCardRender &)` | client-only 唯一 shell；canonical `m_Rect` 服务 display/hit/drag/proxy，统一绘制 handle/subtitle/feedback |
| P2 | `SettingsCardDeckLogic.h/.cpp` 的 order/drop/auto-scroll 纯函数 | 唯一进 `TESTS_EXTRA` 的 Deck owner；完整 renderer 不进 `testrunner` |
| P2 | `SSettingsCardDeckResult CSettingsCardDeck::Render(const IUiContext &, const SSettingsPageLayoutFrame &, const char *, const std::vector<SSettingsCardDefinition> &, qm_card_order::CModel &, CScrollRegion *, const SSettingsCardDeckInput &, const SCardMotionSpec &, const SSettingsCardDeckVisualOptions &)` | Graphics 在 P2 消费；QmClient/TClient 到 P6 才调用 |
| P2 | `CMenus::SettingsCardOrderModel() -> qm_card_order::CModel &` | Qm/标准/TClient 共用一个 model 与 `qm_global_card_order` |
| P2 | `std::vector<qm_card_registry::SCardSearchResult> qm_card_registry::SearchCards(const char *, const qm_card_order::CModel &)` + `void CMenus::NavigateToSettingsCard(const qm_card_registry::SCardNavigationTarget &)` | Search 从 registry 取文案、从全局 model 解析当前 tab，不复制视觉/frame/order |
| P3 | `SInputFieldResult ui_widget::InputField(const IUiContext &, CLineInput *, const CUIRect &, const SInputFieldOptions &)` | 普通、搜索、clear、icon、多行共用 shell/content rect |
| P3 | `SNumericFieldResult ui_widget::NumericField(const IUiContext &, SNumericFieldState &, const void *, int *, int, int, const CUIRect &, const SNumericFieldOptions &)` | 整数、小数、`∞`、unit、slider、delay commit 共用状态 |
| P4 | `CQmScrollState` + `static SQmScrollContainerFrame CQmScrollController::Update(CQmScrollState &, const CUIRect &, float, float, const SQmScrollContainerInput &, const SQmResolvedScrollPolicy &)` | adapter 不拥有 offset/animation/drag 同义状态 |
| P4 | `QmResolveScrollPolicy(const SQmScrollRequest &, float, float)` | settings/list/popup/filter/numeric 五类 profile |
| P4 | `void CUi::RegisterWheelOwner(const void *, EUiWheelOwnerPriority, const CUIRect &, bool)` + `bool CUi::TryConsumeWheel(const void *, float *)` | popup/composite/page 竞争同一 raw wheel |
| P7 | `void QmLogMenuUiFramePerf(const SQmMenuUiFramePerf &, const IClient *)` | 只格式化并转发到既有 `QmPerfLogPayload(...)`，不创建第二个 logger |

禁止下游重新引入这些过渡名称：

```text
RenderQmSettingsGlassCard
BeginSettingsCardDeck
BeginSettingsCardDeckCard
RegisterSettingsCardDeckItemFromFrame
LegacyTextFieldEx
SliderInputField
DoSettingsSliderInputField
CScrollRegion 自有 m_ScrollPos/m_AnimTargetScrollPos
```

P3 允许非卡片菜单的 `TextFieldEx`/Search/Clear/Icon forwarding alias 暂存到 P7，但 alias 只能调用 `InputField(...)`，不能绘制第二个 shell。

`m_SettingsCardDeckOrders`、`m_TClientSettingsCardDragState` 及 QmClient/TClient 私有 drag/drop/order/shell/cache/Search 允许保留到 P6，但 P2 之后两页不得出现“已调用公共 `Render(...)` 又保留私有 coordinator”的半迁移状态。

## 4. 每阶段共同执行节奏

每个阶段按以下 checkpoint 推进，不把多个阶段合为一个不可审查的大提交：

1. 读取该阶段计划、active spec 和最小 `docs/ai-workflow/` 路由。
2. 用 CodeGraph 重查计划列出的生产符号；若出现 pending-sync banner，只读 banner 指定的 changed files。
3. 写该 task 的失败测试，重建 `testrunner`，运行 exact filter 并确认预期红灯。
4. 实现最小行为，运行同一 filter 绿灯，提交该独立 task。
5. 完成阶段所有 task 后，串行运行 `game-client`、`run_cxx_tests`、docs check 和计划指定的 quick/default gate。
6. 执行阶段人工矩阵，未运行项写为 gap。
7. 核心逻辑阶段派发新的只读 review；等待完整 findings-first 报告，修复后重跑全量阶段 gate。
8. 更新阶段证据，不把“build 完成”写成“全部验证完成”。

## 5. 阶段级验证底线

| 阶段 | Focused evidence | Full evidence | Manual evidence |
|---|---|---|---|
| P0 | animation merge anchor | game-client、run_cxx_tests、run_rust_tests、check_docs、default gate | merge 冲突/版本 gap 记录 |
| P1 | theme/layout/card/Graphics | game-client、run_cxx_tests、check_docs、default gate | Graphics 色彩、单双列、motion |
| P2 | registry/model/deck/Search | game-client、run_cxx_tests、check_docs、default gate | cross-column、restart、Search reveal |
| P3 | input/numeric/delay | game-client、run_cxx_tests、check_docs、default gate | IME、focus、整数/小数/∞、窄宽 fallback |
| P4 | scroll/policy/wheel/dropdown | game-client、run_cxx_tests、check_docs、default gate | 五类 profile、短/长 popup 首轮 wheel |
| P5 | 八页结构与行为清单 | game-client、run_cxx_tests、check_docs、default gate | 八页 × viewport/UI scale/语言 |
| P6 | Qm/TClient shell/cache/height | game-client、run_cxx_tests、check_docs、default gate | 主页面与全部复杂子页 |
| P7 | non-card/perf/deletion | game-client、run_cxx_tests、run_rust_tests、check_docs、default gate；full 只作附加 | 长会话、perf 固定场景、最终全菜单矩阵 |

任何阶段若跳过 full evidence 中的一项，只能交接为 incomplete/gap，不能勾选阶段 exit gate。

## 6. R1–R3 后续专项登记

| 专项 | 内容 | 为什么不进入 P0–P7 |
|---|---|---|
| R1 · 公共组件覆盖 | SegmentedControl、ColorPicker shell、Toggle、Button、slider、modal、toast、font icon API | 当前路线只保证 theme token 接入和设置页需要的 input/numeric/scroll/card，不批准全组件重做 |
| R2 · 信息架构与完整层级 | 11 tab、导航枚举迁移、Root Panel、完整 L0/L1/L2 | 会改变用户导航、配置值和 Search 语义，需要重新设计与单独批准 |
| R3 · 画质与渲染管线 | Phosphor/MSDF、SDF 圆角/文本、shader command、GL/Vulkan | 涉及资产、上游引擎核心和跨后端风险，需要独立规格/授权 |

P7 最终报告只引用本表，不得把 R1–R3 未实施写成 P0–P7 功能 gap，也不得为了“旧文档提过”顺手开工。

## 7. 总完成门槛

- [ ] P0–P7 八份计划的 completion/exit gate 全部完成，且顺序可由 commit history 追溯。
- [ ] 设置页 card/theme/layout/deck/registry/Search/input/numeric/scroll/dropdown 每类只有一个生产事实源。
- [ ] General、Player、Tee、Graphics、Sound、DDNet、Appearance、Controls、QmClient、TClient 全部完成结构删除与人工矩阵。
- [ ] 服务器、好友、Demo、资产、皮肤、国旗、语言保留列表/网格语义并使用公共 adapter。
- [ ] `RenderQmSettingsGlassCard`、TClient cache box/private drag、设置页 direct edit/old slider、scroll 双状态和临时 alias 均从目标生产路径删除。
- [ ] 全量 C++/Rust、docs、default gate、独立 review 和性能固定场景均有当前 commit 的证据。
- [ ] 只允许在最终报告中保留明确记录的视觉 gap；任何非视觉 finding、测试失败或结构双路径都必须先修复。
- [x] MMP 版本在 P7 生产迁移和自动验证完成后更新一次；用户 runtime/视觉验收继续独立追踪。

---

## Self-review

- Spec coverage: P0–P7、旧主题映射、R1–R3、自动验证与人工矩阵均有明确 owner。
- Marker scan: 未发现未决占位、虚构版本或未定义阶段。
- Type consistency: 公共接口名称与 P1–P4 计划一致；P5–P7 不拥有 primitive 定义权。
- Execution safety: P0 先合并，P1–P4 稳定契约，P5/P6 串行迁移，P7 最终收口；无跨阶段并行写同一生产文件。
