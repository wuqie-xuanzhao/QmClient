---
name: qmclient-i18n-audit
description: QmClient i18n 审查与修复：数字子集门禁、三头文件 config MACRO 提取、CJK Desc 英文化、DeepSeek 补缺与 validate 绿线。
---

# QmClient i18n 审查与修复

## 何时使用
排查翻译污染、缺失语言、配置 help 中文 key、digit 误杀、跨模块冲突，或迁移后复检绿线。

## 真相源
- 维护源：`translations/i18n/*.toml`
- 产物：`data/languages/*.txt`（`generate_all`）
- API：`.env` 中的 `DEEPSEEK_API_KEY`

## 配置 MACRO
`source_keys.CONFIG_MACRO_HELP_HEADERS`：
- `config_variables.h` → `menus`
- `config_variables_qmclient.h` → `qmclient`
- `config_variables_tclient.h` → `tclient`

匹配必须用完整相对路径 `endswith("src/engine/shared/config_variables.h")`，**禁止 basename**（会误匹配 `*_qmclient.h`）。

`Desc` = `m_pHelpLocalizeKey`，**source 必须英文**。

## CJK → 英文
```bash
py -3 qmclient_scripts/languages_qmclient/migrate_cjk_config_help.py \
  --map translations/_migrations/cjk_config_help_map_ddnet.json \
  --headers-only config_variables.h --generate-map --apply
```
- `cjk_config_help_map.json`：qmclient + tclient  
- `cjk_config_help_map_ddnet.json`：主配置  
- apply：改头文件 Desc + TOML 重映射；简中默认填旧中文  

## 数字门禁
`digits_compatible`：`required ⊆ got ⊆ required∪词(one…ten)`  
禁止把 one 朴素映射成 `1` 再做相等比较。

## 绿线标准
- `python -m unittest discover qmclient_scripts/languages_qmclient/tests` 通过  
- integrity = 0，quality errors = 0，12 语 missing = 0  
- 三头文件 active CJK key = 0  
- length risk、部分 `% to` 类 placeholder WARN 可非阻断  

## 注意
- 跨模块冲突：合并后删非首选  
- 新 key 只有简中时，其它语会批量 missing，同批补译  
- 禁止手改 `data/languages/*.txt`  
