from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from qmclient_scripts.gate.check_settings_ui_migration import PAGE_STABLE_IDS, PRODUCER_COMPLETE_PAGES, audit_page


class SettingsUiMigrationAuditTest(unittest.TestCase):
	def setUp(self):
		self.temp_dir = TemporaryDirectory()
		self.root = Path(self.temp_dir.name)

	def tearDown(self):
		self.temp_dir.cleanup()

	def make_repo(self, *, drop: str = "", add: str = "") -> Path:
		source = """void CMenus::RenderSettingsGeneral(CUIRect MainView)
{
    const SSettingsContentMetrics Metrics = ResolveSettingsContentMetrics(MainView.w);
    const SSettingsPageLayoutFrame Frame = SettingsPageLayout(MainView, 1.0f);
    std::vector<SSettingsCardDefinition> vCards;
    vCards.push_back({{"deck:general-game", "General", nullptr}, MeasureGeneralGame, RenderGeneralGame});
    CScrollRegion ScrollRegion;
    CQmScrollState &Scroll = ScrollRegion.State();
    QmResolveScrollPolicy(Request, 1.0f, 0.1f);
    Request.m_Profile = EQmScrollProfile::SETTINGS_OUTER;
    ui_widget::NumericField(Context);
    const SSettingsCardDeckResult DeckResult = SettingsCardDeckForRenderPass().Render(Context, Frame, "general", vCards, SettingsCardOrderModelForRenderPass(), &ScrollRegion, Input, SettingsCardMotionSpec());
}
"""
		source = source.replace("\n}\n", f"\n    {add}\n}}\n", 1).replace(drop, "")
		files = {
			"src/game/client/components/menus_settings.cpp": source,
			"src/game/client/components/menus.cpp": "",
			"src/game/client/QmUi/QmCardRegistry.cpp": "\n".join(PAGE_STABLE_IDS["general"]),
			"src/game/client/components/qmclient/menus_qmclient.cpp": """static constexpr SQmGlobalSearchTabRoute s_aGlobalSearchTabRoutes[] = {
    {"general", CMenus::SETTINGS_GENERAL},
};
SQmGlobalSearchNavigation ResolveGlobalSearchNavigation(const SQmGlobalSearchCard &Card)
{
    return {};
}
""",
		}
		files["src/game/client/components/menus.cpp"] += """
bool CMenus::SetSettingsPageFromCardTab(const char *pTab)
{
    if(str_comp(pTab, "general") == 0)
        return true;
    return false;
}
"""
		for relative, content in files.items():
			path = self.root / relative
			path.parent.mkdir(parents=True, exist_ok=True)
			path.write_text(content, encoding="utf-8")
		return self.root

	def make_warlist_repo(self, *, drop: str = "", add: str = "", registry_add: str = "") -> Path:
		source = """void CMenus::RenderSettingsTClientWarList(CUIRect MainView, bool PrewarmOnly)
{
    ApplyTClientContentMetrics(MainView.w);
    const SSettingsPageLayoutFrame Frame = SettingsPageLayout(MainView, 1.0f);
    std::vector<SSettingsCardDefinition> vCards;
    SSettingsCardDefinition Card;
    Card.m_Spec = {"deck:tclient-warlist", "War List", nullptr};
    vCards.push_back(std::move(Card));
    SSettingsCardDeckResult Result;
    QmResolveScrollPolicy(Request, 1.0f, 0.1f);
    Request.m_Profile = EQmScrollProfile::SETTINGS_OUTER;
    Result = CardDeck.Render(Context, Frame, "tclient-warlist", vCards, Model, &ScrollRegion, Input, Motion);
}
"""
		source = source.replace("\n}\n", f"\n    {add}\n}}\n", 1).replace(drop, "")
		files = {
			"src/game/client/components/tclient/menus_tclient.cpp": source,
			"src/game/client/QmUi/QmCardRegistry.cpp": '"deck:tclient-warlist"\n' + registry_add,
			"src/game/client/components/menus.cpp": """bool CMenus::SetSettingsPageFromCardTab(const char *pTab)
{
    return str_comp(pTab, "tclient-warlist") == 0;
}
""",
		}
		for relative, content in files.items():
			path = self.root / relative
			path.parent.mkdir(parents=True, exist_ok=True)
			path.write_text(content, encoding="utf-8")
		return self.root

	def test_clean_page_passes(self):
		self.assertEqual(audit_page(self.make_repo(), "general"), [])

	def test_missing_public_contract_fails(self):
		errors = audit_page(self.make_repo(drop="SettingsCardDeckForRenderPass().Render("), "general")
		self.assertTrue(any("SettingsCardDeckForRenderPass().Render" in item for item in errors))

	def test_tee7_requires_render_pass_isolated_deck(self):
		from qmclient_scripts.gate.check_settings_ui_migration import PAGE_REQUIRED

		self.assertIn("SettingsCardDeckForRenderPass().Render(", PAGE_REQUIRED["tee7"])
		self.assertNotIn("m_SettingsCardDeck.Render(", PAGE_REQUIRED["tee7"])

	def test_manifest_covers_every_new_settings_page(self):
		self.assertTrue(
			{
				"general",
				"player",
				"tee",
				"tee7",
				"graphics",
				"sound",
				"ddnet",
				"appearance",
				"controls",
				"qmclient_hud",
				"qmclient_function",
				"qmclient_visual",
				"contributors",
				"global_search",
				"tclient",
				"tclient_bind_wheel",
				"tclient_chat_binds",
				"tclient_warlist",
				"tclient_status_bar",
				"tclient_info",
				"tclient_profiles",
				"tclient_configs",
				"assets",
			}.issubset(PAGE_STABLE_IDS)
		)
		self.assertIn("qm:dummy_miniview", PAGE_STABLE_IDS["qmclient_hud"])
		self.assertIn("qm:favorite_maps", PAGE_STABLE_IDS["qmclient_function"])
		self.assertIn("qm:skin_transition", PAGE_STABLE_IDS["qmclient_visual"])
		self.assertTrue({"appearance", "qmclient_hud", "qmclient_function", "qmclient_visual", "contributors", "tclient_configs", "tclient_warlist"}.issubset(PRODUCER_COMPLETE_PAGES))

	def test_warlist_single_card_contract_passes(self):
		self.assertEqual(audit_page(self.make_warlist_repo(), "tclient_warlist"), [])

	def test_warlist_missing_producer_card_fails(self):
		errors = audit_page(
			self.make_warlist_repo(
				drop='Card.m_Spec = {"deck:tclient-warlist", "War List", nullptr};',
				add='str_startswith(FocusStableId, "deck:tclient-warlist");',
			),
			"tclient_warlist",
		)
		self.assertTrue(any("page producer" in item for item in errors))

	def test_warlist_legacy_split_card_fails(self):
		errors = audit_page(self.make_warlist_repo(add='Card.m_Spec = {"deck:tclient-warlist-editor", "Edit Entry", nullptr};'), "tclient_warlist")
		self.assertTrue(any("deck:tclient-warlist-editor" in item and "legacy path" in item for item in errors))

	def test_warlist_legacy_split_card_in_registry_fails(self):
		errors = audit_page(
			self.make_warlist_repo(registry_add='"deck:tclient-warlist-editor"'),
			"tclient_warlist",
		)
		self.assertTrue(any("deck:tclient-warlist-editor" in item and "legacy registry" in item for item in errors))

	def test_missing_outer_scroll_profile_fails(self):
		errors = audit_page(self.make_repo(drop="Request.m_Profile = EQmScrollProfile::SETTINGS_OUTER;"), "general")
		self.assertTrue(any("SETTINGS_OUTER" in item for item in errors))

	def test_missing_content_metrics_fails(self):
		errors = audit_page(self.make_repo(drop="ResolveSettingsContentMetrics("), "general")
		self.assertTrue(any("ResolveSettingsContentMetrics" in item for item in errors))

	def test_legacy_path_fails(self):
		errors = audit_page(self.make_repo(add="DoSettingsScrollbarOption("), "general")
		self.assertTrue(any("DoSettingsScrollbarOption" in item for item in errors))

	def test_missing_registry_or_navigation_fails(self):
		root = self.make_repo()
		(root / "src/game/client/QmUi/QmCardRegistry.cpp").write_text("", encoding="utf-8")
		errors = audit_page(root, "general")
		self.assertTrue(any("registry/navigation" in item for item in errors))

	def test_route_name_outside_explicit_navigation_contract_fails(self):
		root = self.make_repo()
		navigation = root / "src/game/client/components/menus.cpp"
		navigation.write_text('const char *pUnrelated = "general";', encoding="utf-8")
		errors = audit_page(root, "general")
		self.assertTrue(any("general: general: registry/navigation entry missing" in item for item in errors))


if __name__ == "__main__":
	unittest.main()
