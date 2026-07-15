---
name: qmclient-git-commit
description: QmClient 的 git commit、PR 标题/正文与最终汇报格式；遵循 git-workflow 的 type 分组、中文简述与验证清单。
---

# QmClient Git / PR / 汇报规范

## 何时使用
写 commit、开 PR、写最终汇报时。权威细则：`docs/ai-workflow/git-workflow.md`。

## Subject
```text
<type>(<scope>): <中文简述>
```
- `type` 英文小写：`feat` / `fix` / `perf` / `refactor` / `docs` / `test` / `chore` / `ci` / `revert`
- `scope` 为模块，如 `qmclient`、`browser`、`hud`、`gate`、`i18n`
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
有对应类型才写对应分组。

## 提交流程
默认：本地提交 → 推新分支 → 开 PR → 合并 → 删分支。  
仓库主或明确授权才可直推受保护分支。  
一次提交即可；仅当改动可清晰拆类或用户要求时才拆。

## PR 正文
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

## 版本
- 用 `python qmclient_scripts/bump_version.py --version X.Y.Z` 或 `--tag vX.Y.Z`
- Release 说明用 `generate_release_notes.py`

## 提交前清单
1. review 结论已收口  
2. gate 证据齐全（不能只跑 build）  
3. 先跑 `python qmclient_scripts/gate/check_docs.py`（文档相关时）  
4. 文案符合本规范  
