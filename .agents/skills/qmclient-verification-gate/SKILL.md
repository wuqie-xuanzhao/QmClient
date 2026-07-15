---
name: qmclient-verification-gate
description: QmClient 代码改动后的验证流程：check_gate.py 四层模式选择、构建测试串行约束、全量测试与过滤测试边界、证据格式标准
---

# QmClient 验证与 Gate 流程

## 适用场景
QmClient 代码改动完成后，选择正确的验证命令并产出符合 `docs/ai-workflow/verification.md` 标准的证据。这是声称"测试通过 / 无回归"前的必经步骤。

## 验证分层（按范围选最小覆盖集）

### 1. 纯文档 / harness 改动
改了 `AGENTS.md`、`CLAUDE.md`、`docs/ai-workflow/`、`docs/superpowers/`、workflow 脚本：
```bash
python qmclient_scripts/gate/check_docs.py --sync-only --prefer agents
python qmclient_scripts/gate/check_docs.py
```
先 sync-only，再跑完整检查确认无断链/镜像漂移。

### 2. 常规代码改动
```bash
python qmclient_scripts/gate/check_gate.py --mode quick
```
提交前补到 `--mode default`（覆盖 C++ 和 Rust 全量测试）。

### 3. 集中收口 / 准发布
```bash
python qmclient_scripts/gate/check_gate.py --mode full
```
在 default 基础增加 strict_build / dilate / identifiers / clang_tidy_warn 等高噪音检查。

### 4. i18n / 翻译改动
改了 `qmclient_scripts/languages_qmclient/`、`data/languages/*.txt`、`translations/i18n/*.toml`，或新增/删除 `Localize`、`Localizable`、`Register` help：
```bash
python qmclient_scripts/languages_qmclient/extract_strings.py
python qmclient_scripts/languages_qmclient/generate_all.py
python qmclient_scripts/languages_qmclient/validate.py
python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0
```

## 关键约束

- **过滤测试只用于 TDD/定位**：`testrunner.exe --gtest_filter=...` 只能用于红绿灯和快速复现。最终汇报、验收、声称"无回归"必须跑对应测试入口的**全量版本**。
- **没跑全量测试就必须写成 gap**，不能说"无回归"或"测试通过"。
- **构建目标串行**：同一 build 目录中 `game-client`、`testrunner`、`run_cxx_tests`、`run_rust_tests`、`package_default` 必须串行执行，它们共享生成产物和中间文件。并行只能拆到不同 build 目录。

## 构建命令（Windows）
```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
```
不要直接用裸 `cmake`，除非已确认当前 shell 注入了可用 VS/MSVC 环境。构建目录规范：debug=`cmake-build-debug`，release=`cmake-build-release`，release-pdb=`cmake-build-release-pdb`。

## C++ 全量测试
```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
```

## 证据格式
记录到 `docs/superpowers/plans/` 或 `specs/`：
```text
验证项:
  命令: <实际命令>
  结果: <PASS/FAIL/GAP>
  证据: <输出摘要或链接>
```
没有证据不要把功能标成 `done`。跑不了的明确记成 gap。

## 常见错误
- 用 `--gtest_filter` 过滤测试代替全量测试，然后声称"无回归"
- 只跑 build 不跑 gate，然后用 build 通过代替测试通过
- 用 quick gate 代替 default gate 做提交前验证
- 并行发起同一 build 目录的多个目标
