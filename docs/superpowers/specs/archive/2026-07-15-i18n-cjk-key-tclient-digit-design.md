> **已归档，禁止作为实现依据。** 本文保留为历史记录；当前实现请以活动 spec/plan/skill 为准。

# i18n 残留债设计：CJK source key / tclient 提取 / digit 误检

**日期:** 2026-07-15  
**状态:** archived（用户：设计方案，然后执行）


**配置约定（已落地）：** 所有 `MACRO_CONFIG_*` 的 `Desc` 必须是英文 source key；中文只存在于 TOML 译文。三头文件 `config_variables.h` / `_qmclient.h` / `_tclient.h` 均已英文化并接入提取链。

## 问题

1. **479 条 CJK config help 作 source key**  
   全部来自 `src/engine/shared/config_variables_qmclient.h` 的 `MACRO_CONFIG_*` 最后参数 `Desc`，经 `extract_known_indirect_records` 间接提取。运行时 `BuildLocalizedConfigHelpText` → `Localize(m_pHelpLocalizeKey)`：英文 UI 无译时回退到 source，显示中文 help。

2. **`config_variables_tclient.h` 未进提取链**  
   文件在 `src/` 下会被扫描，但 MACRO 提取硬编码 `endswith config_variables_qmclient.h`。约 **195** 条非空 Desc（几乎全 CJK）。运行时已走同一 Localize 路径，维护库却缺 key。`module_name_for_source` 也未映射该头文件 → 默认 `misc`。

3. **digit 误检导致 draft 失败**  
   仅 `translate_with_local_http.extract_digits` + `language_quality_failure`：`word_digits={"five":"5"}`，多集相等比较。  
   例：`Go back one tick` → `[]`，日文 `1ティック戻る` → `['1']` → mismatch。  
   朴素 `one→1` 会反杀中文「上一个 tick」（`[]`）。

**明确不在本轮：** `config_variables.h`（DDNet 主配置）同样大量 CJK Desc、同样未 MACRO 提取——同类债，单独后续切片。

## 目标

| 项 | 成功标准 |
|----|----------|
| Digit | `one` 类 source + 译文用 `1`/`上一个` 均不误杀；真实数字/`%d` 丢漏仍失败；单测覆盖 |
| tclient 提取 | MACRO Desc 进入 active keys；模块 `tclient`；有单测 |
| CJK→EN | qmclient（+ 同步迁移 tclient）Desc 改为英文 source key；TOML identity 重映射；SC 保留中文；`has_cjk` active keys 对这两文件 → 0；validate 绿 |

## 方案对比

### Digit

| 方案 | 优点 | 缺点 |
|------|------|------|
| A. 扩展 word_digits + 仍 `==` | 改动小 | 反杀无数字译文（中/德/法） |
| **B. 子集门禁（推荐）** | 兼顾 JP 数字形与 CN 词形 | 略弱化「five 必须出现」 |
| C. 双语数字词表 | 覆盖广 | 维护成本高 |

**选用 B：**

- `required` = source 中的阿拉伯数字 token 多集  
- `allowed` = `required ∪ english_word_numbers(source)`（至少 one…ten + five 已有）  
- 通过条件：`required ⊆ digits(translation) ⊆ allowed`（多集）

### tclient 提取

| 方案 | 优点 | 缺点 |
|------|------|------|
| A. 再抄一份 `endswith tclient.h` | 最小 diff | 继续硬编码 |
| **B. 统一 `config_variables_{qmclient,tclient}.h` 列表 + module map（推荐）** | 清晰、可测 | 多几行 |
| C. 所有 `config_variables*.h` | 顺带主配置 | 拉出未迁移 CJK，validate 爆红 |

**选用 B**（主配置仍排除）。`module_name_for_source`：`…tclient.h` → `tclient`。

### CJK → 英文 source key

| 方案 | 优点 | 缺点 |
|------|------|------|
| A. 只加英文译文、保留中文 key | 不改 C++ | 违反「source=英文」；英文 UI 仍显示中文 key |
| **B. 头文件 Desc 改英文 + TOML 重映射（推荐）** | 根治 | 需 EN 生成 + 原子迁移 |
| C. 双字段 MACRO | 架构清晰 | 改引擎契约，过大 |

**选用 B：**

1. 从 MACRO 解析 `(script_name, old_desc)`  
2. 生成 `old_desc → english_key`（DeepSeek 中译英；已有英混 key 可保留英文化润色）  
3. 改写 `config_variables_qmclient.h` / `config_variables_tclient.h` 最后字符串参数  
4. TOML：`key` 从 old→new；`simplified_chinese` 若空则填 old 中文；其它语种字段随 identity 迁移  
5. 更新断言中文 key 的单测  
6. `extract → generate → validate`

**顺序约束：** tclient **提取与 CJK 迁移同批落地**，避免「先提取 195 缺译 → validate 红」的中间态。Digit 与迁移无关，可并行。

## 组件

```
translate_with_local_http.py   # digit 子集门禁
source_keys.py                 # MACRO 提取列表
i18n_store.py                  # module map tclient.h
migrate_cjk_config_help.py     # 新建：映射生成/应用/TOML remap（可放 languages_qmclient/）
config_variables_qmclient.h    # Desc → EN
config_variables_tclient.h     # Desc → EN
translations/i18n/*.toml       # identity remap
tests/...
```

## 风险

- EN 生成质量：产品名/配置名保留；人工抽检 20 条  
- TOML 重映射漏项 → missing 或 orphan：以 extract unique set 为准 diff  
- 缓存 `extracted_records_cache.json` 需全量 extract 刷新  
- 主配置 `config_variables.h` 仍 CJK：文档记后续债，本轮不碰

## 验证

```bash
py -3 -m unittest discover qmclient_scripts/languages_qmclient/tests -q
py -3 qmclient_scripts/languages_qmclient/extract_strings.py
py -3 qmclient_scripts/languages_qmclient/generate_all.py
py -3 qmclient_scripts/languages_qmclient/validate.py --incremental
# 断言：config_variables_qmclient/tclient 的 active CJK count == 0
```
