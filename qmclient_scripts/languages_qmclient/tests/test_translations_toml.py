#!/usr/bin/env python3

import tomllib
import unittest
from pathlib import Path
from unittest import mock
import tempfile

from qmclient_scripts.languages_qmclient import i18n_store


class I18nTomlTest(unittest.TestCase):
    def test_dump_module_writes_context_and_multilang(self):
        text = i18n_store.dump_module(
            [
                (
                    i18n_store.Message("Play", "Start menu"),
                    {
                        "simplified_chinese": "开始游戏",
                        "traditional_chinese": "開始遊戲",
                    },
                )
            ]
        )
        parsed = tomllib.loads(text)
        self.assertEqual(parsed["message"][0]["key"], "Play")
        self.assertEqual(parsed["message"][0]["context"], "Start menu")
        self.assertEqual(
            parsed["message"][0]["translations"]["simplified_chinese"], "开始游戏"
        )

    def test_dump_module_keeps_translation_lines_contiguous(self):
        text = i18n_store.dump_module(
            [
                (
                    i18n_store.Message("Anti Ping Smoothing"),
                    {
                        "simplified_chinese": "平滑预测",
                        "japanese": "アンチピングスムージング",
                        "traditional_chinese": "預測量",
                        "korean": "양",
                    },
                )
            ]
        )

        self.assertNotIn(
            'simplified_chinese = "平滑预测"\n\njapanese = ',
            text,
        )
        self.assertIn(
            "[message.translations]\n"
            'simplified_chinese = "平滑预测"\n'
            'traditional_chinese = "預測量"\n'
            'japanese = "アンチピングスムージング"\n'
            'korean = "양"',
            text,
        )

    def test_patch_module_store_removes_internal_translation_blank_lines(self):
        with tempfile.TemporaryDirectory() as tmp:
            translations_dir = Path(tmp)
            path = translations_dir / "menus.toml"
            path.write_text(
                """[[message]]
key = "Anti Ping Smoothing"
[message.translations]
simplified_chinese = "平滑预测"

japanese = "アンチピングスムージング"
traditional_chinese = "預測量"

[[message]]
key = "Apply"
[message.translations]
simplified_chinese = "应用"
""",
                encoding="utf-8",
                newline="\n",
            )

            with mock.patch.object(i18n_store, "TRANSLATIONS_DIR", translations_dir):
                i18n_store.patch_module_store(
                    "menus",
                    {
                        ("Anti Ping Smoothing", ""): {
                            "simplified_chinese": "平滑预测",
                            "japanese": "アンチピングスムージング",
                            "traditional_chinese": "預測量",
                            "korean": "양",
                        },
                        ("Apply", ""): {
                            "simplified_chinese": "应用",
                        },
                    },
                )

            text = path.read_text(encoding="utf-8")
            self.assertIn(
                "[message.translations]\n"
                'simplified_chinese = "平滑预测"\n'
                'traditional_chinese = "預測量"\n'
                'japanese = "アンチピングスムージング"\n'
                'korean = "양"\n'
                "[[message]]",
                text,
            )
            tomllib.loads(text)

    def test_patch_module_store_rewrites_touched_block_with_stable_language_order(self):
        with tempfile.TemporaryDirectory() as tmp:
            translations_dir = Path(tmp)
            path = translations_dir / "menus.toml"
            path.write_text(
                """[[message]]
key = "Apply"
[message.translations]
polish = "Zastosuj"
simplified_chinese = "应用"
japanese = "適用"
""",
                encoding="utf-8",
                newline="\n",
            )

            with mock.patch.object(i18n_store, "TRANSLATIONS_DIR", translations_dir):
                i18n_store.patch_module_store(
                    "menus",
                    {
                        ("Apply", ""): {
                            "traditional_chinese": "套用",
                        },
                    },
                )

            text = path.read_text(encoding="utf-8")
            self.assertIn(
                "[message.translations]\n"
                'simplified_chinese = "应用"\n'
                'traditional_chinese = "套用"\n'
                'japanese = "適用"\n'
                'polish = "Zastosuj"',
                text,
            )
            tomllib.loads(text)

    def test_module_name_for_source_uses_code_boundaries(self):
        self.assertEqual(
            i18n_store.module_name_for_source(
                Path("src/game/client/components/menus_browser.cpp")
            ),
            "server_browser",
        )
        self.assertEqual(
            i18n_store.module_name_for_source(Path("src/game/client/gameclient.cpp")),
            "loading",
        )

    def test_language_map_for_flattens_selected_language(self):
        store = {
            "menus": {
                ("Play", "Start menu"): {
                    "simplified_chinese": "开始游戏",
                    "traditional_chinese": "開始遊戲",
                }
            }
        }
        self.assertEqual(
            i18n_store.language_map_for(store, "simplified_chinese"),
            {("Play", "Start menu"): "开始游戏"},
        )

    def test_missing_translations_for_reports_identities_without_selected_language(
        self,
    ):
        store = {
            "menus": {
                ("Play", "Start menu"): {
                    "simplified_chinese": "开始游戏",
                },
                ("Quit", ""): {
                    "traditional_chinese": "結束",
                },
            }
        }
        self.assertEqual(
            i18n_store.missing_translations_for(
                store,
                [("Play", "Start menu"), ("Quit", ""), ("Open", "")],
                "simplified_chinese",
            ),
            [("Quit", ""), ("Open", "")],
        )


if __name__ == "__main__":
    unittest.main()
