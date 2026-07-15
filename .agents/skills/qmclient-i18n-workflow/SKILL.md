---
name: qmclient-i18n-workflow
description: QmClient 翻译脚本链：extract → generate → validate → review_duplicate；TOML 真相源、txt 产物；配置 MACRO Desc 必须英文 source key；draft 审核后 write-back。
---

# QmClient i18n 翻译工作流

## 适用场景
改动涉及 `qmclient_scripts/languages_qmclient/`、`data/languages/*.txt`、`translations/i18n/*.toml`，或源码中 `Localize` / `Localizable` / `Register` help / `MACRO_CONFIG_*` Desc 时。

## 真相源分层
- `qmclient_scripts/languages_qmclient/translations/i18n/*.toml` — **翻译维护库**
- `data/languages/*.txt` — **运行时生成产物**，禁止手改

## 配置说明（config help）硬约束
- `MACRO_CONFIG_INT/COL/STR` 的最后参数 `Desc` **必须是英文 source key**（与 DDNet Localize 约定一致）。
- 运行时：`m_pHelpLocalizeKey = Desc`，UI 走 `Localize(Desc)`；英文界面无译时回退显示 source，因此中文 Desc 会在英文 UI 直接露中文。
- 中文、繁中等译文写在 TOML 对应语言字段，**不要**把中文写进头文件 Desc。
- 提取范围：`source_keys.CONFIG_MACRO_HELP_HEADERS` 当前包含：
  - `src/engine/shared/config_variables.h` → 模块 `menus`
  - `src/engine/shared/config_variables_qmclient.h` → `qmclient`
  - `src/engine/shared/config_variables_tclient.h` → `tclient`
- CJK→英文迁移工具：`migrate_cjk_config_help.py`（`--generate-map` / `--apply`，可用 `--map` / `--headers-only`）
- 映射产物：`translations/_migrations/cjk_config_help_map*.json`

## 标准验证链
```bash
python qmclient_scripts/languages_qmclient/extract_strings.py
python qmclient_scripts/languages_qmclient/generate_all.py
python qmclient_scripts/languages_qmclient/validate.py
python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0
```

## 新增英文 source key
1. `extract_strings.py` 更新 active keys  
2. `translate_with_local_http.py --languages <lang>` 生成 draft（补缺**不要** `--rewrite`）  
3. 审核 `translations_draft/` 后 `--write-back`  
4. `generate_all.py` → `validate.py`

## 常见约束
- 不手改 `data/languages/*.txt`
- draft 未审核不得 write-back
- digit 门禁：`required=numeric(src) ⊆ numeric(tgt) ⊆ required∪word(one…ten)`（见 `translate_with_local_http.digits_compatible`）
- 跨模块同 identity：first-wins + integrity 错误；合并后删非首选
