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
                "\n"
                "[[message]]",
                text,
            )
            tomllib.loads(text)

    def test_patch_module_store_keeps_blank_line_between_message_blocks(self):
        with tempfile.TemporaryDirectory() as tmp:
            translations_dir = Path(tmp)
            path = translations_dir / "menus.toml"
            path.write_text(
                """[[message]]
key = "Apply"
[message.translations]
simplified_chinese = "应用"

[[message]]
key = "Cancel"
[message.translations]
simplified_chinese = "取消"
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
                'traditional_chinese = "套用"\n\n[[message]]\nkey = "Cancel"',
                text,
            )
            self.assertEqual(i18n_store.toml_format_errors(path), [])

    def test_toml_format_errors_rejects_adjacent_message_blocks(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "menus.toml"
            path.write_text(
                """[[message]]
key = "Apply"
[message.translations]
simplified_chinese = "应用"
[[message]]
key = "Cancel"
[message.translations]
simplified_chinese = "取消"
""",
                encoding="utf-8",
                newline="\n",
            )

            errors = i18n_store.toml_format_errors(path)

        self.assertEqual(len(errors), 1)
        self.assertIn("missing blank line before [[message]]", errors[0])

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

    def test_missing_translations_for_skips_source_like_tokens(self):
        self.assertEqual(
            i18n_store.missing_translations_for(
                {},
                [
                    ("DeepSeek API Key", ""),
                    ("OpenAI API Key", ""),
                    ("https://ddnet.org/discord", ""),
                    ("Active line font size", ""),
                ],
                "spanish",
            ),
            [
                ("DeepSeek API Key", ""),
                ("OpenAI API Key", ""),
                ("Active line font size", ""),
            ],
        )

    def test_translation_quality_rejects_cjk_source_fallback_for_non_chinese(self):
        store = {
            "qmclient": {
                ("DeepSeek 模型名称", ""): {
                    "spanish": "DeepSeek 模型名称",
                    "simplified_chinese": "DeepSeek模型名称",
                }
            }
        }

        errors = i18n_store.translation_quality_errors(store)

        self.assertTrue(
            any("spanish repeats CJK source key" in item for item in errors)
        )
        self.assertTrue(any("simplified_chinese typography" in item for item in errors))

    def test_translation_quality_rejects_english_source_fallback_for_non_chinese(self):
        store = {
            "qmclient": {
                ("Enable lyrics HUD overlay", ""): {
                    "spanish": "Enable lyrics HUD overlay",
                    "simplified_chinese": "启用歌词 HUD 叠加层",
                }
            }
        }

        errors = i18n_store.translation_quality_errors(store)

        self.assertTrue(
            any("spanish repeats English source key" in item for item in errors)
        )

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

    def test_translation_quality_rejects_simplified_chinese_terminology_mismatch(self):
        store = {
            "menus": {
                ("Grenade", ""): {
                    "simplified_chinese": "榴弹炮",
                }
            }
        }

        errors = i18n_store.translation_quality_errors(
            store,
            terminology_by_language={"simplified_chinese": {"Grenade": "榴弹枪"}},
        )

        self.assertTrue(any("terminology mismatch" in item for item in errors))
        self.assertTrue(any("榴弹枪" in item for item in errors))

    def test_translation_quality_can_be_limited_to_active_identities(self):
        store = {
            "qmclient": {
                ("Old key", ""): {
                    "spanish": "Old key",
                },
                ("Active key with words", ""): {
                    "spanish": "Active key with words",
                },
            }
        }

        errors = i18n_store.translation_quality_errors(
            store, active_identities={("Active key with words", "")}
        )

        self.assertEqual(len(errors), 1)
        self.assertIn("Active key with words", errors[0])

    def test_translation_quality_allows_source_like_tokens(self):
        store = {
            "menus": {
                ("auto", ""): {"spanish": "auto"},
                ("Shotgun", ""): {"portuguese": "Shotgun"},
                ("entity_bg (Workshop)", ""): {
                    "spanish": "entity_bg (Workshop)",
                    "japanese": "entity_bg (Workshop)",
                },
                ("https://ddnet.org/discord", ""): {
                    "spanish": "https://ddnet.org/discord"
                },
                ("Demo", ""): {"spanish": "Demo"},
                ("HUD", ""): {"spanish": "HUD"},
            }
        }

        self.assertEqual(i18n_store.translation_quality_errors(store), [])

    def test_translation_quality_allows_japanese_and_korean_cjk_text(self):
        store = {
            "menus": {
                ("Play", ""): {
                    "japanese": "プレイ",
                    "korean": "플레이",
                    "spanish": "Jugar",
                    "simplified_chinese": "开始游戏",
                }
            }
        }

        self.assertEqual(i18n_store.translation_quality_errors(store), [])

    def test_translation_quality_allows_cjk_names_inside_foreign_sentences(self):
        store = {
            "qmclient": {
                ("Example", ""): {
                    "turkish": "Pasta menüsü yeniden adlandırma listesi, örn: 璇梦1|璇梦2",
                    "simplified_chinese": "示例",
                }
            }
        }

        self.assertEqual(i18n_store.translation_quality_errors(store), [])

    def test_translation_quality_report_warns_for_placeholder_mismatch(self):
        store = {
            "menus": {
                ("Player %s joined", ""): {
                    "spanish": "El jugador se unió",
                    "simplified_chinese": "玩家 %s 加入",
                }
            }
        }

        report = i18n_store.translation_quality_report(store)

        self.assertEqual(report.errors, [])
        self.assertTrue(
            any("placeholder mismatch" in item for item in report.warnings),
            report.warnings,
        )

    def test_translation_quality_report_warns_for_long_translation(self):
        store = {
            "menus": {
                ("Short label", ""): {
                    "spanish": "Esta traducción es deliberadamente demasiado larga para una etiqueta corta",
                }
            }
        }

        report = i18n_store.translation_quality_report(store)

        self.assertEqual(report.errors, [])
        self.assertTrue(
            any("length risk" in item for item in report.warnings),
            report.warnings,
        )


if __name__ == "__main__":
    unittest.main()
