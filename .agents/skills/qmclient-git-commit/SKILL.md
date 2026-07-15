---
name: qmclient-git-commit
description: QmClient 的 git commit、PR 标题/正文和最终汇报格式规范，遵循 docs/ai-workflow/git-workflow.md 的 type 分组、中文简述和验证清单结构
---

# QmClient Git / PR / 汇报规范

## 适用场景
提交 commit、开 PR、写最终汇报时，遵循 `docs/ai-workflow/git-workflow.md` 的格式规范。

## Commit Subject 格式
```text
<type>(<scope>): <中文简述>
```
- `type` 英文小写：`feat`、`fix`、`perf`、`refactor`、`docs`、`test`、`chore`、`ci`、`revert`
- `<scope>` 是受影响模块（如 `qmclient`、`browser`、`hud`、`gate`）
- 简述用中文

示例：`fix(browser): 修复好友分类右键菜单无法触发的问题`

## Commit Body 结构
默认写 body，先写问题/背景，再按类型分组：
```text
问题/背景：为什么需要这次改动。

fix:
- 具体修复点 1
- 具体修复点 2

feat:
- 新增功能点

test:
- 新增/调整的测试
```
分组类型：`feat`、`fix`、`perf`、`refactor`、`docs`、`test`、`chore`、`ci`、`revert`。涉及对应类型就用对应分组。

## 提交流程（受保护分支）
默认路径（非仓库主 / 无直推权限）：本地提交 → 推到新分支 → 开 PR → 合并 PR → 删分支。仓库主或被明确授权才可直推。

- 提交是一次提交，不优先拆分；只有改动可容易拆分出区别较大的分类，或用户明确要求，才拆分。
- 不必在意"干净"的提交历史，用户可能同时进行多个工作。

## PR 正文结构
```markdown
## Summary
1-2 句说清解决了什么问题、核心改动。

<类型分组>
- `feat` / `fix` / ... 分组列出改动

## Verification
- [x] 文档检查
- [x] <范围匹配的 gate>
- [ ] <未覆盖项写为 gap>

## Risks / Gaps
只写真实未覆盖的风险。已人工确认的写"已人工确认"。
```

## 最终汇报格式
沿用类型分组（`feat`/`fix`/`test`/`docs` 等），先说结果再分组。简单改动可退化为短段落。

## 版本 / Release
- 版本统一通过 `python qmclient_scripts/bump_version.py --version X.Y.Z` 或 `--tag vX.Y.Z` 更新
- 完成完整功能或改进后，除非用户限定为调查/纯文本，按 MMP 规则更新版本
- GitHub release 说明由 `qmclient_scripts/generate_release_notes.py` 统一生成

## 提交前检查清单
1. review findings 已收口
2. gate 证据已补齐（不要带着"只跑过 build/test、没跑 gate"进入 commit）
3. 先跑 `python qmclient_scripts/gate/check_docs.py`
4. commit/PR 文案符合本规范
