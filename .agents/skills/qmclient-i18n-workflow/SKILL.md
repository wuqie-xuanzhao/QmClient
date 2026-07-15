---
name: qmclient-i18n-workflow
description: QmClient 翻译工作流：extract → generate → validate → review_duplicate；TOML 为真相源、txt 为产物；配置 MACRO Desc 必须英文 source key；draft 审核后再 write-back。
---

# QmClient 翻译工作流

## 何时使用
改动涉及：
- `qmclient_scripts/languages_qmclient/`
- `data/languages/*.txt`
- `translations/i18n/*.toml`
- 源码中的 `Localize` / `Localizable` / `Register` help / `MACRO_CONFIG_*` 的 `Desc`

## 真相源
| 层级 | 路径 | 说明 |
|------|------|------|
| 维护源 | `qmclient_scripts/languages_qmclient/translations/i18n/*.toml` | 可改 |
| 运行时产物 | `data/languages/*.txt` | **禁止手改**，由 `generate_all.py` 生成 |
| 草稿 | `translations_draft/<语言>/*.toml` | 模型输出，审核后才回填 |

## 配置说明硬约束
`MACRO_CONFIG_INT` / `COL` / `STR` 的最后一个参数 `Desc` 是 **Localize 的 source key**：

- **必须写英文**；中文、繁中等只进 TOML 对应语言字段
- 运行时：`m_pHelpLocalizeKey = Desc`，UI 调 `Localize(Desc)`
- 英文界面无译文时回退显示 source；Desc 若是中文，英文 UI 会直接露中文

### 提取范围
| 头文件 | 模块 |
|--------|------|
| `src/engine/shared/config_variables.h` | `menus` |
| `src/engine/shared/config_variables_qmclient.h` | `qmclient` |
| `src/engine/shared/config_variables_tclient.h` | `tclient` |

历史中文 Desc 用 `migrate_cjk_config_help.py`（`--generate-map` / `--apply`）迁移；映射在 `translations/_migrations/`。

## 标准命令链
```bash
python qmclient_scripts/languages_qmclient/extract_strings.py
python qmclient_scripts/languages_qmclient/generate_all.py
python qmclient_scripts/languages_qmclient/validate.py
python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0
```

## 新增英文 key 后补译
1. `extract_strings.py` 更新 active keys  
2. `translate_with_local_http.py --languages <语言列表>` 生成 draft（补缺**不要**加 `--rewrite`）  
3. 审核 `translations_draft/` 后显式 `--write-back`  
4. 再跑 `generate_all.py` → `validate.py`

## 约束
- 草稿未审核不得 write-back  
- 数字门禁：`required = 数字(src) ⊆ 数字(tgt) ⊆ required ∪ 英文词 one…ten`  
- 跨模块同一 identity：first-wins + integrity 报错；合并后删除非首选副本  
