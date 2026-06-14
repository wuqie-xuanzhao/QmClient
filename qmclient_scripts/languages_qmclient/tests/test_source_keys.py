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

    def test_audit_marks_cjk_source_keys_as_violation(self):
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            path = root / "src" / "game" / "client" / "components" / "foo.cpp"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('Localize("中文提示");\n', encoding="utf-8")

            report = source_keys.build_string_audit_report(paths=(root / "src",))

        violation_texts = {record.text for record in report.violation}
        self.assertIn("中文提示", violation_texts)

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
