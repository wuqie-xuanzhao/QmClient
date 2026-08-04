> **已归档，禁止作为实现依据。** 本文保留为历史记录；当前实现请以活动 spec/plan/skill 为准。

# i18n / Gores / Laser / Translation Pipeline Current-State Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` or `superpowers:subagent-driven-development` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **Status rewrite (v5, 2026-06-27):** This file replaces the older v4 task list with a current-state handoff. The old `hook = 钩子/鉤子` premise was wrong. DDNet/core Simplified Chinese terms must follow `docs/superpowers/explore/2026-06-26-ddnet-official-simplified-chinese-terminology.md`: `Hook = 钩索`, `Hook collisions = 钩索辅助线`, `Grenade = 榴弹枪`. Do not re-apply the old "replace 钩索 with 钩子" task.

**Goal:** Finish the still-open i18n fallback detection, Gores smart weapon-cycle behavior, terminology cleanup, and translation pipeline automation while preserving the already-landed laser preview fix.

**Architecture:** Treat this as five independent tracks. Python i18n quality checks live in `qmclient_scripts/languages_qmclient/`; Chinese terminology source-of-truth is the TOML maintenance store plus official DDNet terminology evidence; Gores behavior lives in `tclient` runtime code plus pure helper tests in `qm_modes_test.cpp`; the laser preview fix is already present and only needs verification or test hardening; automation adds a standalone validation entrypoint and optional CI.

**Tech Stack:** Python 3.11+, TOML translation maintenance files, C++, GoogleTest, CMake Windows wrapper.

---

## Current Status Snapshot

| Area | Current status | Evidence / notes |
|---|---|---|
| Task 1: non-Chinese English fallback detection | **Not implemented** | `qmclient_scripts/languages_qmclient/i18n_store.py` still requires `any(word[:1].islower() for word in words)` in `_looks_like_english_placeholder_key()`, so title-case brand phrases like `OpenAI API Key` can still slip through. |
| Task 2: hook / weapon-switch terminology | **Partly corrected, partly open** | `terminology.toml` now uses official-style `Hook = 钩索` and `Grenade = 榴弹枪`, but `Auto weapon switch` is still `自动切换武器`, `Strong hook` is still `强力钩`, and `Weak hook` is still `弱钩索` in maintained TOML data. |
| Task 3: smart Gores weapon cycle | **Not implemented** | `CTClient::ShouldAppendGoresPrevWeapon()` still gates on `!HasBlockingGoresWeapon()`, and `UpdateGoresWeaponCycle()` still restores fixed `WEAPON_GUN` after hammer. No `m_GoresPreHammerWeapon`, `ShouldPulseGoresHammerOnFire`, or `GoresRestoreWeaponAfterHammer` exists. |
| Task 4: laser preview round caps | **Core fix implemented** | Plain laser branch in `src/game/client/components/items.cpp` now draws round caps when `QmLaserRoundCaps` is enabled, so round caps no longer depend on enhanced laser glow. Existing tests are source-string checks, not runtime render-call checks. |
| Task 6: HTTP translation defaults / concurrency | **Not implemented** | `translate_with_local_http.py` still defaults `--base-url`, `--model`, and `--api-key` to empty strings and still exits unless base URL and model are supplied. |
| Task 7: terminology glossary + prompt wiring | **Partly implemented** | `terminology.toml` has expanded many terms, including official `Hook`, `Hook collision line`, and `Grenade`. `Zhipu AI` is not present as a terminology term. Verify prompt rendering before extending. |
| Task 8: staged quality warnings | **Partly implemented but not staged** | Terminology mismatch checks exist as hard errors. There is no warning channel, placeholder check, length-risk warning, or CJK style warning path. |
| Task 9: standalone validation / draft cleanup / CI | **Mostly not implemented** | `validate_translations.py` and `.github/workflows/i18n-check.yml` do not exist. Draft pruning exists inside write-back flow, but no `--clean-drafts` / `--auto-clean` CLI exists. |

## Scope Rules For The Next Implementer

- Do not change gameplay protocol, physics, prediction, snapshot format, rank behavior, map behavior, or upstream DDNet core unless a task explicitly says so.
- Do not manually edit generated `data/languages/*.txt` as the source of truth. Modify `qmclient_scripts/languages_qmclient/translations/i18n/*.toml`, then regenerate.
- Keep the old-hook premise dead: do not replace official/core `钩索` with `钩子`.
- Treat the existing dirty worktree as user/script-owned unless you can prove your task created the change.
- Put temporary logs or scratch output under `tmp/`.

---

### Task 1: Finish English Fallback Detection For Title-Case Brand Phrases

**Goal:** Detect non-Chinese translations that lazily keep title-case English source strings such as `OpenAI API Key`, while preserving legitimate source-like tokens such as `Demo`, `HUD`, URLs, and short proper nouns.

**Files:**
- Modify: `qmclient_scripts/languages_qmclient/i18n_store.py`
- Modify: `qmclient_scripts/languages_qmclient/tests/test_translations_toml.py`

- [ ] **Step 1: Add the failing title-case fallback test**

Add this test near the existing `translation_quality` tests in `qmclient_scripts/languages_qmclient/tests/test_translations_toml.py`:

```python
    def test_translation_quality_rejects_title_case_brand_phrase_fallback(self):
        store = {
            "qmclient": {
                ("OpenAI API Key", ""): {
                    "spanish": "OpenAI API Key",
                    "simplified_chinese": "OpenAI API 密钥",
                }
            }
        }

        errors = i18n_store.translation_quality_errors(store)

        self.assertTrue(
            any("spanish repeats English source key" in item for item in errors),
            f"expected title-case fallback to be rejected; got: {errors}",
        )
```

- [ ] **Step 2: Add the legitimate source-token guard test**

Extend `test_translation_quality_allows_source_like_tokens` or add a nearby test:

```python
    def test_translation_quality_allows_explicit_source_like_brand_tokens(self):
        store = {
            "qmclient": {
                ("Demo", ""): {"spanish": "Demo"},
                ("HUD", ""): {"spanish": "HUD"},
                ("https://ddnet.org/discord", ""): {
                    "spanish": "https://ddnet.org/discord"
                },
            }
        }

        self.assertEqual(i18n_store.translation_quality_errors(store), [])
```

- [ ] **Step 3: Run the focused test and confirm it fails before implementation**

Run:

```bash
python -m unittest qmclient_scripts.languages_qmclient.tests.test_translations_toml -v
```

Expected before implementation: the new `OpenAI API Key` test fails because the current `_looks_like_english_placeholder_key()` still requires a lowercase-leading word.

- [ ] **Step 4: Loosen `_looks_like_english_placeholder_key()`**

In `qmclient_scripts/languages_qmclient/i18n_store.py`, replace:

```python
    return any(word[:1].islower() for word in words)
```

with:

```python
    return True
```

Keep the existing earlier guard:

```python
    if not any(any(char.isalpha() for char in word) for word in words):
        return False
```

That makes "has at least one alphabetic word" the final positive condition, while `_may_keep_source_text()` remains the allowlist escape hatch.

- [ ] **Step 5: Audit newly reported repeats and update allowlist only for legitimate source tokens**

Run:

```bash
python qmclient_scripts/languages_qmclient/validate.py
```

If legitimate non-translatable tokens are newly rejected, add only exact safe tokens to `_may_keep_source_text()` in `i18n_store.py`. Do not allowlist phrases that should be translated, such as settings labels.

- [ ] **Step 6: Run the i18n unit tests**

Run:

```bash
python -m unittest qmclient_scripts.languages_qmclient.tests.test_translations_toml -v
```

Expected: all tests pass, including the new title-case fallback rejection and allowlist guard.

---

### Task 2: Correct Current Chinese Terminology Without Reviving The Old Hook Premise

**Goal:** Keep official/core `Hook = 钩索` while cleaning the still-open QmClient-specific labels: `Auto weapon switch`, `Strong hook`, and `Weak hook`.

**Files:**
- Modify: `qmclient_scripts/languages_qmclient/translations/i18n/qmclient.toml`
- Modify: `qmclient_scripts/languages_qmclient/translations/i18n/menus.toml`
- Modify if needed: `qmclient_scripts/languages_qmclient/prompt_assets/terminology.toml`
- Generated by scripts: `data/languages/*.txt`

- [ ] **Step 1: Re-read the official terminology exploration**

Read:

```bash
type docs\superpowers\explore\2026-06-26-ddnet-official-simplified-chinese-terminology.md
```

Required conclusion before editing: keep official/core `Hook` as `钩索`; do not replace it with `钩子`.

- [ ] **Step 2: Change Gores weapon-switch label**

In `qmclient_scripts/languages_qmclient/translations/i18n/qmclient.toml`, find:

```toml
key = "Auto weapon switch"
```

Change only Simplified and Traditional Chinese:

```toml
simplified_chinese = "自动切锤"
traditional_chinese = "自動切錘"
```

- [ ] **Step 3: Normalize strong/weak hook display labels**

In `qmclient_scripts/languages_qmclient/translations/i18n/menus.toml`, find keys:

```toml
key = "Strong hook"
key = "Strong hook color"
key = "Weak hook"
key = "Weak hook color"
```

Set Simplified/Traditional Chinese consistently:

```toml
simplified_chinese = "强钩"
traditional_chinese = "強鉤"
```

```toml
simplified_chinese = "强钩颜色"
traditional_chinese = "強鉤顏色"
```

```toml
simplified_chinese = "弱钩"
traditional_chinese = "弱鉤"
```

```toml
simplified_chinese = "弱钩颜色"
traditional_chinese = "弱鉤顏色"
```

This is a QmClient UI shorthand decision. It does not change official/core `Hook = 钩索`.

- [ ] **Step 4: Decide whether `terminology.toml` should include `Strong hook` / `Weak hook`**

If these labels should be enforced during future translation, add:

```toml
[[term]]
source = "Strong hook"
enforce = "exact"
simplified_chinese = "强钩"
traditional_chinese = "強鉤"

[[term]]
source = "Weak hook"
enforce = "exact"
simplified_chinese = "弱钩"
traditional_chinese = "弱鉤"
```

Do not alter the existing `Hook` term away from `钩索`.

- [ ] **Step 5: Regenerate language outputs**

Run in order:

```bash
python qmclient_scripts/languages_qmclient/extract_strings.py
python qmclient_scripts/languages_qmclient/generate_all.py
python qmclient_scripts/languages_qmclient/validate.py
python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0
```

Expected: no TOML validation errors. `data/languages/*.txt` updates are generated outputs.

---

### Task 3: Implement Smart Gores Weapon Cycle

**Goal:** In Gores mode, fire keydown with extra weapons should pulse hammer once, then restore the weapon the player actually held before the pulse. Holding fire should not break normal firing after the initial hammer pulse.

**Files:**
- Modify: `src/game/client/components/tclient/tclient.h`
- Modify: `src/game/client/components/tclient/tclient.cpp`
- Modify: `src/game/client/components/qmclient/modes.h`
- Modify: `src/game/client/components/qmclient/modes.cpp`
- Test: `src/test/qm_modes_test.cpp`

- [ ] **Step 1: Add pure helper declarations**

In `src/game/client/components/qmclient/modes.h`, add near the existing Gores helpers:

```cpp
int GoresRestoreWeaponAfterHammer(int PreHammerWeapon, bool HasPreHammerWeapon);
bool ShouldPulseGoresHammerOnFire(bool GoresCycleActive, bool FireJustPressed, bool CurrentWeaponIsHammer, bool FreezeWakeupActive);
```

- [ ] **Step 2: Add pure helper tests first**

In `src/test/qm_modes_test.cpp`, add:

```cpp
TEST(QmGoresMode, RestoreWeaponAfterHammerUsesRecordedWeapon)
{
	EXPECT_EQ(GoresRestoreWeaponAfterHammer(WEAPON_LASER, true), WEAPON_LASER);
	EXPECT_EQ(GoresRestoreWeaponAfterHammer(WEAPON_GRENADE, true), WEAPON_GRENADE);
	EXPECT_EQ(GoresRestoreWeaponAfterHammer(WEAPON_GUN, false), WEAPON_GUN);
}

TEST(QmGoresMode, FireKeydownPulseRequiresActiveCycleAndNonHammerWeapon)
{
	EXPECT_TRUE(ShouldPulseGoresHammerOnFire(true, true, false, false));
	EXPECT_FALSE(ShouldPulseGoresHammerOnFire(false, true, false, false));
	EXPECT_FALSE(ShouldPulseGoresHammerOnFire(true, false, false, false));
	EXPECT_FALSE(ShouldPulseGoresHammerOnFire(true, true, true, false));
	EXPECT_FALSE(ShouldPulseGoresHammerOnFire(true, true, false, true));
}
```

- [ ] **Step 3: Run focused tests and confirm failure**

Build `testrunner` if needed, then run:

```bash
cmake-build-release\testrunner.exe --gtest_filter=QmGoresMode.*
```

Expected before implementation: compile or link failure because the new helpers do not exist.

- [ ] **Step 4: Implement pure helpers**

In `src/game/client/components/qmclient/modes.cpp`, add:

```cpp
int GoresRestoreWeaponAfterHammer(int PreHammerWeapon, bool HasPreHammerWeapon)
{
	return HasPreHammerWeapon ? PreHammerWeapon : WEAPON_GUN;
}

bool ShouldPulseGoresHammerOnFire(bool GoresCycleActive, bool FireJustPressed, bool CurrentWeaponIsHammer, bool FreezeWakeupActive)
{
	return GoresCycleActive && FireJustPressed && !CurrentWeaponIsHammer && !FreezeWakeupActive;
}
```

Include the weapon constants header if this file does not already have access to `WEAPON_GUN`.

- [ ] **Step 5: Add per-dummy state**

In `src/game/client/components/tclient/tclient.h`, add state next to the existing Gores hammer wakeup state:

```cpp
int m_aGoresPreHammerWeapon[NUM_DUMMIES] = {WEAPON_GUN, WEAPON_GUN};
bool m_aGoresHasPreHammerWeapon[NUM_DUMMIES] = {};
bool m_aPrevFireForGores[NUM_DUMMIES] = {};
```

Use the repo's existing initialization style if the surrounding class does not use in-class initializers.

- [ ] **Step 6: Split "has extra weapon" from "disable because extra weapon"**

In `src/game/client/components/tclient/tclient.cpp`, keep `HasBlockingGoresWeapon()` if other code relies on the old config-sensitive behavior, but add a local helper or method that checks extra weapons independent of `m_QmGoresDisableIfWeapons`:

```cpp
bool CTClient::HasExtraGoresWeapon() const
{
	if(Client()->State() != IClient::STATE_ONLINE || !GameClient()->m_Snap.m_pLocalCharacter)
		return false;

	const CCharacterCore &Core = GameClient()->m_PredictedPrevChar;
	return Core.m_aWeapons[WEAPON_SHOTGUN].m_Got ||
	       Core.m_aWeapons[WEAPON_GRENADE].m_Got ||
	       Core.m_aWeapons[WEAPON_LASER].m_Got ||
	       Core.m_aWeapons[WEAPON_NINJA].m_Got;
}
```

Declare it in `tclient.h` if implemented as a method.

- [ ] **Step 7: Make bind-added `+prevweapon` only handle two-weapon cases**

Update `CTClient::ShouldAppendGoresPrevWeapon()` so it returns false whenever extra weapons exist, regardless of `m_QmGoresDisableIfWeapons`:

```cpp
bool CTClient::ShouldAppendGoresPrevWeapon() const
{
	return Client()->State() == IClient::STATE_ONLINE &&
	       !GameClient()->m_Snap.m_SpecInfo.m_Active &&
	       GameClient()->m_Snap.m_pLocalCharacter != nullptr &&
	       IsGoresModuleEnabled() &&
	       g_Config.m_QmGoresAutoWeaponSwitch != 0 &&
	       !HasExtraGoresWeapon();
}
```

Reason: `+prevweapon` can be correct for hammer/gun two-state cycling, but with three or more weapons it can select the wrong slot.

- [ ] **Step 8: Add multi-weapon pulse path before the current early return**

In `CTClient::UpdateGoresWeaponCycle()`, compute:

```cpp
const bool FireHeld = (Input.m_Fire & 1) != 0;
const bool FireJustPressed = FireHeld && !m_aPrevFireForGores[Dummy];
m_aPrevFireForGores[Dummy] = FireHeld;
```

Before the current `if(!GoresCycleActive) return;`, add a separate multi-weapon cycle gate:

```cpp
const bool MultiWeaponPulseActive =
	Client()->State() == IClient::STATE_ONLINE &&
	!GameClient()->m_Snap.m_SpecInfo.m_Active &&
	GameClient()->m_Snap.m_pLocalCharacter != nullptr &&
	IsGoresModuleEnabled() &&
	g_Config.m_QmGoresAutoWeaponSwitch != 0 &&
	g_Config.m_QmGoresDisableIfWeapons != 0 &&
	HasExtraGoresWeapon();
```

Then, once `ExternalHammerWakeup` is known, use:

```cpp
const bool CurrentWeaponIsHammer = GameClient()->m_Snap.m_pLocalCharacter->m_Weapon == WEAPON_HAMMER;
if(ShouldPulseGoresHammerOnFire(MultiWeaponPulseActive, FireJustPressed, CurrentWeaponIsHammer, ExternalHammerWakeup))
{
	m_aGoresPreHammerWeapon[Dummy] = GameClient()->m_Snap.m_pLocalCharacter->m_Weapon;
	m_aGoresHasPreHammerWeapon[Dummy] = true;
	Input.m_WantedWeapon = WEAPON_HAMMER + 1;
	Input.m_Fire = QmGoresHammerWakeupFireState(Input.m_Fire);
	m_aGoresHammerWakeupFirePendingRelease[Dummy] = !FireHeld;
	m_aWasInFreezeForGoresHammer[Dummy] = InFreeze;
	return;
}
```

Place this after `InFreeze` / `ExternalHammerWakeup` are computed and before returning for inactive two-weapon cycle.

- [ ] **Step 9: Restore recorded weapon after hammer pulse**

Replace the fixed restore:

```cpp
if(GameClient()->m_Snap.m_pLocalCharacter->m_Weapon == WEAPON_HAMMER)
	GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_WantedWeapon = WEAPON_GUN + 1;
```

with:

```cpp
if(GameClient()->m_Snap.m_pLocalCharacter->m_Weapon == WEAPON_HAMMER)
{
	const int RestoreWeapon = GoresRestoreWeaponAfterHammer(
		m_aGoresPreHammerWeapon[Dummy],
		m_aGoresHasPreHammerWeapon[Dummy]);
	GameClient()->m_Controls.m_aInputData[Dummy].m_WantedWeapon = RestoreWeapon + 1;
	m_aGoresHasPreHammerWeapon[Dummy] = false;
}
```

- [ ] **Step 10: Run focused and full C++ tests**

Run:

```bash
cmake-build-release\testrunner.exe --gtest_filter=QmGoresMode.*
cmd /c call qmclient_scripts\cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
```

Expected: focused and full C++ tests pass.

- [ ] **Step 11: Manual gameplay verification**

Verify in Gores mode:

1. No extra weapon: existing hammer/gun behavior still works.
2. With shotgun/grenade/laser: first fire keydown switches to hammer, hammers once, then returns to the original weapon.
3. Holding fire after the initial pulse lets the restored weapon fire normally.
4. Freeze wakeup still takes priority and does not double-pulse.

Record any missing manual verification as a gap.

---

### Task 4: Verify Or Harden The Existing Laser Preview Fix

**Goal:** Keep the already implemented round-cap fix and optionally replace brittle source-string tests with a more meaningful render helper test if the test harness supports it.

**Files:**
- Read: `src/game/client/components/items.cpp`
- Read: `src/game/client/components/menus.cpp`
- Modify optional: `src/test/qmclient_monitoring_test.cpp`

- [ ] **Step 1: Confirm current code still draws round caps in both branches**

Check `src/game/client/components/items.cpp`:

- enhanced branch has `if(g_Config.m_QmLaserRoundCaps)`.
- plain branch also has `if(g_Config.m_QmLaserRoundCaps)` and draws circles at both `From` and `Pos`.

- [ ] **Step 2: Run existing focused test**

Run:

```bash
cmake-build-release\testrunner.exe --gtest_filter=QmMonitoringHelpers.LaserRoundCapsRenderedInBothEnhancedAndPlainPaths
```

Expected: pass.

- [ ] **Step 3: Decide whether to harden the test now**

If time is limited, keep the existing test and document it as source-level coverage. If hardening, replace the string-count assertion with a test helper that exercises a render-call recorder. Do not remove coverage without adding an equivalent assertion that plain and enhanced paths both expose round-cap behavior.

- [ ] **Step 4: Visual verification**

Run the client and verify settings preview:

1. `QmLaserRoundCaps = 1`, `QmLaserEnhanced = 0`: preview endpoints become rounded.
2. `QmLaserRoundCaps = 1`, `QmLaserEnhanced = 1`: enhanced preview still has rounded endpoints.
3. `QmLaserRoundCaps = 0`: preview returns to square/plain endpoints.

Record visual verification result or gap.

---

### Task 5: Add DeepSeek Defaults And Better Translation CLI Defaults

**Goal:** Make `translate_with_local_http.py` usable without always passing base URL and model, using DeepSeek defaults and safer concurrency.

**Files:**
- Modify: `qmclient_scripts/languages_qmclient/translate_with_local_http.py`
- Test: `qmclient_scripts/languages_qmclient/tests/test_translate_with_local_http.py`

- [ ] **Step 1: Add default constants**

Near the top of `translate_with_local_http.py`, add:

```python
DEFAULT_BASE_URL = "https://api.deepseek.com"
DEFAULT_MODEL = "deepseek-v4-flash"
DEFAULT_API_KEY_ENV = "DEEPSEEK_API_KEY"
_CPU_COUNT = os.cpu_count() or 4
DEFAULT_PARALLEL_LANGUAGES = min(MAX_PARALLEL_LANGUAGES, max(1, _CPU_COUNT - 2))
DEFAULT_PARALLEL_REQUESTS = 4
```

Place these after `MAX_PARALLEL_REQUESTS` / `MAX_PARALLEL_LANGUAGES` so the constants exist first.

- [ ] **Step 2: Update parser defaults**

Change parser defaults:

```python
parser.add_argument("--base-url", default=DEFAULT_BASE_URL)
parser.add_argument("--model", default=DEFAULT_MODEL)
parser.add_argument("--api-key", default=os.getenv(DEFAULT_API_KEY_ENV, ""))
parser.add_argument("--parallel-requests", type=int, default=DEFAULT_PARALLEL_REQUESTS)
parser.add_argument("--parallel-languages", type=int, default=DEFAULT_PARALLEL_LANGUAGES)
```

- [ ] **Step 3: Remove obsolete base-url/model required check**

Delete this runtime check:

```python
if not args.base_url or not args.model:
    raise SystemExit(
        "--base-url and --model are required unless --write-back is used"
    )
```

Keep API-key validation behavior in `resolve_api_key()` if it already exists.

- [ ] **Step 4: Add or update unit tests**

In `test_translate_with_local_http.py`, add parser-level tests:

```python
def test_parser_uses_deepseek_defaults(self):
    args = translate_with_local_http.build_parser().parse_args([])
    self.assertEqual(args.base_url, "https://api.deepseek.com")
    self.assertEqual(args.model, "deepseek-v4-flash")
    self.assertGreaterEqual(args.parallel_requests, 1)
    self.assertGreaterEqual(args.parallel_languages, 1)
```

- [ ] **Step 5: Run tests**

Run:

```bash
python -m unittest qmclient_scripts.languages_qmclient.tests.test_translate_with_local_http -v
```

Expected: pass.

---

### Task 6: Finish Terminology Prompt Wiring And Add `Zhipu AI`

**Goal:** Ensure prompt rendering includes the terminology table and add the missing `Zhipu AI` brand term.

**Files:**
- Modify: `qmclient_scripts/languages_qmclient/prompt_assets/terminology.toml`
- Read/modify if needed: `qmclient_scripts/languages_qmclient/prompt_assets/user_prompt.txt`
- Read/modify if needed: `qmclient_scripts/languages_qmclient/translate_with_local_http.py`
- Test: `qmclient_scripts/languages_qmclient/tests/test_translate_with_local_http.py`

- [ ] **Step 1: Add `Zhipu AI` to terminology**

Append to `terminology.toml`:

```toml
[[term]]
source = "Zhipu AI"
enforce = "pattern"
simplified_chinese = "智谱清言"
traditional_chinese = "智譜清言"
korean = "Zhipu AI"
japanese = "Zhipu AI"
russian = "Zhipu AI"
german = "Zhipu AI"
spanish = "Zhipu AI"
french = "Zhipu AI"
brazilian_portuguese = "Zhipu AI"
portuguese = "Zhipu AI"
turkish = "Zhipu AI"
polish = "Zhipu AI"
```

- [ ] **Step 2: Verify prompt asset contains terminology placeholder**

Check `user_prompt.txt` contains a terminology placeholder. If missing, add a short section:

```text
Terminology:
{terminology}
```

- [ ] **Step 3: Verify prompt renderer fills terminology**

Run:

```bash
python qmclient_scripts/languages_qmclient/translate_with_local_http.py --languages korean --dry-run --limit 1
```

Expected: dry-run output contains rendered terminology and no literal `{terminology}`.

- [ ] **Step 4: Add/adjust dry-run test**

In `test_translate_with_local_http.py`, add or update a test that patches prompt assets with a `Zhipu AI` term and asserts the built prompt contains the target-language terminology line.

---

### Task 7: Add Staged Translation Quality Warnings

**Goal:** Add warning-only checks for risky translations without breaking CI immediately.

**Files:**
- Modify: `qmclient_scripts/languages_qmclient/i18n_store.py`
- Modify: `qmclient_scripts/languages_qmclient/validate.py`
- Test: `qmclient_scripts/languages_qmclient/tests/test_translations_toml.py`

- [ ] **Step 1: Add a quality report object**

In `i18n_store.py`, introduce:

```python
@dataclass(frozen=True)
class TranslationQualityReport:
    errors: list[str]
    warnings: list[str]
```

Import `dataclass` if needed.

- [ ] **Step 2: Add `translation_quality_report()` and keep existing API stable**

Implement:

```python
def translation_quality_report(
    store: dict[str, dict[tuple[str, str], dict[str, str]]],
    *,
    terminology_by_language: dict[str, dict[str, object]] | None = None,
    active_identities: set[tuple[str, str]] | None = None,
    limit: int | None = None,
) -> TranslationQualityReport:
    errors = translation_quality_errors(
        store,
        terminology_by_language=terminology_by_language,
        active_identities=active_identities,
        limit=limit,
    )
    warnings: list[str] = []
    # Add warning-only checks in later steps.
    return TranslationQualityReport(errors=errors, warnings=warnings)
```

Do not remove `translation_quality_errors()`; existing callers should continue to work.

- [ ] **Step 3: Add placeholder-count warnings**

Add a helper that compares placeholder tokens in source and translation:

```python
PLACEHOLDER_RE = re.compile(r"%[sdif]|\\{[A-Za-z0-9_]+\\}")
```

Warning condition: if sorted placeholder token lists differ, append:

```text
module: [context] key: language placeholder mismatch: source [...] translation [...]
```

- [ ] **Step 4: Add length-risk warnings**

Warning condition:

```python
if len(key) > 10 and len(translation) > len(key) * 2.5:
    warnings.append(...)
```

Do not make this an error yet.

- [ ] **Step 5: Add tests for warning channel**

Add tests that assert:

```python
report = i18n_store.translation_quality_report(store)
self.assertEqual(report.errors, [])
self.assertTrue(any("placeholder mismatch" in item for item in report.warnings))
```

and similarly for length warning.

- [ ] **Step 6: Surface warnings in `validate.py` without failing**

Update `validate.py` so it prints warnings but exits non-zero only for errors.

- [ ] **Step 7: Run unit tests and validate**

Run:

```bash
python -m unittest qmclient_scripts.languages_qmclient.tests.test_translations_toml -v
python qmclient_scripts/languages_qmclient/validate.py
```

Expected: tests pass; warnings may print but should not fail validation unless existing hard errors exist.

---

### Task 8: Add Standalone `validate_translations.py`, Draft Cleanup CLI, And Optional CI

**Goal:** Provide a direct quality-check entrypoint and CLI cleanup commands for translation drafts.

**Files:**
- Create: `qmclient_scripts/languages_qmclient/validate_translations.py`
- Modify: `qmclient_scripts/languages_qmclient/translate_with_local_http.py`
- Modify if needed: `qmclient_scripts/languages_qmclient/i18n_store.py`
- Optional create: `.github/workflows/i18n-check.yml`
- Modify: `qmclient_scripts/languages_qmclient/README.md` if present

- [ ] **Step 1: Create `validate_translations.py`**

Create:

```python
#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path

try:
    from . import i18n_store
except ImportError:  # pragma: no cover
    import i18n_store


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("--json", dest="json_path", default="")
    parser.add_argument("--languages", nargs="*", default=[])
    return parser


def main() -> None:
    args = build_parser().parse_args()
    store = i18n_store.load_language_store()
    report = (
        i18n_store.translation_quality_report(store)
        if hasattr(i18n_store, "translation_quality_report")
        else None
    )
    if report is None:
        errors = i18n_store.translation_quality_errors(store)
        warnings = []
    else:
        errors = report.errors
        warnings = report.warnings

    for item in warnings:
        print(f"warning: {item}")
    for item in errors:
        print(f"error: {item}")

    if args.json_path:
        Path(args.json_path).write_text(
            json.dumps({"errors": errors, "warnings": warnings}, ensure_ascii=False, indent=2),
            encoding="utf-8",
        )
    if errors:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
```

If language filtering is needed immediately, implement it before using `--languages` in CI.

- [ ] **Step 2: Add parser options for draft cleanup**

In `translate_with_local_http.py`, add:

```python
parser.add_argument("--clean-drafts", action="store_true")
parser.add_argument("--auto-clean", action="store_true")
```

- [ ] **Step 3: Implement empty draft cleanup**

Add a function:

```python
def clean_empty_drafts() -> list[str]:
    removed: list[str] = []
    if not TRANSLATIONS_DRAFT_DIR.exists():
        return removed
    for path in sorted(TRANSLATIONS_DRAFT_DIR.rglob("*.toml")):
        text = path.read_text(encoding="utf-8").strip()
        if not text:
            path.unlink()
            removed.append(str(path))
    for directory in sorted(
        [p for p in TRANSLATIONS_DRAFT_DIR.rglob("*") if p.is_dir()],
        key=lambda p: len(p.parts),
        reverse=True,
    ):
        try:
            directory.rmdir()
            removed.append(str(directory))
        except OSError:
            pass
    return removed
```

Wire `--clean-drafts` to print removed paths and return before translation.

- [ ] **Step 4: Define `--auto-clean` narrowly**

For the first implementation, make `--auto-clean` mean:

1. run the normal selected translation/write-back operation,
2. then run `clean_empty_drafts()`,
3. print cleanup report.

Do not silently write back unreviewed draft translations unless the command also explicitly uses `--write-back`.

- [ ] **Step 5: Add optional CI workflow**

Create `.github/workflows/i18n-check.yml` only if project maintainers want CI for this:

```yaml
name: i18n Quality Check
on:
  pull_request:
    paths:
      - 'src/**'
      - 'qmclient_scripts/languages_qmclient/**'
  push:
    branches: [main, master]
jobs:
  validate-translations:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.11'
      - run: |
          python qmclient_scripts/languages_qmclient/extract_strings.py
          python qmclient_scripts/languages_qmclient/generate_all.py
          python qmclient_scripts/languages_qmclient/validate.py
          python qmclient_scripts/languages_qmclient/validate_translations.py
```

- [ ] **Step 6: Run validation chain**

Run:

```bash
python qmclient_scripts/languages_qmclient/extract_strings.py
python qmclient_scripts/languages_qmclient/generate_all.py
python qmclient_scripts/languages_qmclient/validate.py
python qmclient_scripts/languages_qmclient/validate_translations.py
python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0
```

Expected: no hard errors. Generated language files may change.

---

### Task 9: Final Repository Verification

**Goal:** Verify all modified tracks with the repo-standard checks.

- [ ] **Step 1: Python unit tests**

Run:

```bash
python -m unittest discover -s qmclient_scripts/languages_qmclient/tests -v
```

- [ ] **Step 2: i18n maintenance chain**

Run:

```bash
python qmclient_scripts/languages_qmclient/extract_strings.py
python qmclient_scripts/languages_qmclient/generate_all.py
python qmclient_scripts/languages_qmclient/validate.py
python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0
```

- [ ] **Step 3: C++ tests if Task 3 or Task 4 changed C++**

Run:

```bash
cmd /c call qmclient_scripts\cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
```

- [ ] **Step 4: Quick gate**

Run:

```bash
python qmclient_scripts/gate/check_gate.py --mode quick
```

- [ ] **Step 5: Manual verification gaps**

If Gores behavior or laser preview visual behavior was changed, record manual gameplay/visual verification. If it was not possible in the current session, say exactly which verification is missing.
