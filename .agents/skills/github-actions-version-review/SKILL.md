---
name: github-actions-version-review
description: 系统性审查并升级 QmClient GitHub Actions workflow 中所有 action 版本，基于 gh api 核实最新版本并做 SHA pinning 加固
---

# GitHub Actions 版本审查与升级

## 适用场景
定期审查 QmClient `.github/workflows/*.yml` 中所有 action 引用的版本时效性，并做机械升级和 SHA pinning。这是供应链安全和 Node 运行时兼容性的常规维护。

## 审查程序（只读）

1. **枚举所有 action 引用**：用 `search` 在 `.github/workflows/` 下搜 `uses:` 开头的行，列出所有 action 及其当前版本。
2. **查最新版本（权威方法，不要用 web_search）**：用 `gh api` 直接查 GitHub：
   - `gh api repos/<owner>/<repo>/tags --jq '.[0:8][] | .name'` 拿 tag 列表
   - `gh release view --repo <owner>/<repo> --json tagName --jq '.tagName'` 拿最新 release tag
   - `gh api repos/<owner>/<repo>/git/ref/tags/<tag> --jq '.object.sha'` 拿 SHA
   - **web_search 的版本信息不可靠**：曾返回不存在的版本（msvc-dev-cmd 无 v3、sccache 不是 v0.5.0），必须以 gh api 的 GitHub 官方数据为准
3. **2026 年 6 月核实过的版本基线**（每次仍需重新 gh api 确认）：
   - 官方 `actions/*`：checkout/upload-artifact/download-artifact/cache v6 系列；setup-java v5
   - `github/codeql-action`：v4 系列（init/analyze 都是 @v4）
   - 第三方：softprops/action-gh-release（v3.0.1）、Swatinem/rust-cache（v2.9.1）、hendrikmuhs/ccache-action（v1.2.23）、android-actions/setup-android（v4.0.1）、mozilla-actions/sccache-action（v0.0.10）、ilammy/msvc-dev-cmd（**v1.13.0，注意没有 v3！**）、dtolnay/rust-toolchain（stable 分支）
4. **核对安全清单**：
   - SHA pinning：所有第三方 action（非 actions/* 官方）应 pin 到 40 字符 commit SHA + 注释版本号
   - 每个 workflow 声明最小 `permissions`
   - 无危险的 `pull_request_target`
   - release/publish job 的 `contents: write` 与 build job 隔离
5. **输出报告**：版本核对表 + 安全评审表 + 按优先级排序的改进清单。

## SHA pinning 程序

1. **批量查 SHA**：用 eval/python 跑 gh api 循环，对每个 action 的目标 tag 拿 commit SHA。
2. **格式**：`uses: <action>@<40字符SHA>  # <版本号>`，如 `ilammy/msvc-dev-cmd@a102174a2b586eec2ea151a69e6fd14404a8ce7c  # v1.13.0`。注释保留可读性。
3. **逐文件 edit**：先 `search` 确认行号，用 snapshot tag 锚定，批量 SWAP。
4. **验证**：`search` 搜第三方 action 名，确认所有引用都是 40 字符 SHA。

## 升级程序（版本号替换）

1. **建立版本映射表**：`当前@版本 → 目标@版本`，按 action 名分组。注意同一文件里不同 action 目标版本不同。
2. **逐文件 edit**：用 `edit` 工具按行号替换。先 `read`/`search` 确认行号。
3. **版本一致性**：同一 action 在不同 workflow 应统一版本。
4. **YAML 验证**：用 eval python `yaml.safe_load_all` 验证语法。
5. **治理检查**：改了 .github/workflows 后跑 `python qmclient_scripts/gate/check_docs.py`。

## 常见坑
- **web_search 版本不可靠**：曾返回 msvc-dev-cmd v3（不存在）、sccache v0.5.0（实际 v0.0.10），必须用 gh api 核实。
- **ilammy/msvc-dev-cmd 最新只有 v1.x（v1.13.0），没有 v3**，这是最常被搜索结果误导的。
- `Swatinem/rust-cache@v2` 已是最新 major，不要盲目升 v3。
- `hendrikmuhs/ccache-action@v1.2.22` 是精确 patch 版本，比 `@v1` 更稳。
- Node 20 的 action（upload-artifact@v3 及以下）2026 年 6 月起被强制 Node 24。
- windows-2022 runner 预装 Ninja 1.11.1 + CMake 3.27.7，choco install ninja 冗余。
- dtolnay/rust-toolchain 用 stable 分支，SHA 是 HEAD commit（会随时间变）。
