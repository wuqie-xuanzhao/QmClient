> **已归档，禁止作为实现依据。** 本文保留为历史记录；当前实现请以活动 spec/plan/skill 为准。

# i18n CJK key / tclient extract / digit gate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix digit false-positives on English number-words, extract `config_variables_tclient.h` MACRO helps into the i18n chain, and migrate CJK MACRO `Desc` strings in qmclient+tclient headers to English source keys with TOML remapping.

**Architecture:** (1) Replace multiset digit equality with required-numeric ⊆ translation ⊆ allowed(word-numbers∪required). (2) Generalize MACRO extract + module map for qmclient and tclient headers only. (3) Scripted CJK→EN migration of Desc + TOML identity rename in one atomic pass so validate never sees a half-migrated tclient extract.

**Tech Stack:** Python 3 i18n scripts under `qmclient_scripts/languages_qmclient/`, unittest, optional DeepSeek via existing `translate_with_local_http` / `.env` `DEEPSEEK_API_KEY`, OpenCC for TC if needed.

## Global Constraints

- TOML is truth source; never hand-edit `data/languages/*.txt`
- Source keys for Localize must be English; SC lives in `simplified_chinese` field
- Do **not** extract/migrate `config_variables.h` (DDNet main) in this plan
- Wire tclient extract only together with its CJK→EN migration (or immediately after mapping applied in same session before validate)
- TDD for script changes; no commit unless user asks
- Skip formatters/project-wide C++ builds; Python unittest + i18n validate only

---

### Task 1: Digit subset quality gate

**Files:**
- Modify: `qmclient_scripts/languages_qmclient/translate_with_local_http.py` (`extract_digits`, `language_quality_failure` ~L703-906)
- Modify: `qmclient_scripts/languages_qmclient/tests/test_translate_with_local_http.py`

**Interfaces:**
- Consumes: existing `language_quality_failure(language, source, translation)`
- Produces:
  - `ENGLISH_WORD_DIGITS: dict[str, str]` covering at least one…ten (+ keep five)
  - `numeric_digits(text: str) -> list[str]`
  - `word_number_digits(text: str) -> list[str]`
  - `extract_digits(text: str) -> list[str]` may remain as numeric∪word for debug, but **quality compare must use subset rules**
  - `digits_compatible(source: str, translation: str) -> bool` or inline in `language_quality_failure`

- [ ] **Step 1: Write failing tests** in `test_translate_with_local_http.py`

```python
def test_digit_gate_allows_one_as_arabic_or_omitted(self):
    self.assertIsNone(
        language_quality_failure("japanese", "Go back one tick", "1ティック戻る")
    )
    self.assertIsNone(
        language_quality_failure("simplified_chinese", "Go back one tick", "上一个 tick")
    )

def test_digit_gate_still_requires_source_numerics(self):
    self.assertIsNotNone(
        language_quality_failure("japanese", "wait 60 seconds", "六十秒待つ")
    )
    self.assertIsNotNone(
        language_quality_failure("german", "plain text", "Version 2")
    )

def test_digit_gate_allows_five_word_as_optional_digit(self):
    source = "Shows five points of the ladder (1 by default)"
    self.assertIsNone(
        language_quality_failure("simplified_chinese", source, "显示天梯的 5 个点（默认 1）")
    )
```

- [ ] **Step 2: Run tests — expect FAIL** on digit mismatch for JP `1…`

Run: `py -3 -m unittest qmclient_scripts.languages_qmclient.tests.TestTranslateWithLocalHttp.test_digit_gate_allows_one_as_arabic_or_omitted -q`

- [ ] **Step 3: Implement subset compare**

In `translate_with_local_http.py`:

```python
ENGLISH_WORD_DIGITS = {
    "zero": "0", "one": "1", "two": "2", "three": "3", "four": "4",
    "five": "5", "six": "6", "seven": "7", "eight": "8", "nine": "9", "ten": "10",
}

def numeric_digits(text: str) -> list[str]:
    return sorted(re.findall(r"\d+(?:\.\d+)?", text), key=digit_sort_key)

def word_number_digits(text: str) -> list[str]:
    found = []
    for word in re.findall(r"\b[a-zA-Z]+\b", text):
        digit = ENGLISH_WORD_DIGITS.get(word.lower())
        if digit is not None:
            found.append(digit)
    return sorted(found, key=digit_sort_key)

def extract_digits(text: str) -> list[str]:
    # keep for callers/debug: union
    return sorted(numeric_digits(text) + word_number_digits(text), key=digit_sort_key)

def _multiset_sub(a: list[str], b: list[str]) -> bool:
    from collections import Counter
    ca, cb = Counter(a), Counter(b)
    return all(cb[k] >= v for k, v in ca.items())

def digits_compatible(source: str, translation: str) -> bool:
    from collections import Counter
    required = numeric_digits(source)
    allowed = required + word_number_digits(source)
    got = numeric_digits(translation)
    return _multiset_sub(required, got) and _multiset_sub(got, allowed)
```

Replace `language_quality_failure` digit check:

```python
if not digits_compatible(source, translation):
    return (
        f"digit mismatch: expected required={numeric_digits(source)!r} "
        f"allowed_extra={word_number_digits(source)!r}, got {numeric_digits(translation)!r}"
    )
```

- [ ] **Step 4: Run digit-related tests + full translate unit tests — expect PASS**

Run: `py -3 -m unittest qmclient_scripts.languages_qmclient.tests.test_translate_with_local_http -q`

---

### Task 2: Extract tclient MACRO + module map (code only; apply after Task 3 mapping ready)

**Files:**
- Modify: `qmclient_scripts/languages_qmclient/source_keys.py` (~L580)
- Modify: `qmclient_scripts/languages_qmclient/i18n_store.py` (`module_name_for_source` ~L71-97)
- Modify: `qmclient_scripts/languages_qmclient/tests/test_source_keys.py`
- Modify: any tests asserting only qmclient path

**Interfaces:**
- Consumes: `CONFIG_MACRO_CALL_RE`, `_split_top_level_args`, `_decode_string_argument`
- Produces: MACRO Descs from both headers as `indirect` records; `module_name_for_source(tclient.h)=="tclient"`

- [ ] **Step 1: Failing tests**

```python
def test_extracts_tclient_config_descriptions(self):
    path = source_keys.PROJECT_ROOT / "src/engine/shared/config_variables_tclient.h"
    content = source_keys.strip_cpp_comments(source_keys.read_source_text(path))
    records = source_keys.extract_known_indirect_records(path, content)
    keys = {r.key for r in records}
    # After Task 3 migration keys are English; during greenfield test use a stable substring
    # Prefer: assert any record from this path and count >= 150
    self.assertGreaterEqual(len(keys), 150)
    self.assertTrue(all(r.source == path for r in records if r.source))

def test_module_name_for_tclient_config_header(self):
    path = source_keys.PROJECT_ROOT / "src/engine/shared/config_variables_tclient.h"
    self.assertEqual(i18n_store.module_name_for_source(path), "tclient")
```

Note: If running tests **before** English migration, assert a known CJK sample currently in file (e.g. `显示冻结 Tee HUD`). After migration, switch assert to English sample from the mapping file. Prefer path-based count asserts to avoid brittle strings.

- [ ] **Step 2: Implement extract + module map**

```python
# source_keys.py
CONFIG_MACRO_HELP_HEADERS = (
    "src/engine/shared/config_variables_qmclient.h",
    "src/engine/shared/config_variables_tclient.h",
)

# in extract_known_indirect_records:
if any(normalized.endswith(h) for h in CONFIG_MACRO_HELP_HEADERS):
    ... existing MACRO loop ...
```

```python
# i18n_store.module_name_for_source
if normalized.endswith("src/engine/shared/config_variables_qmclient.h"):
    return "qmclient"
if normalized.endswith("src/engine/shared/config_variables_tclient.h"):
    return "tclient"
```

- [ ] **Step 3: unittest source_keys — PASS**

Run: `py -3 -m unittest qmclient_scripts.languages_qmclient.tests.test_source_keys -q`

**Do not** run full validate until Task 3 remaps tclient keys into TOML.

---

### Task 3: CJK→EN migration for qmclient + tclient MACRO Desc

**Files:**
- Create: `qmclient_scripts/languages_qmclient/migrate_cjk_config_help.py`
- Create: `qmclient_scripts/languages_qmclient/translations/_migrations/cjk_config_help_map.json` (generated artifact: `{old: new}` or list of `{header, script_name, old, new}`)
- Modify: `src/engine/shared/config_variables_qmclient.h`
- Modify: `src/engine/shared/config_variables_tclient.h`
- Modify: `qmclient_scripts/languages_qmclient/translations/i18n/*.toml` (via patch/remap API)
- Modify: tests that hardcode CJK keys (e.g. `test_extracts_qmclient_config_descriptions`)

**Interfaces:**
- Consumes: MACRO parse (same as extract), `i18n_store.load_language_store` / `patch_module_store` or full module rewrite helpers, DeepSeek optional
- Produces: English Desc in headers; TOML keys renamed; SC filled from old CJK when missing

- [ ] **Step 1: Implement migration script CLI**

```text
py -3 qmclient_scripts/languages_qmclient/migrate_cjk_config_help.py --generate-map
py -3 qmclient_scripts/languages_qmclient/migrate_cjk_config_help.py --apply
```

`--generate-map` behavior:
1. Parse both headers' MACRO last-arg Descs where `has_cjk(desc)`
2. For each old key, propose English:
   - Prefer DeepSeek: system "Translate DDNet config help Chinese to concise English UI source key; keep product names; no quotes"
   - Fallback: if already mixed EN+CJK, keep English parts + translate remainder
3. Write `cjk_config_help_map.json` with unique `new` keys (dedupe collisions by appending ` ({script_name})`)

`--apply` behavior:
1. Load map
2. Rewrite header files: replace exact `"old"` Desc string occurrences carefully (only MACRO last string — prefer structural rewrite per MACRO call, not blind global replace)
3. For each old→new:
   - Find store entries with `key==old` (any module)
   - Create/merge entry under preferred module (`qmclient`/`tclient`) with `key=new`, languages from old; if `simplified_chinese` empty set to `old`
   - Remove old identity from store
4. Dump modules via existing write helpers

- [ ] **Step 2: Generate map (DeepSeek if key present)**

Run: `py -3 qmclient_scripts/languages_qmclient/migrate_cjk_config_help.py --generate-map`

Expected: map size ≈ 479 (qmclient) + ≈ 195 (tclient) unique CJK descs.

- [ ] **Step 3: Spot-check 10 map entries** (product names preserved, no empty new)

- [ ] **Step 4: Apply map**

Run: `py -3 qmclient_scripts/languages_qmclient/migrate_cjk_config_help.py --apply`

- [ ] **Step 5: Update tests** that assert CJK samples to English samples from map

- [ ] **Step 6: Full pipeline**

```bash
py -3 -m unittest discover qmclient_scripts/languages_qmclient/tests -q
py -3 qmclient_scripts/languages_qmclient/extract_strings.py
py -3 qmclient_scripts/languages_qmclient/generate_all.py
py -3 qmclient_scripts/languages_qmclient/validate.py --incremental
py -3 - <<'PY'
import sys
sys.path.insert(0,'qmclient_scripts/languages_qmclient')
import source_keys
recs=source_keys.collect_source_key_records()
for needle in ('config_variables_qmclient.h','config_variables_tclient.h'):
    cjk=[r for r in recs if r.source and needle in r.source.as_posix() and source_keys.has_cjk(r.key)]
    print(needle, 'records', sum(1 for r in recs if r.source and needle in r.source.as_posix()), 'cjk', len(cjk))
PY
```

Expected: unittest OK; validate pass; cjk counts 0 for both headers; tclient records ≥ 150.

---

### Task 4: Residual EN missing fill (only if validate reports missing after Task 3)

**Files:** draft/write-back via existing translate script; TOML only

- [ ] Run translate for missing only (no `--rewrite`) for GENERATED langs
- [ ] `--write-back` → `generate_all` → `validate`

---

## Spec coverage self-check

| Spec requirement | Task |
|------------------|------|
| Digit subset gate + tests | Task 1 |
| tclient MACRO extract + module map | Task 2 |
| CJK→EN headers + TOML remap | Task 3 |
| validate green / cjk 0 on two headers | Task 3 Step 6 + Task 4 |
| Out of scope config_variables.h | documented, no task |

## Placeholder scan

No TBD steps; EN generation uses DeepSeek with documented fallback.
