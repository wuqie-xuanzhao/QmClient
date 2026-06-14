#!/usr/bin/env python3

import tempfile
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))

import review_duplicate_entries as audit
import source_keys


class AuditSimplifiedChineseTest(unittest.TestCase):
    def test_reports_duplicate_keys_with_line_numbers(self):
        with tempfile.TemporaryDirectory() as tmp:
            lang_path = Path(tmp) / "simplified_chinese.txt"
            lang_path.write_text(
                "Duplicate\n== 第一项\n\nDuplicate\n== 第二项\n",
                encoding="utf-8",
            )

            report = audit.review_language_files(
                [lang_path], used_keys={("Duplicate", "")}
            )

        self.assertEqual(len(report.duplicate_keys), 1)
        self.assertEqual(report.duplicate_keys[0].title, "Duplicate")
        self.assertEqual(
            [entry.line for entry in report.duplicate_keys[0].entries], [1, 4]
        )

    def test_reports_only_unreferenced_candidates(self):
        with tempfile.TemporaryDirectory() as tmp:
            lang_path = Path(tmp) / "simplified_chinese.txt"
            lang_path.write_text(
                "Used key\n== 已使用\n\n[Menu]\nUnused key\n== 未使用\n",
                encoding="utf-8",
            )

            report = audit.review_language_files(
                [lang_path], used_keys={("Used key", "")}
            )

        self.assertEqual(
            [(entry.key, entry.context) for entry in report.unused],
            [("Unused key", "Menu")],
        )

    def test_decodes_non_ascii_cpp_string_literals(self):
        self.assertEqual(source_keys.decode_cpp_string('中文\\"key'), '中文"key')

    def test_reports_similar_keys_after_normalization(self):
        with tempfile.TemporaryDirectory() as tmp:
            lang_path = Path(tmp) / "english.txt"
            lang_path.write_text(
                "Scoreboard\n== Scoreboard\n\nscoreboard\n== scoreboard\n",
                encoding="utf-8",
            )

            report = audit.review_language_files(
                [lang_path], used_keys={("Scoreboard", ""), ("scoreboard", "")}
            )

        self.assertEqual(len(report.similar_keys), 1)

    def test_ignores_ui_punctuation_only_key_variants(self):
        with tempfile.TemporaryDirectory() as tmp:
            lang_path = Path(tmp) / "english.txt"
            lang_path.write_text(
                "Angle\n== Angle\n\nAngle:\n== Angle:\n",
                encoding="utf-8",
            )

            report = audit.review_language_files(
                [lang_path], used_keys={("Angle", ""), ("Angle:", "")}
            )

        self.assertEqual(report.similar_keys, [])

    def test_unused_uses_final_active_key_names_without_context_drift(self):
        with tempfile.TemporaryDirectory() as tmp:
            lang_path = Path(tmp) / "simplified_chinese.txt"
            lang_path.write_text(
                "[Menu]\nContext key\n== 已使用\n\nUnused key\n== 未使用\n",
                encoding="utf-8",
            )

            report = audit.review_language_files(
                [lang_path], used_keys={("Context key", "Menu")}
            )

        self.assertEqual(
            [(entry.key, entry.context) for entry in report.unused],
            [("Unused key", "")],
        )

    def test_extracts_register_help_strings(self):
        content = """
        Console()->Register("add_bindwheel", "s[name] s[command]", CFGFLAG_CLIENT, ConAddBindwheel, this, "Add a bind to the bindwheel");
        pConsole->Register("friend_category_add", "s[category]", CFGFLAG_CLIENT, ConAddFriendCategory, this, "Add a friend category");
        """

        self.assertEqual(
            source_keys.extract_register_help_strings(
                source_keys.strip_cpp_comments(content)
            ),
            {"Add a bind to the bindwheel", "Add a friend category"},
        )


if __name__ == "__main__":
    unittest.main()
