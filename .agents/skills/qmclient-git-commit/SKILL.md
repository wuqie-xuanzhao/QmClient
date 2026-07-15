---
name: qmclient-git-commit
description: >
  QmClient 的 git commit、PR 标题/正文、最终汇报与 Release 说明格式：type 分组、
  中文简述、验证清单、Stable/Nightly 通道。写 commit、开 PR、整理发布说明、
  生成 release notes 时都要用本 skill。
---

# QmClient Git / PR / 汇报规范

## 何时使用

- 写 commit message
- 开 PR / 填 PR 模板
- 最终汇报
- 生成或润色 GitHub Release 说明

权威全文：`references/git-workflow.md`（豁免规则、Release 补发、完整示例）。

## Subject

```text
<type>(<scope>): <中文简述>
```

- `type` 英文小写：`feat` / `fix` / `perf` / `refactor` / `docs` / `test` / `chore` / `ci` / `revert`
- `scope` 为模块，如 `qmclient`、`browser`、`hud`、`gate`、`i18n`、`agents`
- 简述用中文

示例：`fix(browser): 修复好友分类右键菜单无法触发的问题`

## Body

先写问题/背景，再按类型分组：

```text
问题/背景：为什么需要这次改动。

## fix
- 具体修复点

## feat
- 新增点

## test
- 测试调整
```

有对应类型才写对应分组。校验脚本：`python qmclient_scripts/check_commit_msg.py`。

## 提交流程

默认：本地提交 → 推新分支 → 开 PR → 合并 → 删分支。  
仓库主或明确授权才可直推受保护分支。  
一次提交即可；仅当改动可清晰拆类或用户要求时才拆。

## PR 正文

以 `.github/pull_request_template.md` 为准，结构上仍是：

```markdown
## Summary
1-2 句说清问题与核心改动。

## <类型分组>
- feat / fix / ...

## Verification
- [x] 文档检查
- [x] 匹配范围的 gate
- [ ] 未覆盖项记为 gap

## Risks / Gaps
只写真实未覆盖风险。
```

## Release 说明

- **正式版（Stable）**：`vX.Y.Z` → 普通 Release
- **预发布（Pre-release）**：`nightly`（及 rc/beta）→ GitHub Pre-release，可被下次 Nightly 覆盖
- 脚本：`qmclient_scripts/generate_release_notes.py`（默认排除 `refactor`，可用 `--include-refactor`）
- 终稿规范：`docs/RELEASE_NOTE_TEMPLATE.md`（第 0 节通道）
- 重要用户可见改动可在 commit body 写 `Release-ZH: ...`

## 版本

- `python qmclient_scripts/bump_version.py --version X.Y.Z` 或 `--tag vX.Y.Z`
- 不要手改 `version.h` / workflow 内硬编码版本

## 提交前清单

1. review 结论已收口（核心逻辑 → `qmclient-code-review`）
2. gate 证据齐全（`qmclient-verification-gate`，不能只跑 build）
3. 文档相关先跑 `python qmclient_scripts/gate/check_docs.py`
4. 文案符合本规范
