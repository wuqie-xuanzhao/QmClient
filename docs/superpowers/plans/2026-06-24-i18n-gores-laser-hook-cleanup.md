# i18n Fallback, Gores, Laser, Hook Cleanup and Pipeline Automation Plan

> ⚠️ **术语前提已修正 (2026-06-26):** 本计划中 Task 2 / Task 7 / Appendix A 曾把 QmClient 当前翻译或 `.context/game_terms.txt` 当作术语依据，并写成 `hook = 钩子/鉤子`。这不再成立。官方 DDNet 简体中文 `data/languages/simplified_chinese.txt` 的基线是 `Hook == 钩索`、`Hook collisions == 钩索辅助线`、`Grenade == 榴弹枪`。实施任何 i18n 术语任务前，先读 `docs/superpowers/explore/2026-06-26-ddnet-official-simplified-chinese-terminology.md`，并以官方简中作为 DDNet/core 术语来源。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **修订说明 (v4, 2026-06-24):** 对照用户原始需求 1-4 复核,发现 v3 多处偏离,本轮重写:
> - **Task 1 → 方案B**:v3 把它降级为「清理冗余项」,违背需求1(要能检测非中文偷翻)。改回:放宽 `_looks_like_english_placeholder_key:206` 小写词门槛 + 补 allowlist,让全大写品牌短语偷翻可被检测。
> - **Task 2 扩展**:加需求4(强钩/弱钩:强力钩→强钩、弱钩索→弱钩)+ 需求2a(Auto weapon switch→自动切锤)。
> - **Task 3 重写**:v3 是「锁现状 characterization」,与需求2b(改武器逻辑)**反向**。改为实现智能武器切换:有额外武器时也 fire keydown→切锤→锤击一次→换回动态记录的原武器;`m_QmGoresDisableIfWeapons` 改语义为智能。注:`qm_modes_test.cpp:69-102` 已有决策函数测试(v3「无测试」前提错)。
> - **Task 4 重写**:v3 假设「可能已对齐就跳过」,与需求3(用户观察到渲染失效)矛盾。改为定位并修复激光增强/圆角端点预览渲染失效 + 加测试。
> - **Task 6-9**:用户要求全保留(i18n 流水线自动化)。
> - v2 遗留保留:术语 hook=钩子/鉤子、Task4 反源码字面量测试、CI requirements.txt 修正。

**Goal:** 收口原始需求 1-4 四条功能/翻译 track,再做 i18n 流水线自动化:
(1) **i18n 检测**(需求1):放宽 `_looks_like_english_placeholder_key:206` 小写词门槛 + 精确补 allowlist,让非中文「全大写品牌短语偷翻(如 `OpenAI API Key`)」能被检测;
(2) **术语翻译**(需求4/2a):统一 hook→钩子/鉤子、强钩/弱钩(强力钩/弱钩索→强钩/弱钩)、"Auto weapon switch"→自动切锤;
(3) **Gores 武器智能切换**(需求2b):有额外武器时也走 fire keydown→切锤→锤击一次→换回动态记录的原武器;`m_QmGoresDisableIfWeapons` 改语义为智能切换;
(4) **激光预览修复**(需求3):定位并修复激光增强/圆角端点预览渲染失效,加运行时测试。

随后 i18n 流水线(用户要求全保留):收敛 HTTP 翻译默认值/并发(Task6)、扩展术语表接 prompt(Task7)、增量质量检查 warning→error(Task8)、独立校验入口 + draft 清理 + CI(Task9)。

**Architecture:** 五条 track。(1) Python 侧放宽 `:206` 门槛抓偷翻;(2) TOML 翻译数据 + 术语表统一 hook/强钩弱钩/自动切锤;(3) C++ 侧改 `UpdateGoresWeaponCycle` + `binds` + `HasBlockingGoresWeapon` 实现智能武器切换,扩 `qm_modes_test.cpp`;(4) C++ 侧定位修复激光预览渲染(`DoLaserPreview`/`RenderLaser` 配置读取);(5) Python i18n 流水线自动化。各 track 独立可并行;C++ 改动提交前跑全量 `run_cxx_tests`。

**Tech Stack:** Python, C++, existing QmClient i18n TOML flow, GoogleTest.

---

### Task 1: Catch non-Chinese English fallback (loosen `:206` gate + precise allowlist)

**背景:** 原始需求1 要求 Python 脚本能检测"非中文语言偷懒用英文 fallback"。现状 `translation_quality_errors` 的 `"repeats English source key"` 检查(`i18n_store.py:150-157`)**只对非中文语言生效**,但前置判断 `_looks_like_english_placeholder_key`(`:198-206`)最后一关 `any(word[:1].islower() ...)`(`:206`)要求 key **含小写首字母词**才视为该翻译——导致**全大写/首字母大写品牌短语**(`OpenAI API Key`/`Zhipu AI API Key`/`Tencent Cloud SecretId` 等)在非中文语言整串保留英文时**抓不到**(`OpenAI API Key` 三个词首字母都大写,过不了 `:206`)。

本 Task 放宽 `:206` 门槛让这些品牌短语偷翻能被检测,同时**精确补全 allowlist**(`:209-274`,已覆盖 `Demo`/`Brutal`/`Fun`/`DDmaX`/`HUD`/`API` 等合理保留项)避免误伤。品牌的**中文本地化**(`Zhipu AI→智谱清言`/`Tencent Cloud→腾讯云`)仍归 Task 7 术语表(本检查 `:150` 只对非中文)。

**Files:**
- Modify: `qmclient_scripts/languages_qmclient/i18n_store.py`
- Modify: `qmclient_scripts/languages_qmclient/tests/test_translations_toml.py`

- [ ] **Step 1: Audit 现有翻译数据,列出所有"非中文==英文原文"项并分类(先量化误伤面)**

先跑量化扫描看放宽 `:206` 后会报多少条(评估 audit 工作量):
```bash
python -c "
import tomllib, glob
for f in glob.glob('qmclient_scripts/languages_qmclient/translations/i18n/*.toml'):
    d = tomllib.load(open(f,'rb'))
    for m in d.get('message', []):  # toml 顶层是 [[message]],tomllib 读成 list
        key = m.get('key','')
        for lang, tr in m.get('translations',{}).items():
            if lang not in ('simplified_chinese','traditional_chinese') and tr == key:
                print(f.split('/')[-1], lang, repr(key))
"  # 粗扫;精确判定以 i18n_store.translation_quality_errors 为准
```
再分两类:
- (a) **该翻译的偷翻**(如 `OpenAI API Key` 在 spanish==原文)→ 放宽门槛后应被报
- (b) **合法保留**(如 `Demo`/`DDNet`/`HUD` 某语言保留)→ 应被 allowlist 豁免

把完整清单 + (a)/(b) 计数记进 Task 汇报。

- [ ] **Step 2: 把合法保留的全大写项补进 `_may_keep_source_text` allowlist**

对照 Step 1 (b) 类,确认它们在 allowlist(`:209-262`)。已覆盖的不动,漏网的补进。这是放宽门槛前的前置防误伤。

- [ ] **Step 3: 放宽 `_looks_like_english_placeholder_key:206` 门槛**

把 `:206` `any(word[:1].islower() for word in words)` 改为"含字母词即可"(去掉小写首字母限制),让全大写品牌短语也进入检查。改后靠 allowlist(Step 2)豁免合法项。

- [ ] **Step 4: 加测试,断言放宽后品牌短语偷翻能被报**

```python
def test_translation_quality_flags_uppercase_brand_phrase_fallback(self):
    # 放宽 :206 门槛后,"OpenAI API Key" 在非中文整串保留应被报 repeats-source
    store = {"qmclient": {("OpenAI API Key", ""): {"spanish": "OpenAI API Key"}}}
    errors = i18n_store.translation_quality_errors(store)
    self.assertTrue(
        any("spanish repeats English source key" in item for item in errors),
        f"expected fallback flagged after loosening :206 gate; got: {errors}",
    )
```
同时确认 `test_translation_quality_allows_source_like_tokens`(`:321`)等合法保留测试仍 PASS(allowlist 豁免生效)。**再加反向断言**:`("Demo","")` 在 spanish==原文**不报**(在 allowlist 豁免),确认放宽门槛没误伤合法保留项。

- [ ] **Step 5: 跑全量 i18n 校验链,逐条核对新报错**

Run(按 CLAUDE.md 顺序):
```bash
python qmclient_scripts/languages_qmclient/extract_strings.py
python qmclient_scripts/languages_qmclient/generate_all.py
python qmclient_scripts/languages_qmclient/validate.py
python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0
```
Expected: 新报错都是 Step 1 (a) 类该翻译的;若出现 (b) 类合法项被误伤,补进 allowlist 重跑。把逐条核对结果记进汇报。

---

### Task 2: Unify hook/weapon-switch terminology (需按官方简中重写后再实施)

> ⚠️ **不要直接执行本 Task 的原步骤。** 2026-06-26 官方简中术语调查确认：官方 DDNet 将 `Hook` 译为「钩索」，将 `Hook collision line` / `Hook collisions` 主要译为「钩索辅助线」。因此下面原计划中的「hook = 钩子」「把钩索替换为钩子」方向是错误前提。保留本段仅用于历史追踪；实施前应先基于 `docs/superpowers/explore/2026-06-26-ddnet-official-simplified-chinese-terminology.md` 重写 Task 2。

**原背景（已被纠正）:** 术语已定 hook = 「钩子」(简中)/「鉤子」(繁中),与 `translate.cpp:41` glossary 一致。需统一的翻译有三族:
1. **hook 单数**:现有数据「钩索/鉤索」跨 `menus/qmclient/tclient/touch_controls` 等文件几十处,需 audit 逐条替换;
2. **强钩/弱钩**(原始需求4):`menus.toml:9240`「强力钩」、`:9272/9273`「弱钩索/弱鉤索」、`:9288/9289` 颜色、`qmclient.toml:16589`「弱鉤子」——风格不统一,应统一为「强钩/弱钩」「強鉤/弱鉤」;
3. **自动切锤**(原始需求2a):`qmclient.toml:15220` "Auto weapon switch" 简「自动切换武器」→「自动切锤」、繁「自動切換武器」→「自動切錘」(Gores 模式核心是切锤)。

现有数据存在不一致 bug(如 `menus.toml:3413` 繁中 `鉤索輔助線` 对应 key "Hook collisions" 应为 `鉤索碰撞`)。

**Files:**
- Modify: `qmclient_scripts/languages_qmclient/prompt_assets/terminology.toml`
- Modify: `qmclient_scripts/languages_qmclient/translations/i18n/menus.toml`
- Modify: `qmclient_scripts/languages_qmclient/translations/i18n/qmclient.toml`
- Modify: `qmclient_scripts/languages_qmclient/translations/i18n/tclient.toml`
- Modify: `qmclient_scripts/languages_qmclient/translations/i18n/touch_controls.toml`
- Modify: `qmclient_scripts/languages_qmclient/translations/i18n/misc.toml`

- [ ] **Step 1: 加术语表条目(hook + Strong hook + Weak hook)**

在 `terminology.toml` 新增三个条目,简繁中分别为 钩子/鉤子、强钩/強鉤、弱钩/弱鉤(其余语言沿用现状,**不要写 钩索/强力钩/弱钩索**):

```toml
[[term]]
source = "hook"
simplified_chinese = "钩子"
traditional_chinese = "鉤子"
korean = "갈고리"
japanese = "フック"
russian = "крюк"
german = "Haken"
spanish = "gancho"
french = "grappin"
brazilian_portuguese = "gancho"
portuguese = "gancho"
turkish = "kanca"
polish = "hak"

[[term]]
source = "Strong hook"
simplified_chinese = "强钩"
traditional_chinese = "強鉤"

[[term]]
source = "Weak hook"
simplified_chinese = "弱钩"
traditional_chinese = "弱鉤"
```
(Strong/Weak hook 其余语言取现有 `menus.toml:9242-9251`/`9274-9283` 现状填入)

- [ ] **Step 2: 生成完整 audit 清单(hook + 强钩弱钩)**

Run:
```bash
grep -rnE "钩索|鉤索|强力钩|弱钩索|弱鉤索|弱鉤子" qmclient_scripts/languages_qmclient/translations/i18n/*.toml > /tmp/hook_audit.txt
grep -rnE "^key = .*(Hook|hook)" qmclient_scripts/languages_qmclient/translations/i18n/*.toml > /tmp/hook_keys.txt
```
把清单贴进 Task 汇报。**已知受影响项**:
- `钩索`/`弱钩索`/`彩虹色钩索` → `钩子`/`弱钩`...
- `强力钩`(`menus.toml:9240`)/`强力钩颜色`(`:9256`)→ `强钩`/`强钩颜色`
- `弱钩索`(`:9272`)/`弱鉤索`(`:9273`)/`弱钩索颜色`(`:9288`)/`弱鉤索顏色`(`:9289`)→ `弱钩`/`弱鉤`/`弱钩颜色`/`弱鉤顏色`
- `弱鉤子`(`qmclient.toml:16589`)→ `弱鉤`
- `钩索辅助线`/`钩索强度`/`钩索碰撞` → `钩子...`(顺手修 `menus.toml:3413` 繁中错译)
- `可钩索的方块` → 人工判断(`可用钩子的方块`)

- [ ] **Step 3: 逐条人工确认并替换简繁中翻译**

只改 **simplified_chinese / traditional_chinese** 两列(其余语言不动,统一交 Task 7/8)。

**禁止**无脑 `sed`——按 Step 2 清单逐条确认,每改一条确认中文通顺。强钩/弱钩族统一为「强钩/弱钩」「強鉤/弱鉤」。

- [ ] **Step 4: 改 "Auto weapon switch" → 自动切锤(需求2a)**

`qmclient.toml:15220` 简中 `自动切换武器` → `自动切锤`;`:15221` 繁中 `自動切換武器` → `自動切錘`。其余语言保留现状(本 Task 只改简繁中)。

- [ ] **Step 5: 重跑 i18n 维护链**

Run(按 CLAUDE.md 顺序):
```bash
python qmclient_scripts/languages_qmclient/extract_strings.py
python qmclient_scripts/languages_qmclient/generate_all.py
python qmclient_scripts/languages_qmclient/validate.py
python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0
```
Expected: 无 extraction drift、无 TOML 校验错误;`data/languages/*.txt` 重新生成,简繁中里 `钩索`/`强力钩`/`弱钩索`/`弱鉤子` 不再出现。

- [ ] **Step 6: 同步 Appendix A**

把本文件 Appendix A 的 Hook 行简繁中改为 `钩子/鉤子`,补 Strong hook/Weak hook 行。

---

### Task 3: Smart Gores weapon cycle — hammer pulse + restore original weapon (需求2b)

**背景:** 原始需求2b 要求:即便持有额外武器,Gores 模式下按开火键也应「切锤→锤击一次→换回原来的武器」,而非现状的「有额外武器就完全停用」。

**现状两套机制叠加:**
1. `binds.cpp:200-213 ExecuteBindCommand`:按 `+fire` 时,若 `ShouldAppendGoresPrevWeapon()` 且 bind 含 `+fire` 不含 `+prevweapon` → 追加 `;+prevweapon`(切上一武器);
2. `tclient.cpp:3410 UpdateGoresWeaponCycle`:冻结时切锤解冻 → 锤唤醒(`ShouldTriggerQmGoresHammerWakeup`)→ **`:3453-3454` 锤击后写死换回 `WEAPON_GUN`(枪)**;
3. `HasBlockingGoresWeapon`(`tclient.cpp:3388`)= `m_QmGoresDisableIfWeapons`(=「拿到额外武器停用」选项,`qmclient.toml:18980`)开 + 拥有散弹/手雷/激光/忍者 → `ShouldAppendGoresPrevWeapon`=false → **两套机制全停**。

**设计决策(用户拍板):**
- **换回武器**:动态记录锤击前持有的武器(非固定 gun);
- **触发时机**:**fire keydown 边沿脉冲**——点按 fire=切锤锤一下换回(解冻);按住 fire=首次 keydown 锤一下、之后用换回的原武器连发(解决「枪无法连发」冲突);
- **`m_QmGoresDisableIfWeapons`**:改语义为「智能切换」——有额外武器时不再完全停用,而是仍走 fire→锤→换回原武器。

决策纯函数在 `modes.cpp:62-85`,**`qm_modes_test.cpp:69-102` 已有测试**(本 Task 扩展而非从零加)。

**Files:**
- Modify: `src/game/client/components/tclient/tclient.h`(新增 `m_GoresPreHammerWeapon` 等状态)
- Modify: `src/game/client/components/tclient/tclient.cpp`(`UpdateGoresWeaponCycle`、`HasBlockingGoresWeapon`、`ShouldAppendGoresPrevWeapon`)
- Modify: `src/game/client/components/qmclient/modes.cpp`(新增「锤击后换回原武器」「脉冲触发」决策函数)
- Modify: `src/game/client/components/qmclient/modes.h`(声明)
- Modify: `src/game/client/components/binds.cpp`(`ExecuteBindCommand` 配合)
- Modify: `src/test/qm_modes_test.cpp`(扩展测试)

- [ ] **Step 1: 加状态(原武器 + fire 边沿)**

在 `tclient.h` 加:
- `int m_GoresPreHammerWeapon[NUM_DUMMIES]`(初始化 `WEAPON_GUN`):记录锤脉冲切锤**前**玩家持有的武器。取 `m_Snap.m_pLocalCharacter->m_Weapon`(**实际持有武器**,非 `m_WantedWeapon`——切换过程中两者会暂时不一致,要记实际持有的);
- `bool m_aPrevFireForGores[NUM_DUMMIES]`(初始化 false):记录上一 tick `Input.m_Fire & 1`,用于检测 fire **keydown 边沿**(0→1)。

**edge case 1 处理**:脉冲只在**当前武器≠锤**时触发——玩家已持锤时按 fire 就是正常锤击,不脉冲切锤(避免无意义切回)。`m_GoresPreHammerWeapon` 仅在「当前武器≠锤 且 触发脉冲」时记录。

- [ ] **Step 2: 新增决策纯函数(可单测)**

在 `modes.cpp` 加(与现有 `ShouldTrigger` 同级):
```cpp
// 锤脉冲完成后换回的武器:有记录的原武器用它,否则回退枪
int GoresRestoreWeaponAfterHammer(int PreHammerWeapon, bool HasPreHammerWeapon);
// 是否触发本次 fire keydown 的锤脉冲:
// cycle 允许 + 本次是 fire 边沿(刚按下)+ 当前武器不是锤 + 未在冻结 wakeup 脉冲中
bool ShouldPulseGoresHammerOnFire(bool GoresCycleActive, bool FireJustPressed,
                                  bool CurrentWeaponIsHammer, bool FreezeWakeupActive);
```
`FreezeWakeupActive` 参数处理 **edge case 4**:冻结解冻时的 `ExternalHammerWakeup`(现状路径)优先,fire 脉冲让位——freeze wakeup 在时走现有 hammer wakeup 逻辑(`:3437`),fire 脉冲不叠加。

- [ ] **Step 3: 改 `UpdateGoresWeaponCycle` —— 入口 gate 分离 + 显式切锤 + 边沿脉冲 + 换回原武器**

**入口 gate 分离(edge case A,关键)**:现状 `:3418 GoresCycleActive = ShouldAppendGoresPrevWeapon()`,false 则 `:3419-3424` return。但有额外武器时 `ShouldAppendGoresPrevWeapon`=false → 脉冲路径(`:3437+`)根本跑不到。**必须把 fire 脉冲逻辑放在 `:3419` 的 `if(!GoresCycleActive) return` 之前执行**,或入口 gate 改用新判断(「cycle 允许脉冲」= `m_QmGoresAutoWeaponSwitch` 开 + 在线 + Gores 模块,**不检查** `HasBlockingGoresWeapon`),让有额外武器时脉冲仍跑。freeze wakeup / 换回逻辑维持原 gate。

然后:
1. **显式切锤**(不依赖 binds 的 `+prevweapon`):触发脉冲时直接 `Input.m_WantedWeapon = WEAPON_HAMMER + 1`(`=1`,参照现状 `:3440`),记录 `m_GoresPreHammerWeapon[Dummy] = m_Snap.m_pLocalCharacter->m_Weapon`(切锤前);
2. **fire 边沿**:用 `m_aPrevFireForGores[Dummy]` 检测 `Input.m_Fire` 0→1,调 `ShouldPulseGoresHammerOnFire` 决定是否脉冲;
3. **换回原武器**:脉冲锤击后(`:3453` 位置),调 `GoresRestoreWeaponAfterHammer(m_GoresPreHammerWeapon[Dummy], ...)` 换回,而非固定枪。

脉冲的 fire 状态变化(切锤瞬间给一个 fire pulse)复用现有 `QmGoresHammerWakeupFireState`(`modes.cpp:72`)机制,**对照 `:72-85` FireState/Release 语义**实现,不破坏 freeze wakeup 时序。

- [ ] **Step 4: binds/cycle 分工 —— `ShouldAppendGoresPrevWeapon` 改「有额外武器就 false」(edge case C,关键)**

现状 binds.cpp:204 用 `ShouldAppendGoresPrevWeapon()` 决定追加 `+prevweapon`;`+prevweapon`(`controls.cpp:347`)按武器槽循环(`t % NUM_WEAPONS`),只在锤枪二态切到锤——**有 3+ 武器必切错**。而现状 `ShouldAppendGoresPrevWeapon = !HasBlockingGoresWeapon = !(m_QmGoresDisableIfWeapons && 有额外武器)`,**若用户关掉 `m_QmGoresDisableIfWeapons`(=0),即使有额外武器它也 true** → binds 仍追加 `+prevweapon` → 切错。

**修正**:`ShouldAppendGoresPrevWeapon()` 改成「**有额外武器就 false**(不管 `m_QmGoresDisableIfWeapons` 开关)」——让 binds 永不在多武器追加 `+prevweapon`。即把 `ShouldAppendGoresPrevWeapon` 的 `!HasBlockingGoresWeapon()` 换成独立的「无额外武器」纯判断(不依赖 `m_QmGoresDisableIfWeapons`)。

分工结果:
- **binds**:仅锤枪二态(无额外武器)追加 `+prevweapon`(切锤正确);
- **cycle**(Step 3):有额外武器时显式 `m_WantedWeapon = WEAPON_HAMMER + 1` 切锤,不经 binds。

`m_QmGoresDisableIfWeapons` 选项语义变为「智能切换」总开关(不再"完全停用"):开=走智能(有额外武器也脉冲切锤);关=退回纯 binds 二态行为。`HasBlockingGoresWeapon`(`:3388`)保留「是否有额外武器」判断,供 Step 3 cycle 内部分支用。

- [ ] **Step 5: 改 `ShouldAppendGoresPrevWeapon` 实现(配合 Step 4)**

Step 4 把 `ShouldAppendGoresPrevWeapon` 改成「有额外武器就 false」后,binds.cpp:204 门槛行为变了(有额外武器时不再追加 `+prevweapon`,即使 `m_QmGoresDisableIfWeapons=0`)。**本 Step 改 `ShouldAppendGoresPrevWeapon` 实现**(`tclient.cpp:3400`),把 `!HasBlockingGoresWeapon()` 换成独立的「无额外武器」判断。改完 binds.cpp:204 本身不动(它调 `ShouldAppendGoresPrevWeapon`,语义随实现变)。加注释说明 binds 只负责二态切锤、多武器切锤归 cycle。

- [ ] **Step 6: 扩展 `qm_modes_test.cpp`**

为 Step 2 新纯函数加单测(正反例)。现有 `:69-102` 的 `ShouldTrigger`/`ShouldKeep`/`FireState`/`Release` 测试保持 PASS(确认没破坏现状)。

- [ ] **Step 7: 跑聚焦 + 全量 C++ 测试**

Run 聚焦:
```bash
cmake-build-release/testrunner.exe --gtest_filter=QmGoresMode.*
```
Run 全量:
```bash
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
```
Expected: 全量 PASS。

- [ ] **Step 8: 实际玩法验证(行为变更必须人工确认)**

进 Gores 模式实测:
1. 无额外武器:按 fire 切锤锤击换回枪(现状不变);
2. **有额外武器(枪/激光/炮/散弹)**:按 fire 切锤锤击一次→换回原武器;按住 fire 首锤后原武器连发;
3. 冻结时锤唤醒解冻正常;
4. `m_QmGoresDisableIfWeapons` 开关切换符合「智能」语义。

把验证结果(尤其「换回原武器」「连发不破坏」)记进汇报。不符预期则回 Step 3-5 调整。

---

### Task 4: Fix laser enhancement / round-caps preview rendering (需求3)

**背景:** 原始需求3:激光增强效果(辉光+脉冲)`QmLaserEnhanced` 和圆角端点 `QmLaserRoundCaps` 开关后,**设置页预览图不反映变化**(用户观察到「渲染没了」「没见到预览图变化」「不知生效没」)。

**现状(已读 `RenderLaser` `items.cpp:374-452`):**
- 预览调的是 `RenderLaser` 的 **374 重载**(`DoLaserPreview:1213`);
- `:378-379` `GlowScale = GlowIntensity/100`;`UseEnhancedLaserGlow = m_QmLaserEnhanced && GlowScale > 0`;
- `:391-452` enhanced 分支(13 层 glow + **`:443` 圆角端点 `m_QmLaserRoundCaps` 嵌在此分支内**);
- `:453+` else 分支(普通渲染,**不读 roundcaps**)。

**根因候选(具体,待运行最终确认):**
1. **圆角端点单独开关无效(最可能)**:`m_QmLaserRoundCaps`(`:443`)绘制嵌在 `if(UseEnhancedLaserGlow)`(`:391`)内 → 未开增强时走 else(`:453`),**圆角不绘制**。解释用户「开关圆角顶点没见到变化」。
2. **enhanced glow 依赖 `GlowScale>0`**:`UseEnhancedLaserGlow` 要 `GlowScale>0`(即 `m_QmLaserGlowIntensity>0`)。若默认 0/偏低,即使开 `m_QmLaserEnhanced` 也不渲染 glow → 解释「增强效果渲染没了」。
3. 预览传 `GlowIntensity=m_QmLaserGlowIntensity`,需确认其默认值。

**硬约束**:禁止新增 `ReadRepoFile/ReadTextFile + EXPECT_NE(Source.find(...))` 源码字面量测试(虚假安全感)。

**Files:**
- Modify: `src/game/client/components/items.cpp`(`RenderLaser`:圆角端点从 enhanced 分支提出 / 或 else 分支也支持)
- Modify: `src/game/client/components/menus.cpp`(`DoLaserPreview`)—— 若需调 GlowIntensity
- Read-only: `src/engine/shared/config_variables_qmclient.h`(确认 `QmLaserGlowIntensity` 默认值)
- Modify: `src/test/qmclient_monitoring_test.cpp` 或新增运行时测试

- [ ] **Step 1: 确认 `QmLaserGlowIntensity` 默认值 + 复现三个条件**

查 `config_variables_qmclient.h` 的 `QmLaserGlowIntensity` 默认值。若默认 0,则根因 2 成立(`GlowScale=0` → enhanced 不渲染)。实际运行分别测:① 只开 roundcaps(不开 enhanced)② 只开 enhanced ③ 都开,看预览哪个不生效。把结果记进汇报,锁定根因 1/2/3(或组合)。

- [ ] **Step 2: 定位根因**

基于 Step 1 确认:
- 根因 1(圆角嵌在 enhanced 分支)→ Step 3 把圆角端点绘制提到 enhanced 分支外;
- 根因 2(`GlowScale=0`)→ Step 3 确保预览/默认 `GlowIntensity>0`,或调整 `UseEnhancedLaserGlow` 条件;
- 根因 3 → 调 `DoLaserPreview` 传参。

- [ ] **Step 3: 修复 —— 让 enhanced 与 roundcaps 都能在预览独立生效**

按 Step 2 根因修复。最可能:**在普通渲染 else 分支(`:453+`)也实现圆角端点绘制**——注意 else 分支结构与 enhanced 不同(无 13 层循环,走自己的 Quad 绘制路径,需按其结构在激光两端各画一个 `DrawCircle`),使 roundcaps 不依赖 enhanced;enhanced 分支的圆角(`:443`)保留。同时确保 enhanced 开启时 `GlowScale>0`(预览传有效 GlowIntensity,或默认值合理)。把最终改法记进汇报。

- [ ] **Step 4: 加运行时测试(非源码字面量)**

参考 `qmclient_monitoring_test.cpp` 运行时模式:构造激光渲染调用 → 切换 `m_QmLaserEnhanced`/`m_QmLaserRoundCaps` → 断言渲染调用次数/状态/绘制分支变化(而非断言源码含某字符串)。

- [ ] **Step 5: 跑聚焦 + 全量 C++ 测试 + 实际验证**

Run 聚焦:
```bash
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.*
cmake-build-release/testrunner.exe --gtest_filter=QmNewUiMenuBranches.*
```
Run 全量:
```bash
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
```
实际验证:设置页切换两个开关,预览图实时变化(enhanced 辉光/脉冲、roundcaps 圆角)。把验证结果记进汇报。

---

### Task 5: Final verification across all tracks(收口,不留空 gap)

**Files:** None. 本 Task 是全 plan 提交前的统一收口验证。

- [ ] **i18n 链**(Task 1/2):`extract_strings.py` → `generate_all.py` → `validate.py` → `review_duplicate_entries.py`。确认 Task1 放宽门槛后报错都是该翻的、Task2 简繁中钩子/强钩弱钩/自动切锤已统一。
- [ ] **C++ 武器逻辑**(Task 3):跑全量 `qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14`;**必须**完成 Task3 Step8 实际玩法验证(有额外武器按 fire 切锤换回原武器、连发不破坏)。
- [ ] **激光预览**(Task 4):**必须**完成 Task4 Step5 视觉验证(切换 enhanced/roundcaps 开关预览实时变化)——这是 bug 修复,光跑测试不够,要看实际渲染。
- [ ] 跑 `python qmclient_scripts/gate/check_gate.py --mode quick`(环境允许补到 `--mode default`)。
- [ ] 若某项验证本会话无法完成(视觉/玩法验证需手动看 client),**显式写成 gap**,不要说"测试通过"。gap 写清:缺什么验证、为什么缺、建议何时补。

---

### Task 6: Converge HTTP translation defaults and concurrency

**背景:** 现有 `translate_with_local_http.py` 的 `--base-url/--model/--api-key` 默认全空且为**必填**(`:1255-1258`, `:1296-1298`),`--parallel-requests/--parallel-languages` 默认 1(`:1263-1264`)。`resolve_api_key`(`:374-382`)已有 `if "deepseek" in base_url` 分支,DeepSeek 支持是现成的。`deepseek-v4-flash` 经确认是真实可用模型,直接作默认。

> 这些 Task(6-9)只动 Python TOML 工具链、prompt 资产与翻译流程,**不改任何 C++ 玩法或渲染代码**。

**Files:**
- Modify: `qmclient_scripts/languages_qmclient/translate_with_local_http.py`

- [ ] **Step 1: 顶部加默认常量**

```python
DEFAULT_BASE_URL = "https://api.deepseek.com"
DEFAULT_MODEL = "deepseek-v4-flash"
DEFAULT_API_KEY_ENV = "DEEPSEEK_API_KEY"  # 命名带 provider 前缀,避免与多 provider 冲突
```

- [ ] **Step 2: 接入 argparse,并删掉旧的"base-url/model 必填"校验**

```python
parser.add_argument("--base-url", default=DEFAULT_BASE_URL)
parser.add_argument("--model", default=DEFAULT_MODEL)
parser.add_argument("--api-key", default=os.getenv(DEFAULT_API_KEY_ENV, ""))
```

同步移除 `:1296-1298` 那条 `--base-url and --model are required unless --write-back` 的强制校验(默认值不再为空);保留 `--write-back` 不需要 key 的豁免逻辑。启动时打印生效的 base-url/model/并发值。

- [ ] **Step 3: 加 CPU 感知的并发默认值**

```python
_CPU_COUNT = os.cpu_count() or 4
DEFAULT_PARALLEL_LANGUAGES = min(MAX_PARALLEL_LANGUAGES, max(1, _CPU_COUNT - 2))  # 上限沿用现有 MAX_PARALLEL_LANGUAGES=12
DEFAULT_PARALLEL_REQUESTS = 4
```

`--parallel-requests/--parallel-languages` 的 `default=` 改用上面两个常量。

- [ ] **Step 4: 加 rate-limit 友好的 retry/backoff(提示项)**

`note.md` 记录过 DeepSeek API 429(300min/800req)。在 HTTP 请求路径加最小 retry:遇 429/5xx 指数退避重试 N 次(可用 `tenacity` 或手写)。若不想引入依赖,至少捕获 429 并打印明确提示 + 退出码非 0,**不要静默吞掉**。把最终取舍记进 Task 汇报。

- [ ] **Step 5: 更新 `--help` 与文档**

确认 `--help` 显示新默认值与 `DEEPSEEK_API_KEY` 环境变量。Task 9 末尾统一更新 README/CLAUDE.md。

---

### Task 7: Extend terminology glossary and wire it into the prompt

**背景:** `terminology.toml` 现有 Clan/Dummy/Server/Map/Tencent Cloud/Lyrics/HUD/cache/source/threshold/px/ms,**缺**核心游戏词 hook/hammer/laser/grenade/dummy(已有)/skin/clan(已有)/tee/nameplate。本 Task 扩表 + 接 prompt。**不含自动 back-fix**(那是 Task 8 的事,且只生成 review 报告,不自动改)。

> 2026-06-26 修正：DDNet/core 术语的简中列必须以官方 DDNet 简中为基线，而不是“现有 `translations/i18n/*.toml` 里最常见的译法”或 `.context/game_terms.txt`。已确认的官方差异包括 `Hook=钩索`、`Hook collision line=钩索辅助线`、`Grenade=榴弹枪`。QmClient 专属术语没有官方 source key 时，才使用项目内上下文人工定名。

**Files:**
- Modify: `qmclient_scripts/languages_qmclient/prompt_assets/terminology.toml`
- Modify: `qmclient_scripts/languages_qmclient/prompt_assets/user_prompt.txt`
- Modify: `qmclient_scripts/languages_qmclient/translate_with_local_http.py`
- Reference: `qmclient_scripts/languages_qmclient/.context/game_terms.txt`

- [ ] **Step 1: 扩展术语表覆盖核心游戏词 + 补 `Zhipu AI` 品牌本地化**

(a) 游戏核心词:为 `hook`(已在 Task 2 加)、`hammer`、`laser`、`grenade`、`skin`、`tee`、`nameplate` 补 `[[term]]`。每种语言的取值**优先取现有 `translations/i18n/*.toml` 里最常见的译法**(用 grep 统计),不要凭空造。把统计依据记进 Task 汇报。

(b) 新增品牌条目 **`Zhipu AI`**:简中=「智谱清言」、繁中=「智譜清言」,其余语言保留 "Zhipu AI"。**取值依据**:`Zhipu` 本身是拼音,中文场景用品牌官方中文名更自然;其他语言无惯用本地名,原样保留。

(c) **不要**给 `OpenAI`/`DeepSeek` 加术语表条目——它们全球惯用英文、无本地名,翻译器原样保留即可(与 Task 1 Step 3 保留这两个纯 token 的决定一致)。

**取值总原则(写进 prompt 或汇报)**:术语看"目标语言使用者的常用习惯"——有官方/惯用本地名的(如 `Tencent Cloud→腾讯云`、`Zhipu AI→智谱清言`)用本地名;没有的(如 `OpenAI`/`DeepSeek`/多数语言里的 `Zhipu AI`)保留英文。`terminology.toml:61-74` 的 `Tencent Cloud` 条目就是现成的正确范例。

- [ ] **Step 2: prompt 接入术语表**

在 `user_prompt.txt` 加 `{terminology}` 占位符;`translate_with_local_http.py` 在构造 prompt 时,按目标语言把 `terminology.toml` 对应列渲染进去。

- [ ] **Step 3: 跑一次 dry-run 确认 prompt 正确渲染**

Run: `python qmclient_scripts/languages_qmclient/translate_with_local_http.py --languages korean --dry-run --limit 1`
Expected: 输出的 prompt 含渲染后的韩文术语表,无 `{terminology}` 残留。

---

### Task 8: Incremental translation quality checks (staged warning→error)

**背景:** 原计划一次性上 5 类新检查(placeholder/terminology/length/CJK-style)且术语检查当 error,会让现有 12 语言 × 多术语瞬间产生海量 error、CI 必红。本 Task **分阶段**:新检查先全部 **warning**,数据收敛 + 人工确认后再逐条升 error。

**Files:**
- Modify: `qmclient_scripts/languages_qmclient/i18n_store.py`
- Modify: `qmclient_scripts/languages_qmclient/tests/test_translations_toml.py`

- [ ] **Step 1: 给 `translation_quality_errors` 增加 warning 通道**

把函数拆成返回 `(errors, warnings)` 或加一个 `strict=False` 参数;`warnings` 不影响退出码。保持现有 error 行为不变。

- [ ] **Step 2: 新增检查(初始全部 warning)**

- Placeholder 序列检查:`%s`/`%d` 顺序与数量、`{name}` 占位符与 source 不一致 → warning。
- 术语命中检查:翻译未用 glossary 目标词 → warning(初期)。
- 术语一致性:同一 source term 在模块内译法不一 → warning;跨模块 → warning。
- 长度风险:翻译 >2.5× source(警告)、>3× 且 source 长度>10(warning,初期不升 error)。
- CJK 风格:"你/您"混用、"的地得"、中英标点混排 → warning。

- [ ] **Step 3: 为每类新检查加单元测试**(warning 通道也要测)。

- [ ] **Step 4: 跑全量校验,把 warning 汇总成 review 报告**

Run: `python qmclient_scripts/languages_qmclient/validate.py`(确认现有 error 项不增),并人工审阅新 warning 报告,判断哪些该修、哪些升 error。报告写进 Task 汇报,**不自动改翻译**。

---

### Task 9: validate_translations.py entry point + draft cleanup + CI

**Files:**
- Create: `qmclient_scripts/languages_qmclient/validate_translations.py`
- Modify: `qmclient_scripts/languages_qmclient/i18n_store.py`
- Modify: `qmclient_scripts/languages_qmclient/translate_with_local_http.py`
- Create or modify: `.github/workflows/i18n-check.yml`
- Modify: `qmclient_scripts/languages_qmclient/README.md` 或项目 `CLAUDE.md`

#### Step 9.1: 新建 validate_translations.py

- [ ] 加载 `translations/i18n/*.toml` 与 `prompt_assets/terminology.toml`,跑 `translation_quality_errors`(含 Task 8 的 warnings)。
- [ ] 打印人类可读报告;支持 `--json report.json` 与 `--languages lang1 lang2 ...`。
- [ ] 有 hard error 时退出码非 0;warning 不影响退出码。

```bash
python qmclient_scripts/languages_qmclient/validate_translations.py
python qmclient_scripts/languages_qmclient/validate_translations.py --json report.json
python qmclient_scripts/languages_qmclient/validate_translations.py --languages simplified_chinese korean
```

#### Step 9.2: draft 清理

- [ ] 扩展 `i18n_store.py:prune_written_draft_module`,删空 draft 文件与空语言目录。
- [ ] 给 `translate_with_local_http.py` 加 `--clean-drafts`(扫 draft、删已全部回填的、打印清理报告)与 `--auto-clean`(增量翻译→校验→通过则回填→再清空;校验失败则保留 draft 并退出非 0)。
- [ ] 清理报告样例:
```text
Draft cleanup report:
  removed: translations_draft/korean/menus.toml (all entries written back)
  removed empty dir: translations_draft/korean/
  kept: translations_draft/japanese/qmclient.toml (3 entries still pending validation)
```

#### Step 9.3: 接 CI(修正 requirements.txt 不存在的问题)

- [ ] 先确认依赖:`local_http_client.py` 是否引入第三方库(requests 等)。看脚本顶部 import 决定:
  - 若**无第三方依赖**(纯标准库 + tomllib):CI 里**删掉** `pip install -r requirements.txt` 这行,直接跑校验。
  - 若**有依赖**:先创建 `requirements.txt` 列出依赖,再 install。
- [ ] CI 跑校验**前**必须先刷新 active keys(否则用旧集合):`extract_strings.py` → `generate_all.py` → `validate_translations.py`。
- [ ] workflow 触发 `paths: src/**` + `qmclient_scripts/languages_qmclient/**` 合理(新增英文字符串在 src 里)。

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
      # 若 9.3 确认有第三方依赖,这里才 install;否则省略
      - run: |
          cd qmclient_scripts/languages_qmclient
          python extract_strings.py
          python generate_all.py
          python validate_translations.py
```

#### Step 9.4: 验证自动化链

- [ ] 全链:
```bash
cd qmclient_scripts/languages_qmclient
python extract_strings.py
python generate_all.py
python validate.py
python validate_translations.py
python review_duplicate_entries.py --show-groups 0 --show-unused 0
python translate_with_local_http.py --clean-drafts
```
- [ ] Python 单测:`python -m unittest discover -s tests -v`。
- [ ] 确认 `--help` 显示 Task 6 新默认值与 `DEEPSEEK_API_KEY`。
- [ ] 人工造一个空 draft 文件,确认 `--clean-drafts` 删它及其空父目录。
- [ ] 更新 README/CLAUDE.md:新默认值、`validate_translations.py` 命令、`--auto-clean` 与 `--clean-drafts` 区别。

---

## Appendix A: Core terminology reference (extracted from existing translations)

> hook 简繁中已统一为 钩子/鉤子(见 Task 2)。其余语言取自现有数据,Task 7 扩表时以 grep 统计的最常见译法为准。

| English | 简中 | 繁中 | 日文 | 韩文 | 俄文 | 德文 |
|---|---|---|---|---|---|---|
| Hook | 钩子 | 鉤子 | フック | 갈고리 | Крюк | Haken |
| Hammer | 锤子 | 錘子 | ハンマー | 핸머 | Молот | Hammer |
| Laser | 激光 | 雷射 | レーザー | 레이저 | Лазер | Laser |
| Grenade | 榴弹 | 榴彈 | グレネード | 유탄 | Граната | Granate |
| Dummy | 分身 | 分身 | ダミー | 더미 | Дамми | Dummy |
| Skin | 皮肤 | 皮膚 | スキン | 스킨 | скин | Skin |
| Clan | 战队 | 戰隊 | クラン | 클랜 | Клан | Clan |
| Tee | 玩家 | 玩家 | Tee | 티 | Игрок | Tee |

> ⚠️ 2026-06-26 修正：本 Appendix A 来自现有 QmClient 数据抽取，不能作为简中官方术语依据。DDNet/core 简中术语以 `docs/superpowers/explore/2026-06-26-ddnet-official-simplified-chinese-terminology.md` 为准；`.context/game_terms.txt` 仅可作为待审计候选清单。

完整跨语言候选参考见 `qmclient_scripts/languages_qmclient/.context/game_terms.txt`，但不能替代官方 DDNet 简中证据。

---

## Appendix B: Quick command reference

```bash
# 增量翻译(DeepSeek 默认,DEEPSEEK_API_KEY 环境变量)
python qmclient_scripts/languages_qmclient/translate_with_local_http.py

# 校验已提交翻译
python qmclient_scripts/languages_qmclient/validate_translations.py

# 删已全部回填的 draft
python qmclient_scripts/languages_qmclient/translate_with_local_http.py --clean-drafts

# 生成→校验→回填→清理 一条龙
python qmclient_scripts/languages_qmclient/translate_with_local_http.py --auto-clean

# Python 单测
python -m unittest discover -s qmclient_scripts/languages_qmclient/tests -v

# C++ 全量测试(提交前必跑,过滤测试只用于红绿灯/定位)
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14

# gate
python qmclient_scripts/gate/check_gate.py --mode quick
```
