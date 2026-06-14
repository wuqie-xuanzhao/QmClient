# QmClient i18n 分类规则与审计脚本设计

## 状态

- 日期：2026-06-14
- 状态：draft
- 适用范围：`qmclient_scripts/languages_qmclient/`、QmClient 客户端文案分类、通知栏字符串治理

## 目标

在现有“英文 source key + 模块化 TOML 维护源 + 生成运行时语言文件”的基础上，再补一层稳定的**字符串职责分类规则**，解决下面三个问题：

1. 哪些字符串必须进入 i18n 主链。
2. 哪些字符串本质上是业务/匹配数据，不应被误判成“漏翻译”。
3. 哪些字符串只允许出现在测试中，不应污染正式运行时或翻译维护源。

本次不改运行时 i18n 模型，不改通知栏行为，只补**规则、脚本审计能力和现状分类清单**。

## 已确认前提

- 当前 active i18n 主链已经统一成英文 source key。
- `qmclient_scripts/languages_qmclient/translations/i18n/*.toml` 是翻译维护源。
- `data/languages/simplified_chinese.txt` 是生成产物，不再作为手工真相源。
- 通知栏存在两类文本：
  - 客户端自有规范通知文案
  - 外部服务器消息的兼容匹配/透传文本

## 规则总则

不要再用“是不是中文字符串”来判断是否应该进 i18n。

统一改成按**职责**判断：

1. 它是给用户读的，还是给程序匹配的。
2. 它是客户端自己写给用户的，还是外部输入原样透传的。
3. 它在运行时承担“展示”职责，还是承担“识别/解析/兼容”职责。

只有“客户端自有、面向用户展示的稳定文案”才属于 i18n 主链范围。

## 分类定义

### 1. `must_i18n`

定义：客户端自有、直接面向玩家展示、语义稳定的产品文案。

必须包含：

- `Localize(...)` / `Localizable(...)` source key
- `Register(..., help_text, ...)` help 文案
- 菜单、按钮、标题、标签、tooltip、popup、设置项文本
- HUD、通知栏、状态栏、错误提示、加载提示
- 客户端主动生成的、稳定可枚举的提示文案

判定标准：

- 去掉服务器输入和外部数据后，这句话仍然是客户端自己要说的话
- 这句话应该允许多语言翻译
- 这句话应该进入 `translations/i18n/*.toml`

### 2. `business_data`

定义：业务匹配、兼容识别、解析、映射用字面量，不是产品文案。

包含：

- 服务器消息兼容匹配字面量
- 通知栏规则中的英文/中文上游消息别名
- 协议/命令/关键字/资源别名/解析模板
- 正则、分支判定前缀、状态识别字面量
- 外部输入透传文本的比较样本

判定标准：

- 字符串的职责是“让程序识别某类输入”
- 即使它长得像一句中文/英文自然语言，它也不是翻译源
- 不应进入 i18n active key 集合

### 3. `test_only`

定义：仅用于测试覆盖、回归保护、fixture 或断言的字符串。

包含：

- `src/test/` 下测试样本
- `qmclient_scripts/languages_qmclient/test_*.py` 中的测试文本
- 非运行时的 fixture、错误样本、边界样本

判定标准：

- 它只为验证代码行为而存在
- 不参与正式运行时逻辑
- 不进入翻译维护源

### 4. `needs_review`

定义：脚本无法仅靠路径和调用形态可靠判定职责的字符串。

典型情况：

- 同时可能承担 UI 展示和匹配职责
- 裸字符串位于客户端代码中，但不在 `Localize(...)` / `Register help` / 测试路径内
- 通知栏、聊天、资源映射等混合区域中的新字面量

处理规则：

- 报告出来
- 不自动阻断迁移
- 需要人工归类到 `must_i18n` 或 `business_data`

### 5. `violation`

定义：规则已足够明确，可以认定为不符合当前 i18n 口径。

包含：

- 明显面向用户展示的稳定文案，却没有进入 i18n 主链
- 明显是匹配/兼容字面量，却误被当作翻译源维护
- 测试字符串泄漏到正式翻译维护源

处理规则：

- 审计脚本返回非 0
- 必须修复或显式调整规则后再通过

## 通知栏专项规则

通知栏不是单一路径 i18n，必须拆成两层：

### A. 规范通知文案

这类文本属于 `must_i18n`：

- 客户端自己定义的 canonical English 通知文案
- 规则归一化后的目标文本
- 预览通知文案

要求：

- 统一用英文 canonical key
- 进入 `translations/i18n/*.toml`
- 运行时通过 `Localize(...)` 展示

### B. 上游消息匹配字面量

这类文本属于 `business_data`：

- 服务器发来的英文消息原文
- 为兼容中文服务器/旧消息保留的中文匹配串
- 仅用于识别消息语义的别名串

要求：

- 保留在规则层
- 不作为 i18n source key
- 不因其是中文而被当作“漏翻译”

### C. 未归一化透传文本

这类文本默认按外部输入看待，不算客户端缺翻译：

- 无法识别、无法归一的服务器消息
- 原样进通知栏/聊天显示的外部输入

规则：

- 不进入 `must_i18n`
- 不进入翻译维护源
- 若后续确认属于稳定客户端通知语义，再人工提升为 canonical 文案

## 脚本设计

### 1. `extract_strings.py`

在保留现有 active source key 提取输出的前提下，增加**职责分类审计输出**。

输出拆成两部分：

1. active i18n source key 清单
   - 继续写 `extracted_strings.txt`
   - 只包含当前 i18n 主链需要的 source key

2. 分类审计报告
   - 新增审计输出文件
   - 至少包含：
     - `must_i18n`
     - `business_data`
     - `test_only`
     - `needs_review`
     - `violation`

脚本职责：

- 继续负责 i18n 主链 key 提取
- 额外扫描指定范围内的字符串职责
- 不改源码
- 不自动迁移

### 2. 分类识别优先级

优先按**强证据**分类：

1. `src/test/**`、`test_*.py` -> `test_only`
2. `Localize(...)` / `Localizable(...)` / `Register help` -> `must_i18n`
3. 已知通知栏规则匹配表/匹配函数中的字符串 -> `business_data`
4. 其他明确的匹配/映射表路径 -> `business_data`
5. 剩余无法确认项 -> `needs_review`

仅在满足明显违规条件时标记为 `violation`。

### 3. `validate.py`

保持现有检查：

- `extracted_strings.txt` 新鲜度
- `simplified_chinese.txt` 覆盖 active source keys
- 模块化 i18n store 可读
- legacy overlay 已移除

新增检查：

- 审计报告中的 `violation` 数量

失败条件：

- 任一现有硬性校验失败
- `violation > 0`

非失败提示：

- `needs_review > 0` 仅打印提醒，不返回失败

## 审计输出格式

建议新增一个稳定文件，例如：

- `qmclient_scripts/languages_qmclient/extracted_audit_report.json`

字段至少包含：

- `summary`
- `must_i18n`
- `business_data`
- `test_only`
- `needs_review`
- `violation`

每条记录至少包含：

- `file`
- `line`
- `text`
- `reason`
- `category`

如果已有 `context`、`source`、`function`、`module` 信息，允许附带，但不是硬要求。

## 通知栏现状清单要求

本次实现后，需要产出一份通知栏字符串分类清单，至少覆盖：

- 规范通知文案样本
- 服务器消息英文匹配样本
- 服务器消息中文兼容匹配样本
- 透传消息路径说明

目标不是列出所有字符串，而是把规则边界讲清楚，避免后续重复误判。

建议归档到 `docs/superpowers/explore/`，作为当前状态证据文档。

## 明确不做

- 不改通知栏运行时行为
- 不把所有业务匹配字面量强行改成 `Localize(...)`
- 不把 `needs_review` 默认提升为失败
- 不接入仓库级 `check_gate.py` 作为默认阻断项
- 不新增临时测试文件

## 成功标准

满足以下条件即可视为本轮完成：

1. `extract_strings.py` 能稳定输出 active key 清单和分类审计报告
2. `validate.py` 能对 `violation` 返回非 0，对 `needs_review` 只提示
3. 仓库文档明确写清分类规则
4. 通知栏现状分类清单已归档
5. 不改变现有 runtime i18n 生成链和通知栏行为

## 验证

至少覆盖：

```text
python qmclient_scripts/languages_qmclient/extract_strings.py
python qmclient_scripts/languages_qmclient/validate.py
python qmclient_scripts/languages_qmclient/generate_all.py
python qmclient_scripts/gate/check_docs.py
```

如本轮改动涉及通用脚本文档入口，再补：

```text
python qmclient_scripts/gate/check_docs.py --sync-only --prefer agents
```

## 后续实现顺序

1. 扩展 `extract_strings.py` 和共享扫描逻辑
2. 扩展 `validate.py` 的违规检查
3. 更新 `qmclient_scripts/scripts_overview.md`
4. 归档通知栏字符串分类清单
5. 跑脚本验证
