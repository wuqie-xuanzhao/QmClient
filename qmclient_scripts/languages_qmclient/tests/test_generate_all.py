#!/usr/bin/env python3

import tempfile
import unittest
from pathlib import Path
from unittest import mock

from qmclient_scripts.languages_qmclient import generate_all


class GenerateAllTest(unittest.TestCase):
    def test_runtime_language_path_uses_data_languages_directory(self):
        self.assertEqual(
            generate_all.runtime_language_path("russian").name,
            "russian.txt",
        )

    def test_format_language_entry_escapes_translation_newlines(self):
        self.assertEqual(
            generate_all.format_language_entry(
                "Line one\\nLine two", "ctx", "第一行\n第二行"
            ),
            "[ctx]\nLine one\\nLine two\n== 第一行\\n第二行",
        )

    def test_generate_configured_languages_writes_each_language(self):
        strings = [generate_all.SourceString("Server"), generate_all.SourceString("Clan")]
        store = {
            "menus": {
                ("Server", ""): {
                    "simplified_chinese": "服务器",
                    "russian": "Сервер",
                },
                ("Clan", ""): {
                    "simplified_chinese": "战队",
                    "russian": "Клан",
                },
            }
        }

        with tempfile.TemporaryDirectory() as tmp:
            out_dir = Path(tmp)
            with mock.patch.object(generate_all, "BASE_LANGUAGES_DIR", out_dir), mock.patch.object(
                generate_all.i18n_store,
                "load_language_store",
                return_value=store,
            ):
                written = generate_all.generate_configured_languages(
                    strings, ["simplified_chinese", "russian"]
                )

            self.assertEqual(written, 2)
            self.assertIn("== 服务器", (out_dir / "simplified_chinese.txt").read_text(encoding="utf-8"))
            self.assertIn("== Сервер", (out_dir / "russian.txt").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
