# AGENTS.md

QmClient（Q1menG Client）是基于 DDNet / TaterClient 的第三方定制客户端。

- 主要语言：C++
- 辅助语言：Rust、Python、少量平台相关语言
- 构建系统：CMake
- 依赖管理：Git Submodules（`ddnet-libs/`）
- 目标平台：Windows、Linux、macOS、Android（后续也许有 IOS）

## 文档系统

- docs 中 是各贡献者的 AI 工作流 的文档，其功能可共享通用
- `docs/superpowers/` 下的探索、计划、规格和提示词全部保留，作为版本化的任务记录与本地 AI 工作流入口。
- 项目级 Agent skills 放在 `.agents/skills/<name>/SKILL.md`，供 **Codex** 与 **OMP** 共同发现；说明见 `.agents/README.md`。`docs/superpowers/prompts/` 保留长文提示词，不再作为 skill 入口。
- `docs/superpowers/` 下的探索/计划/规格会随时间推移而老化；已标注 `文档已过时` 或 `部分内容已过时` banner 的文件仅供参考，不应作为实现依据。
- 无标注或标注为当前有效的文档才是本次开发的权威来源。
- 有效文档的判定以最新日期和 `status` 字段（如 `active`、`draft`）为准。

### 文档地图

| 路径 | 内容 | 何时阅读 |
|------|--------------|--------------|
| `docs/ai-workflow/meta.md` | `docs/ai-workflow/` 自身的边界：什么该放这里，什么不该放这里。 | 需要修改 ai 工作流文档时 |
| `docs/ai-workflow/ddnet-development.md` | DDNet/QmClient 的 C++ 规则、兼容性约束、风格、所有权、性能和风险边界。 | 每次修改 cpp、h 文件 代码实现时 |
| `docs/ai-workflow/verification.md` | 构建、测试、quick/default/full gate 命令、视觉检查和证据标准。 | 完成任务后 |
| `docs/ai-workflow/review.md` | 代码审查立场、严重级别格式、DDNet 特有风险点和输出格式。 | 完成任务后 |
| `docs/ai-workflow/git-workflow.md` | Git - commit、PR 标题/描述和最终汇报格式规范。 | 提交 git 和 PR 时 |
| `docs/ai-workflow/advanced/README.md` | 性能、量化系统、重构、安全、内存、jobs、观测和回归防护等专项稳定规则的阅读路由。 | 任务涉及对应专项风险时 |
| `qmclient_scripts/scripts_overview.md` | 脚本分层、推荐入口和 gate 工作流语义。 | 使用脚本时 |
| `.agents/skills/` | 项目级 skills（i18n、gate、质量审计等） | 任务匹配 skill 描述时，或用户显式 `/skill:` |

## 极简工作流

### 范围边界

- 一次只做一个功能或一个明确问题；超出当前需求的上游改动、协议/物理/预测/格式改动默认不做。
- 实现时保持补丁聚焦：遵循 DDNet/QmClient 现有模式，不顺手重构无关代码，不把“现代化”当目标。

### 启动顺序

- 先读匹配的 `docs/superpowers/plans/` 或 `docs/superpowers/specs/`。
- 再读与当前任务匹配的最小 `docs/ai-workflow/` 规则。
- 修改前检查附近源码、调用点、配置变量、翻译和测试；不理解现状时不要直接写代码。

### 完成任务后

- 先按 `docs/ai-workflow/verification.md` 跑对应验证，至少覆盖当前改动的 build/test/gate。
- 除非用户明确把任务限制为纯调查、纯文档同步或只要求某个单项命令，否则不要只用 build/test 代替 gate；代码改动完成后，至少补一条与范围匹配的 `python qmclient_scripts/gate/check_gate.py --mode ...` 验证。
- 默认口径：纯文档改动人工核对引用和内容；常规代码改动至少跑 `python qmclient_scripts/gate/check_gate.py --mode quick`；提交前如环境允许优先补到 `--mode default`，该模式覆盖 C++ 全量测试和 Rust 全量测试；集中收口或准发布改动再用 `--mode full`，full 只是在 default 基础上增加高噪音/更重附加检查，不作为“全量测试”的默认入口。
- 过滤测试只用于 TDD 红绿灯、定位和快速复现；最终汇报、交给用户验收或提交前，必须按 `docs/ai-workflow/verification.md` 跑对应测试入口的全量版本。没跑全量测试就必须写成 gap，不能说“无回归”或“测试通过”。
- 同一 build 目录中的 `game-client`、`testrunner`、`run_cxx_tests`、`run_rust_tests`、`package_default` 必须串行执行，不要并行；要并行只能拆到不同 build 目录。
- 影响核心逻辑时，必须派发一个新的只读子代理，按 `docs/ai-workflow/review.md` 做代码审查；审查先列 findings，再给总体结论。
- 子代理指出的问题修完后，再看这次改动能否最小化提交：只保留和当前任务直接相关的文件与说明。

### 提交 commit / PR 前（用户说要提交改动的时候）

- 提交不必在意干净的提交, 用户同时可能进行多个工作, 所以可能会有多种的改动.
- 必须先读 `docs/ai-workflow/git-workflow.md` , 提交是一次提交, 不优先拆分多个提交, 只有用户明确拆分或者改动可以容易的拆分出区别较大的分类的情况下, 才拆分, 否则根据文档里面的, 多填写 git commit 的信息即可
- 先确认 review findings 已收口、gate 证据已补齐；不要带着“只跑过 build/test、没跑 gate”的状态进入 commit / PR。
- 如果仓库开启了受保护分支，而当前操作者不是仓库主或没有直推权限，默认走：本地提交 -> 推到新分支 -> 开 PR -> 合并 PR -> 删分支。只有仓库主或被明确授予直推权限的人，才可以不走这条默认路径。
- commit 和 PR 文案按 `docs/ai-workflow/git-workflow.md` 编写：标题统一用 `<type>(<scope>): <中文简述>`，正文先写问题/背景，再按 `fix`、`test`、`docs` 等分组。
- 如果准备提 PR，先确保这轮审查结论已经收口，不要带着已知 review finding 进入 PR。

### 修改文档后

- 人工核对文档路径、相对链接、状态字段和权威来源说明；纯文档改动不运行代码 gate。
- 不要批量删除 `docs/superpowers/`；过时内容通过 banner、`status` 和新文档的 supersedes 关系标注。

### 最终汇报

- 最终汇报必须写清：改了什么、跑了哪些验证、结果如何、还有哪些 gaps。
- 没跑的不要说通过；没收口的不要说完成。

### 发版（用户说要发新版本时）

1. `python qmclient_scripts/bump_version.py --tag vX.Y.Z`（更新 `src/game/version.h` + `docs/info.json`；版本倒退/重复会 stderr 警告）
2. 提交版本号改动：`chore: bump version to X.Y.Z`
3. `git tag vX.Y.Z && git push origin vX.Y.Z`
4. CI（`.github/workflows/build.yml` 的 `release-desktop` / `release-android`）自动构建多平台产物、调 `generate_release_notes.py` 生成 release note 并发布 GitHub Release。
5. release note 是机械草稿，按 `docs/RELEASE_NOTE_TEMPLATE.md` 人工润色后编辑该 Release。

> nightly 由 `.github/workflows/nightly.yml` 定时构建并覆盖 `nightly` tag，无需手动发版。补发/重发见 `docs/ai-workflow/git-workflow.md`「Release 补发与重发」。

## 全局硬约束（简略版，详细看 `docs/ai-workflow/ddnet-development.md`）

- 仓库就是记录系统：决策、计划、功能状态、验证证据和交接说明都应该写进版本化文件。
- 一次只处理一个功能。直接以当前用户请求和对应的 superpowers plan/spec 作为范围边界。
- 如果功能请求有歧义，先问清行为、范围和兼容性边界，再开始实现。
- 改行为之前先读真实代码。优先遵循本地模式和 DDNet 兼容性，而不是套用泛化的现代 C++ 偏好。
- 如果有 图谱代码类 MCP（如 codegraph），优先使用它获取代码上下文，而不是直接阅读真实代码。
- 保护 DDNet 兼容性：没有明确批准，不要改协议、demo/skin 格式、物理、预测、碰撞、地图行为、rank 可达性或既有玩法语义（通用现代 C++ 最佳实践（见 context7）与 DDNet 既有风格或兼容性约束冲突时，优先服从 DDNet 约束）
- 补丁必须聚焦。不要重写无关的上游 DDNet 代码，也不要为小改动引入大抽象（优先遵循 DDNet 现有实现模式，不为了”现代化”重写既有代码）
- QmClient 特有工作通常应落在 `src/game/client/components/qmclient/`、`src/game/client/QmUi/`、QmClient 配置头、翻译、文档、metadata 和 `qmclient_scripts/`
- 超出范围的区域需要明确批准：上游引擎核心、服务端玩法、地图编辑器、第三方库、CI release 工作流、协议字段、物理、预测、snapshot、输入、碰撞、时序和回放语义
- 默认不要修改根目录 `CMakeLists.txt`、协议字段、序列化布局或文件格式定义，除非任务明确要求
- QmClient 的配置项统一使用 `qm_` / `Qm` 前缀，不使用 `cl_` 前缀
- 完成一个完整功能或改进后，除非用户明确把任务限制为调查或纯文本输出，否则按 MMP 规则更新 QmClient 版本
- 新功能、玩法变化或较大行为改动，默认先讨论，不直接扩展实现
- 不要写空模块、空文档、stub 或”以后再决定”这类占位式交付描述
- 工程实现默认走 TDD：先写失败测试，再做最小实现通过测试，最后再整理代码
- 编码必须使用 UTF-8，保留原 BOM 状态（如有）
- 修改前检测原文件换行符（CRLF/LF）和缩进风格（Tab/空格数），修改后保持一致
- 真实 commit / PR 标题按 `docs/ai-workflow/git-workflow.md`：`<type>(<scope>): <中文简述>`，并默认补全 commit body
- `FEAT`、`FIX`、`DEL` 可用于 commit body 分组，也可用于最终汇报分组
- 仓库文档中的文件、命令行和目录路径统一使用前斜杠 `/`（如 `src/game/client/components/qmclient/`, `qmclient_scripts/cmake-windows.cmd`）
- 注释必须使用中文（如 `// 这是一个注释`）

## 构建与命令

- 优先用脚本，不要依赖记忆。具体命令见 `docs/ai-workflow/verification.md` 和 `qmclient_scripts/scripts_overview.md`。
- 修改 `qmclient_scripts/languages_qmclient/`、`data/languages/*.txt`、`qmclient_scripts/languages_qmclient/translations/i18n/*.toml`，或新增/删除 `Localize`、`Localizable`、`Register` help 文本后，按顺序运行 `python qmclient_scripts/languages_qmclient/extract_strings.py`、`python qmclient_scripts/languages_qmclient/generate_all.py`、`python qmclient_scripts/languages_qmclient/validate.py`、`python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0`
- `qmclient_scripts/languages_qmclient/translations/i18n/*.toml` 是翻译维护库；`data/languages/*.txt` 是生成产物，不作为手工维护的长期真相源。
- 新增英文 source key 后，先用 `extract_strings.py` 更新 active key，再用 `translate_with_local_http.py --languages ...` 生成 `translations_draft/<language>/*.toml`；审核 draft 后才允许显式 `--write-back` 回填 `translations/i18n/*.toml`，随后运行 `generate_all.py` 生成运行时语言文件。
- Windows 上默认用 `qmclient_scripts/cmake-windows.cmd` 作为构建入口；常规构建/测试目录是 `cmake-build-release`，交互式完整构建命令：`cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14`。自动化子进程显式走 `cmd.exe` 宿主时再使用 `cmd /c qmclient_scripts/cmake-windows.cmd ...`；只有已确认当前 shell 已注入可用的 VS/MSVC 环境时，才直接使用裸 `cmake`
- 构建目录名规范：debug - `cmake-build-debug`；release - `cmake-build-release`
- 同一 build 目录中的 `game-client`、`testrunner`、`run_cxx_tests`、`run_rust_tests`、`package_default` 不要并行发起；这些目标会共享生成产物和中间文件，必须串行执行。需要并行时，只能拆到不同 build 目录。
- 一切日志, 或者临时文件, 一定要放到 tmp/ 下面去, 不要放到根目录!!!

## 十二原则：软件工程

除非另有明确说明，本项目中的所有任务都遵循以下规则。
基本倾向：遇到非简单任务时，宁可慢一点，也要更谨慎。简单任务则自行判断，不必过度流程化。

## 规则 1 — 写代码前先想清楚

明确说出你的假设。
不确定时先问，不要猜。
如果需求有歧义，列出可能的理解。
如果有更简单的做法，要主动指出。
如果卡住了，就停下来，说明哪里不清楚。

## 规则 2 — 简单优先

用能解决问题的最少代码。不要做预判式扩展。
不要实现需求之外的功能。不要为了只用一次的代码引入抽象。
自检标准：如果一位资深工程师会觉得这太复杂，那就简化。

## 规则 3 — 精准修改

只改必须改的地方。只清理你自己造成的问题。
不要顺手“优化”旁边的代码、注释或格式。
没坏的东西不要重构。保持和现有代码风格一致。

## 规则 4 — 以目标为导向

先定义成功标准，再不断验证，直到达成。
不要只是机械地执行步骤。要明确什么叫完成，并围绕它迭代。
清晰的成功标准能让你独立推进，而不是不断迷失在流程里。

## 规则 5 — 只把模型用于判断类工作

可以用我来做：分类、起草、总结、提取。
不要用我来做：路由、重试、确定性转换。
如果代码能给出答案，就让代码来回答。

## 规则 6 — Token 预算必须认真对待

单个任务：4,000 tokens。单个会话：30,000 tokens。
如果快接近预算，就先总结，再重新开始。
预算可能超限时要明确说出来，不要悄悄超支。

## 规则 7 — 暴露冲突，不要折中混合

如果两种模式互相冲突，选择其中一种，优先选更新的或验证更多的。
说明你为什么这样选，并把另一种标记为后续需要清理。
不要把冲突的做法混在一起。

## 规则 8 — 写之前先读

加代码之前，先看导出项、直接调用方和共享工具。
“看起来互不相关”并不安全。
如果不明白代码为什么这样组织，就先问。

## 规则 9 — 测试要验证意图，而不只是行为

测试应该体现这个行为为什么重要，而不只是检查它做了什么。
如果业务逻辑变了，测试却不会失败，那这个测试就是错的。

## 规则 10 — 重要步骤后要设检查点

每完成一个重要步骤，都总结：做了什么、验证了什么、还剩什么。
不要在一个自己都说不清的状态上继续往前走。
如果发现自己丢了上下文，就停下来，重新整理当前状态。

## 规则 11 — 遵循代码库约定，即使你不认同

在代码库内部，一致性优先于个人偏好。
如果你确实认为某个约定有问题，要明确指出。不要偷偷另起一套风格。

## 规则 12 — 失败要说清楚

如果有任何内容被跳过，就不能说“已完成”。
如果有任何测试被跳过，就不能说“测试通过”。
默认暴露不确定性，而不是把它藏起来。
