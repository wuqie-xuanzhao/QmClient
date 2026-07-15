---
name: qmclient-i18n-audit
description: QmClient i18n 审查与修复：digit 子集门禁、三头文件 config MACRO 提取（ddnet/qmclient/tclient）、CJK Desc→英文迁移、DeepSeek 补缺与 validate 绿线。
---

# QmClient i18n 审查与修复

## 真相源
- 维护源：`qmclient_scripts/languages_qmclient/translations/i18n/*.toml`
- 产物：`data/languages/*.txt`（`generate_all`）
- API：`.env` 的 `DEEPSEEK_API_KEY`

## 配置 help / MACRO
`source_keys.CONFIG_MACRO_HELP_HEADERS`：
- `config_variables.h` → 模块 `menus`
- `config_variables_qmclient.h` → `qmclient`
- `config_variables_tclient.h` → `tclient`

匹配必须用**完整相对路径** `endswith("src/engine/shared/config_variables.h")`，禁止 basename（会误匹配 `*_qmclient.h`）。

运行时：`m_pHelpLocalizeKey` = MACRO `Desc`；source 必须英文。

## CJK → 英文
```bash
# 全量或按头文件
py -3 qmclient_scripts/languages_qmclient/migrate_cjk_config_help.py \
  --map translations/_migrations/cjk_config_help_map_ddnet.json \
  --headers-only config_variables.h --generate-map --apply
```
- 旧 map：`cjk_config_help_map.json`（qmclient+tclient）
- 主配置 map：`cjk_config_help_map_ddnet.json`
- apply：改 Desc + TOML remap；SC 默认填旧中文

## Digit 门禁
`digits_compatible`：`required=numeric(src) ⊆ numeric(tgt) ⊆ required∪word(one…ten)`  
禁止朴素 one→1 相等比较。

## 标准链
extract →（draft/write-back）→ generate_all → validate --incremental

## 绿线
- unittest discover OK
- integrity 0、quality errors 0、12 语 missing 0
- 三头文件 active CJK key count 0
- length risk / 部分 `% to` placeholder WARN 可非阻断

## 注意
- 跨模块冲突：merge 后删非首选
- 新 key 仅 SC 时其它语批量 missing，同批补译
- 勿手改 `data/languages/*.txt`
