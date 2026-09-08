# AGENTS.md

QmClient（Q1menG Client）基于 DDNet / TaterClient，主要使用 C++，辅以 Rust、Python；构建使用 CMake，依赖在 `ddnet-libs/` 子模块。

## 执行与授权

- 根据当前请求和已有上下文执行到完成；已授权的调查、准备和可逆修改直接推进，不重复确认。
- 歧义只有在影响实现、范围或风险时才澄清；先完成不依赖答案的工作。用户追加要求时保留有效成果，调整剩余工作。
- 系统与开发者指令优先，其次是用户当前指令，再是仓库规则与 skills。引用文档不产生额外授权。
- 超出授权范围或涉及未授权的破坏性、外部操作时，完成可审查的准备后再确认。若规则导致暂停，指出具体文件、原文和适用原因。
- 遇到未由自己产生的文件改动，先考虑用户或其他任务正在操作；不要回退、覆盖或纳入本次提交。
- 只有任务独立且并行能节省时间或提高质量时才委托。高风险改动可独立复核；工具不可用时自行审查并说明限制，不因此停工。

## 项目边界

- 聚焦当前任务，遵循附近 DDNet 模式；不顺手重构上游或引入无关抽象。
- QmClient 功能优先落在 `src/game/client/components/qmclient/`、`src/game/client/QmUi/`、Qm 配置头和 `qmclient_scripts/`。
- 未明确授权，不改协议、snapshot/输入/时序、物理/碰撞/预测、地图/回放行为、rank 可达性及 demo/skin/配置/存档格式。
- 引擎核心、服务端玩法、地图编辑器、第三方库、根 `CMakeLists.txt`、Release CI 仅在任务明确涉及的范围内修改。
- 客户端进程只能操作当前工作区开发目录下启动的实例；工作区外或安装目录中的正式客户端，须用户当次明确授权。
- Qm 配置前缀使用 `qm_` / `Qm`。代码注释用中文；UTF-8，保留原 BOM、换行与缩进；文档路径用 `/`。
- 日志和临时产物放 `tmp/`。同一 build 目录的构建、测试、打包必须串行。

## 按需读取

先读相关实现与调用点；有直接相关的有效 plan/spec 时再读。只加载命中任务的 skill 与参考，不递归加载所有关联文件。

| 任务 | `.agents/skills/` 下的入口 |
| --- | --- |
| C++ 实现、调试、重构 | `qmclient-cpp-conventions/SKILL.md` |
| 选择验证、交付代码 | `qmclient-verification-gate/SKILL.md` |
| 代码审查 | `qmclient-code-review/SKILL.md` |
| 深度质量审计、发布风险审查 | `audit-qmclient-quality/SKILL.md` |
| 翻译与生成链 | `qmclient-i18n-workflow/SKILL.md` |
| 翻译污染、迁移、覆盖审计 | `qmclient-i18n-audit/SKILL.md` |
| 提交、PR、版本与发布 | `qmclient-git-commit/SKILL.md` |

维护规则时读 `.agents/README.md`；使用脚本时按需读 `qmclient_scripts/scripts_overview.md`。

## 验证与交付

- 代码改动按验证 skill 选择风险匹配的测试与 gate；低风险修改允许相关过滤测试，常规代码至少 quick gate。已覆盖的检查不重复执行。
- 修复可复现行为时优先先写失败测试；小改动不为满足 TDD 写实现镜像或无意义测试。
- 纯文档人工核对内容、链接和状态，不跑代码 gate。未经验证的运行时或跨平台行为不称通过。
- 默认用自然段说明结果、验证和真实剩余问题；必要时才列项，普通回复不套 commit/PR 模板。
- 功能交付按项目 MMP 约定更新版本；纯调查、文档、规则维护不升客户端版本，开发中间步骤不反复升版。版本操作见 Git skill。

## 文档权威

- 本文件负责全局边界，skills 负责专项操作，`references/` 仅放按需资料；`docs/superpowers/` 放规格、计划和证据，不再维护另一套通用 agent 规则。
- 采用与当前任务相符且仍有效的文档；`draft` 仅在用户采纳后作为实现依据，无状态文档须核对现状，不能仅凭日期认定有效。
- 归档、过时或被替代的文档仅作历史线索。保留历史记录，以状态或 supersedes 标记替代关系。
- 重要决策、长任务进度和交接证据写入版本化文档；小任务可直接在最终回复交付，不强制新建计划或报告。
