---
type: quality-audit
date: 2026-07-14
updated: 2026-07-14
status: active
mode: implementation-followup
baseline: working-tree-vs-HEAD
branch: dyl_dev
commit: 3cbef99a14
skill: docs/superpowers/prompts/audit-qmclient-quality/SKILL.md
authority: 当前代码、有效规格、gate 实现；原始只读 finding 已按实施结果复核
scope:
  - src/game/client/QmUi/SettingsCard*
  - src/game/client/components/menus*.cpp
  - src/game/client/components/qmclient/menus_qmclient.cpp
  - src/game/client/components/tclient/menus_tclient.cpp
  - qmclient_scripts/gate/
  - AGENTS.md
  - docs/ai-workflow/
  - docs/superpowers/explore/
  - docs/superpowers/specs/2026-07-10-QmClient-设置页UI统一与滚动体系规格.md
---

# QmClient 质量审计报告（当前 diff · 只读）

> 2026-07-14 实施复核：拖拽契约已校准为标题行热区且右上角只保留折叠按钮；彩虹标题已与额外动画解耦；`settings_ui` trigger 已覆盖公共卡片、输入框、滚动物理和 TClient。原始 findings 保留作为审计记录，以上三项不再是当前阻断项。Profiles/Configs 旧默认布局迁移、Tee 单卡搜索元数据、输入框单层 focus ring、Alt 1:3 native-step、Voice attempt 去重及独立动效卡已落地，最终状态以本轮全量测试和复审结果为准。

## 任务参数

| 项 | 值 |
|---|---|
| 模式 | 只读报告（审计时未改代码） |
| 范围 | working tree vs `HEAD`（`dyl_dev` @ `3cbef99a14`） |
| 平台 | 受影响：Windows 优先；无跨平台实测 |
| finding 上限 | 10 |
| 主代码面 | SettingsCard 平台、menus 设置页、gate 重构、文档/explore 治理 |

## 路由结果

| 高风险域 | 理由 | 模板 |
|---|---|---|
| 设置卡片平台 UI/交互 | `SettingsCard*` / Qm / TClient / 配置文案 diff | 临时功能模板「设置卡片平台」+ **2.1** + 局部 **3.10** |
| gate / 测试合同 | `check_gate` 大改、`settings_ui` 新增、`check_docs` 删除 | **2.15** |
| 文档/规格漂移 | AGENTS/verification、explore 归档、规格 vs 实现 | **2.16** |

未选：Jobs/网络/预测/i18n 主链（本 diff 无实质触及）。

## Findings（按严重度）

### [重要] SettingsCard 删除 canonical drag handle，与有效规格冲突

**复核：已解决。** 产品契约明确选择标题行拖拽，不恢复独立三点 handle；有效规格和测试已同步。

- 文件：`src/game/client/QmUi/SettingsCard.cpp`（删除 `RenderCanonicalSettingsCardHandle`）
- 规格：`docs/superpowers/specs/2026-07-10-QmClient-设置页UI统一与滚动体系规格.md` §4.3 / §5.2
- 触发条件：打开任意标准设置页卡片；无 `HeaderAction` 时 header 可拖但无可视 handle
- 问题：有效规格要求 shell 绘制唯一 drag handle；本 diff 移除三点 handle，测试改为断言无 handle。Deck 拖拽仍用 `m_HeaderRect`，`m_HandleRect` 留给 collapse 按钮
- 玩家影响：拖拽可发现性下降；按规格验收会判定未完成
- 代码证据：`SettingsCard.cpp` 不再调用 handle 绘制；`SettingsCardDeck.cpp` 仍用 header 命中；Qm 折叠占用 `m_HandleRect`
- 最小建议：二选一——恢复 shell handle；或改规格为「header 整区可拖 + handle 仅 header action」，并补 hover 提示
- 验证方式：人工打开 Graphics/Qm Visual；无 handle 时尝试拖拽
- 结果：已确认

### [重要] `settings_ui` 触发面与审计面错位

**复核：已解决 trigger 缺口。** 当前 trigger 已包含 `SettingsCard*`、`UiForms`、`QmScroll` 和 `menus_tclient.cpp`；行为覆盖仍由 C++ 测试与 default gate 共同承担。

- 文件：`qmclient_scripts/gate/checks/settings_ui.py:13-22`；`qmclient_scripts/gate/check_settings_ui_migration.py`（仅 P5 八页）
- 触发条件：只改 `SettingsCard*` / `menus_tclient.cpp`；或只改 Qm Visual/Function/HUD
- 问题：`TRIGGER_PATHS` 仅 4 文件，不含 `SettingsCard*`/`menus_tclient`。改 `menus_qmclient` 会跑 `--all`，但只审 P5 合同，不审 Qm/TClient 页
- 玩家影响：设置平台/TClient 回退时 gate 仍可 `PASS`
- 代码证据：实测 `SettingsCard.cpp` / `menus_tclient.cpp` → `should_run=False` → `NOT_APPLICABLE` → outcome `PASS`
- 最小建议：扩展 TRIGGER 与审计页，或拆「平台合同 / P5 合同」；合同测试锁定 TRIGGER 集合
- 验证方式：mock `changed=[menus_tclient.cpp]` / 删 Qm `m_SettingsCardDeck.Render` 后看 gate
- 结果：已确认

### [重要] 删除 `check_docs` 后活跃入口仍断链

- 文件：`.claude/settings.json:9`；`docs/QmClient_docs/维护说明.html:103`；`docs/QmClient_docs/调研/脚本工具状态.html:25,48`；多份 `docs/superpowers/plans/2026-07-11-QmClient-设置页UI统一-P*.md`
- 触发条件：按上述文档/Claude hook 跑文档治理
- 问题：`check_docs.py` / `agents_sync` 已删；AGENTS/verification 已改人工核对，但 hook 与 QmClient_docs/plan 仍写旧命令
- 玩家影响：无运行时；验收假失败/跳过，文档漂移无人拦
- 最小建议：修/删 `.claude` hook；活跃文档改「人工清单」或 scope-aware docs check；plan 命令改 `check_gate`
- 验证方式：`Path('qmclient_scripts/gate/check_docs.py').exists()` + `rg check_docs`
- 结果：已确认

### [重要] 规格仍要求彩虹受 `qm_extra_animations` 控制，实现已解耦

**复核：已解决。** 有效规格现明确 `qm_ui_card_rainbow_titles` 独立控制，不再依赖 `qm_extra_animations`。

- 文件：规格 §6.2；`src/game/client/components/menus.cpp` `SettingsCardDeckVisualOptions`；`src/engine/shared/config_variables_qmclient.h`
- 触发条件：按规格验收彩虹；或关额外动画期望彩虹关闭
- 问题：代码已是 `m_RainbowTitles = QmUiCardRainbowTitles != 0`；help/tooltip 去掉「需要额外动画」；规格仍写由 `qm_extra_animations` 控制。i18n 仍残留旧 help key
- 玩家影响：规格误导验收；旧 key 污染翻译维护链
- 最小建议：规格改双开关独立，或恢复代码依赖（二选一）；re-extract/generate 清旧 key
- 验证方式：开彩虹、关额外动画，看标题是否仍彩虹
- 结果：已确认

### [重要] explore archive 仍大量 `status:active`，与 README/AGENTS 冲突

- 文件：`docs/superpowers/explore/README.md`；`docs/superpowers/explore/archive/*`（审计时 **27** 份 `status: active`）
- 触发条件：Agent 按 AGENTS「以 status 判定有效文档」扫描 explore
- 问题：README 声明现行只认 2026-07-14 一份；archive 多数仍 active，可被当实现依据
- 玩家影响：无直接；易带回 FBO/中文 key/过期 perf 等已作废方向
- 最小建议：archive 批量 `status: outdated|archived`；仅 07-14 保持 active
- 验证方式：扫描 archive frontmatter status
- 结果：已确认

### [重要] `scripts_overview` 仍声称 path-trigger 翻译/文档专项

- 文件：`qmclient_scripts/scripts_overview.md:216`；`qmclient_scripts/gate/check_gate.py` `_CHECK_SPECS`
- 触发条件：读者认为改 docs/i18n 会被 gate 自动专项检查
- 问题：gate 唯一 `scope_kind=changed` 专项是 `settings_ui`；docs/i18n path-trigger 已不存在
- 玩家影响：间接——漂移不被 gate 阻断却被表述为已覆盖
- 最小建议：改为「当前仅 settings_ui 为 path-trigger；i18n/文档走独立脚本/人工」
- 验证方式：对照 `_CHECK_SPECS` 与 overview
- 结果：已确认

### [轻微] base-ref 不可用只 WARN，Outcome 仍可 PASS；DEGRADED exit 0

- 文件：`check_gate.py` base-ref 处理；`lib/report.py:outcome`；`test_gate_contract.py`
- 触发条件：merge-base 失败或 `--skip-*` 必需检查
- 问题：不完整 gate 可被当成通过（只看 exit code）
- 最小建议：base-ref 失败 → `DEGRADED`/`SCOPE_DEGRADED`；文档明确 exit≠完整通过
- 结果：已确认

### [轻微] 结构字符串测试保护 token，行为回归可能仍绿

- 文件：`src/test/qm_new_ui_menu_branch_test.cpp`；`src/test/qmclient_monitoring_test.cpp`；对照正面：`settings_card_deck_logic_test.cpp`
- 触发条件：保留 API 字符串但改语义/参数
- 问题：大量 `find("…")` 锁字形；`SettingsCardDeckAllowsDragStart` 纯逻辑测是好对照
- 最小建议：关键玩家意图迁逻辑/状态测试；结构测限迁移禁令 token
- 结果：已确认

### [轻微] perf 测试命令在 ai-workflow 内互相矛盾

- 文件：`docs/ai-workflow/advanced/perf-system-workflow.md` → `npm test`；`docs/ai-workflow/verification.md` + `qmclient_scripts/perf/package.json` → `bun test.ts`
- 最小建议：advanced 统一为 `bun test.ts`
- 结果：已确认

### [轻微] 纯文档改动无机械门禁，git-workflow「文档检查」占位悬空

- 文件：`AGENTS.md` / `verification.md` 明文人工；`git-workflow.md` 仍有「文档检查」无新命令
- 最小建议：补可勾选 docs 清单，或 scope-aware 静态 docs 检查进 quick
- 结果：已确认

## 已排除 / 正向改动（非 finding）

| 项 | 结果 |
|---|---|
| FocusMode `(LabelWidth, LabelWidth)` → `(LineSpacing, LabelWidth)` | **修复**：第 5 参是 `ColumnGap`，旧值过宽 |
| TClient `LayoutSection` 引用捕获 → 值捕获 | **修复**悬垂引用风险 |
| TClient Configs filter/tags 大括号 | **修复** tags 回到 `RenderFilters` |
| 彩虹默认值仍 0 | 解耦不会意外默认开启 |
| 边框改为填充+内缩 surface | display/hit 仍用 `Frame.m_Rect`；focus 用加粗边框（对齐规格「增加边框可见性」） |
| `m_AllowHeaderDrag` 默认 true | 标准页未显式赋值也能拖；符合「可拖」产品目标 |
| 动画中禁止 drag start | 有 `settings_card_deck_logic_test` 保护 |

## 模板覆盖摘要

| 模板 | 假设数 | 已确认 | 已排除 | 待运行时 | 超出范围 |
|---|---:|---:|---:|---:|---:|
| 临时·设置卡片平台 + 2.1 + 3.10 局部 | 10 | 1 | 8 | 1 | 0 |
| 2.15 gate/测试 | 10 | 4 | 5 | 1 | 0 |
| 2.16 文档漂移 | 11 | 7 | 3 | 1 | 0 |

- **实际执行模板**：设置卡片平台（临时）、2.1、2.15、2.16、3.10（局部）
- **未执行**：2.2–2.14、3.1–3.9、3.11（与本 diff 无直接匹配）

### 待运行时验证

1. 无 handle 时玩家是否发现可拖
2. `MeasureEachFrame` 页 reflow 抖动是否长期锁拖
3. 彩虹开 + 额外动画关 的实际标题色
4. 本轮 **未跑** C++ 测试 / `check_gate` / `game-client`

## 总体结论

**需要修复**

不是崩溃/协议级不安全，但本 diff 同时留下：

1. **规格 vs 实现**（handle、彩虹依赖）
2. **gate 覆盖假安全感**（settings_ui 触发面、docs path-trigger 表述）
3. **文档治理断链**（check_docs 残留、archive status）

### 优先收口建议（需用户确认后再改）

1. 规格/实现对齐 handle + 彩虹
2. 修 settings_ui TRIGGER/审计面 + overview 表述
3. 清 `.claude`/QmClient_docs 断链；archive status 批量 outdated

## 已运行 / 未运行验证

| 验证 | 结果 |
|---|---|
| 代码/规格/gate 静态证据 | 已做 |
| `settings_ui.should_run` 实测 | 已做 |
| archive `status:active` 计数（27） | 已做 |
| C++ 测试 / game-client / check_gate | **未跑**（只读审计） |
| 游戏内视觉/拖拽/彩虹 | **未跑** |

## 关联

- 技能：`docs/superpowers/prompts/audit-qmclient-quality/SKILL.md`
- 提示词：`docs/superpowers/prompts/DDNet-QmClient-客户端质量扫描提示词.md`
- 设置页现状 explore：`docs/superpowers/explore/2026-07-14-QmClient-设置页UI与布局现状.md`
- 有效规格：`docs/superpowers/specs/2026-07-10-QmClient-设置页UI统一与滚动体系规格.md`
