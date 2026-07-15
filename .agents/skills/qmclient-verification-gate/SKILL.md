---
name: qmclient-verification-gate
description: >
  QmClient 改动后的验证流程：check_gate 四层模式、构建测试串行、全量与过滤测试边界、
  视觉检查与证据格式。代码改完、准备声称「通过 / 无回归 / 可提交」之前必须用本 skill；
  不要只 build 或只跑 filter 测试就收工。
---

# QmClient 验证与 Gate

## 何时使用

- 实现/修复完成后
- 用户要验收证据
- 准备 commit / PR
- 改了 i18n 脚本或 Localize 源

权威全文：`references/verification.md`（构建目录、WSL、i18n 链、视觉、证据细节）。

## 按范围选最小覆盖

### 1. 纯文档 / harness

改了 `AGENTS.md`、`.agents/`、`docs/superpowers/` 等：

```bash
python qmclient_scripts/gate/check_docs.py --sync-only --prefer agents
python qmclient_scripts/gate/check_docs.py
```

### 2. 常规代码

```bash
python qmclient_scripts/gate/check_gate.py --mode quick
```

提交前升到 `--mode default`（含 C++ / Rust 全量测试）。

### 3. 收口 / 准发布

```bash
python qmclient_scripts/gate/check_gate.py --mode full
```

在 default 上增加 strict_build / dilate / identifiers / clang_tidy_warn 等。

### 4. 翻译

```bash
python qmclient_scripts/languages_qmclient/extract_strings.py
python qmclient_scripts/languages_qmclient/generate_all.py
python qmclient_scripts/languages_qmclient/validate.py
python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0
```

细节与历史译法审计见 `references/verification.md` 与 `qmclient-i18n-workflow`。

## 硬约束

- **过滤测试只用于定位**：`testrunner --gtest_filter=...` 不能当验收；验收必须跑对应入口的**全量**版本
- 没跑全量就必须写成 gap，禁止写「无回归」
- **同一 build 目录目标串行**：`game-client`、`testrunner`、`run_cxx_tests`、`run_rust_tests`、`package_default` 共享中间产物
- 除非用户明确限制为纯调查/纯文档/单项命令，否则不要只用 build/test 代替 gate

## Windows 构建

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
```

目录约定：debug / release / release-pdb 分别对应 `cmake-build-*`。Linux/WSL 见 `references/verification.md`。

## 证据格式

```text
验证项:
  命令: <实际命令>
  结果: PASS / FAIL / GAP
  证据: <摘要或链接>
```

没有证据不要标 `done`。环境/时间跑不了 → 明确记 gap。

## 常见错误

- 用 filter 测试冒充全量
- 只 build 不跑 gate
- 用 quick 代替 default 做提交前验证
- 同一 build 目录并行多目标
