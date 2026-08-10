# 上游 6c1710f UI 优先整合计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 在 `codex/upstream-sync-6c1710f` 分支上，先按上游 UI 理念整合 `1f362650d` 与 `35a28185c` 中对菜单/UI 的有效改动，并明确把汉化与非 UI 功能延期到后续批次。

**架构：** 本轮不整段 rebase，也不整提交硬吃，继续采用逐提交 `git cherry-pick -x` + 手工摘取。优先吸收影响 `menus.cpp`、`menus_settings.cpp`、`menus_browser.cpp` 与 `qmclient/menus_qmclient.cpp` 的 UI 入口，明确排除版本号、轨迹、HUD、nameplates、过时版本提示及其他非 UI 子改动。

**技术栈：** Git cherry-pick、C++、DDNet/QmClient 菜单系统、Windows Ninja 构建脚本、GoogleTest

---

## 范围与约束

- 当前分支：`codex/upstream-sync-6c1710f`
- 当前已吸收：
  - `4fc26774d` 的游戏内菜单/进服加载显示修复，已在本分支以手工择优方式落地
- 当前优先级：
  - 先 UI
  - 后汉化
  - 文档体系不跟上游走
  - 版本号体系保持本地口径
- 当前显式排除：
  - `322214ed5`、`597893411` 的汉化整合
  - `35a28185c` 中武器轨迹、轮廓、轨迹日志、过时版本警告、版本号 bump
  - `1f49a7344` 实时观战
  - 粒子、武器动画、图像处理重构等非当前 UI 批内容

## 热区概览

- `1f362650d`
  - `src/engine/shared/config_variables_qmclient_extra.h`
  - `src/game/client/components/menus_settings.cpp`
  - `src/game/client/components/qmclient/menus_qmclient.cpp`
  - `src/game/version.h`
- `35a28185c`
  - `src/game/client/components/menus.cpp`
  - `src/game/client/components/menus_browser.cpp`
  - `src/game/client/components/menus_settings.cpp`
  - `src/game/client/components/qmclient/menus_qmclient.cpp`
  - 以及一批本轮暂不吸收的非 UI 文件

本轮整合判断标准：

1. 只吸收能直接增强菜单/UI 一致性、设置入口或 UI 渲染语义的改动。
2. 遇到 `menus_qmclient.cpp` 这类高分叉文件，按“上游理念 + 本地结构”手工摘取，禁止为了吃提交而覆盖本地体系。
3. 不接受 `src/game/version.h` 改动。

### 任务 1：补齐 UI 优先整合的基线记录

**文件：**
- 创建：`docs/superpowers/plans/2026-06-04-upstream-sync-6c1710f-ui-first.md`
- 参考：`docs/Promting.md`
- 参考：`docs/superpowers/specs/2026-05-27-菜单-ui-统一设计.md`
- 参考：`docs/superpowers/explore/2026-05-26-上游合并与设置页性能现状探索.md`

- [ ] **步骤 1：记录当前基线**

运行：

```bash
git status --short --branch
git log --oneline --reverse b27f9b15328b799d198106a972b111672b80289a..6c1710fb0cebd9f84ac2d2433ff17d479ff691c2
```

预期：

```text
当前分支为 codex/upstream-sync-6c1710f
工作区干净或只存在明确保留的用户文档改动
上游区间提交列表与计划中的范围一致
```

- [ ] **步骤 2：固化本轮边界**

把以下边界写入本计划并保持为后续实现依据：

```text
先整合 UI，再处理汉化
按上游 UI 理念整合，但保留本地版本/文档/核心资源调度体系
仅对高分叉文件手工摘取，不做粗暴全量覆盖
```

### 任务 2：分析并摘取 `1f362650d` 的 UI 改动

**文件：**
- 修改：`src/engine/shared/config_variables_qmclient_extra.h`
- 修改：`src/game/client/components/menus_settings.cpp`
- 修改：`src/game/client/components/qmclient/menus_qmclient.cpp`
- 排除：`src/game/version.h`

- [ ] **步骤 1：先查看提交实际改动**

运行：

```bash
git show --stat --oneline --summary 1f362650d
git show 1f362650d -- src/engine/shared/config_variables_qmclient_extra.h src/game/client/components/menus_settings.cpp src/game/client/components/qmclient/menus_qmclient.cpp src/game/version.h
```

预期：

```text
确认它主要是“新版 UI”配置与设置入口调整
确认 version.h 仅为版本号更新，应排除
```

- [ ] **步骤 2：编写最小失败验证思路**

在开始摘取前，先确定本轮最小行为验证：

```text
菜单设置页仍可正常渲染
新增或调整的 UI 选项不会打乱现有布局
QmClient 设置页入口仍可进入对应分组
```

这一步不必新增自动化测试代码，但要明确后续至少运行对应 gtest 子集和构建验证。

- [ ] **步骤 3：手工摘取 UI 代码**

优先采用：

```bash
git cherry-pick -n 1f362650d
```

然后只保留以下方向的改动：

```text
新版 UI 的配置开关
menus_settings.cpp 中与设置逻辑/UI 入口相关的部分
menus_qmclient.cpp 中与新版 UI 选项接线直接相关的部分
```

必须手工回退以下内容：

```text
src/game/version.h
任何不属于 UI 入口的附带改动
```

如果 `git cherry-pick -n` 冲突过大，则改为人工对照 `git show` 逐块搬运。

- [ ] **步骤 4：跑最小验证**

运行：

```bash
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 10
cmake-build-release/testrunner.exe --gtest_filter=*Menus*:*Browser*
```

预期：

```text
至少菜单/浏览器相关测试继续通过
若该提交只影响设置页而无对应测试，则明确记录测试缺口
```

### 任务 3：拆分 `35a28185c`，只吸收 UI 菜单子集

**文件：**
- 修改：`src/game/client/components/menus.cpp`
- 修改：`src/game/client/components/menus_browser.cpp`
- 修改：`src/game/client/components/menus_settings.cpp`
- 修改：`src/game/client/components/qmclient/menus_qmclient.cpp`
- 排除：`src/engine/client/client.cpp`
- 排除：`src/engine/shared/console.cpp`
- 排除：`src/game/client/components/hud.cpp`
- 排除：`src/game/client/components/hud_editor.cpp`
- 排除：`src/game/client/components/nameplates.cpp`
- 排除：`src/game/client/components/qmclient/weapon_trajectory.cpp`
- 排除：`src/game/client/components/tclient/outlines.cpp`
- 排除：`src/game/client/components/tclient/trails.cpp`
- 排除：`src/game/client/components/voting.cpp`
- 排除：`src/game/version.h`

- [ ] **步骤 1：识别可吸收 UI 子块**

运行：

```bash
git show --stat --oneline --summary 35a28185c
git show 35a28185c -- src/game/client/components/menus.cpp src/game/client/components/menus_browser.cpp src/game/client/components/menus_settings.cpp src/game/client/components/qmclient/menus_qmclient.cpp
```

预期：

```text
分离出“UI 颜色处理函数”和“菜单 tab 渲染逻辑”这两类可吸收块
确认不把轨迹、轮廓、nameplates、投票和版本号混入
```

- [ ] **步骤 2：先加测试或补验证点**

若能找到已有菜单颜色/helper 测试入口，则补一条最小自动化测试；
若仓库当前没有合适单测入口，则至少补充本计划中的验证清单，确保后续检查：

```text
菜单 tab 的默认态/激活态颜色不回退
browser/settings 页面未因颜色 helper 替换而变成不可读或透明异常
```

- [ ] **步骤 3：分块搬运代码**

优先策略：

```text
先人工对照 git show，把 menus.cpp / menus_browser.cpp / menus_settings.cpp 的 UI helper 与调用点搬到当前分支
只在必要时从 menus_qmclient.cpp 摘对应设置项
```

禁止策略：

```text
不要用整提交 cherry-pick 后再大面积清理
不要接受非 UI 文件的附带修改
```

- [ ] **步骤 4：验证 UI 子批**

运行：

```bash
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 10
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 10
cmake-build-release/testrunner.exe --gtest_filter=*Menus*:*Browser*:*FastPractice*
```

预期：

```text
编译通过
4fc26774d 相关的 FastPractice/Menus/Browser 子集不回退
```

### 任务 4：记录 checkpoint，并决定是否进入汉化批

**文件：**
- 修改：`docs/superpowers/plans/2026-06-04-upstream-sync-6c1710f-ui-first.md`

- [ ] **步骤 1：记录证据**

按以下格式把 UI 整合结果写回本计划：

```text
Command: <exact command>
Result: <pass/fail and key output>
Scope: <what this proves>
Gaps: <what was not verified>
```

- [ ] **步骤 2：给出汉化批前置判断**

明确记录：

```text
哪些 UI 改动已吸收
哪些高冲突文件仍需后续人工整合
322214ed5 / 597893411 是否继续按模块摘取，而不是整提交 cherry-pick
```

## 当前 checkpoint

### 已吸收的 UI 子块

- `1f362650d`
  - 新增 `qm_new_ui` 配置开关
  - 在 QmClient 设置页“梦的小功能”中接入 `新版UI`
  - `RenderSettings` 支持在当前玻璃卡片式新 UI 和旧式 tabbar UI 之间切换
- `35a28185c`
  - 仅吸收 `menus.cpp` 中菜单 tab 默认态 / hover / active 的 `ui_color` 染色表达
  - 仅吸收 menubar 活动下划线跟随 `ui_color`
  - 未吸收 settings 面板配色重写、HUD/nameplates/轨迹/版本号等非本轮 UI 子块

### 当前明确不吸收的内容

- `35a28185c` 中：
  - `menus_settings.cpp` 的整套 `ui_color` 面板染色替换
  - `menus_browser.cpp` 的非 UI 健壮性修复
  - `hud*`、`nameplates.cpp`、`weapon_trajectory.cpp`、`outlines.cpp`、`trails.cpp`、`version.h`
- `1f362650d` 中：
  - `version.h`
  - 依赖未先落地链路的“显示版本过旧提示”

### 验证证据

Command: `python qmclient_scripts/gate/check_docs.py`
Result: pass
Scope: 文档入口与治理检查通过，新增计划文档没有破坏仓库文档入口
Gaps: 不证明运行时代码正确

Command: `qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 10`
Result: pass
Scope: `qm_new_ui` 配置接线和菜单相关改动至少能通过测试目标编译
Gaps: 不证明完整客户端已链接

Command: `cmake-build-release/testrunner.exe --gtest_filter=*Menus*:*Browser*:*FastPractice*`
Result: pass, 2 tests
Scope: 现有菜单/浏览器/FastPractice 相关最小回归集未被打坏
Gaps: 当前仓库没有直接覆盖 `QmNewUi` 切换路径或 menubar 着色的自动化测试

Command: `qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 10`
Result: shell 返回码噪音，改用日志重定向后确认 `DDNet.exe` 链接成功
Scope: 当前 UI 整合补丁可完成完整客户端构建
Gaps: 尚未做用户要求之外的截图式视觉验收

### 汉化批前置判断

- 当前可进入汉化批，但不适合整吃 `322214ed5` / `597893411`
- 更合理的方式是按模块摘取：
  - 先看 `menus_qmclient.cpp` 与 `menus_ingame.cpp` 的纯文案/UI 接线
  - 再判断 `hud.cpp`、`tclient/menus_tclient.cpp` 是否需要跟随
- 在进入汉化批前，应先完成本轮 UI 整合的只读审查并收口审查问题

### 任务 5：核心整合完成后做只读审查

**文件：**
- 审查：本轮实际修改文件

- [ ] **步骤 1：派发只读子代理**

审查范围：

```text
src/game/client/components/menus.cpp
src/game/client/components/menus_browser.cpp
src/game/client/components/menus_settings.cpp
src/game/client/components/qmclient/menus_qmclient.cpp
src/engine/shared/config_variables_qmclient_extra.h
```

审查重点：

```text
是否混入了非 UI 逻辑
是否有菜单渲染回退或设置接线遗漏
是否无意回退本地文档/版本/资源调度体系
```

- [ ] **步骤 2：修复审查问题后再考虑提交**

在子代理返回前，不把审查标记为完成。

## 规格覆盖自检

- 用户要求“下一步整合，先改 UI 再汉化（按照上游的理念）”，本计划已把 UI 放在汉化前，并把上游 UI 理念约束落到可执行任务。
- 当前已知高风险区 `menus_qmclient.cpp`、`menus_settings.cpp` 都有显式任务覆盖。
- 版本号、文档体系、性能资源调度体系都被明确标记为本轮非目标，避免误吸收。
