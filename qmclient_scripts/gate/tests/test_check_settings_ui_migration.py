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
            path.write_text(content, encoding="utf-8")
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
        root = self.make_repo()
        (root / "src/game/client/QmUi/QmCardRegistry.cpp").write_text(
            "", encoding="utf-8"
        )
        errors = audit_page(root, "general")
        self.assertTrue(any("registry/navigation" in item for item in errors))


if __name__ == "__main__":
    unittest.main()