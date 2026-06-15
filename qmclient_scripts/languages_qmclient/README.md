# QmClient i18n 脚本说明

这个目录维护 QmClient 的 i18n 生成、校验、审计和本地模型辅助翻译流程。运行时语言文件由源码 key 和按模块拆分的 TOML 维护源生成；不要把生成产物当作长期维护源手工编辑。

## 真相源

- `translations/i18n/*.toml` 是翻译维护源。
- TOML 按代码模块拆分，不按语言拆分。
- 每个 `[[message]]` 使用英文 source key 作为 `key`，可选 `context`，并在 `[message.translations]` 下维护一个或多个语言译文。
- `data/languages/*.txt` 是生成产物；当前由 `generate_all.py` 统一生成 `GENERATED_LANGUAGES` 中登记的运行时语言文件。
- `translations_draft/<language>/*.toml` 是模型生成的待审核草稿，不参与运行时生成链。

## 常规 i18n 工作流

修改 `Localize`、`Localizable`、`Register` help 文本，或修改翻译 TOML 后，按顺序运行：

```bash
python qmclient_scripts/languages_qmclient/extract_strings.py
python qmclient_scripts/languages_qmclient/generate_all.py
python qmclient_scripts/languages_qmclient/validate.py
python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0
```

语言脚本单测入口：

```bash
python -m unittest discover qmclient_scripts/languages_qmclient/tests -v
```

## 新增英文 key 后的多语言维护流程

新增英文 source key 后，先提取 active key，再让模型只为缺失语言生成 draft：

```bash
python qmclient_scripts/languages_qmclient/extract_strings.py
python qmclient_scripts/languages_qmclient/translate_with_local_http.py --languages simplified_chinese,traditional_chinese,japanese,korean,russian,german,spanish,french,brazilian_portuguese,portuguese,turkish,polish --base-url https://api.deepseek.com --model deepseek-v4-flash --chat-extra-json '{"thinking":{"type":"disabled"}}' --batch-size 1024 --parallel-requests 10 --resume
```

审核 `translations_draft/<language>/*.toml` 后，显式回填维护源：

```bash
python qmclient_scripts/languages_qmclient/translate_with_local_http.py --languages simplified_chinese,traditional_chinese,japanese,korean,russian,german,spanish,french,brazilian_portuguese,portuguese,turkish,polish --write-back --resume
```

回填后必须生成运行时语言文件并验证：

```bash
python qmclient_scripts/languages_qmclient/generate_all.py
python qmclient_scripts/languages_qmclient/validate.py
python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0
```

不要手工编辑 `data/languages/*.txt` 来维护翻译；需要改译文时改 `translations/i18n/*.toml`，再重新生成。

## 主要脚本

- `extract_strings.py`：从 `src/` 提取 active 英文 source keys，写出 `extracted_strings.txt` 和 `extracted_audit_report.json`。
- `generate_all.py`：根据 active keys 和 `translations/i18n/*.toml` 生成 `GENERATED_LANGUAGES` 中登记的运行时语言文件。
- `validate.py`：校验提取结果新鲜度、全部生成语言文件覆盖、模块化 TOML 可读性、legacy overlay 删除状态和 blocking audit violations。
- `review_duplicate_entries.py`：只读报告重复、相似、空译文和疑似未使用项，用于人工清理。
- `audit_translation_drift.py`：把当前 TOML 译文和 Git 历史里的简中译法做只读对比。
- `translate_with_local_http.py`：生成本地模型翻译草稿；审核通过后，可显式 `--write-back` 回填 `translations/i18n/*.toml`。

## 翻译草稿工作流

所有模型生成的语言都必须先进入 draft：

示例：

```bash
python qmclient_scripts/languages_qmclient/translate_with_local_http.py --languages traditional_chinese,korean,japanese --base-url http://127.0.0.1:1337/v1 --model local-model --batch-size 1024 --parallel-requests 1 --resume
```

回填前必须审核 `translations_draft/<language>/*.toml`，至少检查：

- 没有提示词泄漏或解释性文本
- 没有错语言、简繁混杂或英文原样误收
- `%s`、`%d`、`%.2f`、`\n` 等占位符保持一致且顺序不变
- 项目术语符合既有风格，例如简中语境下 `Clan` / `Team` 使用“战队”

审核通过后，才允许显式回填：

```bash
python qmclient_scripts/languages_qmclient/translate_with_local_http.py --languages korean --module server_browser --write-back --resume
```

回填后必须重新运行 `generate_all.py`、`validate.py` 和 `review_duplicate_entries.py`。

## 翻译风格维护

翻译风格不要写死在脚本里，优先维护在 `prompt_assets/`：

- `terminology.toml`：术语表。适合维护稳定译名，例如 `Tencent Cloud -> 腾讯云 / 騰訊雲`、`Clan -> 战队 / 戰隊`。
- `few_shots.toml`：少量高价值示例。适合维护上下文敏感或容易机翻跑偏的完整句子。
- `system_prompt.md`：通用规则。适合维护占位符、URL、短专名、reasoning 输出等全局约束。

新增风格规则时，优先顺序是：先加术语；如果术语不能表达上下文，再加 few-shot；只有影响所有翻译请求的规则才改 system prompt。修改后重新生成相关语言 draft，并在回填前检查是否符合既有项目口径。

## HTTP 后端

脚本只直接调用 OpenAI-compatible `/chat/completions` HTTP 接口。Jan、LM Studio、llama.cpp server 或厂商 API 都需要先在外部启动为兼容 HTTP 服务，然后通过 `--base-url` 和 `--model` 接入。

本地兼容服务：

```bash
python qmclient_scripts/languages_qmclient/translate_with_local_http.py --languages traditional_chinese --base-url http://127.0.0.1:1337/v1 --model local-model --batch-size 1024 --parallel-requests 1 --resume
```

DeepSeek 官方 API：

```bash
python qmclient_scripts/languages_qmclient/translate_with_local_http.py --languages traditional_chinese --base-url https://api.deepseek.com --model deepseek-v4-flash --chat-extra-json '{"thinking":{"type":"disabled"}}' --batch-size 1024 --parallel-requests 2 --resume
```

翻译草稿任务默认不需要开启 thinking / reasoning 模式。OpenAI 兼容服务如果支持 `reasoning_effort = none`，可以用 `--reasoning-effort none` 关闭推理以提高吞吐；但不同厂商字段不统一，不能把 `none` 当成通用值。DeepSeek 官方 V4 接口按文档使用 `thinking.type` 控制思考模式，关闭时通过 `--chat-extra-json '{"thinking":{"type":"disabled"}}'` 透传厂商字段；如果启用思考，需要同步提高 `--max-tokens`，因为部分模型会先消耗 token 输出 `reasoning_content`，最终译文在 `content` 中才可用。

## 审核边界

draft 文件不是翻译真相源，只是候选结果。若 draft 中存在繁中条目的简体混入、占位符损坏、提示词泄漏或术语漂移，必须先修复，再使用 `--write-back`。
