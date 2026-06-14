# QmClient i18n 英文 Key 统一收口

## 状态

- 日期：2026-06-12
- 状态：draft
- 适用范围：QmClient 客户端 UI 文案与本地化脚本

## 目标

把 QmClient 当前残留的中文源码 key、中文硬编码 UI 文案、以及 `data/qmclient/languages/` overlay 体系，统一收敛为一套 DDNet 原生风格的 i18n 模型：

- UI 文案统一走 `Localize("English key")`
- 客户端源码中不再保留 `Localize("中文")` 与 `Localizable("中文")`
- 客户端 UI 中不再保留未本地化的中文硬编码
- QmClient 语言条目并回 `data/languages/*.txt`
- 移除 `data/qmclient/languages/` overlay 加载链

## 已确认边界

### 包含

- `src/game/client/` 下客户端 UI 相关文案
- QmClient / TClient / QmUi / 菜单 / HUD / 通知栏 / 聊天导出 / console UI / tooltip / popup / 占位文案
- 已经是 `Localize("中文")` 的调用点
- 已经是 `Localizable("中文")` 的调用点
- 还没有走 `Localize(...)` 的中文 UI 字符串
- `qmclient_scripts/languages_qmclient/` 下现有 i18n 提取、迁移、生成、校验脚本
- `data/qmclient/languages/` 与 `data/languages/` 的语言资源收口
- 语言初始化与语言切换时的加载链

### 不包含

- 服务器侧文案
- 协议、游戏逻辑、预测、物理、地图行为
- 纯日志 / perf / 调试 / 诊断 payload 的英文硬编码

### 文案原则

- 中文原意不改，只补对应英文 key
- 迁移的目标是“中文显示内容保留，源码 key 改成英文”
- log 相关文本不强制进入 i18n
- 源码替换必须人工逐处修改，禁止脚本自动改源码
- 脚本只负责扫描、报告、生成语言资源、校验，不负责直接替换调用点

## 当前问题

1. 仓库里仍存在 `Localize("中文")` 与 `Localizable("中文")` 残留。
2. 仍存在不少中文 UI 硬编码，未经过 `Localize(...)`。
3. QmClient 当前语言加载依赖主语言文件后再叠加 `data/qmclient/languages/<lang>.txt`。
4. `qmclient_scripts/languages_qmclient/` 现有脚本已经部分收敛到英文 key 模型，但仓库代码现状没有完全跟上。
5. 当前缺少“先调查、再分类、再人工迁移”的统一口径，容易在扫描、替换、资源并回时产生漂移。
6. 继续维持 overlay + 混合 key，会让文本池预热、提取脚本、语言资源、运行时行为长期漂移。

## 目标模型

### 运行时

- 只保留 `data/languages/<lang>.txt` 作为客户端语言源
- 移除 QmClient overlay 二次加载
- 语言切换时只重载主语言文件并触发现有 UI 缓存失效

### 源码

- 所有 UI 文案统一使用 `Localize("English key")`
- 所有面向用户的 `Localizable("中文")` 统一改成英文 key
- 非本地化 UI 中文硬编码统一迁入 `Localize(...)`
- log / perf / 诊断文本若本来就是英文，可继续直接硬编码

### 语言资源

- 英文 key 进入 `data/languages/*.txt`
- `simplified_chinese.txt` 提供中文翻译
- 其他语言若无现成译文，可先保持英文占位，后续再补

## 实施前提

本 spec 不允许直接按“全仓替换”执行。正确顺序固定为：

1. 先调查现状
2. 再分类调用点与字符串类型
3. 再产出人工迁移清单
4. 最后按清单逐处手工修改

如果调查结果显示运行时加载链、测试钉子、语言资源生成口径与当前假设不一致，应先修订 spec / plan，再开始实现。

## 调查与分类

### 调查目标

在开始任何源码修改前，先回答下面几个问题：

- `Localize("中文")` 还剩多少处，分别在哪些模块
- `Localizable("中文")` 还剩多少处，分别在哪些模块
- 未本地化中文硬编码还剩多少处，其中哪些属于可见 UI
- overlay 加载链、语言切换链、测试钉子目前分别在哪些文件
- `data/qmclient/languages/` 与 `data/languages/` 的条目重叠、冲突、缺失情况如何

### 分类口径

扫描与人工审阅后，至少分为以下三类：

1. **必须迁移**
   - 直接面向用户的可见 UI 文案
   - `Localize("中文")`
   - `Localizable("中文")`
   - 中文按钮、标题、tooltip、popup、HUD、通知栏、菜单项

2. **待判定**
   - 命令预览文案
   - 帮助文本
   - 规则说明
   - 同时涉及 UI 展示与业务语义的混合文本

3. **排除项**
   - 服务器侧文案
   - 协议 / 解析 / 兼容性依赖字符串
   - 纯日志 / perf / 调试 / 诊断 payload
   - 注释、测试说明文字、非运行时用户可见文本

所有待判定项必须先进入审阅清单，不能交给脚本自动裁决。

## 推荐方案

采用“先调查分类、再人工迁移、最后收运行时”的分阶段方案：

1. **调查与分类**
   - 扫描所有 `Localize("中文")`
   - 扫描所有 `Localizable("中文")`
   - 扫描所有中文 UI 硬编码
   - 扫描应排除的 log / perf / 诊断文本
   - 生成可人工审阅的分类报告与未处理清单

2. **人工迁移与补洞**
   - 人工逐处把 `Localize("中文")` 改成 `Localize("English key")`
   - 人工逐处把 `Localizable("中文")` 改成英文 key
   - 人工逐处处理中文硬编码 UI 文案，补成 `Localize("English key")`
   - 对无法立即确定的文案保留未处理清单，人工补映射后再改

3. **并回资源**
   - 把 `data/qmclient/languages/` 条目并回 `data/languages/*.txt`
   - 让脚本链改为直接面向 `data/languages/`

4. **运行时收口**
   - 在语言资源与调用点稳定后，再移除 overlay 加载代码和专用生成口径
   - 同步修正相关测试与验证口径

这个方案优先保证可审计性，避免误伤注释、服务器侧文本、日志文本和兼容性字符串。

## 实施设计

### 1. 扫描层

保留并改造现有脚本：

- `qmclient_scripts/languages_qmclient/migrate_chinese_keys_to_english.py`
- `qmclient_scripts/languages_qmclient/extract_strings.py`
- `qmclient_scripts/languages_qmclient/generate_all.py`
- `qmclient_scripts/languages_qmclient/validate.py`

新增或补强的能力：

- 识别 `Localize("中文")`
- 识别 `Localizable("中文")`
- 识别客户端 UI 中文硬编码
- 区分“必须迁移 / 待判定 / 排除项”
- 输出未处理清单、冲突清单、人工审阅清单

脚本硬约束：

- 不允许自动改源码
- 不允许直接重写 `src/game/client/` 下调用点
- 只允许生成报告、语言资源和校验结果

### 2. 替换层

替换规则：

- `Localize("中文")` -> `Localize("English key")`
- `Localizable("中文")` -> 英文 key
- 中文 UI 字面量 -> `Localize("English key")`
- 格式化文本保留原占位符语义，如 `%s`、`%d`、`%.1f`
- 不改变中文翻译内容，只新增英文 key

替换方式：

- 所有调用点必须人工逐处修改
- 不使用脚本自动替换
- 每一批人工修改都应有对应的审阅范围

对以下情况允许人工兜底：

- 同一中文文案在不同上下文需要不同英文 key
- 同一英文 key 已在 DDNet base 中存在但语义不完全一致
- 文案既参与 UI 又参与协议/日志的混合路径

### 3. 资源层

资源收口规则：

- `data/qmclient/languages/` 逐步迁空并删除
- QmClient 专属 key 进入 `data/languages/simplified_chinese.txt`
- 若其它语言没有译文，先写英文占位，不阻塞主迁移
- 迁移后不再依赖 `english.txt` 反向映射或 overlay 占位

### 4. 运行时加载层

需要调整：

- 删除 `LoadQmClientLanguageOverlay(...)` 及其调用链
- 删除语言切换时对 overlay 的依赖
- 保留现有缓存失效逻辑，但以主语言文件为唯一源

这一层不与“文案 key 人工迁移”强绑定交付。只有在以下前提满足后才进入本阶段：

- 调用点迁移清单已基本收口
- 语言资源并回完成
- overlay 相关测试已同步更新
- 运行时加载链现状已复核

### 5. 通知栏适配

通知栏属于本次收口范围，要求：

- 所有通知栏 UI 文案走英文 key
- 静态规则中的用户可见文案也要完成英文 key 迁移
- 服务器消息是否进入通知栏的判定逻辑不变，本次只收口文案与 i18n 资源

## 风险与控制

### 风险

1. 批量替换误伤非 UI 字符串
2. 同义中文文案映射到错误英文 key
3. 删除 overlay 后遗漏语言资源，造成 fallback 直接显示英文
4. 文本池 key 变化后引入新的预热 miss

### 控制措施

- 所有脚本先支持 `--dry-run` 和报告输出
- 先生成未处理清单，再做人工修改
- 保留占位符一致性检查
- 迁移后补跑文本提取/校验脚本
- 针对 settings / notification / HUD 做一次重点复查
- 对 overlay 删除单独做一轮运行时与测试复查

## 验证要求

至少覆盖：

```text
Command: python qmclient_scripts/gate/check_docs.py
Result: pass
Scope: 文档与脚本入口一致性
Gaps: 不证明运行时 i18n 正确
```

```text
Command: python qmclient_scripts/gate/check_gate.py --mode quick
Result: pass
Scope: 仓库级快速门禁
Gaps: 不证明所有 UI 页面运行时显示都正确
```

以及本次新增/改造的 i18n 脚本自验证：

- 扫描结果稳定
- 报告结果可复现
- 语言文件 key 集一致
- overlay 移除后主语言链可正常工作

如果改到 `gameclient.cpp` 或主菜单加载链，补一条构建验证：

```text
Command: qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
Result: pass
Scope: 验证语言加载链改动可编译
Gaps: 不证明所有语言运行时无遗漏
```

## 交付物

1. 客户端 UI 文案不再出现 `Localize("中文")`
2. 客户端 UI 文案不再出现 `Localizable("中文")`
3. 客户端 UI 中文硬编码迁入 `Localize("English key")`
4. `data/qmclient/languages/` 并回并删除
5. overlay 加载链删除
6. i18n 脚本适配为 QmClient 当前目标模型
7. 输出迁移报告，便于后续补译和审计

## 不做的事

- 本轮不追求补齐所有非简中语言的人工翻译
- 本轮不改服务器文案本地化策略
- 本轮不顺手重构无关菜单结构或文本池实现

## 建议的执行顺序

1. 先改脚本扫描/报告能力
2. 再做现状调查与分类清单
3. 再人工迁移 `Localize("中文")` 与 `Localizable("中文")`
4. 再补中文硬编码 UI 文案
5. 再并回语言资源
6. 最后删除 overlay 加载链并收验证
