> **已归档，禁止作为实现依据。** 本文保留为历史记录；当前实现请以活动 spec/plan/skill 为准。

# QmClient 设置页 UI 统一 P0 基线与规格收敛 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在当前 `dyl_dev` checkout 保留用户既有改动边界的前提下，逐提交审查并显式合并 GitHub `wxj881027/QmClient` 的固定远端范围，建立后续 P1–P7 唯一可构建、可测试、可追溯的基线。

**Architecture:** 执行时在当前 `dyl_dev` checkout 更新 `origin/master` 与 `origin/dyl_dev`，用不可变 commit range 建立审查台账。先提交本计划前的文档状态；其后只暂存本任务明确列出的文件，绝不处理用户既有改动。合并只使用 `--no-ff`，不 rebase；冲突按当前权威规格选择单一路径，完成后以基线报告、全量 C++ 测试、docs check 和 default gate 固化检查点。

**Tech Stack:** Git、PowerShell、CMake/MSVC、GoogleTest、Python gate、Markdown。

## Global Constraints

- 权威规格：`docs/superpowers/specs/2026-07-10-QmClient-设置页UI统一与滚动体系规格.md`。
- 目标 range 固定为 `4f76dcb4e7b5bc86bba9a271b563a8b6a34d53f3^..8ee4fa22ba0172a66605b2c5033f0736d66ced34`，预期为 11 个普通提交和 1 个 merge 路由提交；`1ea8259dc3dd894d02fe5c69a0046fccec20dff4` 仅路由 `4f76dcb4e7b5bc86bba9a271b563a8b6a34d53f3`，不重复作为功能提交处理。
- 目标分支为 `dyl_dev`；禁止 rebase，使用显式 `--no-ff` merge。
- 当前 checkout 中不属于本计划的图标、`src/engine/client/backend_sdl.cpp`、`ddnet-libs` 或用户并行改动不得 stash、回退、覆盖或混入提交；任何阶段只可暂存任务清单明确列出的路径。
- `d161bd10adffdff42a9b85d09f7336a64a708f60` 的动画、输入、token、菜单和配置改动逐文件审查；`8f2446b` 的编辑器术语重命名只做兼容审查，不扩大为设置页功能。
- 协议、物理、预测、snapshot、Demo/地图/skin 格式、服务端玩法和编辑器业务行为不因本合并自动进入 P1–P7。
- 同一 `cmake-build-release` 的 `game-client`、`testrunner`、`run_cxx_tests`、`run_rust_tests` 和 `package_default` 始终串行。
- P0 不额外更新 QmClient 功能版本；远端 merge 的 `2.74.23` 仅在版本审查确认后随 tip 进入基线。P1–P7 全部收口后由 P7 从 P0 实际版本顺序执行一次 MMP 版本更新。

---

### Task 1: 确认当前 checkout、子模块与不可变提交清单

**Files:**
- Modify: none

**Interfaces:**
- Consumes: 本地 `dyl_dev`、远端 `origin/master`、远端 `origin/dyl_dev`。
- Produces: 当前 `dyl_dev` checkout 的已记录改动边界、已初始化子模块，以及按时间正序排列的 full hash 清单。

- [ ] **Step 1: 记录当前分支与既有改动边界**

用户已要求直接在当前分支实施，且已在所有迁移前提交本计划状态。禁止创建或使用本轮隔离 worktree。先记录 `dyl_dev`、`HEAD` 和既有 dirty 路径；这些路径不是 P0 输入，不能 `stash`、回退、覆盖或暂存。每次提交前都必须复核 staged path 仅含本任务文件。

```powershell
git rev-parse --show-toplevel
git branch --show-current
git rev-parse HEAD
git status --short
$PreexistingStagedPaths = @(git diff --cached --name-only)
if($PreexistingStagedPaths.Count -ne 0) { throw "index is not empty: $($PreexistingStagedPaths -join ', ')" }
```

Expected: 当前分支为 `dyl_dev`；输出完整记录既有用户改动；index 为空。既有用户改动可存在，不要求 clean checkout。

- [ ] **Step 1.1: 在当前 checkout 初始化构建依赖**

所有构建、测试和 gate 之前，必须在当前 checkout 递归初始化子模块。

```powershell
git submodule update --init --recursive
git submodule status --recursive
if($LASTEXITCODE -ne 0) { throw 'submodule initialization failed' }
```

Expected: 命令退出码为 `0`，`git submodule status --recursive` 的每个已登记路径均不以 `-`（未初始化）开头。若网络或权限导致失败，停止在构建前并记录为环境阻断，禁止将缺失子模块误报为 C++ 回归。

- [ ] **Step 2: 更新目标远端引用**

Run:

```powershell
git fetch --prune origin master dyl_dev
git rev-parse origin/master
git rev-parse origin/dyl_dev
```

Expected: fetch 成功，两个远端分支都解析为 40 位 commit。

- [ ] **Step 3: 写入并验证提交清单**

Run:

```powershell
$Commits = @(git rev-list --reverse 4f76dcb4e7b5bc86bba9a271b563a8b6a34d53f3^..8ee4fa22ba0172a66605b2c5033f0736d66ced34)
$Commits
$Commits.Count
git rev-parse dyl_dev
git merge-base --is-ancestor 4f76dcb4e7b5bc86bba9a271b563a8b6a34d53f3 8ee4fa22ba0172a66605b2c5033f0736d66ced34
```

Expected: 全部对象数为 `12`，其中普通提交数为 `11`、merge commit 数为 `1`；`merge-base --is-ancestor` 退出码为 `0`。审查台账把 `1ea8259dc3dd894d02fe5c69a0046fccec20dff4` 标为路由提交，并关联已单独审查的 `4f76dcb4e7b5bc86bba9a271b563a8b6a34d53f3`。

- [ ] **Step 4: 证明目标 tip 尚未进入基线**

Run:

```powershell
git merge-base --is-ancestor 8ee4fa22ba0172a66605b2c5033f0736d66ced34 HEAD
```

Expected: 合并前退出码为 `1`；若为 `0`，停止执行并把 P0 标记为已被其他工作覆盖，不再创建重复 merge。

### Task 2: 逐提交审查固定远端范围

**Files:**
- Create: `docs/superpowers/reviews/archive/2026-07-11-settings-ui-p0-remote-review.md`
- Modify: none

**Interfaces:**
- Consumes: Task 1 的 full hash 清单与权威规格范围。
- Produces: 每个 commit 的 `accept`、`accept-with-conflict-resolution` 或 `exclude-from-ui-scope` 结论；结论不改变 commit 历史，只指导 merge 冲突选择。

- [ ] **Step 1: 为每个 commit 生成 diff 证据**

Run:

```powershell
$Commits = @(git rev-list --reverse 4f76dcb4e7b5bc86bba9a271b563a8b6a34d53f3^..8ee4fa22ba0172a66605b2c5033f0736d66ced34)
$Commits | ForEach-Object { git show --no-ext-diff --stat --summary $_; git diff --no-ext-diff "$($_)^!" -- }
```

Expected: 依次输出 12 个对象的完整 patch；没有 `bad object`、浅克隆缺失或被截断的 hash。merge `1ea8259dc3dd894d02fe5c69a0046fccec20dff4` 只记录路由与覆盖关系，不重复判断 `4f76dcb4e7b5bc86bba9a271b563a8b6a34d53f3` 的功能正确性。

- [ ] **Step 2: 写逐提交 findings-first 审查文档**

文档固定使用以下字段，每个 hash 各一节，并把实际 subject、路径和结论直接写入，不保留空字段：

```markdown
## Findings

按 P0/P1/P2/P3 严重度列出可复现 finding；无 finding 时写“未发现阻断项”。

## Commit decisions

每项记录 full hash、subject、UI 相关路径、非 UI 路径、版本文件影响、结论和冲突处理规则。

## Range conclusion

记录 11 个普通提交已审查、1 个 merge 路由关系已记录、允许进入 merge 的范围，以及明确排除在 P1–P7 之外的非 UI 行为。
```

Expected: `d161bd10adffdff42a9b85d09f7336a64a708f60` 有独立小节；`git rev-parse 8f2446b^{commit}` 的 full hash 有独立小节；findings 位于总体结论之前。

- [ ] **Step 3: 检查版本文件与分支命名影响**

Run:

```powershell
git diff 4f76dcb4e7b5bc86bba9a271b563a8b6a34d53f3^ 8ee4fa22ba0172a66605b2c5033f0736d66ced34 -- src/game/version.h docs/info.json CMakeLists.txt
git tag --sort=-version:refname | Select-Object -First 10
```

Expected: review 文档明确写出版本是否前进、倒退或重复；不根据远端 patch 自动接受版本号。

- [ ] **Step 4: Commit review evidence**

```powershell
git add docs/superpowers/reviews/archive/2026-07-11-settings-ui-p0-remote-review.md
git commit -m "docs(settings-ui): 记录远端基线逐提交审查" -m "docs: 固化 11 个普通提交、1 个路由 merge 的范围、findings 与冲突处理结论"
```

Expected: commit 只包含 review 文档。

### Task 3: 记录合并前红灯基线

**Files:**
- Create: `docs/superpowers/reports/archive/2026-07-11-settings-ui-p0-baseline.md`
- Test: existing build and gate targets

**Interfaces:**
- Consumes: 未合并的当前 `dyl_dev` checkout。
- Produces: 可与 merge 后结果逐项比较的构建、测试、gate 基线。

- [ ] **Step 1: 串行运行合并前基线**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
python qmclient_scripts/gate/check_docs.py
python qmclient_scripts/gate/check_gate.py --mode quick
```

Expected: 每条命令的退出码和失败测试名被原样记录；已有失败是合并前基线，不得在报告中写成 merge regression。

- [ ] **Step 2: 写基线报告头与证据表**

```markdown
# Settings UI P0 Baseline Report

**Start commit:** `git rev-parse dyl_dev` 的实际 40 位 hash。
**Target range:** `4f76dcb4e7b5bc86bba9a271b563a8b6a34d53f3^..8ee4fa22ba0172a66605b2c5033f0736d66ced34`
**Commit count:** 12 objects（11 个普通提交 + 1 个 merge 路由提交）

| Check | Command | Before merge evidence |
|---|---|---|
| game-client | 仓库规定的 Windows game-client build 命令 | 实际退出码、耗时和首个错误；成功时写 PASS |
| run_cxx_tests | 仓库规定的全量 C++ target | 实际通过/失败数与失败测试名 |
| check_docs | `python qmclient_scripts/gate/check_docs.py` | 实际退出码与摘要 |
| quick gate | `python qmclient_scripts/gate/check_gate.py --mode quick` | 实际退出码与失败阶段 |
```

Expected: “Before merge” 四项均填写实际命令、退出码和摘要；报告不声称未运行项通过。

- [ ] **Step 3: Commit pre-merge baseline**

```powershell
git add docs/superpowers/reports/archive/2026-07-11-settings-ui-p0-baseline.md
git commit -m "docs(settings-ui): 固化远端合并前验证基线" -m "test: 记录构建、C++ 回归、文档检查与 quick gate 结果"
```

Expected: commit 只包含 baseline 报告。

### Task 4: 显式 merge 并按单一路径解决冲突

**Files:**
- Modify: 仅 `git merge --no-ff 8ee4fa22ba0172a66605b2c5033f0736d66ced34` 报告的冲突文件
- Modify: `docs/superpowers/reviews/archive/2026-07-11-settings-ui-p0-remote-review.md`
- Test: merge tree and targeted tests

**Interfaces:**
- Consumes: Task 2 的逐提交结论和 Task 3 的合并前基线。
- Produces: 一个包含目标 range 的非 fast-forward merge commit；冲突文件不存在“远端新能力 + 本地旧 wrapper”双路径。

- [ ] **Step 1: 启动唯一允许的 merge**

Run:

```powershell
git merge --no-ff --no-commit 8ee4fa22ba0172a66605b2c5033f0736d66ced34
$MergeHead = git rev-parse --verify MERGE_HEAD
if($LASTEXITCODE -ne 0) { throw 'merge did not enter MERGE_HEAD state' }
$MergeHead = $MergeHead.Trim()
if($MergeHead -ne '8ee4fa22ba0172a66605b2c5033f0736d66ced34') { throw "unexpected MERGE_HEAD: $MergeHead" }
$MergeCachedPaths = @(git diff --cached --name-only)
$UnresolvedPaths = @(git diff --name-only --diff-filter=U)
$MergeCachedPaths
$UnresolvedPaths
```

Expected: `MERGE_HEAD` 精确等于目标 tip。无冲突 merge 的自动合并结果已在 `$MergeCachedPaths` 中，即使工作树 `git diff --name-only` 无输出也不得判定 merge 为空或失败；有冲突时 `$UnresolvedPaths` 只列出真实未解决路径。

- [ ] **Step 2: 应用冲突决策矩阵**

每个冲突只允许以下一种结论，并在 review 文档对应 commit 小节写明所选项：

```text
运行时新能力与本地旧 wrapper 冲突 -> 保留新能力，删除旧 wrapper
同一配置键默认值冲突 -> 保留当前已发布语义，另写显式迁移
版本号冲突 -> 选择不倒退且不重复已发布 tag 的版本
编辑器术语冲突 -> 保留上游兼容命名，不扩展设置页业务
协议/物理/预测/格式冲突 -> 停止 merge，升级为用户授权问题
```

Run:

```powershell
$ConflictPaths = @(git diff --name-only --diff-filter=U)
$ConflictPaths
if($ConflictPaths.Count -gt 0) {
	git add -- $ConflictPaths
	if($LASTEXITCODE -ne 0) { throw 'failed to stage resolved conflict paths' }
}
git diff --check
if($LASTEXITCODE -ne 0) { throw 'unstaged merge resolution failed diff --check' }
git diff --cached --check
if($LASTEXITCODE -ne 0) { throw 'staged merge resolution failed diff --check' }
$UnresolvedPaths = @(git diff --name-only --diff-filter=U)
$UnresolvedPaths
if($UnresolvedPaths.Count -ne 0) { throw 'merge still has unresolved paths' }
```

Expected: 先逐个按决策矩阵编辑 `$ConflictPaths`，再执行上述命令标记已解决；已暂存和未暂存的 `diff --check` 都无输出，最终未解决列表为空。不得在未逐文件检查冲突标记和决策结果前运行 `git add`。

- [ ] **Step 3: 对 d161 动画呈现值能力先写红灯测试**

在 `src/test/QmAnimTest.cpp` 增加一个直接覆盖 merge 后 exact signature 的测试：连续重定向 target 后 presentation value 保持有限、保留当前位置并向新目标前进。

```cpp
TEST(UiV2Animation, PresentationStateRetargetKeepsContinuousValue)
{
	CUiV2AnimationRuntime Runtime;
	constexpr uint64_t NodeKey = 0xD161BD10ULL;
	Runtime.SetValue(NodeKey, EUiAnimProperty::POS_X, 0.0f);
	ResolveUiPresentationStateValue(Runtime, NodeKey, EUiAnimProperty::POS_X, 10.0f, ui_spring::SNAPPY, 2, 0.01f);
	AdvanceFor(Runtime, 0.10f);
	const float BeforeRetarget = ResolveUiPresentationStateValue(Runtime, NodeKey, EUiAnimProperty::POS_X, 10.0f, ui_spring::SNAPPY, 2, 0.01f);
	const float AtRetarget = ResolveUiPresentationStateValue(Runtime, NodeKey, EUiAnimProperty::POS_X, -5.0f, ui_spring::SNAPPY, 2, 0.01f);
	EXPECT_TRUE(std::isfinite(BeforeRetarget));
	EXPECT_FLOAT_EQ(AtRetarget, BeforeRetarget);
	AdvanceFor(Runtime, 0.08f);
	const float AfterRetarget = ResolveUiPresentationStateValue(Runtime, NodeKey, EUiAnimProperty::POS_X, -5.0f, ui_spring::SNAPPY, 2, 0.01f);
	EXPECT_TRUE(std::isfinite(AfterRetarget));
	EXPECT_LT(AfterRetarget, BeforeRetarget);
	EXPECT_GT(AfterRetarget, -5.0f);
}
```

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=UiV2Animation.PresentationStateRetargetKeepsContinuousValue
```

Expected: 测试在接口或语义未正确合入时 FAIL；若 merge 后直接 PASS，仍作为远端行为的本地回归锚点保留。

- [ ] **Step 4: 完成 merge commit**

Run:

```powershell
$UnresolvedPaths = @(git diff --name-only --diff-filter=U)
$UnresolvedPaths
if($UnresolvedPaths.Count -ne 0) { throw 'merge still has unresolved paths before staging' }

$CachedMergePaths = @(git diff --cached --name-only)
$UnstagedResolvedPaths = @(git diff --name-only)
$AllowedEvidencePaths = @(
	'docs/superpowers/reviews/archive/2026-07-11-settings-ui-p0-remote-review.md',
	'src/test/QmAnimTest.cpp'
)
$UnexpectedUnstagedPaths = @($UnstagedResolvedPaths | Where-Object { $_ -notin $CachedMergePaths -and $_ -notin $AllowedEvidencePaths })
if($UnexpectedUnstagedPaths.Count -ne 0) { throw "unstaged paths are outside the merge/evidence scope: $($UnexpectedUnstagedPaths -join ', ')" }
$MergePaths = @($CachedMergePaths + $UnstagedResolvedPaths)
$MergePaths = @($MergePaths | Sort-Object -Unique)
$CachedMergePaths
$UnstagedResolvedPaths
$MergePaths
if($UnstagedResolvedPaths.Count -gt 0) {
	git add -- $UnstagedResolvedPaths
	if($LASTEXITCODE -ne 0) { throw 'failed to stage resolved paths and merge evidence' }
}

$UnresolvedPaths = @(git diff --name-only --diff-filter=U)
$UnresolvedPaths
if($UnresolvedPaths.Count -ne 0) { throw 'merge still has unresolved paths before commit' }

$CachedPathList = @(git diff --cached --name-only)
$CachedPathList
$MissingCachedPaths = @($MergePaths | Where-Object { $_ -notin $CachedPathList })
$UnexpectedCachedPaths = @($CachedPathList | Where-Object { $_ -notin $MergePaths })
if($MissingCachedPaths.Count -ne 0) { throw "paths missing from index: $($MissingCachedPaths -join ', ')" }
if($UnexpectedCachedPaths.Count -ne 0) { throw "unexpected cached paths: $($UnexpectedCachedPaths -join ', ')" }

git diff --cached

git diff --cached --check
if($LASTEXITCODE -ne 0) { throw 'cached merge diff failed diff --check' }
git commit -m "merge(settings-ui): 整合远端 UI 基线" -m "fix: 按权威规格收敛冲突并避免保留旧 UI 双路径" -m "test: 覆盖动画呈现值重定向连续性"
```

Expected: `$MergePaths` 以 merge 自动写入的 `$CachedMergePaths` 为主，再与未暂存但已解决的跟踪路径求并集；在 `git add` 前人工确认并集只包含 merge 结果、review 文档和该回归测试，任何计划外路径都先停止并查明来源。commit 前依次独立检查 unresolved list、cached path list、cached diff 和 cached `diff --check`；不以工作树是否有 diff 判定 merge 是否成功。`git show --first-parent --summary HEAD` 显示两个 parent，第二 parent 可达目标 tip。

### Task 5: 合并后校准规格并完成基线验收

**Files:**
- Modify: `docs/superpowers/reports/archive/2026-07-11-settings-ui-p0-baseline.md`
- Modify: `docs/superpowers/specs/2026-07-10-QmClient-设置页UI统一与滚动体系规格.md` — 仅同步 merge 改变的当前事实或经证明的 exact interface 变化。
- Modify if and only if an exact interface changed: `docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一实施索引.md`
- Modify if and only if affected by that exact interface change: `docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P1-Theme与SettingsCard基础.md`
- Modify if and only if affected by that exact interface change: `docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P2-Deck注册表Search与持久化.md`
- Modify if and only if affected by that exact interface change: `docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P3-InputField与NumericField.md`
- Modify if and only if affected by that exact interface change: `docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P4-Scroll与Dropdown.md`
- Modify if and only if affected by that exact interface change: `docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P5-标准设置页迁移.md`
- Modify if and only if affected by that exact interface change: `docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P6-QmClient与TClient迁移.md`
- Modify if and only if affected by that exact interface change: `docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P7-非卡片菜单性能与最终收口.md`
- Test: existing build, C++/Rust tests, docs and default gate

Conditional owner rule: P0 只负责将已证明的 merge 签名变化作为一个原子校准事务传播。实施索引必须参与每个 `changed` 事务；定义该接口的 P1/P2/P3/P4/P7 计划是 owner，其他 P1–P7 计划仅在实际消费该 exact interface 时纳入。未被 matrix 标为受影响的计划不得修改或暂存。

**Interfaces:**
- Consumes: Task 4 merge commit。
- Produces: 与 merge 后实际代码一致的 active spec、完成的前后基线报告，以及在确有签名变化时同步收敛的实施索引、interface owner 计划和所有受影响的 P1–P7 消费计划。

- [ ] **Step 1: 用 CodeGraph 重查 B1–B8 和 P1–P4 接口事实**

至少覆盖这些符号：

```text
ResolveUiPresentationStateValue
RenderQmSettingsGlassCard
BeginSettingsCardDeck
TextFieldEx
SliderInputField
CQmScrollState
CQmScrollController
CScrollRegion
QmResolveDropdownPopupPolicy
```

Expected: 只修改被 merge 改变的“当前事实”；目标设计和 R1–R3 边界不因远端实现自动变化。

先在 baseline 报告写入 interface delta matrix，每行记录 symbol、merge 前/后 exact signature、`HEAD^1..HEAD` 实际 path/hunk、owner、受影响的 P1–P7 计划和 `unchanged` / `changed`。`changed` 必须由 merge diff 和 merge 后 CodeGraph 签名共同证明旧契约无法继续使用；仅名称相似、远端已有实现或“顺便对齐”都不是改签名依据。

| Interface owner | Locked symbols | 受影响计划候选集 |
|---|---|---|
| P1 | `ResolveUiTheme`、`ResolveSettingsPageLayout`、`SettingsCard` | P1 owner；P2–P7 中引用该签名或其类型的每个消费计划 |
| P2 | `CSettingsCardDeck::Render`、`SettingsCardOrderModel`、`SearchCards`、`NavigateToSettingsCard` | P2 owner；P3–P7 中引用该签名或其类型的每个消费计划 |
| P3 | `ui_widget::InputField`、`ui_widget::NumericField` | P3 owner；P4–P7 中引用该签名或其类型的每个消费计划 |
| P4 | `CQmScrollController::Update`、`QmResolveScrollPolicy`、`CUi::RegisterWheelOwner`、`CUi::TryConsumeWheel` | P4 owner；P5–P7 中引用该签名或其类型的每个消费计划 |
| P7 | `QmLogMenuUiFramePerf` | P7 owner |

对每个 `changed` 行，必须同时修改 active spec、实施索引、owner 计划，以及扫描命中该 symbol/type 的所有受影响 P1–P7 计划。若所有行都是 `unchanged`，实施索引和 P1–P7 计划必须保持无 diff。修改前先确认条件性文档无既有 dirty 状态：

```powershell
$ContractDocs = @(
	'docs/superpowers/specs/2026-07-10-QmClient-设置页UI统一与滚动体系规格.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一实施索引.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P1-Theme与SettingsCard基础.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P2-Deck注册表Search与持久化.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P3-InputField与NumericField.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P4-Scroll与Dropdown.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P5-标准设置页迁移.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P6-QmClient与TClient迁移.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P7-非卡片菜单性能与最终收口.md'
)
$DirtyContractDocs = @(git status --short -- $ContractDocs)
$DirtyContractDocs
if($DirtyContractDocs.Count -ne 0) { throw 'contract documents were dirty before P0 calibration; identify the owner and do not reset them' }
```

校准后运行 exact-interface 一致性检查：

```powershell
$ContractDocs = @(
	'docs/superpowers/specs/2026-07-10-QmClient-设置页UI统一与滚动体系规格.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一实施索引.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P1-Theme与SettingsCard基础.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P2-Deck注册表Search与持久化.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P3-InputField与NumericField.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P4-Scroll与Dropdown.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P5-标准设置页迁移.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P6-QmClient与TClient迁移.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P7-非卡片菜单性能与最终收口.md'
)
$LockedSymbols = @(
	'ResolveUiTheme', 'ResolveSettingsPageLayout', 'SettingsCard',
	'CSettingsCardDeck::Render', 'SettingsCardOrderModel', 'SearchCards', 'NavigateToSettingsCard',
	'ui_widget::InputField', 'ui_widget::NumericField',
	'CQmScrollController::Update', 'QmResolveScrollPolicy', 'RegisterWheelOwner', 'TryConsumeWheel',
	'QmLogMenuUiFramePerf'
)
$MergeParents = @((git rev-list --parents -n 1 HEAD).Trim() -split '\s+')
if($MergeParents.Count -ne 3) { throw 'HEAD is not the Task 4 two-parent merge commit' }
if($MergeParents[2] -ne '8ee4fa22ba0172a66605b2c5033f0736d66ced34') { throw "unexpected second parent: $($MergeParents[2])" }
$MergeParents -join ' '
git diff --name-only HEAD^1 HEAD
foreach($Symbol in $LockedSymbols) {
	$Matches = @(rg -n --fixed-strings -- $Symbol $ContractDocs)
	if($LASTEXITCODE -ne 0) { throw "interface symbol missing from contract documents: $Symbol" }
	"=== $Symbol ==="
	$Matches
}
$ChangedContractPaths = @(git diff --name-only -- ($ContractDocs | Select-Object -Skip 1))
$ChangedContractPaths
git diff -- $ContractDocs
git diff --check -- $ContractDocs
if($LASTEXITCODE -ne 0) { throw 'contract documents failed diff --check' }
```

Expected: `HEAD` 是 Task 4 的双 parent merge commit，第二 parent 精确等于目标 tip。逐个对照 symbol 输出与 matrix：每个 `changed` exact signature 在 active spec、实施索引、owner 和全部受影响计划中一致，无旧参数、旧返回类型或兼容 wrapper。无签名变化时 `$ChangedContractPaths` 为空；有签名变化时，它精确包含实施索引、owner 和 matrix 中全部受影响的 P1–P7 计划。

- [ ] **Step 2: 串行运行合并后完整基线**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_rust_tests -j 14
python qmclient_scripts/gate/check_docs.py --sync-only --prefer agents
python qmclient_scripts/gate/check_docs.py
python qmclient_scripts/gate/check_gate.py --mode default
git diff --check
```

Expected: 全部退出码为 `0`；若失败，baseline 报告按“合并前已有 / merge 新增 / 环境失败”分类，未收口前 P0 不完成。

- [ ] **Step 3: 完成 after-merge 对照表**

在 baseline 报告追加实际结果，不保留空字段：

```markdown
## After merge comparison

| Check | After merge evidence | Delta classification |
|---|---|---|
| game-client | 实际退出码、耗时和首个错误；成功时写 PASS | unchanged / fixed / merge regression / environment |
| run_cxx_tests | 实际通过/失败数与失败测试名 | unchanged / fixed / merge regression / environment |
| check_docs | 实际退出码与摘要 | unchanged / fixed / merge regression / environment |
| default gate | 实际退出码与失败阶段 | unchanged / fixed / merge regression / environment |
```

Expected: 四行均写入实际证据和一种分类；不存在空白占位或未运行却标 PASS 的单元格。

- [ ] **Step 4: 派发独立只读 review 并收口 findings**

review 范围固定为 Task 4 merge commit、Task 5 spec/report diff，以及 interface delta matrix 列出的实施索引、owner 和全部受影响 P1–P7 diff，按 `docs/ai-workflow/review.md` 先列 findings。每个 P0/P1 finding 修复后重跑对应 focused test、`run_cxx_tests` 和 default gate。

Expected: 没有未处理的 P0/P1 finding；子代理报告已返回并附入 baseline 报告。

- [ ] **Step 5: Commit P0 evidence and proven contract calibration**

先对照 interface delta matrix 检查条件性文档 diff：无 `changed` 行时 `$ChangedContractPaths` 必须为空；有 `changed` 行时，它必须精确等于实施索引、对应 owner 与 matrix 列出的全部受影响 P1–P7 计划。不得仅因某个条件性文件已 dirty 就将它纳入。

```powershell
$EvidenceCandidates = @(
	'docs/superpowers/specs/2026-07-10-QmClient-设置页UI统一与滚动体系规格.md',
	'docs/superpowers/reports/archive/2026-07-11-settings-ui-p0-baseline.md'
)
$ConditionalContractCandidates = @(
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一实施索引.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P1-Theme与SettingsCard基础.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P2-Deck注册表Search与持久化.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P3-InputField与NumericField.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P4-Scroll与Dropdown.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P5-标准设置页迁移.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P6-QmClient与TClient迁移.md',
	'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一-P7-非卡片菜单性能与最终收口.md'
)
$IndexPath = 'docs/superpowers/plans/archive/2026-07-11-QmClient-设置页UI统一实施索引.md'
$PreexistingCachedPaths = @(git diff --cached --name-only)
if($PreexistingCachedPaths.Count -ne 0) { throw "index is not empty before P0 evidence staging: $($PreexistingCachedPaths -join ', ')" }
$EvidencePaths = @(git diff --name-only -- $EvidenceCandidates)
$ChangedContractPaths = @(git diff --name-only -- $ConditionalContractCandidates)
if($ChangedContractPaths.Count -gt 0 -and $ChangedContractPaths -notcontains $IndexPath) { throw 'conditional contract changes must include the execution index' }
$StagePaths = @($EvidencePaths + $ChangedContractPaths)
$StagePaths = @($StagePaths | Sort-Object -Unique)
if($StagePaths.Count -eq 0) { throw 'P0 evidence produced no changes' }
$StagePaths
git diff -- $StagePaths
git add -- $StagePaths
if($LASTEXITCODE -ne 0) { throw 'failed to stage P0 evidence paths' }
$CachedPathList = @(git diff --cached --name-only)
$CachedPathList
$MissingCachedPaths = @($StagePaths | Where-Object { $_ -notin $CachedPathList })
$UnexpectedCachedPaths = @($CachedPathList | Where-Object { $_ -notin $StagePaths })
if($MissingCachedPaths.Count -ne 0) { throw "P0 paths missing from index: $($MissingCachedPaths -join ', ')" }
if($UnexpectedCachedPaths.Count -ne 0) { throw "unexpected cached P0 paths: $($UnexpectedCachedPaths -join ', ')" }
git diff --cached
git diff --cached --check
if($LASTEXITCODE -ne 0) { throw 'cached P0 evidence failed diff --check' }
git commit -m "docs(settings-ui): 收口 P0 合并基线" -m "docs: 同步 merge 后代码事实、验证证据与保留 gap"
```

Expected: P0 报告明确记录 11 个普通提交的审查、1 个 merge 路由关系、merge commit、所有验证结果、interface delta matrix 和剩余 gap。无签名变化时 commit 不包含实施索引或 P1–P7；有签名变化时，commit 包含实施索引、owner 和全部受影响计划，且 cached path list 与 matrix 精确一致。P1 只从该 merge commit 开始。

---

## Self-review

- Spec coverage: 覆盖远端更新、逐提交审查、显式 merge、d161 动画测试、8f2446b 范围边界、版本检查、基线验证和 merge 后条件性规格/实施索引/P1–P7 契约校准。
- Marker scan: 未发现未决占位、虚构版本号或未定义命令参数。
- Type consistency: P0 不引入 P1–P4 公共 API；唯一新增测试直接绑定 merge 后的 `ResolveUiPresentationStateValue(...)`。只有 interface delta matrix 证明签名变化时，才同步 active spec、实施索引、owner 和所有受影响的 P1–P7 计划。
- Exit gate: 11 个普通提交和 1 个 merge 路由提交可追溯、merge 为双 parent、default gate 通过、独立 review 返回且无未收口 P0/P1 finding。
