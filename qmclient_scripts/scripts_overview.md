# scripts_overview

这个文档统一说明 `qmclient_scripts/` 的脚本分层、推荐入口，以及 `check_gate.py` 相关工作流语义。

目标：

- 给 agent 一个单一脚本说明入口
- 不再把“脚本总览”和“gate 工作流说明”拆成两份重复文档

## 脚本分层

### `scripts/` 与 `qmclient_scripts/` 边界

根目录 `scripts/` 视为 DDNet 上游同步目录。QmClient 自有逻辑、gate 入口、生成脚本和检查脚本都放在 `qmclient_scripts/`，不要在 gate 或 GitHub workflow 中直接调用 QmClient 修改过的 `scripts/` 文件。

需要复用上游脚本时，先把脚本复制到 `qmclient_scripts/`，在复制件上做 QmClient 适配；不要让 `qmclient_scripts/` 里的脚本反向依赖根目录 `scripts/` 的 QmClient 特化行为。唯一例外是明确保持上游原样的脚本或平台入口，例如 `scripts/android/cmake_android.sh`。

### 1. `gate/`

这是仓库级门禁与脚本治理层。

当前规范入口：

| 入口 | 说明 |
|------|------|
| `qmclient_scripts/gate/check_gate.py` | Python 版仓库级门禁总入口 |
| `checks/strict_build` | 严格构建与静态分析（`check_gate.py` 调度模块） |
| `qmclient_scripts/gate/check_docs.py` | 治理文档一致性检查（可带 `--sync-only`） |
| `qmclient_scripts/gate/baseline_debt_allowlist.json` | 基线白名单数据 |

适用：

- 跑仓库级 `quick/default/full` 门禁
- 跑严格构建、`/analyze`、clang-tidy、ASan
- 校验 `AGENTS.md` / `CLAUDE.md` / 精简 `docs/ai-workflow/` / CI 入口是否一致
- 维护 baseline debt allowlist

### 2. 构建与平台辅助

这类脚本负责让构建或平台行为成立，不是门禁总入口。

当前主要脚本：

- `qmclient_scripts/cmake-windows.cmd`
- `qmclient_scripts/darwin_fix_install_names.py`
- `qmclient_scripts/make_lib_openssl.sh`

### 3. 代码卫生与内容生成辅助

这类脚本是具体检查项或生成项，不负责仓库级编排。

当前主要脚本：

- `qmclient_scripts/check_config_variables.py`
- `qmclient_scripts/check_header_guards.py`
- `qmclient_scripts/bump_version.py`
- `qmclient_scripts/fix_style.py`
- `qmclient_scripts/export_settings_commands_table.py`
- `qmclient_scripts/generate_release_notes.py`

### 4. 其他专用脚本

与门禁主链无直接关系，按各自职责独立存在：

- `qmclient_scripts/languages_qmclient/`
- `qmclient_scripts/qmclient_center_server/`
- `qmclient_scripts/diff_update.py`
- `qmclient_scripts/tw_api.py`
- `qmclient_scripts/update.zsh`

`qmclient_scripts/languages_qmclient/` 语言脚本入口：

- `source_keys.py`：共享源码 key 提取器，扫描全 `src/`，提取 `Localize` / `Localizable`、`Register` help 和 QmClient 间接 key
- `extract_strings.py`：写出 `extracted_strings.txt`，并生成 `extracted_audit_report.json`；active key 清单继续只承载 i18n 主链 source key，审计报告另外输出 `must_i18n`、`business_data`、`test_only`、`needs_review`、`violation`
- `translations/i18n/*.toml`：按代码模块拆分的翻译维护源；单条记录可同时维护多语言翻译，不要求全量语言留空
- `generate_all.py`：从当前源码 key 和模块化 TOML 维护源生成 `generate_all.GENERATED_LANGUAGES` 中登记的 `data/languages/*.txt`，缺失时回退英文 key
- `review_duplicate_entries.py`：只读审查重复、相似、空译文和疑似未使用项；unused 直接按最终 active source key 集合判断，避免 context 漂移误报
- `audit_translation_drift.py`：只读对比当前 `translations/i18n/*.toml` 与 Git 历史里的 `data/languages/simplified_chinese.txt`，用于审查历史译法是否被新维护源改偏；默认基线为 `HEAD`
- `translate_with_local_http.py`：通过 OpenAI-compatible HTTP 接口生成翻译 draft；所有语言默认只写 `translations_draft/<language>/*.toml`，审核通过后才允许显式 `--write-back` 回填主 TOML 维护源；回填必须按审核通过的条目做 patch，不重写整份模块 TOML
- `validate.py`：校验提取文件与审计报告新鲜度、生成产物覆盖、模块化 i18n store 可读性和 legacy overlay 删除状态；`violation` 会返回失败，`needs_review` 只作为人工清理 backlog 提示

推荐 i18n 工作流：

1. 修改源码中的英文 key 或新增 `Localize` / `Localizable` / `Register` help 调用
2. 运行 `python qmclient_scripts/languages_qmclient/extract_strings.py`
3. 按需更新 `qmclient_scripts/languages_qmclient/translations/i18n/*.toml`
4. 运行 `python qmclient_scripts/languages_qmclient/generate_all.py`
5. 运行 `python qmclient_scripts/languages_qmclient/validate.py` 与 `python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0`

说明：

- `data/languages/simplified_chinese.txt` 是运行时生成产物，不再作为手工维护的长期真相源。
- `translations/i18n/*.toml` 才是翻译维护源；按代码模块拆分，单条记录可带多语言翻译，未填写的语言在生成时回退英文 key。
- `translations_draft/<language>/*.toml` 是 HTTP 模型生成的草稿维护源，用于人工审阅或后续回填，不参与运行时生成链；所有语言都先走 draft，回填必须显式使用 `--write-back`，且只 patch 审核通过的目标条目。
- 新增英文 source key 后，推荐流程是：`extract_strings.py` 提取 key -> `translate_with_local_http.py --languages ...` 生成多语言 draft -> 人工审核 draft -> `--write-back` 回填主 TOML -> `generate_all.py` 生成运行时语言文件 -> `validate.py` 验证。
- 字符串分类按职责判断，不再按“是不是中文”判断是否漏翻译：客户端自有展示文案进入 i18n；兼容匹配/解析字面量留在业务层；测试样本文本只留测试。
- 当需要核对“当前 TOML 是否偏离项目原有简中口径”时，运行 `audit_translation_drift.py`。它是历史译法审计工具，不参与运行时生成链，也不阻断 `validate.py`。

## 推荐入口

### 仓库级门禁

```bash
python qmclient_scripts/gate/check_gate.py --mode quick
python qmclient_scripts/gate/check_gate.py --mode default
python qmclient_scripts/gate/check_gate.py --mode full
```

### 文档入口一致性

```bash
python qmclient_scripts/gate/check_docs.py
python qmclient_scripts/gate/check_docs.py --sync-only --prefer agents
```

### GitHub Release 说明

```bash
python qmclient_scripts/generate_release_notes.py --version vX.Y.Z --current-tag vX.Y.Z --output tmp/release-notes.md
```

### 版本号收口

```bash
python qmclient_scripts/bump_version.py --version X.Y.Z
python qmclient_scripts/bump_version.py --tag vX.Y.Z
```

### baseline allowlist

```bash
python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/check-gate-report.json
python qmclient_scripts/gate/tools/refresh_allowlist.py --report tmp/check-gate-report.json --output qmclient_scripts/gate/baseline_debt_allowlist.json
```

说明：

- `refresh_allowlist.py` 是人工确认后的维护工具，不会被 gate 自动调用
- 先看 JSON 报告，再决定是否增量合并或 `--rewrite` 全量重写

## `check_gate.py` 工作流语义

### 角色

`qmclient_scripts/gate/check_gate.py` 是仓库级总入口。

它负责：

- 把源码卫生检查、严格调试检查、测试、allowlist 与 JSON 报告收口成统一工作流
- 用 `check_docs.py` 的内建最小规则校验根规则和文档入口是否齐全
- 区分“已知历史债务”和“当前新增阻断”

### 模式

#### `quick`

- 开发期快速自查
- 不跑真实构建
- 不跑测试

默认内容：

- 配置变量使用检查
- 文档一致性检查
- 头文件 guard 检查
- 标准头文件检查
- `fix_style.py -n`

#### `default`

- 日常提交前严格门
- 跑真实构建、严格静态分析、C++ 全量测试和 Rust 全量测试

默认内容：

- `quick` 全部
- `checks/strict_build`
- `run_cxx_tests`（会构建 `testrunner`，并在 build 目录下执行测试二进制；源码结构测试通过测试源码根读取 `src/...` / `data/...`）
- `run_rust_tests`

#### `full`

- 集中收口 / 准发布门
- 在 `default` 基础上增加更重检查，不作为“全量测试”的默认入口

默认内容：

- `default` 全部
- 标识符命名检查
- clang-tidy warn 等附加检查（按当前 gate 开关和 mode 配置）

### 默认构建口径

- 运行/测试目录默认是 `cmake-build-release`，Windows MSVC Release 构建默认生成 PDB 符号文件用于崩溃符号化
- 严格调试检查目录默认是 `cmake-build-debug` / `cmake-build-analyze`
- Windows 默认通过 `qmclient_scripts/cmake-windows.cmd` 进入 CMake
- 在 Windows 宿主上验证 Linux 构建时，推荐通过 WSL Ubuntu + GCC/G++ + CMake + Ninja 走原生 Linux 口径，并使用独立目录（例如 `cmake-build-linux-release`），不要复用 Windows 的 `cmake-build-release`
- 同一 build 目录里的 `game-client`、`testrunner`、`run_cxx_tests`、`run_rust_tests`、`package_default` 必须串行执行，不要并行调度；并行验证只能通过拆分到不同 build 目录实现

### 结果分类

`check_gate.py` 的失败应至少能区分为：

- `环境/工具`
- `仓库基线债务`
- `当前改动/构建阻断`

### 常用命令

```bash
python qmclient_scripts/gate/check_gate.py --mode default --explain-scope --report-json-path tmp/check-gate-report.json
```

## 不要这样用

- 不要把 `check_config_variables.py`、`fix_style.py`、`check_header_guards.py` 误当成仓库级总入口
- 不要绕开 `qmclient_scripts/gate/check_gate.py` 自己临时拼一套等价门禁
- 不要把 `qmclient_scripts/` 根目录当成完全平级；门禁相关内容统一以 `gate/` 为准
- 不要在 QmClient gate 或 workflow 中直接调用根目录 `scripts/` 下的 QmClient 特化脚本；改用 `qmclient_scripts/` 复制件

## 关联文档

- `docs/ai-workflow/meta.md`
- `docs/ai-workflow/verification.md`
