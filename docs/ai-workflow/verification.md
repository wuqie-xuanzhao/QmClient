# 验证

用能覆盖改动风险的最小验证集合，然后把证据记录到当前 `docs/superpowers/plans/` 或 `docs/superpowers/specs/`。

## Harness 与文档

```bash
python qmclient_scripts/gate/check_docs.py
```

当你改了 `AGENTS.md`、`CLAUDE.md`、`docs/ai-workflow/`、`docs/superpowers/plans/`、`docs/superpowers/specs/`、governance workflow 文件或 gate 脚本后，都要跑这一项。

## i18n 脚本工作流

当改动 `qmclient_scripts/languages_qmclient/`、`data/languages/simplified_chinese.txt`，或任何会新增/删除 `Localize`、`Localizable`、`Register` help 文本的源码时，默认按这条顺序验证：

```bash
python qmclient_scripts/languages_qmclient/extract_strings.py
python qmclient_scripts/languages_qmclient/generate_all.py
python qmclient_scripts/languages_qmclient/validate.py
python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0
```

说明：

- `extract_strings.py` 负责从全 `src/` 提取 active source keys，并输出分类统计。
- `translations/i18n/*.toml` 是按代码模块拆分的翻译维护源；单条记录可同时维护多语言翻译，不要求全语言补齐。
- `data/languages/simplified_chinese.txt` 是运行时生成产物，不作为手工维护的长期真相源。
- `generate_all.py` 会以英文 source key 作为缺省回退，并在生成简中运行时文件时保留已有的非 active 条目。
- `review_duplicate_entries.py` 是只读审查脚本；duplicate/similar 报告用于人工收口，unused 口径必须基于最终 active source key 集合。
- `translate_with_local_http.py` 通过 OpenAI-compatible HTTP 模型生成翻译 draft；所有语言默认只写 `translations_draft/<language>/*.toml`，审核通过后才允许显式 `--write-back` 回填 `translations/i18n/*.toml`，不属于运行时生成主链。

### 历史译法审计

当需要核对当前 `translations/i18n/*.toml` 是否偏离项目既有简中口径时，补跑历史译法审计：

```bash
python qmclient_scripts/languages_qmclient/audit_translation_drift.py --git-ref HEAD
```

说明：

- 这是只读审计，不参与运行时生成链。
- 结果只用于人工判断历史译法是否需要回退或统一风格。
- 它不替代 `extract_strings.py` / `generate_all.py` / `validate.py`，也不阻断 `validate.py`。

## 构建

Windows 推荐：

```pwsh
qmclient_scripts/cmake-windows.cmd -G Ninja -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
```

说明：当前仓库的自动化与 Agent 会话在 Windows 上默认走 `qmclient_scripts/cmake-windows.cmd`，因为不能假设当前 PowerShell 已经注入了可用的 MSVC 环境。当前 canonical 的 `cmake-build-*` 目录按 Ninja 生成器维护；只有在调用方已经明确处于可用的 VS/MSVC shell 时，才可以直接使用裸 `cmake`。

Linux/macOS：

```sh
cmake -G Ninja -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release --target game-client -j 14
```

说明：如果当前宿主是 Windows，但需要验证 Linux 构建，优先在 WSL Ubuntu 中使用 GCC/G++、CMake 和 Ninja 走原生 Linux 构建，不要复用 Windows 的 `cmake-build-release` 目录。推荐单独使用 `cmake-build-linux-release` 之类的目录，避免和 Windows 生成的 `CMakeCache.txt` 冲突。已验证可用的 WSL 口径示例：

```pwsh
wsl env HOME=/home/<user> bash -lc 'set -e; . "$HOME/.cargo/env"; cd /mnt/<drive>/<path-to-repo>; cmake -G Ninja -S . -B cmake-build-linux-release -DCMAKE_BUILD_TYPE=Release -DDOWNLOAD_GTEST=ON; cmake --build cmake-build-linux-release --target game-client -j 14'
```

如果需要 Linux 打包，可直接把 target 切到 `package_default`：

```pwsh
wsl env HOME=/home/<user> bash -lc 'set -e; . "$HOME/.cargo/env"; cd /mnt/<drive>/<path-to-repo>; cmake -G Ninja -S . -B cmake-build-linux-release -DCMAKE_BUILD_TYPE=Release -DDOWNLOAD_GTEST=ON; cmake --build cmake-build-linux-release --target package_default -j 14'
```

## 测试

Windows:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_rust_tests
```

说明：常规运行/测试目录默认是 `cmake-build-release`；C++ 测试主路径是 `run_cxx_tests`，该目标会构建 `testrunner` 并在 build 目录下执行测试二进制，测试产物会留在 build 目录的 `tmp/tests/` 下。源码结构测试需要通过测试源码根解析 `src/...` / `data/...` 文件，不能依赖当前工作目录。单测过滤或快速复现时，可以从 build 目录运行 `./testrunner.exe --gtest_filter=<suite.test>`，或在其他目录运行 `cmake-build-release/testrunner.exe --gtest_filter=<suite.test>`。`default/full` gate 里的严格构建与静态分析会另外使用 `cmake-build-debug` 和 `cmake-build-analyze`。

重要：同一 build 目录中的 `game-client`、`testrunner`、`run_cxx_tests`、`run_rust_tests`、`package_default` 不要并行发起。它们会共享生成产物与中间文件，代理或脚本必须串行执行；如果确实要并行，只能拆到不同的 build 目录。

Linux/macOS:

```sh
cmake --build cmake-build-release --target run_cxx_tests
cmake --build cmake-build-release --target run_rust_tests
```

如果走 Windows 宿主下的 WSL Linux 验证，对应地把目录替换成独立的 Linux build 目录，例如：

```pwsh
wsl env HOME=/home/<user> bash -lc 'set -e; . "$HOME/.cargo/env"; cd /mnt/<drive>/<path-to-repo>; cmake --build cmake-build-linux-release --target run_cxx_tests -j 14; cmake --build cmake-build-linux-release --target run_rust_tests -j 14'
```

## Gate 模式

```bash
python qmclient_scripts/gate/check_gate.py --mode quick
python qmclient_scripts/gate/check_gate.py --mode default
python qmclient_scripts/gate/check_gate.py --mode full
```

说明：除非用户明确把任务限制为纯调查、纯文档同步或只要求某个单项命令，否则不要只用 build/test 代替 gate。至少选择一条与本轮范围匹配的 gate 作为验收证据：

- 纯文档 / harness 变更：`python qmclient_scripts/gate/check_docs.py`
- 常规代码改动：至少 `python qmclient_scripts/gate/check_gate.py --mode quick`
- 提交前日常严格门：优先 `python qmclient_scripts/gate/check_gate.py --mode default`
- 集中收口 / 准发布：`python qmclient_scripts/gate/check_gate.py --mode full`

版本 / release 相关修改后，至少额外验证：

```bash
python qmclient_scripts/bump_version.py --version 2.58.0 --dry-run
python qmclient_scripts/generate_release_notes.py --version "$(git describe --tags --abbrev=0)" --current-tag "$(git describe --tags --abbrev=0)"
```

## 视觉改动

对菜单、HUD、UI 控件、浏览器列表行、设置页、覆盖层和动画类改动：

- Build the client.
- Launch `DDNet.exe`.
- Verify the target screen at normal UI scale and at least one non-default scale if the layout is scale-sensitive.
- Check hover, selected, disabled, modal, keyboard, and controller paths if relevant.
- Capture screenshots when preparing a PR or visual handoff.

## 证据格式

记录格式：

```text
Command: <exact command>
Result: <pass/fail and key output>
Scope: <what this proves>
Gaps: <what was not verified>
```

没有证据就不要把功能标成 `done`。如果某项检查因为环境或时间跑不了，也要明确记成 gap。
