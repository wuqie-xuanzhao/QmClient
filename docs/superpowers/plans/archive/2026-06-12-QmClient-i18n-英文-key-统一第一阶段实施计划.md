# QmClient i18n 英文 Key 统一第一阶段实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 完成 QmClient 第一阶段 i18n 收口：调查并分类所有中文 key / 中文 UI 文案，人工统一到英文 key，收敛语言资源生成口径，并产出进入第二阶段前所需的切换清单，为后续 overlay 删除与 i18n 性能优化建立干净基线。

**Architecture:** 本计划只覆盖第一阶段：“调查、分类、人工迁移、语言资源收口准备 + 第二阶段切换清单”。第一阶段确保 `Localize("中文")`、`Localizable("中文")` 和中文 UI 硬编码都进入统一英文 key 模型，但不删除 overlay、不改运行时主加载链、不修改第二阶段测试钉子。脚本仅用于扫描、报告、生成语言资源和校验，禁止自动改源码。

**Tech Stack:** C++（客户端 UI 与本地化调用点）、Python（语言扫描/生成/校验脚本）、DDNet/QmClient 现有 txt 语言格式、gate/docs 检查脚本。

---

## 文件结构与职责

- **调查与文档**
  - Create: `docs/superpowers/explore/2026-06-12-QmClient-i18n-调用点与分类清单.md`
    - 汇总 `Localize("中文")`、`Localizable("中文")`、中文 UI 硬编码、overlay/旁路加载链、待判定项。
  - Create: `docs/superpowers/reports/archive/2026-06-12-QmClient-i18n-人工迁移批次清单.md`
    - 记录每一批手工修改的模块、key 命名、待人工确认项。

- **脚本层**
  - Modify: `qmclient_scripts/languages_qmclient/extract_strings.py`
  - Modify: `qmclient_scripts/languages_qmclient/migrate_chinese_keys_to_english.py`
  - Modify: `qmclient_scripts/languages_qmclient/validate.py`
    - 只增强扫描、分类、报告、校验能力；不允许自动改 `src/` 源码。

- **源码层（按批次人工修改）**
  - Modify: `src/game/client/components/**/*.cpp`
  - Modify: `src/game/client/components/**/*.h`
  - Modify: `src/game/client/*.cpp`
  - Modify: `src/game/client/*.h`
    - 逐批把 `Localize("中文")` / `Localizable("中文")` / 中文 UI 硬编码迁到英文 key。

- **语言资源层**
  - Modify: `data/qmclient/languages/*.txt`
  - Modify: `data/languages/*.txt`
    - 第一阶段先完成 key 并表准备与一致性校验，不在本计划前半段删除 overlay。

- **第二阶段预留（不在本计划内实施）**
  - Future modify: `src/game/client/gameclient.cpp`
  - Future modify: `src/test/qm_new_ui_menu_branch_test.cpp`
    - 仅在调用点与语言资源收口稳定后，才进入 overlay 删除与测试迁移。

## Task 1: 固化调查清单与分类规则

**Files:**
- Create: `docs/superpowers/explore/2026-06-12-QmClient-i18n-调用点与分类清单.md`
- Modify: `docs/superpowers/specs/2026-06-12-QmClient-i18n-英文-key-统一收口.md`
- Test: `python qmclient_scripts/gate/check_docs.py --sync-only --prefer agents`

- [ ] **Step 1: 写调查文档框架**

```md
---
title: QmClient i18n 调用点与分类清单
date: 2026-06-12
type: question
status: active
scope:
  - src/game/client
  - qmclient_scripts/languages_qmclient
  - data/qmclient/languages
  - data/languages
confidence: medium
commit: HEAD
---

# QmClient i18n 调用点与分类清单

## 分类规则

### 必须迁移
- `Localize("中文")`
- `Localizable("中文")`
- 直接面向用户的中文按钮、标题、tooltip、popup、HUD、通知栏文本

### 待判定
- 命令预览
- 帮助文本
- 规则说明
- 混合 UI / 业务语义文本

### 排除项
- 日志 / perf / 调试 payload
- 协议 / 解析 / 兼容性依赖字符串
- 注释与非运行时用户可见文本
```

- [ ] **Step 2: 运行扫描命令，收集第一版列表**

Run:

```powershell
rg -n 'Localize\(".*[一-龥]' src/game/client
rg -n 'Localizable\(".*[一-龥]' src/game/client
rg -n '[一-龥]' src/game/client/components src/game/client
rg -n 'LoadQmClientLanguageOverlay|qmclient/languages|simplified_chinese.txt' src qmclient_scripts data
```

Expected:

```text
输出调用点列表与 overlay/旁路加载链位置；人工可据此写入分类清单。
```

- [ ] **Step 3: 把扫描结果人工归类写入调查文档**

```md
## 调查结果

### 必须迁移
- [ ] `src/game/client/components/console.cpp:548` `Localize("QmClient 聊天记录")`
- [ ] `src/game/client/components/hud.cpp:4525` `Localize("卡键: ?")`

### 待判定
- [ ] `src/game/client/components/chat.cpp:548` `"查询玩家%s的分数"`
- [ ] `src/game/client/components/chat.cpp:639` `"请求交换!球球惹"`

### 排除项
- [ ] `src/game/client/race_parse.cpp:31` `" 分钟 "`
- [ ] `src/game/client/race_parse.cpp:61` `" 完成了地图，用时："`
```

- [ ] **Step 4: 跑 docs 检查确认文档体系无漂移**

Run:

```powershell
python qmclient_scripts/gate/check_docs.py --sync-only --prefer agents
python qmclient_scripts/gate/check_docs.py
```

Expected:

```text
治理文档入口一致，未发现断链。
```

## Task 2: 限定脚本职责，只做扫描/报告/校验

**Files:**
- Modify: `qmclient_scripts/languages_qmclient/extract_strings.py`
- Modify: `qmclient_scripts/languages_qmclient/migrate_chinese_keys_to_english.py`
- Modify: `qmclient_scripts/languages_qmclient/validate.py`
- Test: `python qmclient_scripts/languages_qmclient/validate.py`

- [ ] **Step 1: 先写脚本行为约束测试或断言目标**

```python
# 目标行为（写入注释或测试辅助脚本）：
# 1. 扫描脚本输出分类报告
# 2. validate.py 校验 key / 占位符一致性
# 3. migrate_chinese_keys_to_english.py 不得写 src/ 下源码文件
```

- [ ] **Step 2: 修改 `migrate_chinese_keys_to_english.py`，把“自动改源码”逻辑移除或封死**

```python
def main():
    parser.add_argument("--report-only", action="store_true", default=True)
    # 仅生成映射建议、未处理清单、冲突清单
    # 不允许直接回写 src/game/client 下文件
```

- [ ] **Step 3: 修改 `extract_strings.py`，让输出包含分类标签**

```python
record = {
    "file": path,
    "line": line_no,
    "kind": "localize_zh",  # or localizable_zh / ui_hardcoded_zh
    "bucket": "must_migrate",  # or needs_review / excluded
    "text": text,
}
```

- [ ] **Step 4: 修改 `validate.py`，补“英文 key 与中文翻译配对完整性”检查**

```python
def validate_key_presence(english_key, translations):
    assert english_key in translations, f"missing key: {english_key}"
```

- [ ] **Step 5: 运行脚本校验**

Run:

```powershell
python qmclient_scripts/languages_qmclient/validate.py
```

Expected:

```text
现存问题被清楚报出；脚本不会尝试改源码文件。
```

## Task 3: 先做一批低风险人工迁移

**Files:**
- Modify: `src/game/client/components/console.cpp`
- Modify: `src/game/client/components/hud.cpp`
- Modify: `src/game/client/components/menus_ingame.cpp`
- Test: `qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14`

- [ ] **Step 1: 从调查清单里挑“纯 UI label / tooltip / popup”低风险项**

```md
优先批次：
- console 导出/提示文案
- HUD 状态行
- ingame menu 按钮文案
```

- [ ] **Step 2: 先写本批次人工迁移记录**

```md
## 批次 A
- 模块：console / hud / menus_ingame
- 原则：只改纯 UI 文案，不碰聊天预览和协议兼容字符串
- key 命名：沿用 DDNet 风格，必要时加 `qm.` 前缀
```

- [ ] **Step 3: 手工逐处修改源码**

```cpp
// before
Localize("聊天记录导出失败");

// after
Localize("Chat export failed");
```

```cpp
// before
Localizable("切换分身");

// after
Localizable("Toggle dummy");
```

- [ ] **Step 4: 把对应中文翻译补到语言资源**

```text
Chat export failed
== 聊天记录导出失败

Toggle dummy
== 切换分身
```

- [ ] **Step 5: 构建验证**

Run:

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
```

Expected:

```text
Build finished successfully.
```

- [ ] **Step 6: UI 视觉验收**

Run:

```text
启动客户端，至少检查以下页面：
- console 聊天记录导出界面
- HUD 相关状态行
- ingame menu 按钮区

检查维度：
- 默认 UI scale
- 至少一个非默认 UI scale
- hover / selected / disabled（如适用）
```

Expected:

```text
迁移后的英文 key 在运行时仍显示原中文译文；未出现截断、错位、状态切换异常。
```

## Task 4: 处理中风险项与待判定项

**Files:**
- Modify: `src/game/client/components/chat.cpp`
- Modify: `src/game/client/components/menus_settings_controls.cpp`
- Modify: `src/game/client/components/menus_start.cpp`
- Test: `python qmclient_scripts/languages_qmclient/validate.py`

- [ ] **Step 1: 为待判定项建立人工决策表**

```md
| File | Text | Decision | Reason |
| ---- | ---- | -------- | ------ |
| chat.cpp | 查询玩家%s的分数 | migrate | 用户可见命令预览 |
| race_parse.cpp | 完成了地图，用时： | exclude | 解析兼容路径 |
```

- [ ] **Step 2: 只迁移被确认属于用户可见 UI 的项**

```cpp
// before
str_format(pBuf, PreviewBufSize, "查询玩家%s的分数", aRestArg);

// after
str_format(pBuf, PreviewBufSize, Localize("Query %s's score"), aRestArg);
```

- [ ] **Step 3: 对仍有争议的字符串保留在清单，不强行改**

```md
- [ ] `src/game/client/race_parse.cpp` 保持排除，等待兼容性专项处理
```

- [ ] **Step 4: 跑语言校验**

Run:

```powershell
python qmclient_scripts/languages_qmclient/validate.py
```

Expected:

```text
新增 key 与翻译条目格式正确，占位符一致。
```

- [ ] **Step 5: UI 视觉验收**

Run:

```text
启动客户端，检查本批次涉及页面：
- chat 翻译设置相关界面
- browser/demo 中受影响的列表项、按钮或提示

检查维度：
- 默认 UI scale
- 至少一个非默认 UI scale
- hover / selected / disabled（如适用）
```

Expected:

```text
文本显示、布局和交互状态与迁移前一致；仅 key 口径变化，不引入可见 UI 回退。
```

## Task 5: 并回语言资源，保留 overlay 作为过渡兼容

**Files:**
- Modify: `data/qmclient/languages/*.txt`
- Modify: `data/languages/simplified_chinese.txt`
- Modify: `data/languages/index.txt`（仅当需要）
- Test: `python qmclient_scripts/languages_qmclient/validate.py`

- [ ] **Step 1: 建立并表规则**

```md
- 已确认英文 key 进入 `data/languages/*.txt`
- 中文翻译并入 `data/languages/simplified_chinese.txt`
- 未收口前，overlay 文件允许保留作为过渡镜像
```

- [ ] **Step 2: 手工并入本阶段新增 key**

```text
data/languages/simplified_chinese.txt
------------------------------------
Chat export failed
== 聊天记录导出失败
```

- [ ] **Step 3: 运行资源校验**

Run:

```powershell
python qmclient_scripts/languages_qmclient/validate.py
```

Expected:

```text
key 集一致；新增条目无格式错误。
```

## Task 6: 第一阶段收口与第二阶段切换清单

**Files:**
- Modify: `docs/superpowers/reports/archive/2026-06-12-QmClient-i18n-人工迁移批次清单.md`
- Test: `python qmclient_scripts/gate/check_gate.py --mode quick`

- [ ] **Step 1: 汇总第一阶段剩余项**

```md
## 阶段切换检查
- [ ] `Localize("中文")` 剩余数
- [ ] `Localizable("中文")` 剩余数
- [ ] 中文 UI 硬编码剩余数
- [ ] overlay 旁路依赖列表
- [ ] 私有简中库旁路列表
```

- [ ] **Step 2: 判断是否满足进入 overlay 删除阶段**

```md
进入第二阶段的条件：
1. 调用点迁移清单基本收口
2. 语言资源并回完成
3. 待判定项已裁决
4. overlay 测试迁移范围已明确
```

- [ ] **Step 3: 跑 quick gate，确认第一阶段补丁整体可进入下一轮**

Run:

```powershell
python qmclient_scripts/gate/check_gate.py --mode quick
```

Expected:

```text
Quick gate pass；若失败，记录是否为本任务外既有问题。
```

## 本计划明确不做

- 不在本计划内把语言文件改成 TOML
- 不在本计划内做 `Localize` 热点性能专项优化
- 不在本计划内处理服务器侧文案本地化策略
- 不在没有调查证据前引入新的 i18n 缓存层
- 不在本计划内删除 `LoadQmClientLanguageOverlay(...)`
- 不在本计划内修改 `src/test/qm_new_ui_menu_branch_test.cpp`
- 不在本计划内删除 `data/qmclient/languages/`

## 第二阶段后续计划入口

第一阶段完成后，再单独起下一份 plan：

1. 删除 `LoadQmClientLanguageOverlay(...)`
2. 并回 `data/qmclient/languages/`
3. 修复 `src/test/qm_new_ui_menu_branch_test.cpp`
4. 复核聊天/服务端的简中旁路查找
5. 再起一轮 `Localize` 热点与文本容器重建的性能调查
