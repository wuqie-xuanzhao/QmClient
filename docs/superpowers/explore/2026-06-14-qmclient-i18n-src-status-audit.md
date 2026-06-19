---
type: question
date: 2026-06-14
status: active
confidence: high
scope:
  - src/
  - qmclient_scripts/languages_qmclient/
  - data/languages/simplified_chinese.txt
commit: cecb13e74
related:
  - file: 2026-06-12-QmClient-i18n-英文-key-统一收口.md
    relation: complements
---

## Quick Answer

当前 `src/` 的 i18n 主路径已经统一成英文 source key：`extract_strings.py` 最新提取结果为 `2628` 个 active key，`CJK unique strings: 0`，说明 `Localize/Localizable/Register help` 主链上已经没有中文 source key 残留。运行时简中生成产物也覆盖了全部 `2628` 个 active key，`validate.py` 已通过，所以“当前 active key 无简中覆盖缺口”这个结论成立。

刚才服务器列表加载英文残留的直接原因不是运行时缓存，而是 `server_browser.toml` 当时缺了 3 个条目；补齐后已经能在维护源和运行时产物中同时找到。仍需注意的是，`src/` 中还有不少中文字符串并不走 `Localize` 主链，它们主要分成三类：测试数据、注释/日志、以及面向中文语境的业务字符串或匹配规则。这些不等于“漏翻译”，但意味着“整个项目的文本并没有完全收敛到统一 i18n 系统”。

## Key Evidence

| # | Conclusion | Evidence | Location |
|---|-----------|----------|----------|
| 1 | active i18n 主链已无中文 source key | `extract_strings.py` 最新输出显示 `CJK unique strings: 0`，`Total unique strings: 2628` | `qmclient_scripts/languages_qmclient/extract_strings.py:28`, `qmclient_scripts/languages_qmclient/extracted_strings.txt:1` |
| 2 | 当前 active key 已被简中运行时文件全量覆盖 | `validate.py` 输出 `OK: simplified_chinese.txt covers 2628 source keys` | `qmclient_scripts/languages_qmclient/validate.py:58-60` |
| 3 | 模块化维护源已覆盖 active key，且 legacy overlay 已移除 | `validate.py` 输出 `OK: module i18n store loaded 2631 simplified_chinese translations from 9 modules` 和 `OK: legacy overlay directory removed` | `qmclient_scripts/languages_qmclient/validate.py:62-79`, `qmclient_scripts/languages_qmclient/validate.py:81-87` |
| 4 | 服务器列表英文残留的根因是维护源缺条目，而非源码未提取 | 这 3 个字符串在源码中确实走了 `Localize(...)`，但此前不在 `server_browser.toml`/运行时 txt 中；现已补入 `server_browser.toml` | `src/game/client/components/menus_browser.cpp:767`, `src/game/client/components/menus_browser.cpp:784`, `src/game/client/components/menus_browser.cpp:800`, `qmclient_scripts/languages_qmclient/translations/i18n/server_browser.toml:207`, `qmclient_scripts/languages_qmclient/translations/i18n/server_browser.toml:262`, `qmclient_scripts/languages_qmclient/translations/i18n/server_browser.toml:432` |
| 5 | 之前 4 个中文 help/source key 已统一收为英文 key | `rules/info/+showweapontrajectory/qm_timeout_disconnect` 的 help 文本已改为英文，且提取结果中能找到对应英文 key | `src/game/server/gamecontext.cpp:3987`, `src/game/server/gamecontext.cpp:3993`, `src/game/client/components/controls.cpp:146`, `src/engine/client/client.cpp:5318`, `qmclient_scripts/languages_qmclient/extracted_strings.txt:1896`, `qmclient_scripts/languages_qmclient/extracted_strings.txt:1951`, `qmclient_scripts/languages_qmclient/extracted_strings.txt:1957`, `qmclient_scripts/languages_qmclient/extracted_strings.txt:1983` |
| 6 | `src/` 仍存在不少中文字符串，但多数不属于 active i18n source key 缺口 | 例子包括资产别名表、聊天命令预览文案、HUD/消息匹配规则、中文语言名数组等，说明“项目文本”与“i18n 主链 source key”不是同一集合 | `src/game/client/components/assets_resource_registry.cpp:17-66`, `src/game/client/components/chat.cpp:548-675`, `src/game/client/components/qmclient/menus_qmclient.cpp:3399`, `src/game/client/components/tclient/swap_countdown_message.cpp:40-55` |

## Details

### 1. 现在“没有遗漏”可以怎么定义

如果定义是：
- 所有走 `Localize/Localizable/Register help` 的 active source key
- 都应该有英文 key 身份
- 并且在简中运行时文件里可落到译文或保留项

那当前这条链路已经收口：
- active key 数：`2628`
- 简中运行时覆盖：`2628`
- 中文 source key：`0`
- duplicate key groups：`0`
- candidate unused entries：`0`

如果定义放宽成“整个 `src/` 里出现的所有面向用户的文本都必须统一纳入 i18n”，那当前答案是否定的。仓库里还存在若干未进入统一 i18n 主链的文本来源。

### 2. `src/` 文本现状分层

按现状大致可分为四层：

1. **主 i18n 链路文本**
   - 来源：`Localize` / `Localizable` / `Register` help / 少量间接提取
   - 状态：已统一为英文 key，简中全量覆盖

2. **运行时中文业务字符串 / 规则匹配文本**
   - 例如聊天命令预览、HUD 通知中文规则、swap 倒计时中文匹配
   - 这些很多是“业务逻辑内容”或“兼容中文服务端消息”的匹配样本，不一定适合直接进当前翻译表

3. **资产/别名/语言名等数据字符串**
   - 例如 `assets_resource_registry.cpp` 的中文别名，`menus_qmclient.cpp` 里的语言名数组 `中文/日本語/...`
   - 这类更像静态数据或数据驱动映射，不完全等同于 UI 文案

4. **测试与注释中的中文**
   - 不属于用户运行时 i18n 范围

### 3. 为什么还会出现“维护源 2631 条，active key 2628 条”

`validate.py` 当前只证明两件事：
- active key 全被覆盖
- 模块化维护源可正常读取

它不强制维护源必须与 active key 数完全相等。所以现在维护源里多出少量非 active 条目是允许状态。结合 `review_duplicate_entries.py --show-unused 0` 的结果 `Candidate unused entries: 0`，至少没有被当前 unused 检查判为无效垃圾条目，但这 3 条额外记录仍建议后续做一次精确盘点。

## Exploration Scope

- Focused directories: `src/`, `qmclient_scripts/languages_qmclient/`
- Files involved: `extract_strings.py`, `source_keys.py`, `validate.py`, `review_duplicate_entries.py`, `translations/i18n/server_browser.toml`, `data/languages/simplified_chinese.txt`, `src/game/client/components/menus_browser.cpp`, `src/game/server/gamecontext.cpp`, `src/engine/client/client.cpp`, `src/game/client/components/controls.cpp`
- Skipped: 没有逐个核对 `src/` 中每一条中文业务字符串是否都应该进入 i18n；本次只确认了主链覆盖状态和明显漏项类别

## Confidence Notes

**confidence: high**

- active source key 提取、运行时生成、维护源校验三条证据链都已重跑并通过
- 服务器列表英文残留已经定位到具体缺失条目并补齐
- `src/` 中文字符串扫描只用于分类说明，不直接等价于“未国际化缺陷列表”

## Open Questions

- 模块化维护源比 active key 多出的 3 条记录具体是哪几条，仍值得做一次离线精确盘点
- `src/` 中聊天预览、中文服务端消息匹配、资产别名等文本，哪些应该继续留在业务层，哪些值得纳入统一 i18n，需要单独定规则
- 是否要把“启发式扫描裸字符串”的检查也纳入 `languages_qmclient` 或 gate，防止以后再漏 `Localize(...)` 但忘补维护源

## Related Documents

- `2026-06-12-QmClient-i18n-英文-key-统一收口.md` — 本文补充当前收口后的全仓库状态与剩余文本类型

## Next Steps

如果你要继续把 `src/` 的“非主链文本”也系统化，下一步最值钱的是先把“应该进 i18n 的文本类型边界”定清楚，再按类型分批清理，而不是继续无差别扫中文字符串。
