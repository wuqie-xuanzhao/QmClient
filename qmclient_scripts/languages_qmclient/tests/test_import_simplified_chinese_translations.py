#!/usr/bin/env python3

import unittest
from pathlib import Path
from unittest import mock

from qmclient_scripts.languages_qmclient import import_simplified_chinese_translations


class ImportSimplifiedChineseTranslationsTest(unittest.TestCase):
    def test_main_merges_simplified_chinese_without_wiping_other_languages(self):
        sc_only_store = {
            "menus": {
                ("Play", ""): {"simplified_chinese": "开始游戏"},
                ("Quit", ""): {"simplified_chinese": "退出"},
            }
        }
        existing_store = {
            "menus": {
                ("Play", ""): {
                    "simplified_chinese": "开始",
                    "korean": "플레이",
                    "german": "Spielen",
                },
                ("Quit", ""): {
                    "korean": "종료",
                },
                ("Settings", ""): {
                    "korean": "설정",
                    "simplified_chinese": "设置",
                },
            }
        }
        patch_calls: list[tuple[str, dict]] = []

        def fake_patch(module_name, entries):
            patch_calls.append((module_name, entries))
            module = existing_store.setdefault(module_name, {})
            for identity, translations in entries.items():
                module.setdefault(identity, {}).update(translations)

        with (
            mock.patch.object(
                import_simplified_chinese_translations,
                "import_translations",
                return_value=sc_only_store,
            ),
            mock.patch.object(
                import_simplified_chinese_translations.i18n_store,
                "load_language_store",
                return_value=existing_store,
            ),
            mock.patch.object(
                import_simplified_chinese_translations.i18n_store,
                "patch_module_store",
                side_effect=fake_patch,
            ) as patch_module_store,
            mock.patch.object(
                import_simplified_chinese_translations.i18n_store,
                "write_language_store",
            ) as write_language_store,
        ):
            import_simplified_chinese_translations.main()

        write_language_store.assert_not_called()
        patch_module_store.assert_called()
        self.assertEqual(len(patch_calls), 1)
        module_name, entries = patch_calls[0]
        self.assertEqual(module_name, "menus")
        self.assertEqual(
            entries[("Play", "")],
            {"simplified_chinese": "开始游戏"},
        )
        self.assertEqual(
            entries[("Quit", "")],
            {"simplified_chinese": "退出"},
        )
        # other languages preserved on existing identities
        self.assertEqual(
            existing_store["menus"][("Play", "")],
            {
                "simplified_chinese": "开始游戏",
                "korean": "플레이",
                "german": "Spielen",
            },
        )
        self.assertEqual(
            existing_store["menus"][("Quit", "")],
            {"korean": "종료", "simplified_chinese": "退出"},
        )
        # identities not in SC import stay untouched
        self.assertEqual(
            existing_store["menus"][("Settings", "")],
            {"korean": "설정", "simplified_chinese": "设置"},
        )

    def test_main_patches_each_module_from_sc_store(self):
        sc_only_store = {
            "menus": {("Play", ""): {"simplified_chinese": "开始游戏"}},
            "chat": {("Say", ""): {"simplified_chinese": "说"}},
        }
        existing_store = {
            "menus": {("Play", ""): {"korean": "플레이"}},
            "chat": {("Say", ""): {"korean": "말하기"}},
        }
        patched_modules: list[str] = []

        def fake_patch(module_name, entries):
            patched_modules.append(module_name)
            module = existing_store.setdefault(module_name, {})
            for identity, translations in entries.items():
                module.setdefault(identity, {}).update(translations)

        with (
            mock.patch.object(
                import_simplified_chinese_translations,
                "import_translations",
                return_value=sc_only_store,
            ),
            mock.patch.object(
                import_simplified_chinese_translations.i18n_store,
                "load_language_store",
                return_value=existing_store,
            ),
            mock.patch.object(
                import_simplified_chinese_translations.i18n_store,
                "patch_module_store",
                side_effect=fake_patch,
            ),
            mock.patch.object(
                import_simplified_chinese_translations.i18n_store,
                "write_language_store",
            ) as write_language_store,
        ):
            import_simplified_chinese_translations.main()

        write_language_store.assert_not_called()
        self.assertEqual(sorted(patched_modules), ["chat", "menus"])
        self.assertEqual(
            existing_store["menus"][("Play", "")],
            {"korean": "플레이", "simplified_chinese": "开始游戏"},
        )
        self.assertEqual(
            existing_store["chat"][("Say", "")],
            {"korean": "말하기", "simplified_chinese": "说"},
        )


if __name__ == "__main__":
    unittest.main()
