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

`extract_strings.py` 默认使用 Git diff 与 `extracted_records_cache.json` 增量更新，但输出的 `extracted_strings.txt` 仍是完整 active key 集。需要重建缓存时使用：

```bash
python qmclient_scripts/languages_qmclient/extract_strings.py --full
```

`validate.py` 默认重扫源码做严格核对；本地快速校验可显式使用 `--incremental`。

语言脚本单测入口：

```bash
python -m unittest discover qmclient_scripts/languages_qmclient/tests -v
```

## 新增英文 key 后的多语言维护流程

新增英文 source key 后，先提取 active key，再让模型只为缺失语言生成 draft：

```bash
python qmclient_scripts/languages_qmclient/extract_strings.py
python qmclient_scripts/languages_qmclient/translate_with_local_http.py --languages simplified_chinese,traditional_chinese,japanese,korean,russian,german,spanish,french,brazilian_portuguese,portuguese,turkish,polish --base-url https://api.deepseek.com --model deepseek-chat --batch-size 48 --parallel-requests 8 --resume
```

生成 draft 用于补缺时不要传 `--rewrite`。`--rewrite` 会把维护源里已有的译文也纳入任务，等同于接近全量重翻；只在明确需要重翻已有译文，或回填已审核 draft 并允许覆盖维护源时使用。补缺流程应依赖默认行为：跳过已有且质量检查通过的译文，只生成缺失或质量不合格的条目。

审核 `translations_draft/<language>/*.toml` 后，显式回填维护源：

```bash
python qmclient_scripts/languages_qmclient/translate_with_local_http.py --languages simplified_chinese,traditional_chinese,japanese,korean,russian,german,spanish,french,brazilian_portuguese,portuguese,turkish,polish --write-back --resume
```

`--write-back` 只会把审核通过的 draft 条目 patch 到对应 `translations/i18n/*.toml`，不应重写整份模块 TOML，也不应重排未触碰的 `[[message]]` block。修改写回逻辑后，先跑：

```bash
python -m unittest discover qmclient_scripts/languages_qmclient/tests
```

回填后必须生成运行时语言文件并验证：

```bash
python qmclient_scripts/languages_qmclient/generate_all.py
python qmclient_scripts/languages_qmclient/validate.py
python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0
```

不要手工编辑 `data/languages/*.txt` 来维护翻译；需要改译文时改 `translations/i18n/*.toml`，再重新生成。

## 主要脚本

- `extract_strings.py`：默认按 Git diff 增量提取 active 英文 source keys，写出完整 `extracted_strings.txt`、`extracted_records_cache.json` 和 `extracted_audit_report.json`；`--full` 会重扫源码并重建缓存。
- `generate_all.py`：根据 active keys 和 `translations/i18n/*.toml` 生成 `GENERATED_LANGUAGES` 中登记的运行时语言文件。
- `validate.py`：默认重扫源码校验提取结果新鲜度、全部生成语言文件覆盖、模块化 TOML 可读性、legacy overlay 删除状态和 blocking audit violations；`--incremental` 会使用增量缓存做本地快速校验。
- `review_duplicate_entries.py`：只读报告重复、相似、空译文和疑似未使用项；unused 默认读取 `extracted_strings.txt`，避免重复全量扫描。
- `audit_translation_drift.py`：把当前 TOML 译文和 Git 历史里的简中译法做只读对比。
- `translate_with_local_http.py`：生成本地模型翻译草稿；审核通过后，可显式 `--write-back` 按条目 patch 回填 `translations/i18n/*.toml`。

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

回填后，成功写入维护源的 draft 条目应从 `translations_draft/<language>/<module>.toml` 中移除；如果该模块草稿没有剩余有效条目，应删除对应 draft 文件。失败或未写回的有效条目必须保留在 draft 中，方便后续重试或人工处理。

回填后必须重新运行 `generate_all.py`、`validate.py` 和 `review_duplicate_entries.py`。如果回填结果出现未触碰条目的排序或空行变化，应先修写回脚本，不要把整模块格式化作为正常回填结果接受。

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
python qmclient_scripts/languages_qmclient/translate_with_local_http.py --languages traditional_chinese,korean,japanese --base-url https://api.deepseek.com --model deepseek-chat --batch-size 48 --parallel-requests 8 --resume
```

翻译草稿任务默认不需要开启 thinking / reasoning 模式。DeepSeek 官方 API 优先使用 `deepseek-chat`，不要使用 `deepseek-reasoner`，也不要为普通翻译传 `--reasoning-effort`。如果某个兼容服务默认开启 thinking，必须用该服务文档指定的字段显式关闭；不同厂商字段不统一，不能把某个厂商的 `thinking` 或 `reasoning_effort` 当成通用值。如果确实启用思考，需要同步提高 `--max-tokens`，因为部分模型会先消耗 token 输出 `reasoning_content`，最终译文在 `content` 中才可用。

性能注意：

- 多语言补缺时，确认命令没有 `--rewrite`；否则会把已有翻译也重翻，任务量会从几百条放大到数千条。
- `--parallel-requests` 控制单个语言内部的请求并发；如果脚本支持 `--parallel-languages`，再用它控制多语言并发。不要误以为逗号分隔的 `--languages` 天然并发。
- DeepSeek 官方 API 可以使用较高并发。常规补缺建议从 `--batch-size 32` 到 `48`、`--parallel-requests 8` 起步；如果错误率低，再逐步提高。

超时注意：

- `translate_with_local_http.py --timeout` 只控制单个 HTTP 请求等待时间，不控制整条命令的总运行时间。
- Codex、CI、外层 shell 或任务调度器可能有自己的总命令超时；总命令超时触发时，Python 进程会被直接终止，尚未写出的 draft 会丢失。
- 大批量远端翻译不要一次性跑 12 种语言全量任务。推荐按语言或模块分段运行，例如先跑 `--languages korean,russian,german`，或加 `--modules qmclient,menus,misc` 分批。
- 长任务中断后，先运行 `--write-back --resume` 回填已经生成并审核过的 draft，再继续用不带 `--rewrite` 的生成命令补剩余缺口。
- 如果看到任务量异常放大，先用 `generate_all.py` / `validate.py --incremental` 和 TOML 缺失数量确认真实缺口，不要直接提高超时时间掩盖全量重翻问题。

## 审核边界

draft 文件不是翻译真相源，只是候选结果。若 draft 中存在繁中条目的简体混入、占位符损坏、提示词泄漏或术语漂移，必须先修复，再使用 `--write-back`。
