# Git / PR 规范

这份文档定义 4 类输出的写法：

- commit subject
- commit body
- PR 标题与正文
- 最终汇报

目标只有 3 个：

- 让人快速看懂这次改了什么
- 让验证证据足够短，但足够用
- 让文案和实际改动边界一致

## 一般原则

- 先写问题，再写做法。尤其是 `fix`，优先说明“修复了什么问题”。
- 文案直接描述行为和结果，不写“收口”“调整相关逻辑”“优化若干细节”这类空话。
- 正文分组默认使用提交类型体系，而不是临时自造分类。
- 文档、测试、脚本、构建相关改动都应放进对应分组，不要全部挤进 `fix`。
- 如果某类改动这轮不存在，就省略该分组，不写占位。
- 已经人工确认的内容，不要继续写成风险或 gap。
- 如果仓库开启了受保护分支，而当前操作者不是仓库主或没有受保护分支直推权限，默认走：本地提交 -> 推到新分支 -> 开 PR -> 合并 PR -> 删分支。只有仓库主或被明确授予直推权限的人，才可以跳过这条默认路径。

## 分组类型

正文分组默认使用以下类型：

- `feat`
- `fix`
- `perf`
- `refactor`
- `docs`
- `test`
- `chore`
- `ci`
- `revert`

使用规则：

- 只要这轮改动涉及对应类型，就使用对应分组。
- 一个提交、PR 或最终汇报可以同时包含多个分组。
- `FEAT`、`FIX`、`DEL` 只作为最低限度的退化方案；只有在没有必要继续细分时才使用。
- `DEL` 不是默认分组。只有确实存在删除行为，且上面的标准类型不足以表达时，才额外补充。

## Commit 规范

### Commit Subject

格式：

```text
<type>(<scope>): <中文简述>
```

规则：

- `type` 使用英文小写：`feat`、`fix`、`perf`、`refactor`、`docs`、`test`、`chore`、`ci`、`revert`
- `scope` 使用短英文或仓库内模块名，如 `hud`、`docs`、`gate`、`settings`
- subject 使用中文动宾短语
- 不写“修改代码”“更新一下”这种无信息标题

示例：

```text
fix(hud): 修复通知栏编辑位置错误
docs(ai): 重写 git 和 PR 文案规范
test(score): 补充完赛消息解析回归测试
```

### Commit Body

本仓库默认写 body。

推荐结构：

```text
<为什么要改>

## fix
- ...

## test
- ...

## docs
- ...
```

规则：

- 开头先写背景、原因或边界
- 正文按类型分组，不把不相干的内容塞进同一段
- `fix` 先写修复了什么问题，再写必要的实现变化
- 不要把大段验证日志贴进 body
- 只有明显属于本地临时产物或与本轮无关的内容才排除在提交外

### 特殊提交（边界）

以下提交可豁免 `<type>(<scope>):` 格式或使用固定 scope：

- **Merge commit**：保留 git 默认 message（`Merge branch ...` / `Merge pull request #N`），不必改写。
- **上游同步**：用 `chore(sync): <简述>`，如 `chore(sync): 对齐上游版本号到 2.63.3`。
- **Cherry-pick**：前缀仍按 `fix(...)`/`feat(...)` 写，可保留 `(cherry picked from commit ...)` 尾注。
- **版本号提交**：`chore: bump version to X.Y.Z`（配合 `bump_version.py`）。

这些规则与 `qmclient_scripts/check_commit_msg.py` 的豁免逻辑一致。

## PR 规范

> GitHub PR 正文模板见 `.github/pull_request_template.md`（含兼容性、高风险区域 checklist）。**PR 正文以模板为准**；下面的 `## Summary` / 类型分组结构同样适用于 commit body 和最终汇报，分组类型一致。

### PR 标题

PR 标题默认与最终 squash commit 保持同一风格：

```text
<type>(<scope>): <中文简述>
```

### PR 正文结构

PR 正文默认使用以下结构：

```text
## Summary
<1 到 2 句总体说明>

## fix
- ...

## test
- ...

## docs
- ...

## Verification
- [x] ...

## Risks / Gaps
- ...
```

#### 1. Summary

`Summary` 不能省略。

要求：

- 用 1 句到 2 句写清楚这次 PR 解决了什么问题、涉及哪些核心改动
- 先写主问题，再写伴随修改
- 不在 `Summary` 里堆细节，细节放到各类型分组

#### 2. 类型分组

规则：

- 正文分组默认使用 `feat`、`fix`、`perf`、`refactor`、`docs`、`test`、`chore`、`ci`、`revert`
- 只要涉及对应改动，就使用对应分组
- `fix` 优先写用户实际遇到的问题，不要只写底层实现细节
- 文档类改动写到 `docs`
- 测试类改动写到 `test`
- 构建、打包、脚本整理写到 `chore`

示例：

```text
## fix
- 修复 HUD 编辑器中通知栏位置无法正确拖拽的问题。
- 修复中文练习命令列表被误显示为通知的问题。

## test
- 补充通知栏锚点和完赛消息解析测试。

## docs
- 补充服务端汉化现状探索文档。
```

#### 3. Verification

`Verification` 默认使用 checklist。

格式：

```text
## Verification
- [x] 文档检查：`<命令>`
- [x] gate 门禁：`<命令>`
- [x] 客户端构建：`<命令>`
```

规则：

- 只保留通用检查和本次任务直接相关的检查
- 同类检查合并写，不要拆成很多行
- 默认只保留命令和高信号结果
- 不重复解释“证明了什么”
- 没有运行的检查不要勾选

常见检查项：

- [x] 文档检查
- [x] gate 门禁
- [x] 客户端构建
- [x] 相关测试
- [x] 完整包构建

#### 4. Risks / Gaps

只写真实还没覆盖的风险和缺口。

优先写：

- 视觉验收
- 运行时行为
- 上游兼容性风险

规则：

- 已经人工确认的内容，明确写“已人工确认”，不要继续列为 gap
- 不要把已经有明确证据覆盖的内容重复写成风险
- 如果没有额外 gap，可以只保留真正没覆盖的 1 到 2 项

## 最终汇报

最终汇报默认也沿用类型分组：

- `feat`
- `fix`
- `perf`
- `refactor`
- `docs`
- `test`
- `chore`
- `ci`
- `revert`

要求：

- 先说结果，再分组
- 只写用户需要知道的高信号内容
- 不按文件罗列变更
- 不把验证日志原样贴出来

如果这轮改动非常简单，允许退化为短段落，不强制展开全部分组。

## Release 说明

GitHub release 说明由 `qmclient_scripts/generate_release_notes.py` 统一生成，输出按「功能领域」分组（依据 commit 的 scope）、纯中文；工程类提交（ci/build/gate 等）不进 release note。

脚本输出是机械化草稿，终稿需按 `docs/RELEASE_NOTE_TEMPLATE.md` 人工润色（改写为玩家视角、合并相近条目、删除冗余前缀等）。

如果某个提交需要更稳定的发布说明，可以在 commit body 里补：

```text
Release-ZH: 中文发布说明
```

规则：

- `Release-ZH:` 是可选项
- 如果缺失，脚本回退到 commit subject 的描述
- 面向用户的重要功能或修复，优先补 `Release-ZH:`
- 主题分组依赖 commit 的 scope，提交时务必带规范 scope（如 `feat(settings):`）

### Release 补发与重发（边界）

- **补发产物**：tag 已存在但 GitHub Release 缺失或产物不全时，重新 `git push origin <tag>` 不会重新触发 CI（tag 未变）。需在 GitHub Actions 中从对应 tag ref 手动运行 `build.yml`，让 release jobs 按该 tag 重建 Release。
- **不要强推正式 tag**：`vX.Y.Z` 一旦发布不要 `git tag -f` 强推，会破坏已下载用户的版本标识；要修正只能发新版本。
- **nightly 可覆盖**：`nightly` tag 每次 nightly 构建都 force-push 覆盖，属正常行为。

## 版本 / Tag / Release

- 仓库内版本统一通过 `python qmclient_scripts/bump_version.py --version X.Y.Z` 或 `--tag vX.Y.Z` 更新
- 不要在 workflow 或本地脚本里直接改 `version.h`
- tag 构建时，CI 也应调用同一个 `bump_version.py`
- `CLIENT_RELEASE_VERSION` 的源头是 `QMCLIENT_VERSION`

## 简例

```text
## Summary
本次 PR 主要修复 HUD 编辑器无法正确编辑通知栏的问题，并同步修正通知栏相关的中文系统消息识别。

## fix
- 修复通知栏预览区域和拖拽区域不一致，导致位置不能正确编辑的问题。
- 修复中文练习命令列表进入通知栏的问题。

## test
- 补充通知栏锚点和完赛消息解析测试。

## docs
- 补充服务端汉化现状探索文档。

## Verification
- [x] 文档检查：`python qmclient_scripts/gate/check_docs.py`
- [x] gate 门禁：`python qmclient_scripts/gate/check_gate.py --mode quick`
- [x] 客户端构建：`cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14`

## Risks / Gaps
- HUD 编辑器相关改动已人工确认。
- 第三方客户端实机联调未覆盖。
```
