# QmClient i18n 脚本改进方案

> 调研日期：2026-06-24
> 基于 `translate_with_local_http.py`、`i18n_store.py`、`source_keys.py`、`local_http_client.py` 及 prompt assets 的代码与数据。
> 2026-06-26 术语修正：本笔记的术语扩表示例只代表当时从 QmClient 现有数据抽取出的候选，不是官方术语结论。DDNet/core 简中术语必须以官方 DDNet 简体中文翻译为基线；已确认 `Hook=钩索`、`Hook collision line=钩索辅助线`、`Grenade=榴弹枪`。

---

## 1. 添加 DeepSeek 默认 URL 和模型

### 现状

`build_parser()` 中 `--base-url` 和 `--model` 默认均为空字符串：

```python
parser.add_argument("--base-url", default="")
parser.add_argument("--model", default="")
```

`main()` 里只要非 `--write-back` 模式就强制校验：

```python
if not args.base_url or not args.model:
    raise SystemExit("--base-url and --model are required unless --write-back is used")
```

### 目标

默认使用 DeepSeek 官方端点与 `deepseek-v4-flash`，减少每次调用时的参数输入。

### 改动

在 `translate_with_local_http.py` 顶部新增常量：

```python
DEFAULT_BASE_URL = "https://api.deepseek.com"
DEFAULT_MODEL = "deepseek-v4-flash"
```

修改参数默认值：

```python
parser.add_argument("--base-url", default=DEFAULT_BASE_URL)
parser.add_argument("--model", default=DEFAULT_MODEL)
```

### 命令行变化

| 场景 | 改动前 | 改动后 |
|---|---|---|
| 日常回写 | `python translate_with_local_http.py --base-url https://api.deepseek.com --model deepseek-v4-flash --write-back --languages all` | `python translate_with_local_http.py --write-back --languages all` |
| 指定 API key | 同上 + `--api-key $KEY` | `python translate_with_local_http.py --api-key $KEY --write-back --languages all` |
| 换模型 | `--base-url ... --model deepseek-v4-pro` | `--model deepseek-v4-pro` |

### 兼容性说明

`local_http_client.py` 使用 OpenAI 兼容的 `/chat/completions` 接口，`https://api.deepseek.com` 与 `deepseek-v4-flash` 完全兼容，无需改 client 逻辑。

---

## 2. 性能评估与优化

### 2.1 当前性能特征

| 环节 | 实现 | 评估 |
|---|---|---|
| 源 key 提取 | `collect_incremental_source_key_records()` 基于 git diff，只扫变更文件并合并缓存 | **优秀**，避免全量扫描 |
| 全量扫描 | `collect_source_key_records()` 遍历 `src/` 下所有 `.c/.cpp/.h/.hpp` | 仅在首次或 extractor 逻辑变更时触发，可接受 |
| 缓存 | `extracted_records_cache.json` 文件缓存 | 合理，但缺少版本校验 |
| API 调用 | 每个语言按 module 分 batch，batch 内顺序/并发可调 | 合理，API 等待是主要耗时 |
| 并发 | `--parallel-languages`（默认 1）+ `--parallel-requests`（默认 1） | **严重不足**，默认基本串行 |
| I/O | 每个 batch 前后反复读写 TOML、加载 prompt assets | 有优化空间 |

### 2.2 主要性能瓶颈

1. **默认并发太低**
   - `--parallel-languages` 默认 `1`，12 个语言串行跑；
   - `--parallel-requests` 默认 `1`，单个语言内部的多个 batch 也串行；
   - 实际 wall-clock 时间 ≈ 语言数 × batch 数 × 单次 API 耗时。

2. **prompt assets 每次 batch 重复加载**
   - `render_prompt` 中 `prompt_assets or load_prompt_assets()`，虽然调用方传入了 `prompt_assets`，但 `neighboring_translations` 和 `simplified_reference` 每次都会重新从 store 中读取；
   - store 本身在内存中，这部分开销不大，但当 batch 数多时仍会重复查找。

3. **每次翻译都重新加载整个语言 store**
   - `i18n_store.load_language_store()` 会读取 `translations/i18n/*.toml` 全部 55K+ 行；
   - 该调用在 `main()` 中只执行一次，不是问题；但要避免在 `render_prompt` 中误触发。

4. **TOML 草稿频繁读写**
   - `write_draft_module` 每完成一个 module 就写一次文件；
   - 属于合理范围，但如果改为每语言只写一次可减少 I/O。

5. **API 等待是主导因素**
   - 以 `menus.toml` 约 9299 行估算，可能有 500~800 个 key；
   - batch size 1024 时，一个 module 可能只需 1 个 batch；
   - 但全部 9 个 module × 12 语言 = 108 个 batch；
   - 若默认串行，耗时 = 108 × API 耗时；若并发拉满，可降到约 1~2 个 API 耗时。

### 2.3 优化建议

#### A. 把默认并发拉满（收益最大）

```python
import os

parser.add_argument(
    "--parallel-languages",
    type=int,
    default=min(12, max(1, (os.cpu_count() or 4) - 2)),
)
parser.add_argument(
    "--parallel-requests",
    type=int,
    default=4,
)
```

- 12 语言并发不会给本地 CPU 带来压力，因为主要耗时在 I/O；
- 单个语言内部 4 并发足够，避免触发 DeepSeek 速率限制；
- 保留命令行覆盖能力。

#### B. 对 `neighboring_translations` 做缓存

当前 `neighboring_translations(language, module, store=store)` 每次按 language + module 重新 slice。可以在 `translate_language_to_drafts` 里预计算一次：

```python
neighbors = neighboring_translations(language, module, store=store)
```

然后传给 `render_prompt`。这样每个 module 只计算一次。

#### C. 批量写草稿（可选）

当前是每个 module 翻译完立刻写文件。可以改为每个 language 全部 module 翻译完再统一写，减少小文件 I/O。但收益不如并发大，且会延迟看到结果，优先级较低。

#### D. API 层面优化

- 考虑对 `deepseek-v4-flash` 使用 `temperature=0.2`（当前已是默认值）；
- 对重复出现的 key（跨 module 或跨语言）可尝试复用已有翻译，避免重复请求；
- 对失败 batch 实施指数退避，当前是固定 `retry_backoff_seconds * (attempt + 1)`，可改为指数退避。

### 2.4 性能预期

假设 API 单次响应 5 秒：

| 配置 | 大致 batch 数 | 耗时 |
|---|---|---|
| 默认串行 | 108 | ~9 分钟 |
| 12 语言并发 + 每语言 4 请求并发 | ~ ceiling(108 / (12×4)) = 3 | ~15 秒 |
| 实际（有速率限制、部分 module key 少） | - | 1~3 分钟 |

**结论：默认并发是当前最大性能短板，改完后性能提升 5~10 倍。**

---

## 3. 术语表扩充与质量检查增强

### 3.1 当前术语表问题

`prompt_assets/terminology.toml` 仅 11 个条目：

- Clan、Dummy、Server、Map、Tencent Cloud、Lyrics、HUD、cache、source、threshold、px、ms

明显缺失游戏核心术语：

- 强弱钩（Strong/Weak hook）
- Tee / Tee 0.7
- Hammer、Shotgun、Grenade、Laser
- Freeze、Unfreeze
- Dummy、Clone
- DDRace、DDNet、DDmaX
- Rank、Points、Time
- Skin、Emoticon、Kaomoji
- Jitter、Ping、Latency
- Demo、Recorder
- Spectator、Voting
- 各种难度等级（Novice、Moderate、Brutal、Insane 等已在 `may_keep_source_text` 中，但术语表没有）

### 3.2 术语表扩充方案

建议按游戏模块分类，每个术语给出全部 12 种语言翻译。示例：

```toml
[[term]]
source = "Strong hook"
simplified_chinese = "强钩"
traditional_chinese = "強鉤"
korean = "강한 후크"
japanese = "強いフック"
russian = "Сильный хук"
german = "Starker Haken"
spanish = "Gancho fuerte"
french = "Grapin fort"
brazilian_portuguese = "Gancho forte"
portuguese = "Gancho forte"
turkish = "Güçlü kanca"
polish = "Mocny hak"

[[term]]
source = "Weak hook"
simplified_chinese = "弱钩"
traditional_chinese = "弱鉤"
korean = "약한 후크"
japanese = "弱いフック"
russian = "Слабый хук"
german = "Schwacher Haken"
spanish = "Gancho débil"
french = "Grapin faible"
brazilian_portuguese = "Gancho fraco"
portuguese = "Gancho fraco"
turkish = "Zayıf kanca"
polish = "Słaby hak"
```

建议先批量提取高频英文词，再人工校验翻译。可用脚本：

```python
from collections import Counter
from qmclient_scripts.languages_qmclient.source_keys import collect_source_key_records
records = collect_source_key_records()
words = Counter()
for r in records:
    for w in r.key.lower().split():
        words[w.strip(".,()[]%:")] += 1
for w, c in words.most_common(200):
    print(c, w)
```

### 3.3 Few-shots 扩充

当前 few_shots.toml 只有 3 个例子，且只覆盖简中/繁中/韩/日。建议：

1. 增加覆盖全部 12 种语言的示例；
2. 加入含 placeholder 的示例；
3. 加入上下文敏感示例（如 `Screen` 在 Assets editor 中译为“滤色”）；
4. 加入“保留产品名”示例。

### 3.4 质量检查增强

当前 `language_quality_failure` 已检查：

- 空翻译
- placeholder 一致性（`%s`、`%d` 等）
- 数字一致性
- prompt echo / 原样返回
- 繁中混入简体字
- 韩/日/俄输出是否包含目标文字

建议新增：

#### a. 占位符顺序检查（更严格）

当前只检查 placeholder 集合是否相同。应检查顺序也一致，例如：

```python
def extract_placeholders_ordered(text: str) -> list[str]:
    return re.findall(r"%%|%(?:\d+\$)?[+#0\- ]?(?:\d+|\*)?(?:\.\d+|\.\*)?[hljztL]*[diuoxXfFeEgGaAcspn]", text)

# 在 language_quality_failure 中
if extract_placeholders_ordered(source) != extract_placeholders_ordered(translation):
    return "placeholder order mismatch"
```

#### b. 术语一致性检查

在 `write_back` 或最终校验阶段，扫描所有 module，确保同一个英文 key 在所有地方翻译成同一个词。可简单实现为：

```python
def terminology_consistency_errors(store):
    errors = []
    seen: dict[tuple[str, str], dict[str, str]] = {}
    for module, entries in store.items():
        for (key, context), translations in entries.items():
            for lang, trans in translations.items():
                if (key, lang) not in seen:
                    seen[(key, lang)] = {"translation": trans, "modules": {module}}
                elif seen[(key, lang)]["translation"] != trans:
                    errors.append(f"terminology inconsistency: [{lang}] {key!r}: {module}={trans!r} vs {seen[(key, lang)]['modules']}={seen[(key, lang)]['translation']!r}")
                else:
                    seen[(key, lang)]["modules"].add(module)
    return errors
```

#### c. 长度/截断风险检查

游戏 UI 对按钮长度敏感。可基于经验阈值：

```python
MAX_LENGTH_RATIO = {
    "simplified_chinese": 1.5,
    "traditional_chinese": 1.5,
    "japanese": 1.5,
    "korean": 1.5,
    "russian": 2.0,
    "german": 2.0,
    # ...
}

def length_risk(source, translation, language):
    ratio = len(translation) / max(len(source), 1)
    if ratio > MAX_LENGTH_RATIO.get(language, 2.0):
        return f"translation too long ({ratio:.1f}x source)"
    return ""
```

#### d. 简中风格统一检查

在 `chinese_text_style.normalize_simplified_chinese_text` 基础上增加：

- 统一“你/您”：游戏 UI 建议统一用“你”；
- 统一“的/地/得”；
- 检查中英文之间空格；
- 检查是否混用全角半角标点。

#### e. 黑名单/敏感词检查

对每个语言的翻译增加脏话、敏感词过滤，避免 AI 生成不当内容进入游戏。

#### f. 术语表命中检查

新增一个 lint：如果源文本包含术语表中的 source，但翻译没有使用对应语言的术语，则报错。注意：这里原示例 `Strong hook` 必须译为 `强钩` 只是候选示例；官方简中只确认了 `Hook=钩索`，没有确认 `Strong hook` / `Weak hook` 的独立译法。上线术语 lint 前必须先用官方简中证据或项目内人工决策确认术语表。

```python
from qmclient_scripts.languages_qmclient.prompt_utils import load_terminology  # 待实现

def terminology_violation(language, source, translation, terminology):
    for term_source, term_translations in terminology.items():
        if term_source.lower() in source.lower():
            expected = term_translations.get(language)
            if expected and expected.lower() not in translation.lower():
                return f"expected terminology {expected!r} for {term_source!r}"
    return ""
```

### 3.5 新增一个 `validate_translations.py` 入口

建议把质量检查独立成一个命令：

```bash
python -m qmclient_scripts.languages_qmclient.validate
# 或
python validate_translations.py
```

功能：

- 加载 `translations/i18n/*.toml`；
- 运行 `translation_quality_errors`；
- 运行新增的术语一致性、长度风险、placeholder 顺序等检查；
- 输出结构化报告；
- 返回非 0 退出码供 CI 使用。

---

## 4. 完整改进实施计划

### Phase 1：默认配置优化（1~2 小时）

- [ ] 添加 `DEFAULT_BASE_URL` 和 `DEFAULT_MODEL` 常量；
- [ ] 修改 `build_parser()` 默认值；
- [ ] 更新 README 或 help 文本；
- [ ] 本地跑 `--dry-run` 验证默认参数生效。

### Phase 2：并发性能优化（2~3 小时）

- [ ] 把 `--parallel-languages` 默认值改为 `min(12, os.cpu_count() - 2)`；
- [ ] 把 `--parallel-requests` 默认值改为 `4`；
- [ ] 缓存 `neighboring_translations` 结果；
- [ ] 对失败 batch 使用指数退避；
- [ ] 跑一组完整翻译计时对比。

### Phase 3：术语表与质量检查（4~6 小时）

- [ ] 用脚本提取高频词，生成候选术语表；
- [ ] 人工补全 12 语言翻译；
- [ ] 扩展 few_shots.toml；
- [ ] 在 `language_quality_failure` 中新增 placeholder 顺序检查；
- [ ] 新增术语一致性、长度风险、术语表命中检查；
- [ ] 新增 `validate_translations.py` 入口；
- [ ] 跑 `--write-back` 验证无回归。

### Phase 4：CI 与自动化（2~3 小时）

- [ ] 在 GitHub Actions 中新增 i18n check job；
- [ ] PR 变更触及 `src/` 时自动跑 `validate_translations.py`；
- [ ] 可选： nightly 自动跑增量翻译并生成报告；
- [ ] 更新 `CLAUDE.md` 记录 i18n 工作流规范。

### Phase 5：可选实验（后续）

- [ ] 对比“单语言请求” vs “一次多语言请求”在小 module 上的质量与成本；
- [ ] 评估是否引入向量缓存/术语 RAG。

---

## 5. 关键改动代码示例

### 5.1 默认 URL/模型 + 默认并发

```python
# translate_with_local_http.py
import os

DEFAULT_BASE_URL = "https://api.deepseek.com"
DEFAULT_MODEL = "deepseek-v4-flash"
DEFAULT_PARALLEL_LANGUAGES = min(12, max(1, (os.cpu_count() or 4) - 2))
DEFAULT_PARALLEL_REQUESTS = 4

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("--languages", default="simplified_chinese")
    parser.add_argument("--base-url", default=DEFAULT_BASE_URL)
    parser.add_argument("--model", default=DEFAULT_MODEL)
    parser.add_argument("--api-key", default="")
    parser.add_argument("--module", default="")
    parser.add_argument("--modules", default="")
    parser.add_argument("--limit", type=int)
    parser.add_argument("--batch-size", type=int, default=1024)
    parser.add_argument("--parallel-requests", type=int, default=DEFAULT_PARALLEL_REQUESTS)
    parser.add_argument("--parallel-languages", type=int, default=DEFAULT_PARALLEL_LANGUAGES)
    ...
```

### 5.2 neighboring_translations 缓存

```python
def translate_language_to_drafts(...):
    ...
    neighbors_by_module = {
        module: neighboring_translations(language, module, store=store)
        for module in grouped
    }
    ...
```

并在 `render_prompt` 中接受可选的 `neighbors` 参数。

### 5.3 术语表命中检查（在 `language_quality_failure` 中）

```python
# 在 translate_with_local_http.py 顶部加载术语表
TERMINOLOGY = load_terminology_by_language(PROMPT_ASSETS_DIR / "terminology.toml")

def language_quality_failure(language, source, translation):
    ...
    expected_terms = TERMINOLOGY.get(language, {})
    for term_source, expected in expected_terms.items():
        if term_source.lower() in source.lower() and expected.lower() not in translation.lower():
            return f"terminology violation: expected {expected!r} for {term_source!r}"
    ...
```

---

## 6. 风险与注意事项

1. **DeepSeek 速率限制**：默认 12 语言 × 4 请求 = 48 并发可能触发限流。建议：
   - 在 `local_http_client.py` 中对 429 做指数退避；
   - 保留 `--parallel-requests` 命令行覆盖。

2. **默认并发对 `--dry-run` 无影响**：当前代码在 dry-run 时跳过多语言并发，改为顺序打印 prompt，这是合理的。

3. **术语表需要人工校验**：自动提取的术语需要母语者或游戏老玩家确认，不能全靠脚本。

4. **质量检查变严可能导致旧翻译被标错**：新增检查上线后，先跑一遍全量验证，把历史遗留问题清理掉，再接入 CI。

---

## 7. 总结

当前 i18n 脚本系统的设计（增量提取、草稿/源两层、按语言分目录、质量过滤）是合理的。主要改进空间在：

1. **默认体验**：补齐 DeepSeek URL/模型，减少参数；
2. **性能**：把默认并发从 1 拉到合理值，收益最大；
3. **质量**：扩充术语表、few-shots，增强 placeholder/术语/长度/风格检查；
4. **自动化**：新增 `validate_translations.py` 并接入 CI。

按 Phase 1 → Phase 2 → Phase 3 顺序实施，每步都可以独立验证。
