#!/usr/bin/env python3

import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from qmclient_scripts.languages_qmclient import source_keys


class SourceKeysTest(unittest.TestCase):
    def test_extracts_localize_first_argument_literals(self):
        content = 'Localize("Open"); Localize("Close", "Menu");'
        self.assertEqual(
            source_keys.extract_localize_keys(source_keys.strip_cpp_comments(content)),
            {("Open", ""), ("Close", "Menu")},
        )

    def test_extracts_concatenated_literals_in_first_argument(self):
        content = 'Localize("Demo " "Player");'
        self.assertEqual(
            source_keys.extract_localize_keys(source_keys.strip_cpp_comments(content)),
            {("Demo ", ""), ("Player", "")},
        )

    def test_preserves_newline_escape_as_language_file_key(self):
        content = r'Localize("%d\n(%d/%d)");'
        self.assertEqual(
            source_keys.extract_localize_keys(source_keys.strip_cpp_comments(content)),
            {(r"%d\n(%d/%d)", "")},
        )

    def test_ignores_commented_localize_calls(self):
        content = '// Localize("Nope")\nLocalize("Yes")'
        self.assertEqual(
            source_keys.extract_localize_keys(source_keys.strip_cpp_comments(content)),
            {("Yes", "")},
        )

    def test_ignores_localize_text_inside_string_literal(self):
        content = 'EXPECT_NE(Source.find("Localize(\\"Nope\\")"), std::string::npos);'
        self.assertEqual(
            source_keys.extract_localize_keys(source_keys.strip_cpp_comments(content)),
            set(),
        )

    def test_extracts_register_help_text(self):
        content = (
            'Console()->Register("cmd", "", CFGFLAG_CLIENT, Fn, this, "Help text");'
        )
        self.assertEqual(
            source_keys.extract_register_help_strings(
                source_keys.strip_cpp_comments(content)
            ),
            {"Help text"},
        )

    def test_register_help_strings_should_use_english_source_keys(self):
        content = (
            'Console()->Register("rules", "", CFGFLAG_CHAT | CFGFLAG_SERVER, Fn, this, '
            '"Show the server rules");'
        )
        self.assertEqual(
            source_keys.extract_register_help_strings(
                source_keys.strip_cpp_comments(content)
            ),
            {"Show the server rules"},
        )

    def test_console_cmdlist_help_uses_english_source_key(self):
        path = Path("src/engine/shared/console.cpp")
        content = source_keys.strip_cpp_comments(source_keys.read_source_text(path))
        help_strings = source_keys.extract_register_help_strings(content)

        self.assertIn("List all commands which are accessible for users", help_strings)
        self.assertNotIn("列出普通玩家可用的所有命令", help_strings)

    def test_collects_category_summary_from_source_file(self):
        records = source_keys.collect_source_key_records(
            paths=(Path(__file__),),
            extra_strings={"Extra test key"},
        )

        summary = source_keys.summarize_source_key_records(records)

        self.assertGreaterEqual(summary.localize_or_localizable, 1)
        self.assertEqual(summary.extra, 1)
        self.assertEqual(summary.total_unique, len({record.key for record in records}))

    def test_audit_marks_notification_aliases_as_business_data(self):
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            path = (
                root
                / "src"
                / "game"
                / "client"
                / "components"
                / "qmclient"
                / "hud_notifications"
                / "hud_notification_static_rules.h"
            )
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(
                'X("你正在发起投票，请等当前投票结束后再试", '
                '"You are running a vote, please try again after the vote is done!")\n',
                encoding="utf-8",
            )

            report = source_keys.build_string_audit_report(paths=(root / "src",))

        business_texts = {record.text for record in report.business_data}
        must_i18n_texts = {record.text for record in report.must_i18n}
        self.assertIn("你正在发起投票，请等当前投票结束后再试", business_texts)
        self.assertIn(
            "You are running a vote, please try again after the vote is done!",
            must_i18n_texts,
        )

    def test_audit_marks_semantic_notification_matchers_as_business_data(self):
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            path = (
                root
                / "src"
                / "game"
                / "client"
                / "components"
                / "qmclient"
                / "hud_notifications"
                / "hud_notification_static_alias_rules.h"
            )
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(
                "#define QM_HUD_NOTIFICATION_STATIC_ALIAS_RULES(X) \\\n"
                '\tX("你现在会收到私聊消息", WhispersOn)\n',
                encoding="utf-8",
            )

            report = source_keys.build_string_audit_report(paths=(root / "src",))

        business_texts = {record.text for record in report.business_data}
        self.assertIn("你现在会收到私聊消息", business_texts)

    def test_audit_marks_assertions_and_command_templates_as_business_data(self):
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            path = root / "src" / "game" / "client" / "components" / "foo.cpp"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(
                "\n".join(
                    [
                        'dbg_assert_failed("Client state %d is invalid for RenderMenubar");',
                        'static_assert(true, "Metadata table out of sync");',
                        'str_format(aCmd, sizeof(aCmd), "auth_add %s admin %s", pUser, pPassword);',
                    ]
                )
                + "\n",
                encoding="utf-8",
            )

            report = source_keys.build_string_audit_report(paths=(root / "src",))

        business_texts = {record.text for record in report.business_data}
        self.assertIn("Client state %d is invalid for RenderMenubar", business_texts)
        self.assertIn("Metadata table out of sync", business_texts)
        self.assertIn("auth_add %s admin %s", business_texts)

    def test_audit_marks_external_parser_diagnostics_as_business_data(self):
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            path = (
                root
                / "src"
                / "game"
                / "client"
                / "components"
                / "qmclient"
                / "translate"
                / "translate_parse.cpp"
            )
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(
                'str_copy(Out.m_aError, "No choices in response", sizeof(Out.m_aError));\n',
                encoding="utf-8",
            )

            report = source_keys.build_string_audit_report(paths=(root / "src",))

        business_texts = {record.text for record in report.business_data}
        self.assertIn("No choices in response", business_texts)

    def test_audit_marks_bind_commands_as_business_data(self):
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            path = (
                root
                / "src"
                / "game"
                / "client"
                / "components"
                / "menus_settings_controls.cpp"
            )
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(
                '{EBindOptionGroup::MOVEMENT, Localizable("Pause"), "say /pause"},\n',
                encoding="utf-8",
            )

            report = source_keys.build_string_audit_report(paths=(root / "src",))

        business_texts = {record.text for record in report.business_data}
        must_i18n_texts = {record.text for record in report.must_i18n}
        self.assertIn("say /pause", business_texts)
        self.assertIn("Pause", must_i18n_texts)

    def test_audit_marks_localized_constant_aliases_as_business_data(self):
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            path = root / "src" / "game" / "client" / "components" / "hud.cpp"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(
                'constexpr const char *pLine1 = "practice mode";\n'
                "TextRender()->Text(0.0f, 0.0f, 10.0f, Localize(pLine1), -1.0f);\n",
                encoding="utf-8",
            )

            report = source_keys.build_string_audit_report(paths=(root / "src",))

        business_texts = {record.text for record in report.business_data}
        self.assertIn("practice mode", business_texts)

    def test_audit_marks_preview_and_format_templates_as_business_data(self):
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            path = root / "src" / "game" / "client" / "components" / "chat.cpp"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(
                'static const SPreviewLine s_aPreviewLines[] = {{"Server", "Welcome to QmClient"}};\n'
                'str_format(aCount, sizeof(aCount), "%s（/%s %s）", pHelp, pName, pParams);\n',
                encoding="utf-8",
            )

            report = source_keys.build_string_audit_report(paths=(root / "src",))

        business_texts = {record.text for record in report.business_data}
        self.assertIn("Welcome to QmClient", business_texts)
        self.assertIn("%s（/%s %s）", business_texts)

    def test_extracts_statusbar_item_labels_and_descriptions(self):
        path = (
            source_keys.PROJECT_ROOT
            / "src"
            / "game"
            / "client"
            / "components"
            / "tclient"
            / "statusbar.h"
        )
        content = source_keys.strip_cpp_comments(source_keys.read_source_text(path))
        records = source_keys.extract_known_indirect_records(path, content)
        keys = {record.key for record in records}

        self.assertIn("Snapshot Latency", keys)
        self.assertIn("Displays server snapshot latency", keys)
        self.assertNotIn("u", keys)

    def test_extracts_tclient_cached_section_titles(self):
        path = (
            source_keys.PROJECT_ROOT
            / "src"
            / "game"
            / "client"
            / "components"
            / "tclient"
            / "menus_tclient.cpp"
        )
        content = source_keys.strip_cpp_comments(source_keys.read_source_text(path))
        records = source_keys.extract_known_indirect_records(path, content)
        keys = {record.key for record in records}

        self.assertIn("Visual: Nameplates", keys)
        self.assertIn("Tee status bar", keys)

    def test_extracts_qmclient_config_descriptions(self):
        path = (
            source_keys.PROJECT_ROOT
            / "src"
            / "engine"
            / "shared"
            / "config_variables_qmclient.h"
        )
        content = source_keys.strip_cpp_comments(source_keys.read_source_text(path))
        records = source_keys.extract_known_indirect_records(path, content)
        keys = {record.key for record in records}

        self.assertIn("计分板查分", keys)
        self.assertIn("通知栏接管服务器系统提示（入场版本信息除外）", keys)
        self.assertNotIn("qm_scoreboard_points", keys)

    def test_extracts_asset_editor_blend_modes_with_context(self):
        path = (
            source_keys.PROJECT_ROOT
            / "src"
            / "game"
            / "client"
            / "components"
            / "menus.h"
        )
        content = source_keys.strip_cpp_comments(source_keys.read_source_text(path))
        records = source_keys.extract_known_indirect_records(path, content)
        identities = {record.identity() for record in records}

        self.assertIn(("Screen", "Assets editor blend mode"), identities)
        self.assertIn(("Overlay", "Assets editor blend mode"), identities)

    def test_audit_marks_dynamic_localize_context_as_business_data(self):
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            path = root / "src" / "game" / "client" / "components" / "menus.cpp"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(
                'const char *pName = Localize(GetName(), "Dynamic context");\n',
                encoding="utf-8",
            )

            report = source_keys.build_string_audit_report(paths=(root / "src",))

        business_texts = {record.text for record in report.business_data}
        review_texts = {record.text for record in report.needs_review}
        self.assertIn("Dynamic context", business_texts)
        self.assertNotIn("Dynamic context", review_texts)

    def test_audit_marks_display_format_shells_as_business_data(self):
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            path = root / "src" / "game" / "client" / "components" / "menus.cpp"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(
                'str_format(aBuf, sizeof(aBuf), "[%d]  ", Index);\n',
                encoding="utf-8",
            )

            report = source_keys.build_string_audit_report(paths=(root / "src",))

        business_texts = {record.text for record in report.business_data}
        review_texts = {record.text for record in report.needs_review}
        self.assertIn("[%d]  ", business_texts)
        self.assertNotIn("[%d]  ", review_texts)

    def test_audit_marks_chat_preview_samples_as_business_data(self):
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            path = (
                root / "src" / "game" / "client" / "components" / "menus_settings.cpp"
            )
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(
                'SetPreviewLine(PREVIEW_TEAM, 11, "Your Teammate", "Let\\\'s speedrun this!", FLAG_TEAM, 0);\n',
                encoding="utf-8",
            )

            report = source_keys.build_string_audit_report(paths=(root / "src",))

        business_texts = {record.text for record in report.business_data}
        review_texts = {record.text for record in report.needs_review}
        self.assertIn("Your Teammate", business_texts)
        self.assertIn("Let's speedrun this!", business_texts)
        self.assertNotIn("Let's speedrun this!", review_texts)

    def test_audit_marks_localized_layout_measurement_keys_as_business_data(self):
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            path = (
                root
                / "src"
                / "game"
                / "client"
                / "components"
                / "menus_settings_assets.cpp"
            )
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(
                'const float Width = ComputeToolbarButtonWidth("Assets directory");\n'
                'DoButton_Menu(&s_Id, Localize("Assets directory"), 0, &Button);\n',
                encoding="utf-8",
            )

            report = source_keys.build_string_audit_report(paths=(root / "src",))

        business_texts = {record.text for record in report.business_data}
        must_i18n_texts = {record.text for record in report.must_i18n}
        self.assertIn("Assets directory", business_texts)
        self.assertIn("Assets directory", must_i18n_texts)

    def test_audit_marks_test_strings_as_test_only(self):
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            path = root / "src" / "test" / "sample_test.cpp"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('EXPECT_STREQ("test sample text", "test sample text");\n')

            report = source_keys.build_string_audit_report(paths=(root / "src",))

        test_texts = {record.text for record in report.test_only}
        self.assertIn("test sample text", test_texts)

    def test_audit_marks_unknown_client_strings_for_review(self):
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            path = root / "src" / "game" / "client" / "components" / "foo.cpp"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('const char *pLabel = "Review this user-facing string";\n')

            report = source_keys.build_string_audit_report(paths=(root / "src",))

        review_texts = {record.text for record in report.needs_review}
        self.assertIn("Review this user-facing string", review_texts)

    def test_audit_marks_obvious_machine_literals_as_business_data(self):
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            path = root / "src" / "game" / "client" / "components" / "foo.cpp"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(
                "\n".join(
                    [
                        'log_info("qmclient", "event=sample duration_ms=%.3f");',
                        'Writer.WriteAttribute("version");',
                        'static constexpr const char *PATH = "qmclient/map_notes.json";',
                        'const char *pLabel = "Review this user-facing string";',
                    ]
                )
                + "\n",
                encoding="utf-8",
            )

            report = source_keys.build_string_audit_report(paths=(root / "src",))

        business_texts = {record.text for record in report.business_data}
        review_texts = {record.text for record in report.needs_review}
        self.assertIn("event=sample duration_ms=%.3f", business_texts)
        self.assertIn("qmclient/map_notes.json", business_texts)
        self.assertNotIn("version", review_texts)
        self.assertIn("Review this user-facing string", review_texts)

    def test_audit_marks_cjk_source_keys_as_violation(self):
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            path = root / "src" / "game" / "client" / "components" / "foo.cpp"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('Localize("中文提示");\n', encoding="utf-8")

            report = source_keys.build_string_audit_report(paths=(root / "src",))

        violation_texts = {record.text for record in report.violation}
        self.assertIn("中文提示", violation_texts)

    def test_audit_accepts_qmclient_config_descriptions_as_indirect_i18n(self):
        report = source_keys.build_string_audit_report(
            paths=(
                source_keys.PROJECT_ROOT
                / "src"
                / "engine"
                / "shared"
                / "config_variables_qmclient.h",
            )
        )

        must_i18n_texts = {record.text for record in report.must_i18n}
        violation_texts = {record.text for record in report.violation}
        self.assertIn("计分板查分", must_i18n_texts)
        self.assertNotIn("计分板查分", violation_texts)

    def test_audit_report_round_trips_through_json_file(self):
        report = source_keys.StringAuditReport(
            must_i18n=[
                source_keys.StringAuditRecord(
                    Path("src/game/client/components/foo.cpp"),
                    10,
                    "Play game",
                    "must_i18n",
                    "active source key (localize_or_localizable)",
                )
            ],
            business_data=[],
            test_only=[],
            needs_review=[],
            violation=[],
        )

        with TemporaryDirectory() as tmpdir:
            report_path = Path(tmpdir) / "audit.json"
            source_keys.write_string_audit_report(report_path, report)
            loaded = source_keys.read_string_audit_report(report_path)

        self.assertEqual(loaded.must_i18n[0].line, 10)
        self.assertEqual(loaded.must_i18n[0].text, "Play game")
        self.assertEqual(loaded.summary()["must_i18n"], 1)

    def test_extracts_browser_localize_keys_for_offline_and_local_server_ui(self):
        path = Path("src/game/client/components/menus_browser.cpp")
        content = source_keys.strip_cpp_comments(source_keys.read_source_text(path))
        keys = source_keys.extract_localize_keys(content)

        self.assertIn(("Clan Members", ""), keys)
        self.assertIn(("Offline", ""), keys)
        self.assertIn(("No local servers found (ports %d-%d)", ""), keys)
        self.assertIn(("Start and connect to local server", ""), keys)


if __name__ == "__main__":
    unittest.main()
