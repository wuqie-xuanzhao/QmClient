# QmClient 设置页 UI 统一 P5 标准设置页迁移 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不重做 P1–P4 公共 primitive 的前提下，将 General、Player、Tee、Graphics、Sound、DDNet、Appearance、Controls 八个标准设置页完整迁移到统一 page layout、card、deck、input、numeric 与 scroll 契约，并逐页删除旧实现路径。

**Architecture:** P5 只做页面适配：页面入口先由 `ResolveSettingsPageLayout(...)` 产生 `SSettingsPageLayoutFrame`，再提交 `std::vector<SSettingsCardDefinition>` 给 `CSettingsCardDeck::Render(...)`；只有 Deck 内部可调用 `SettingsCard(...)` 并产生 canonical `SSettingsCardFrame`，页面不直接绘制或持有 frame。P3 已完成输入/数值清退，P5 只把既有 `ui_widget::InputField(...)` / `ui_widget::NumericField(...)` 调用原样移入 definition content callback 并用结构 gate 防止旧路径回归。每页保留一个 P4 `CScrollRegion` adapter，其唯一滚动状态来自 `CScrollRegion::State()` 返回的 `CQmScrollState`，policy 只由 `QmResolveScrollPolicy(...)` 解析。全局 `QmCardRegistry` 继续作为 stable ID、metadata 和默认 placement 事实源，全局 model 提供当前搜索导航 tab；每个页面切片必须在同一个提交中完成 registry/导航、行为测试、结构删除检查和页面实现。

**Tech Stack:** C++17、QmUi P1–P4 公共接口、GoogleTest、Python 3 结构清单、CMake/Ninja/MSVC、仓库 gate。

## Global Constraints

- 权威规格只有 `docs/superpowers/specs/2026-07-10-QmClient-设置页UI统一与滚动体系规格.md`；归档规格和旧截图不作为实现依据。
- P1–P4 已完成是硬前置。开始 P5 前必须确认 `ResolveSettingsPageLayout(...)`、`SSettingsPageLayoutFrame`、`SSettingsCardDefinition`、`CSettingsCardDeck::Render(...)`、`SSettingsCardDeckResult`、`ui_widget::InputField(...)`、`ui_widget::NumericField(...)`、`CQmScrollState`、`QmResolveScrollPolicy(...)` 均已存在且测试通过；缺任一项就停止 P5，回到对应前置阶段收口。
- 不新增页面 wrapper，不复制 P1–P4 的 layout/card/deck/input/numeric/scroll 逻辑，不保留“公共 wrapper + 页面旧实现”双路径。
- P5 只迁移 General、Player、Tee、Graphics、Sound、DDNet、Appearance、Controls；QmClient、TClient、非卡片菜单和 R1–R3 均不进入本计划。
- Graphics 是 P1/P2 试点：保留其既有 stable ID、card placement、跨列拖拽与搜索接入，本计划只补齐剩余 input、numeric、scroll 和视觉验收，不重建 card/deck。
- Appearance 必须删除私有 `BeginAppearanceCard`；不得把它改名或包到另一层 lambda。
- Controls 垃圾桶继续是独立 destructive action button；不得塞进 `ui_widget::InputField(...)` 的清除 affordance。
- 每页新增或核对 registry stable ID、默认 tab/column/order、搜索元数据和导航目标；页面切换、搜索跳转和持久化顺序必须有行为证据。
- 每页必须先写失败测试，再写最小实现，再跑绿灯；过滤测试只用于红绿循环，最终必须跑全量 `run_cxx_tests`。
- 不改变协议、物理、预测、碰撞、地图、Demo/skin/配置格式、回放或 rank 语义；Tee 的资源调度、preview cache 与性能 telemetry 只做保持性适配。
- C++ 注释使用中文；保持原文件 UTF-8、BOM、CRLF/LF、Tab/空格风格；临时日志只能写入 `tmp/`。
- 同一个 `cmake-build-release` 中的 `testrunner`、focused tests、`game-client`、`run_cxx_tests`、`run_rust_tests`、`package_default` 必须串行，禁止并行发起。
- 每个任务的 commit 只暂存本任务文件；工作区中的其他改动按用户或脚本所有处理，不回退、不顺手格式化。

---

## File / Interface Map

| 文件 | P5 职责 |
|---|---|
| `src/game/client/components/menus_settings.cpp` | General 680–901、TeeIdentity 971–1037、Player 1039–1215、Tee 1224–3025、Graphics 3026–3698、Sound 4357–4630、Appearance 5527–6918、DDNet 6919–7401 的页面迁移。 |
| `src/game/client/components/menus_settings_controls.cpp` | Controls `Render`、卡片分组、搜索、bind、Mouse、Controller 的页面实现。 |
| `src/game/client/components/menus_settings_controls.h` | 保留一个 `CScrollRegion` adapter，并只通过其 `State()` 使用 `CQmScrollState`；保留 dropdown 必需状态和 destructive action 状态。 |
| `src/game/client/QmUi/QmCardRegistry.cpp` | 增加 General、Player、Tee、Controls stable ID；核对 Graphics、Sound、DDNet、Appearance 已有条目。 |
| `src/game/client/components/qmclient/menus_qmclient.cpp` | 为 `general`、`player`、`tee`、`controls` 增加搜索导航 route；核对已有 Graphics/Sound/DDNet/Appearance route。 |
| `src/test/qm_card_registry_test.cpp` | 按页验证注册、默认 placement、全局模型顺序和序列化/重载持久化。 |
| `src/test/qm_new_ui_menu_branch_test.cpp` | 按页验证生产入口只使用公共契约、stable ID 与导航 route，并断言旧路径消失。 |
| `qmclient_scripts/gate/check_settings_ui_migration.py` | 以函数边界为单位执行 P5 结构清单，禁止旧 layout/card/input/scroll 调用重新进入迁移页面。 |
| `qmclient_scripts/gate/tests/test_check_settings_ui_migration.py` | 用临时源码验证结构清单的通过、缺公共接口、残留旧路径、缺 registry/导航四类结果。 |
| `qmclient_scripts/scripts_overview.md` | 登记结构清单用途、逐页用法和最终 `--all` 用法。 |

**Shared interfaces（只消费，不在 P5 修改或另造替代品）：**

- Layout：`ResolveSettingsPageLayout(...)` → `SSettingsPageLayoutFrame`
- Card/Deck：页面只生成 `SSettingsCardDefinition`并调用 `CSettingsCardDeck::Render(...)` → `SSettingsCardDeckResult`；`SettingsCard(...)` 与 `SSettingsCardFrame` 仅在 Deck 内部
- Input：`ui_widget::InputField(...)`
- Numeric：`ui_widget::NumericField(...)`
- Scroll：`CScrollRegion::State() -> CQmScrollState &` + `QmResolveScrollPolicy(...)`
- Registry/order：`qm_card_registry::Defaults()`、`qm_card_registry::FindByStableId(...)`、`qm_card_registry::BuildDefaultEntries()`、`qm_card_order::CModel::StableIdOrder(...)`

---

### Task 1: 建立可执行的 P5 结构清单

**Files:**
- Create: `qmclient_scripts/gate/check_settings_ui_migration.py`
- Create: `qmclient_scripts/gate/tests/test_check_settings_ui_migration.py`
- Modify: `qmclient_scripts/scripts_overview.md`

**Interfaces:**
- Produces: `audit_page(repo_root: Path, page: str) -> list[str]`
- Produces: CLI `python qmclient_scripts/gate/check_settings_ui_migration.py --page general`（`general/player/tee/graphics/sound/ddnet/appearance/controls` 八选一）and `--all`
- Consumes: the exact shared interface names listed in the File / Interface Map.

- [ ] **Step 1: Write the failing Python tests**

Create four tests with synthetic `menus_settings.cpp`, registry and navigation files. The test module must define the fixture explicitly so it never reads the developer's real checkout:

```python
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from qmclient_scripts.gate.check_settings_ui_migration import PAGE_STABLE_IDS, audit_page


class SettingsUiMigrationAuditTest(unittest.TestCase):
    def setUp(self):
        self.temp_dir = TemporaryDirectory()
        self.root = Path(self.temp_dir.name)

    def tearDown(self):
        self.temp_dir.cleanup()

    def make_repo(self, *, drop: str = "", add: str = "") -> Path:
        source = """void CMenus::RenderSettingsGeneral(CUIRect MainView)
{
    const SSettingsPageLayoutFrame Frame = ResolveSettingsPageLayout(MainView, false, 1.0f);
    std::vector<SSettingsCardDefinition> vCards;
    vCards.push_back({{"deck:general-game", "General", nullptr}, MeasureGeneralGame, RenderGeneralGame});
    CScrollRegion ScrollRegion;
    CQmScrollState &Scroll = ScrollRegion.State();
    QmResolveScrollPolicy(Request, 1.0f, 0.1f);
    ui_widget::NumericField(Context);
    const SSettingsCardDeckResult DeckResult = m_SettingsCardDeck.Render(Context, Frame, "general", vCards, SettingsCardOrderModel(), &ScrollRegion, Input, SettingsCardMotionSpec());
}
"""
        source = source.replace("\n}\n", f"\n    {add}\n}}\n", 1).replace(drop, "")
        files = {
            "src/game/client/components/menus_settings.cpp": source,
            "src/game/client/QmUi/QmCardRegistry.cpp": "\n".join(PAGE_STABLE_IDS["general"]),
            "src/game/client/components/qmclient/menus_qmclient.cpp": '{"general", CMenus::SETTINGS_GENERAL},',
        }
        for relative, content in files.items():
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content.replace(drop, ""), encoding="utf-8")
        return self.root

    def test_clean_page_passes(self):
        self.assertEqual(audit_page(self.make_repo(), "general"), [])

    def test_missing_public_contract_fails(self):
        errors = audit_page(self.make_repo(drop="m_SettingsCardDeck.Render("), "general")
        self.assertTrue(any("m_SettingsCardDeck.Render" in item for item in errors))

    def test_legacy_path_fails(self):
        errors = audit_page(self.make_repo(add="DoSettingsScrollbarOption("), "general")
        self.assertTrue(any("DoSettingsScrollbarOption" in item for item in errors))

    def test_missing_registry_or_navigation_fails(self):
        errors = audit_page(self.make_repo(drop="deck:general-game"), "general")
        self.assertTrue(any("registry/navigation" in item for item in errors))
```

- [ ] **Step 2: Run the tests and verify the red state**

Run: `python -m unittest qmclient_scripts.gate.tests.test_check_settings_ui_migration -v`

Expected: FAIL with `ModuleNotFoundError` because `check_settings_ui_migration.py` does not exist.

- [ ] **Step 3: Implement the manifest and function-boundary audit**

The script must define these page keys and stable IDs exactly:

```python
PAGE_STABLE_IDS = {
    "general": ("deck:general-game", "deck:general-language", "deck:general-client", "deck:general-recording"),
    "player": ("deck:player-identity", "deck:player-country"),
    "tee": ("deck:tee-identity", "deck:tee-skin-options", "deck:tee-skin-list"),
    "graphics": ("deck:graphics-display", "deck:graphics-visual", "deck:graphics-backend", "deck:graphics-modes"),
    "sound": ("deck:sound-toggle", "deck:sound-volume", "deck:sound-audio-pack"),
    "ddnet": ("deck:ddnet-demo", "deck:ddnet-gameplay", "deck:ddnet-background", "deck:ddnet-miscellaneous"),
    "appearance": (
        "deck:appearance-hud-main", "deck:appearance-hud-ddrace",
        "deck:appearance-chat-settings", "deck:appearance-chat-messages", "deck:appearance-chat-preview",
        "deck:appearance-name-plate-settings", "deck:appearance-name-plate-preview",
        "deck:appearance-hook-collision-main", "deck:appearance-hook-collision-preview",
        "deck:appearance-info-messages", "deck:appearance-laser-enhanced",
        "deck:appearance-laser-colors", "deck:appearance-laser-preview",
    ),
    "controls": (
        "deck:controls-movement", "deck:controls-weapon", "deck:controls-voting",
        "deck:controls-chat", "deck:controls-dummy", "deck:controls-miscellaneous",
        "deck:controls-custom", "deck:controls-mouse", "deck:controls-controller",
    ),
}

PAGE_FUNCTIONS = {
    "general": ("CMenus::RenderSettingsGeneral",),
    "player": ("CMenus::RenderSettingsTeeIdentity", "CMenus::RenderSettingsPlayer"),
    "tee": ("CMenus::RenderSettingsTee",),
    "graphics": ("CMenus::RenderSettingsGraphics",),
    "sound": ("CMenus::RenderSettingsSound",),
    "ddnet": ("CMenus::RenderSettingsDDNet",),
    "appearance": ("CMenus::RenderSettingsAppearance",),
    "controls": ("CMenusSettingsControls::Render",),
}

PAGE_ROUTE_TABS = {
    "general": ("general",),
    "player": ("player",),
    "tee": ("tee",),
    "graphics": ("graphics",),
    "sound": ("sound",),
    "ddnet": ("ddnet",),
    "appearance": (
        "appearance-hud", "appearance-chat", "appearance-name-plate",
        "appearance-hook-collision", "appearance-info-messages", "appearance-laser",
    ),
    "controls": ("controls",),
}

COMMON_REQUIRED = (
    "ResolveSettingsPageLayout(", "SSettingsPageLayoutFrame", "SSettingsCardDefinition",
    "m_SettingsCardDeck.Render(", "SSettingsCardDeckResult", ".State()", "CQmScrollState", "QmResolveScrollPolicy(",
)
COMMON_FORBIDDEN = (
    "SettingsCard(",
    "BeginSettingsCardDeck(", "BeginSettingsCardDeckCard(",
    "DoSettingsScrollbarOption(", "DoSettingsSliderInputField(",
    "ui_widget::TextFieldEx(", "ui_widget::SearchFieldEx(",
    "ui_widget::ClearableTextFieldEx(", "ui_widget::IconTextFieldEx(",
    "ui_widget::LegacyTextFieldEx(",
    "ui_widget::TextField(", "ui_widget::SearchField(",
    "ui_widget::ClearableTextField(", "ui_widget::IconTextField(",
    "Ui()->DoEditBox(", "Ui()->DoScrollbarH(",
)

PAGE_REQUIRED = {
    "general": ("ui_widget::NumericField(",),
    "player": ("ui_widget::InputField(",),
    "tee": ("ui_widget::InputField(", "ui_widget::NumericField("),
    "graphics": ("ui_widget::NumericField(",),
    "sound": ("ui_widget::NumericField(",),
    "ddnet": ("ui_widget::InputField(", "ui_widget::NumericField("),
    "appearance": ("ui_widget::NumericField(",),
    "controls": ("ui_widget::InputField(", "ui_widget::NumericField("),
}

PAGE_FORBIDDEN = {
    "graphics": ("s_GraphicsSettingsScrollRegion",),
    "sound": ("s_SoundSettingsScrollRegion",),
    "ddnet": ("s_DDNetSettingsScrollRegion",),
    "appearance": (
        "BeginAppearanceCard", "s_ChatSettingsScrollRegion",
        "s_NamePlateSettingsScrollRegion", "s_LaserSettingsScrollRegion",
    ),
    "controls": ("RenderSettingsBlock",),
}
```

`audit_page(...)` must extract only the `PAGE_FUNCTIONS` bodies with a brace-aware scanner, combine them, require `COMMON_REQUIRED + PAGE_REQUIRED[page]`, reject `COMMON_FORBIDDEN + PAGE_FORBIDDEN.get(page, ())`, then verify every stable ID in `QmCardRegistry.cpp` and every `PAGE_ROUTE_TABS` token in `menus_qmclient.cpp`. `COMMON_FORBIDDEN` 中的 `SettingsCard(` 不会命中 `m_SettingsCardDeck.Render(`；该断言专门防止页面绕过 Deck 直调 shell。Pages without a standalone text field must not add a dummy `InputField` call merely to satisfy the inventory. Exit code is 0 only when no error exists; every error prints `page: token: reason`.

- [ ] **Step 4: Run the unit tests and script baseline**

Run: `python -m unittest qmclient_scripts.gate.tests.test_check_settings_ui_migration -v`

Expected: PASS, four tests passed.

Run: `python qmclient_scripts/gate/check_settings_ui_migration.py --all`

Expected before page migration: non-zero, with a separate error group for every page that still uses old paths; this is the intentional P5 baseline red state.

- [ ] **Step 5: Document and commit the harness**

Add the exact `--page` and `--all` commands to `qmclient_scripts/scripts_overview.md`, then run:

```powershell
python qmclient_scripts/gate/check_docs.py
git add qmclient_scripts/gate/check_settings_ui_migration.py qmclient_scripts/gate/tests/test_check_settings_ui_migration.py qmclient_scripts/scripts_overview.md
git commit -m "test(settings): 增加标准设置页迁移结构清单"
```

Expected: docs check PASS; one focused harness commit, with no page implementation changes.

---

### Task 2: 迁移 General 页面

**Files:**
- Modify: `src/game/client/components/menus_settings.cpp:680-901`
- Modify: `src/game/client/QmUi/QmCardRegistry.cpp:72-101`
- Modify: `src/game/client/components/qmclient/menus_qmclient.cpp:115-127`
- Modify: `src/test/qm_card_registry_test.cpp`
- Modify: `src/test/qm_new_ui_menu_branch_test.cpp`

**Interfaces:**
- Consumes: shared layout/card/deck/numeric/scroll interfaces; General uses `ui_widget::NumericField(...)` for update rate and retention limits and does not invent a text field.
- Produces: stable IDs `deck:general-game`, `deck:general-language`, `deck:general-client`, `deck:general-recording`; navigation tab `general -> CMenus::SETTINGS_GENERAL`.
- Produces test helper: `RegistryModelAfterRoundTrip() -> qm_card_order::CModel` for all later page-placement tests.

- [ ] **Step 1: Write the failing behavior and deletion tests**

Add the reusable round-trip helper, append the four IDs to `CoversCurrentSettingsDeckIds`, and add a registry/model test using the reloaded model:

```cpp
static qm_card_order::CModel RegistryModelAfterRoundTrip()
{
	const std::vector<qm_card_order::SEntry> Defaults = qm_card_registry::BuildDefaultEntries();
	qm_card_order::CModel Source;
	Source.SetEntries(Defaults);
	char aSerialized[32768];
	EXPECT_TRUE(Source.Serialize(aSerialized, sizeof(aSerialized)));
	qm_card_order::CModel Reloaded;
	EXPECT_TRUE(Reloaded.LoadMerged(aSerialized, Defaults));
	return Reloaded;
}

const qm_card_order::CModel Model = RegistryModelAfterRoundTrip();
EXPECT_EQ(Model.StableIdOrder("deck:", "general", 1),
	(std::vector<std::string>{"deck:general-game", "deck:general-client"}));
EXPECT_EQ(Model.StableIdOrder("deck:", "general", 2),
	(std::vector<std::string>{"deck:general-language", "deck:general-recording"}));
```

Add `QmNewUiMenuBranches.GeneralStandardPageUsesUnifiedSettingsStack` asserting the General function and route contain the required common tokens/stable IDs and do not contain the common forbidden tokens. Extend the registry test by checking all four IDs individually plus the existing no-duplicate invariant; do not assert a global registry total.

- [ ] **Step 2: Run the focused tests and verify red**

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.GeneralStandardPageCardsPersistInVisualOrder:QmNewUiMenuBranches.GeneralStandardPageUsesUnifiedSettingsStack
```

Expected: FAIL because General registry entries, route and public stack calls are absent.

- [ ] **Step 3: Implement the minimal General migration**

Keep current behavior and split content into the four declared definitions. Top-level width, columns, insets and overflow come only from `SSettingsPageLayoutFrame`; each definition renders only inside the content rect supplied by `CSettingsCardDeck::Render(...)`. Preserve the P3 `ui_widget::NumericField(...)` calls, including the already-unified `SCROLLBAR_OPTION_DELAYUPDATE` commit policy, and keep file/directory actions as buttons. Use one outer `CScrollRegion` adapter, its `State()` returning the only `CQmScrollState`, and a `QmResolveScrollPolicy(...)` settings-page policy; nested Language list keeps the P4 list adapter.

Add these exact registry/navigation rows:

```cpp
{"deck:general-game", "general", ECardColumn::Left, 0, "Game", "general game camera weapon"},
{"deck:general-language", "general", ECardColumn::Right, 0, "Language", "general language localization"},
{"deck:general-client", "general", ECardColumn::Left, 1, "Client", "general client theme files"},
{"deck:general-recording", "general", ECardColumn::Right, 1, "Demo", "general demo screenshot csv recording"},
{"general", CMenus::SETTINGS_GENERAL},
```

- [ ] **Step 4: Run green tests and page inventory**

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.GeneralStandardPageCardsPersistInVisualOrder:QmNewUiMenuBranches.GeneralStandardPageUsesUnifiedSettingsStack
python qmclient_scripts/gate/check_settings_ui_migration.py --page general
```

Expected: both commands PASS; inventory reports `general: clean`.

- [ ] **Step 5: Commit the General slice**

```powershell
git add src/game/client/components/menus_settings.cpp src/game/client/QmUi/QmCardRegistry.cpp src/game/client/components/qmclient/menus_qmclient.cpp src/test/qm_card_registry_test.cpp src/test/qm_new_ui_menu_branch_test.cpp
git commit -m "refactor(settings): 迁移 General 标准设置页"
```

---

### Task 3: 迁移 Player 页面

**Files:**
- Modify: `src/game/client/components/menus_settings.cpp:971-1215`
- Modify: `src/game/client/QmUi/QmCardRegistry.cpp:72-105`
- Modify: `src/game/client/components/qmclient/menus_qmclient.cpp:115-131`
- Modify: `src/test/qm_card_registry_test.cpp`
- Modify: `src/test/qm_new_ui_menu_branch_test.cpp`

**Interfaces:**
- Consumes: `ResolveSettingsPageLayout(...)`, `SSettingsCardDefinition`, `CSettingsCardDeck::Render(...)`, `ui_widget::InputField(...)`, `CQmScrollState`, `QmResolveScrollPolicy(...)`.
- Produces: `deck:player-identity` (Left/0), `deck:player-country` (Right/0); navigation tab `player -> CMenus::SETTINGS_PLAYER`.

- [ ] **Step 1: Write failing Player behavior and deletion tests**

```cpp
const qm_card_order::CModel Model = RegistryModelAfterRoundTrip();
EXPECT_EQ(Model.StableIdOrder("deck:", "player", 1),
	(std::vector<std::string>{"deck:player-identity"}));
EXPECT_EQ(Model.StableIdOrder("deck:", "player", 2),
	(std::vector<std::string>{"deck:player-country"}));
```

Append both Player IDs to `CoversCurrentSettingsDeckIds`. The structure test must cover both `RenderSettingsTeeIdentity` and `RenderSettingsPlayer`, require `ui_widget::InputField(...)`, require route `player`, and reject `ui_widget::TextField(...)` / `ui_widget::SearchField(...)`. Check the two IDs and uniqueness semantically; do not update or introduce a fixed total.

- [ ] **Step 2: Verify the focused red state**

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.PlayerStandardPageCardsPersistInVisualOrder:QmNewUiMenuBranches.PlayerStandardPageUsesUnifiedSettingsStack
```

Expected: FAIL on missing Player registry/route and legacy text/search fields.

- [ ] **Step 3: Implement the minimal Player migration**

Keep Player/Dummy tabs full width and outside the card grid. Move name/clan/country controls into the identity definition and the filter/grid into the country definition. Preserve the P3 `ui_widget::InputField(...)` calls for name, clan and flag search; retain `CListBox` only as the P4 list adapter inside the card viewport. Entry animation must not change card hit or drag rects.

Add these exact registry/navigation rows:

```cpp
{"deck:player-identity", "player", ECardColumn::Left, 0, "Player", "player dummy name clan identity"},
{"deck:player-country", "player", ECardColumn::Right, 0, "Choose country flag", "player dummy country flag"},
{"player", CMenus::SETTINGS_PLAYER},
```

- [ ] **Step 4: Run green tests and inventory**

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.PlayerStandardPageCardsPersistInVisualOrder:QmNewUiMenuBranches.PlayerStandardPageUsesUnifiedSettingsStack
python qmclient_scripts/gate/check_settings_ui_migration.py --page player
```

Expected: PASS; Player/Dummy selection and `SetNeedSendInfo(...)` behavior remain covered.

- [ ] **Step 5: Commit the Player slice**

```powershell
git add src/game/client/components/menus_settings.cpp src/game/client/QmUi/QmCardRegistry.cpp src/game/client/components/qmclient/menus_qmclient.cpp src/test/qm_card_registry_test.cpp src/test/qm_new_ui_menu_branch_test.cpp
git commit -m "refactor(settings): 迁移 Player 标准设置页"
```

---

### Task 4: 迁移 Tee 页面

**Files:**
- Modify: `src/game/client/components/menus_settings.cpp:1224-3025`
- Modify: `src/game/client/QmUi/QmCardRegistry.cpp:72-108`
- Modify: `src/game/client/components/qmclient/menus_qmclient.cpp:115-134`
- Modify: `src/test/qm_card_registry_test.cpp`
- Modify: `src/test/qm_new_ui_menu_branch_test.cpp`

**Interfaces:**
- Consumes: shared layout/card/deck/input/numeric/scroll interfaces; Tee skin list continues to consume existing cache/resource scheduling APIs unchanged.
- Produces: `deck:tee-identity` (Full/0), `deck:tee-skin-options` (Left/0), `deck:tee-skin-list` (Full/1); navigation tab `tee -> CMenus::SETTINGS_TEE`.

- [ ] **Step 1: Write failing Tee behavior and deletion tests**

```cpp
const qm_card_order::CModel Model = RegistryModelAfterRoundTrip();
EXPECT_EQ(Model.StableIdOrder("deck:", "tee", 0),
	(std::vector<std::string>{"deck:tee-identity", "deck:tee-skin-list"}));
EXPECT_EQ(Model.StableIdOrder("deck:", "tee", 1),
	(std::vector<std::string>{"deck:tee-skin-options"}));
```

Append all three Tee IDs to `CoversCurrentSettingsDeckIds`. Add a source test requiring the three IDs, unified stack, `ui_widget::InputField(...)` and `ui_widget::NumericField(...)`, while preserving calls that update visible skin/resource telemetry. Check the IDs and uniqueness semantically; do not introduce a fixed total.

- [ ] **Step 2: Verify the focused red state**

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.TeeStandardPageCardsPersistInVisualOrder:QmNewUiMenuBranches.TeeStandardPageUsesUnifiedSettingsStack
```

Expected: FAIL on missing Tee registry/route and legacy skin-prefix/search input.

- [ ] **Step 3: Implement the minimal Tee migration**

Keep Player/Dummy/Profiles tabs full width; Profiles continues to dispatch to its existing page and is not card-wrapped by P5. Use the identity definition for preview/identity, the options definition for download/custom-color/eyes controls, and the full-width list definition for skin filter/list. Preserve the P3 `ui_widget::InputField(...)` / `ui_widget::NumericField(...)` paths while moving them into content callbacks; retain current list virtualization, background request budgets, preview cache, `SetSettingsTeeVisibleSnapshot` and perf logging exactly.

Add these exact registry/navigation rows:

```cpp
{"deck:tee-identity", "tee", ECardColumn::Full, 0, "Player", "tee player dummy identity preview"},
{"deck:tee-skin-options", "tee", ECardColumn::Left, 0, "Skin", "tee skin options colors eyes"},
{"deck:tee-skin-list", "tee", ECardColumn::Full, 1, "Search", "tee skins search filter list"},
{"tee", CMenus::SETTINGS_TEE},
```

- [ ] **Step 4: Run green tests and Tee regression filters**

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.TeeStandardPageCardsPersistInVisualOrder:QmNewUiMenuBranches.TeeStandardPageUsesUnifiedSettingsStack:SettingsResourceJobs.*:SettingsSkinPreviewCache.*
python qmclient_scripts/gate/check_settings_ui_migration.py --page tee
```

Expected: PASS; no cache/resource behavior regression and inventory reports `tee: clean`.

- [ ] **Step 5: Commit the Tee slice**

```powershell
git add src/game/client/components/menus_settings.cpp src/game/client/QmUi/QmCardRegistry.cpp src/game/client/components/qmclient/menus_qmclient.cpp src/test/qm_card_registry_test.cpp src/test/qm_new_ui_menu_branch_test.cpp
git commit -m "refactor(settings): 迁移 Tee 标准设置页"
```

---

### Task 5: 补齐 Graphics 试点的 input 与 scroll

**Files:**
- Modify: `src/game/client/components/menus_settings.cpp:3026-3698`
- Modify: `src/test/qm_card_registry_test.cpp`
- Modify: `src/test/qm_new_ui_menu_branch_test.cpp`

**Interfaces:**
- Consumes: the existing P1/P2 Graphics `SSettingsCardDefinition` / `CSettingsCardDeck::Render(...)` slice; adds only `CQmScrollState` and `QmResolveScrollPolicy(...)` page adaptation. NumericField remains P3-owned.
- Preserves: `deck:graphics-display`, `deck:graphics-visual`, `deck:graphics-backend`, `deck:graphics-modes` and `graphics -> CMenus::SETTINGS_GRAPHICS`.

- [ ] **Step 1: Write failing completion tests**

Add `QmCardRegistry.GraphicsPilotPlacementSurvivesSerialization` for the four existing IDs and `QmNewUiMenuBranches.GraphicsPilotHasNoRemainingLegacyInputOrScrollPath`. The latter must reject `BeginSettingsCardDeck`, `BeginSettingsCardDeckCard`, `DoScrollbarH`, old settings scrollbar wrappers and outer `s_GraphicsSettingsScrollRegion`, while requiring `NumericField`, `CQmScrollState`, and `QmResolveScrollPolicy`; P4 dropdown scroll adapters remain allowed.

- [ ] **Step 2: Verify the focused red state**

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.GraphicsPilotPlacementSurvivesSerialization:QmNewUiMenuBranches.GraphicsPilotHasNoRemainingLegacyInputOrScrollPath
```

Expected: registry behavior PASS; structure test FAIL on remaining Graphics input/scroll paths.

- [ ] **Step 3: Implement only the missing Graphics completion work**

Do not recreate layout, card registration, drag, navigation or P3 input work. Keep display/backend numeric rows on `ui_widget::NumericField(...)` and keep one page `CScrollRegion` whose `State()` is the sole `CQmScrollState`, with settings-page `QmResolveScrollPolicy(...)`. Keep P4 dropdown ownership and popup scroll adapters intact; do not add a standalone text field where Graphics has none.

- [ ] **Step 4: Run green tests and inventory**

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.GraphicsPilotPlacementSurvivesSerialization:QmNewUiMenuBranches.GraphicsPilotHasNoRemainingLegacyInputOrScrollPath:QmAnim.QmResolveScrollPolicy*
python qmclient_scripts/gate/check_settings_ui_migration.py --page graphics
```

Expected: PASS; inventory reports `graphics: clean`.

- [ ] **Step 5: Commit the Graphics completion slice**

```powershell
git add src/game/client/components/menus_settings.cpp src/test/qm_card_registry_test.cpp src/test/qm_new_ui_menu_branch_test.cpp
git commit -m "refactor(settings): 补齐 Graphics 公共滚动适配"
```

---

### Task 6: 迁移 Sound 页面

**Files:**
- Modify: `src/game/client/components/menus_settings.cpp:4357-4630`
- Modify: `src/test/qm_card_registry_test.cpp`
- Modify: `src/test/qm_new_ui_menu_branch_test.cpp`

**Interfaces:**
- Consumes: shared layout/card/deck/numeric/scroll interfaces; Sound has no standalone text input in this slice.
- Preserves: `deck:sound-toggle`, `deck:sound-volume`, `deck:sound-audio-pack`; navigation `sound -> CMenus::SETTINGS_SOUND`.

- [ ] **Step 1: Write failing Sound behavior and deletion tests**

```cpp
const qm_card_order::CModel Model = RegistryModelAfterRoundTrip();
EXPECT_EQ(Model.StableIdOrder("deck:", "sound", 1),
	(std::vector<std::string>{"deck:sound-toggle", "deck:sound-volume"}));
EXPECT_EQ(Model.StableIdOrder("deck:", "sound", 2),
	(std::vector<std::string>{"deck:sound-audio-pack"}));
```

Add a structure test that requires `NumericField`, public card/deck, shared scroll state/policy and rejects `BeginSettingsCardDeck*`, outer `s_SoundSettingsScrollRegion`, `DoScrollbarH` and settings scrollbar wrappers.

- [ ] **Step 2: Verify the focused red state**

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.SoundStandardPageCardsPersistInVisualOrder:QmNewUiMenuBranches.SoundStandardPageUsesUnifiedSettingsStack
```

Expected: registry behavior PASS; structure test FAIL on old deck/slider/scroll paths.

- [ ] **Step 3: Implement the minimal Sound migration**

Retain the three current definitions and audio-pack editor behavior. Preserve P3's `ui_widget::NumericField(...)` volume/rate controls and use one outer `CScrollRegion::State()`/`CQmScrollState` plus settings-page policy. Do not add a standalone text field to Sound, and do not card-wrap the separate audio-pack editor screen.

- [ ] **Step 4: Run green tests and inventory**

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.SoundStandardPageCardsPersistInVisualOrder:QmNewUiMenuBranches.SoundStandardPageUsesUnifiedSettingsStack:QmNewUiMenuBranches.AudioPack*
python qmclient_scripts/gate/check_settings_ui_migration.py --page sound
```

Expected: PASS; audio-pack selection/editor behavior remains covered.

- [ ] **Step 5: Commit the Sound slice**

```powershell
git add src/game/client/components/menus_settings.cpp src/test/qm_card_registry_test.cpp src/test/qm_new_ui_menu_branch_test.cpp
git commit -m "refactor(settings): 迁移 Sound 标准设置页"
```

---

### Task 7: 迁移 DDNet 页面

**Files:**
- Modify: `src/game/client/components/menus_settings.cpp:6919-7401`
- Modify: `src/test/qm_card_registry_test.cpp`
- Modify: `src/test/qm_new_ui_menu_branch_test.cpp`

**Interfaces:**
- Consumes: shared layout/card/deck/input/numeric/scroll interfaces.
- Preserves: `deck:ddnet-demo`, `deck:ddnet-gameplay`, `deck:ddnet-background`, `deck:ddnet-miscellaneous`; navigation `ddnet -> CMenus::SETTINGS_DDNET`.

- [ ] **Step 1: Write failing DDNet behavior and deletion tests**

```cpp
const qm_card_order::CModel Model = RegistryModelAfterRoundTrip();
EXPECT_EQ(Model.StableIdOrder("deck:", "ddnet", 1),
	(std::vector<std::string>{"deck:ddnet-demo", "deck:ddnet-gameplay"}));
EXPECT_EQ(Model.StableIdOrder("deck:", "ddnet", 2),
	(std::vector<std::string>{"deck:ddnet-background", "deck:ddnet-miscellaneous"}));
```

Add a structure test requiring `InputField`, `NumericField`, public card/deck and shared scroll, and rejecting old deck/slider/scroll wrappers including `SCROLLBAR_OPTION_DELAYUPDATE`.

- [ ] **Step 2: Verify the focused red state**

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.DDNetStandardPageCardsPersistInVisualOrder:QmNewUiMenuBranches.DDNetStandardPageUsesUnifiedSettingsStack
```

Expected: registry behavior PASS; structure test FAIL on old numeric/deck paths.

- [ ] **Step 3: Implement the minimal DDNet migration**

Retain the four existing card groupings and config semantics. Preserve P3's `ui_widget::NumericField(...)` rows and `ui_widget::InputField(...)` text entries while moving them into definitions. Delay commit remains a NumericField policy, not a legacy flag. Keep one outer `CScrollRegion` adapter and remove only its page-private duplicate state/parameters.

- [ ] **Step 4: Run green tests and inventory**

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.DDNetStandardPageCardsPersistInVisualOrder:QmNewUiMenuBranches.DDNetStandardPageUsesUnifiedSettingsStack
python qmclient_scripts/gate/check_settings_ui_migration.py --page ddnet
```

Expected: PASS; inventory reports `ddnet: clean`.

- [ ] **Step 5: Commit the DDNet slice**

```powershell
git add src/game/client/components/menus_settings.cpp src/test/qm_card_registry_test.cpp src/test/qm_new_ui_menu_branch_test.cpp
git commit -m "refactor(settings): 迁移 DDNet 标准设置页"
```

---

### Task 8: 迁移 Appearance 并清退私有 card helper

**Files:**
- Modify: `src/game/client/components/menus_settings.cpp:5527-6918`
- Modify: `src/test/qm_card_registry_test.cpp`
- Modify: `src/test/qm_new_ui_menu_branch_test.cpp`

**Interfaces:**
- Consumes: shared layout/card/deck/numeric/scroll interfaces plus existing Appearance preview/color-picker APIs.
- Preserves: the 13 existing `deck:appearance-*` IDs and their six subpage navigation targets.
- Removes: private lambda `BeginAppearanceCard` and all page-private card sizing/registration glue it owns.

- [ ] **Step 1: Write failing Appearance behavior and deletion tests**

Extend the existing Appearance placement test with serialize/reload checks for all 13 IDs. Add `QmNewUiMenuBranches.AppearanceStandardPageUsesUnifiedSettingsStack` requiring shared layout/card/deck/numeric/scroll and explicitly asserting:

```cpp
EXPECT_EQ(RenderSettingsAppearance.find("BeginAppearanceCard"), std::string::npos);
EXPECT_EQ(RenderSettingsAppearance.find("BeginSettingsScrollRegion("), std::string::npos);
EXPECT_EQ(RenderSettingsAppearance.find("DoSettingsScrollbarOption("), std::string::npos);
```

- [ ] **Step 2: Verify the focused red state**

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.AppearanceDeckDefaultsUseSubPagePlacements:QmNewUiMenuBranches.AppearanceStandardPageUsesUnifiedSettingsStack
```

Expected: registry test PASS; structure test FAIL on `BeginAppearanceCard` and legacy numeric/scroll calls.

- [ ] **Step 3: Implement the minimal Appearance migration**

Each sub-tab keeps its existing content grouping and stable IDs but submits definitions to `CSettingsCardDeck::Render(...)`; only Deck obtains frames from `SettingsCard(...)`. Delete `BeginAppearanceCard`, its temporary stable-ID deque, private ordering migration and manual registration. Preserve P3 numeric rows and make each subpage outer `CScrollRegion::State()` the sole `CQmScrollState` with settings-page policy；popup/list adapters and dedicated color-picker tracks remain P4/P3-owned.

- [ ] **Step 4: Run green tests and inventory**

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.AppearanceDeckDefaultsUseSubPagePlacements:QmNewUiMenuBranches.AppearanceStandardPageUsesUnifiedSettingsStack:QmNewUiMenuBranches.Nameplate*:QmNewUiMenuBranches.Laser*
python qmclient_scripts/gate/check_settings_ui_migration.py --page appearance
```

Expected: PASS; inventory reports `appearance: clean`, including no `BeginAppearanceCard`.

- [ ] **Step 5: Commit the Appearance slice**

```powershell
git add src/game/client/components/menus_settings.cpp src/test/qm_card_registry_test.cpp src/test/qm_new_ui_menu_branch_test.cpp
git commit -m "refactor(settings): 迁移 Appearance 并清退私有卡片路径"
```

---

### Task 9: 迁移 Controls 并保留独立 destructive action

**Files:**
- Modify: `src/game/client/components/menus_settings_controls.cpp:142-904`
- Modify: `src/game/client/components/menus_settings_controls.h:57-104`
- Modify: `src/game/client/QmUi/QmCardRegistry.cpp:72-117`
- Modify: `src/game/client/components/qmclient/menus_qmclient.cpp:115-138`
- Modify: `src/test/qm_card_registry_test.cpp`
- Modify: `src/test/qm_new_ui_menu_branch_test.cpp`

**Interfaces:**
- Consumes: all shared interfaces; `ui_widget::InputField(...)` owns filter/text shell, `ui_widget::NumericField(...)` owns mouse/controller numeric rows.
- Produces: nine `deck:controls-*` IDs from the Task 1 manifest; navigation `controls -> CMenus::SETTINGS_CONTROLS`.
- Preserves: `CKeyBinder::DoKeyReader(...)` and a separate reset/delete `CButtonContainer` destructive action.

- [ ] **Step 1: Write failing Controls behavior and deletion tests**

```cpp
const qm_card_order::CModel Model = RegistryModelAfterRoundTrip();
EXPECT_EQ(Model.StableIdOrder("deck:", "controls", 1),
	(std::vector<std::string>{"deck:controls-mouse", "deck:controls-controller", "deck:controls-movement", "deck:controls-weapon"}));
EXPECT_EQ(Model.StableIdOrder("deck:", "controls", 2),
	(std::vector<std::string>{"deck:controls-voting", "deck:controls-chat", "deck:controls-dummy", "deck:controls-miscellaneous", "deck:controls-custom"}));
```

Append all nine Controls IDs to `CoversCurrentSettingsDeckIds`. Add a structure test requiring all public stack/input/scroll tokens and `DoKeyReader`, while rejecting `RenderSettingsBlock`, duplicate scroll state, `DoSettingsControlsScrollbarOption`, direct `DoScrollbarH` and `DoValueSelector`. Assert the delete/reset button remains outside the `InputField` call body. Check the nine IDs and uniqueness semantically; do not update or introduce a fixed total.

- [ ] **Step 2: Verify the focused red state**

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.ControlsStandardPageCardsPersistInVisualOrder:QmNewUiMenuBranches.ControlsStandardPageUsesUnifiedSettingsStack
```

Expected: FAIL on missing registry/route, private block shell, direct sliders and private scroll region.

- [ ] **Step 3: Implement the minimal Controls migration**

Map Movement/Weapon/Voting/Chat/Dummy/Miscellaneous/Custom plus Mouse and Controller to the nine stable definitions. Replace `RenderSettingsBlock` with definition content callbacks and let Deck perform the only measurement/frame construction. Preserve P3's `ui_widget::InputField(...)` filter and `ui_widget::NumericField(...)` mouse/controller rows; keep `m_SettingsScrollRegion` only as the P4 adapter and use `m_SettingsScrollRegion.State()` as the sole `CQmScrollState`. Keep key reader, expand/collapse behavior, search reveal and delete/reset button behavior unchanged; the garbage can remains a sibling action button.

Add these exact registry/navigation rows:

```cpp
{"deck:controls-mouse", "controls", ECardColumn::Left, 0, "Mouse", "controls mouse sensitivity"},
{"deck:controls-controller", "controls", ECardColumn::Left, 1, "Controller", "controls controller joystick"},
{"deck:controls-movement", "controls", ECardColumn::Left, 2, "Movement", "controls movement binds"},
{"deck:controls-weapon", "controls", ECardColumn::Left, 3, "Weapon", "controls weapon binds"},
{"deck:controls-voting", "controls", ECardColumn::Right, 0, "Voting", "controls voting binds"},
{"deck:controls-chat", "controls", ECardColumn::Right, 1, "Chat", "controls chat binds"},
{"deck:controls-dummy", "controls", ECardColumn::Right, 2, "Dummy", "controls dummy binds"},
{"deck:controls-miscellaneous", "controls", ECardColumn::Right, 3, "Miscellaneous", "controls miscellaneous binds"},
{"deck:controls-custom", "controls", ECardColumn::Right, 4, "Custom", "controls custom binds"},
{"controls", CMenus::SETTINGS_CONTROLS},
```

- [ ] **Step 4: Run green tests and inventory**

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.ControlsStandardPageCardsPersistInVisualOrder:QmNewUiMenuBranches.ControlsStandardPageUsesUnifiedSettingsStack
python qmclient_scripts/gate/check_settings_ui_migration.py --page controls
```

Expected: PASS; inventory reports `controls: clean`; destructive action assertion passes.

- [ ] **Step 5: Commit the Controls slice**

```powershell
git add src/game/client/components/menus_settings_controls.cpp src/game/client/components/menus_settings_controls.h src/game/client/QmUi/QmCardRegistry.cpp src/game/client/components/qmclient/menus_qmclient.cpp src/test/qm_card_registry_test.cpp src/test/qm_new_ui_menu_branch_test.cpp
git commit -m "refactor(settings): 迁移 Controls 标准设置页"
```

---

### Task 10: 串行全量验证、人工矩阵与独立只读审查

**Files:**
- Modify only if a finding requires a fix: files already listed in Tasks 1–9
- Evidence source of truth: this plan's checked steps plus the final task/PR report; do not create a second migration specification.

**Interfaces:**
- Verifies: all P5 interfaces and all eight page slices.
- Review contract: use `/code-review`, read `docs/ai-workflow/review.md`, findings first, then conclusion; the reviewer is read-only and independent from the implementer.

- [ ] **Step 1: Run the final structural inventory and Python tests**

```powershell
python -m unittest qmclient_scripts.gate.tests.test_check_settings_ui_migration -v
python qmclient_scripts/gate/check_settings_ui_migration.py --all
git diff --check
```

Expected: all Python tests PASS; all eight pages report `clean`; `git diff --check` produces no output and exits 0.

- [ ] **Step 2: Rebuild `testrunner` before trusting focused tests**

Run: `cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14`

Expected: exit 0 and a freshly linked `cmake-build-release/testrunner.exe`.

- [ ] **Step 3: Run the complete P5 focused filter**

Run: `cmake-build-release/testrunner.exe --gtest_filter=QmCardRegistry.*StandardPage*:QmCardRegistry.GraphicsPilot*:QmCardRegistry.AppearanceDeck*:QmNewUiMenuBranches.*StandardPage*:QmNewUiMenuBranches.GraphicsPilot*`

Expected: PASS with zero failed tests and coverage for all eight pages.

- [ ] **Step 4: Build the client**

Run: `cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14`

Expected: exit 0 and updated `cmake-build-release/DDNet.exe`.

- [ ] **Step 5: Run the full C++ regression**

Run: `cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14`

Expected: exit 0, no failed C++ test and no skipped test hidden as a pass.

- [ ] **Step 6: Run docs and repository gate**

```powershell
python qmclient_scripts/gate/check_docs.py
python qmclient_scripts/gate/check_gate.py --mode default
```

Expected: docs check PASS; default gate PASS, including C++ and Rust full-test targets. If the environment cannot support default, run `--mode quick`, report default as an explicit gap, and do not claim submission-ready completion.

- [ ] **Step 7: Execute the manual visual/interaction matrix**

Launch: `cmake-build-release/DDNet.exe`

Record a result or screenshot for every row below at 100% UI scale, one non-default scale, default language and one long-localization language:

| Page | Required checks |
|---|---|
| General | two-column/single-column switch, language list clip, update-rate `∞`, focus ring, no overlap |
| Player | Player/Dummy switch, name/clan IME and cursor, flag search, list wheel ownership |
| Tee | Player/Dummy/Profiles switch, skin prefix/search, fast scroll, preview/cache settle, no stale card rect |
| Graphics | dropdown first-wheel ownership, backend/display inputs, NumericField track width, overflow rail AUTO |
| Sound | enable/disable state, volume NumericField, audio-pack search/editor entry, no spherical track |
| DDNet | delay commit, units/opacity, card reorder persistence, no clipped long label |
| Appearance | every sub-tab, single shell, rainbow title, cross-column drag, no `BeginAppearanceCard` visual residue |
| Controls | filter/search reveal, expanded groups, mouse/controller NumericField, key capture, independent garbage-can action |

Expected: no card overlap, double shell, white fog, stale hit rect, scroll leak, clipped focus ring or old input visual. Any unverified cell remains a named gap.

- [ ] **Step 8: Dispatch and wait for an independent read-only review**

The new reviewer must inspect the actual P5 diff only, run `/code-review`, and cover correctness, old-path deletion, registry/navigation completeness, Tee cache/perf preservation, Controls destructive action, and test strength. Wait for the full report; do not close or interrupt a healthy reviewer. Expected output: findings ordered by severity, then `正确` / `需要修复` / `不安全`.

- [ ] **Step 9: Fix every actionable finding with a red-green cycle**

For each finding, first add or tighten the smallest reproducing test, run its exact filter and observe FAIL, apply the minimal fix, rerun the filter and observe PASS, then rerun Steps 1–6 in the same serial order. If there are no findings, record that fact and do not manufacture a cleanup commit.

- [ ] **Step 10: Commit review fixes or the final evidence update**

If code changed:

```powershell
git add -p -- src/game/client/components/menus_settings.cpp src/game/client/components/menus_settings_controls.cpp src/game/client/components/menus_settings_controls.h src/game/client/QmUi/QmCardRegistry.cpp src/game/client/components/qmclient/menus_qmclient.cpp src/test/qm_card_registry_test.cpp src/test/qm_new_ui_menu_branch_test.cpp qmclient_scripts/gate/check_settings_ui_migration.py qmclient_scripts/gate/tests/test_check_settings_ui_migration.py qmclient_scripts/scripts_overview.md
git commit -m "fix(settings): 收口标准设置页迁移审查问题"
```

If no code changed, commit only the completed verification evidence recorded in this plan:

```powershell
git add docs/superpowers/plans/2026-07-11-QmClient-设置页UI统一-P5-标准设置页迁移.md
git commit -m "docs(settings): 记录 P5 标准设置页验收证据"
```

Expected: the final report states changes, exact verification results and remaining gaps; it does not equate build completion with full verification.

---

## Completion Gate

P5 is complete only when all of the following are true:

- [ ] General、Player、Tee、Graphics、Sound、DDNet、Appearance、Controls all pass `check_settings_ui_migration.py --all`.
- [ ] Every page's stable IDs exist once in `QmCardRegistry`, default placement survives serialize/reload, and Search navigation opens the correct page/sub-tab.
- [ ] No migrated page calls old card/layout/input/numeric/scroll wrappers; `BeginAppearanceCard` is absent.
- [ ] Graphics was completed in place without recreating P1/P2 card/deck work.
- [ ] Controls garbage can remains an independent destructive action.
- [ ] Fresh `testrunner`, complete focused filters, `game-client`, `run_cxx_tests`, docs check and default gate all have recorded results.
- [ ] The eight-page visual matrix is recorded; unverified cells are explicit gaps.
- [ ] Independent read-only review has returned and every actionable finding is closed.
- [ ] R1–R3、QmClient、TClient and non-card menus were not pulled into the P5 diff.
