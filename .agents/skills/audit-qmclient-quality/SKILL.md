---
name: audit-qmclient-quality
description: 使用仓库定制的功能级与横向精准模板，审计 DDNet/QmClient 客户端的当前 diff、模块、功能、规格或发布状态；逐条验证玩家场景和失效假设，覆盖 UI 与文本布局、正确性、生命周期、线程与 Jobs、性能、观测、安全、兼容性、测试有效性和文档漂移，并把已验证问题路由到 C++ 测试、确定性脚本、gate 或人工验证。用户要求扫描、审查、找高质量问题、评估实现质量、检查边界情况、改进 gate、建立回归防护或做提交/发布前质量检查时使用。
---

# QmClient 客户端质量审计

## 确定本轮模式

根据用户请求只选择一种主模式：

- **只读审计**：默认模式。检查并报告，不修改代码或文档。
- **修复发现**：用户明确要求修复时，验证 finding 后按 TDD 做最小修改。
- **自动化加固**：把已经证实、可确定复现的问题固化为 C++ 测试、脚本或 gate。
- **提交/发布审计**：检查当前 diff、验证证据、跨平台 gap 和发布阻断项。

不要把一次审计扩成仓库全量重构。用户未授权实现时，停在证据充分的报告。

## 加载权威规则

1. 从本文件所在位置向上定位仓库根目录，先读根目录 `AGENTS.md`。
2. 读取当前有效的 spec/plan；忽略带有过时 banner 的文档，除非用于历史对照。
3. 读取 [`../../../docs/superpowers/prompts/DDNet-QmClient-客户端质量扫描提示词.md`](../../../docs/superpowers/prompts/DDNet-QmClient-客户端质量扫描提示词.md) 的以下内容：
   - 每次读取第 0、1、4 节，建立参数、模板执行合同、执行阶段、停止条件、finding 合同和自动化边界。
   - 审计具体功能时，先读取并执行第 3 节所有直接匹配的功能级模板。
   - 再读取第 2 节直接相关的横向专项，用于补足功能模板未覆盖的风险维度。
   - 没有匹配功能模板时，按第 0 节的模板执行合同构造本轮临时功能级模板。
   - 提交或发布审计时，额外读取第 5 节。
4. 代码修改读取 [`../../../docs/ai-workflow/ddnet-development.md`](../../../docs/ai-workflow/ddnet-development.md)；审查读取 [`../../../docs/ai-workflow/review.md`](../../../docs/ai-workflow/review.md)；验证读取 [`../../../docs/ai-workflow/verification.md`](../../../docs/ai-workflow/verification.md)。
5. 仅在任务触发对应风险时，读取下方列出的 advanced 文档。不要一次性加载全部文档。

## 路由 Advanced 工作流

| 触发风险 | 必读规则 |
| --- | --- |
| 新功能、新配置、用户可见行为 | [`../../../docs/ai-workflow/advanced/feature-introduction.md`](../../../docs/ai-workflow/advanced/feature-introduction.md) |
| 卡顿、长帧、页面降温、性能优化 | [`../../../docs/ai-workflow/advanced/performance-workflow.md`](../../../docs/ai-workflow/advanced/performance-workflow.md) |
| telemetry、perf 日志或报表系统本身 | [`../../../docs/ai-workflow/advanced/perf-system-workflow.md`](../../../docs/ai-workflow/advanced/perf-system-workflow.md) |
| 缓存、纹理、指针、UI element、异步结果 | [`../../../docs/ai-workflow/advanced/memory-lifetime.md`](../../../docs/ai-workflow/advanced/memory-lifetime.md) |
| 后台任务、取消、队列、主线程发布、GPU context | [`../../../docs/ai-workflow/advanced/threading-jobs.md`](../../../docs/ai-workflow/advanced/threading-jobs.md) |
| debug 面板、日志、debug bundle、复现证据 | [`../../../docs/ai-workflow/advanced/observability-debugging.md`](../../../docs/ai-workflow/advanced/observability-debugging.md) |
| 文件、下载、解压、外部输入、隐私和脱敏 | [`../../../docs/ai-workflow/advanced/safety-security.md`](../../../docs/ai-workflow/advanced/safety-security.md) |
| 行为保持型结构调整 | [`../../../docs/ai-workflow/advanced/refactor-workflow.md`](../../../docs/ai-workflow/advanced/refactor-workflow.md) |
| bug 回归、固定场景、A/B、release checklist | [`../../../docs/ai-workflow/advanced/regression-prevention.md`](../../../docs/ai-workflow/advanced/regression-prevention.md) |

同时触发多个风险时组合读取，但只采用与当前 diff 直接相关的规则。专项规则与 DDNet 兼容性或仓库现有模式冲突时，优先遵守 DDNet 兼容性和当前有效规范，并明确记录冲突。

## 强制执行精准模板

通用扫描只负责路由，不能作为审计主体。完成路由后必须执行命中的精准模板：

1. 优先匹配功能级模板，以玩家操作路径和功能状态为主轴。
2. 选择横向专项补充生命周期、线程、性能、安全、兼容性或验证风险。
3. 将模板里的每条失效假设映射到真实符号、owner、调用链、状态转换和已有测试。
4. 为每条假设标记 `已确认`、`已排除`、`待运行时验证` 或 `超出范围`，并保存对应证据。
5. 没有现成功能模板时，使用实际功能入口构造临时精准模板，至少覆盖玩家场景、状态矩阵、reset/reconnect、失败回退、平台/规模边界和可自动化部分。

不要因为 finding 上限而提前停止验证模板。finding 上限只限制最终输出数量，不限制假设覆盖。若直接命中超过 3 个互相独立的模板，拆成多轮审查，逐轮完成覆盖，不得用一次宽泛扫描替代。

## 执行审计

1. **解析范围**：记录目标、基线、平台、玩家场景、允许修改与 finding 上限。缺省基线为当前 diff，缺省平台为受影响平台，缺省模式为只读审计。
2. **读取真实实现**：先看 diff；结构问题优先使用 CodeGraph 获取定义、调用链、影响面和数据流，文字、配置键和日志再使用 `rg`。不要只依据旧文档下结论。
3. **映射模板**：先匹配功能级模板，再选择直接相关的横向模板和 advanced 规则。记录选择理由与未选择边界。
4. **执行假设**：逐条执行模板中的失效假设，从玩家可见后果反推真实代码路径、状态组合和已有保护。维护模板覆盖表，不得只做常规扫描。
5. **验证候选**：为每个候选补齐可达路径、失败条件、现有保护和最小复现。无法证明的内容降为 gap 或待验证假设，不写成 finding。
6. **输出 findings**：按 `docs/ai-workflow/review.md` 和质量扫描提示词的统一合同，先列 findings，再给总体结论。每项必须包含证据位置、触发条件、影响、修复方向和测试建议，并附模板覆盖摘要。
7. **建立回归闭环**：对已验证问题选择 C++ 测试、脚本/gate 或人工场景；不要长期依赖 AI 重复发现同一确定性问题。

## 使用子代理

主代理先完成风险路由，不要按章节逐个派发子代理。仅当用户明确要求并行审计，或仓库规则要求核心逻辑必须独立审查时，派发边界互斥的只读子任务：

- 每个子任务只负责一个已选中的精准模板、明确文件/符号范围、需要验证的失效假设和统一 finding 合同。
- 禁止子代理继续派发子代理；禁止让子代理反问边界。
- 等全部子代理返回后再汇总；未返回时不能声称审计完成。
- 主代理负责去重、验证跨域结论和确定最终严重级别。

## 选择自动化落点

遵守“第一次用 AI 找问题，第二次用测试防问题”：

| 问题性质 | 首选落点 |
| --- | --- |
| 纯函数、parser、状态机、生命周期决策、配置迁移、调用顺序 | C++ GoogleTest |
| 文档链接、生成物一致性、命名、版本同步、确定性静态合同 | Python 脚本或 gate |
| 构建、打包、跨语言合同、固定输入输出 | 集成测试或 gate |
| 视觉、手感、真实 GPU/IME/触控/音频设备 | 固定场景与人工/设备证据 |
| 风险路由、未知边界、跨模块候选发现 | AI 审计 |

新增 gate 检查必须确定、无副作用、可解释、能给出修复命令，并有自身合同测试。优先做 scope-aware 检查；不要让 gate 自动修改代码、刷新 allowlist、初始化子模块或把 AI 判断作为阻断条件。构建前只检测 `ddnet-libs/` 子模块是否初始化并给出明确命令。

审计 gate 本身时，读取 `qmclient_scripts/gate/check_gate.py`、`qmclient_scripts/scripts_overview.md` 和验证文档，重点检查：

- 各 mode 的实际检查与文档是否一致；
- run、skip、失败和环境阻断是否被准确区分；
- scope/base-ref 不可用时是否明确降低结论可信度；
- 文档、翻译、配置、格式、版本和迁移检查是否能按改动范围触发；
- gate 自身是否有测试防止“跳过却报告通过”或 mode 覆盖漂移。

## 完成标准

- 列出实际执行的功能级和横向模板，以及已验证、已排除、待运行时验证的假设数量。
- 不能只凭通用扫描声称完成；每个命中模板都已追到真实代码路径并逐条得出结果。
- 报告区分已验证 finding、待验证假设和未执行验证。
- 修复模式完成 focused 验证后，还运行仓库要求的全量入口和匹配 gate；未运行项明确记为 gap。
- 不以 build 成功代替测试、gate、视觉或跨平台验证。
- 不弱化现有测试以迁就实现，不回退用户或其他进程的并行改动。
