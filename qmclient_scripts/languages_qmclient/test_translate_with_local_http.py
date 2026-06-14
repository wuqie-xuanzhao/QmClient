#!/usr/bin/env python3

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from qmclient_scripts.languages_qmclient import i18n_store
from qmclient_scripts.languages_qmclient import local_http_client
from qmclient_scripts.languages_qmclient import translate_with_local_http


class LocalHttpClientTest(unittest.TestCase):
    def test_build_chat_payload_uses_openai_compatible_shape(self):
        payload = local_http_client.build_chat_payload(
            "local-model",
            [local_http_client.ChatMessage("system", "rules")],
            temperature=0.1,
        )
        self.assertEqual(payload["model"], "local-model")
        self.assertEqual(payload["messages"][0]["role"], "system")
        self.assertEqual(payload["temperature"], 0.1)

    def test_extract_response_text_reads_first_choice_message(self):
        text = local_http_client.extract_response_text(
            {"choices": [{"message": {"content": "[]"}}]}
        )
        self.assertEqual(text, "[]")


class TranslateWithLocalHttpTest(unittest.TestCase):
    def test_parse_response_requires_json_array(self):
        parsed = translate_with_local_http.parse_response(
            '[{"key":"Play","context":"","translation":"开始"}]'
        )
        self.assertEqual(parsed[0]["translation"], "开始")
        with self.assertRaises(ValueError):
            translate_with_local_http.parse_response('{"key":"Play"}')

    def test_parse_response_rejects_non_object_items(self):
        with self.assertRaises(ValueError):
            translate_with_local_http.parse_response('["bad"]')

    def test_write_draft_module_writes_mirror_toml(self):
        tasks = [
            translate_with_local_http.TranslationTask(
                "menus", ("Play", "Start menu"), "Play"
            )
        ]
        with tempfile.TemporaryDirectory() as tmp:
            draft_root = Path(tmp)
            with mock.patch.object(
                translate_with_local_http, "TRANSLATIONS_DRAFT_DIR", draft_root
            ):
                out_path = translate_with_local_http.write_draft_module(
                    "simplified_chinese",
                    "menus",
                    tasks,
                    {("Play", "Start menu"): "开始游戏"},
                )
            self.assertTrue(out_path.exists())
            parsed = out_path.read_text(encoding="utf-8")
            self.assertIn('key = "Play"', parsed)
            self.assertIn('context = "Start menu"', parsed)
            self.assertIn('simplified_chinese = "开始游戏"', parsed)

    def test_write_draft_module_preserves_existing_draft_entries(self):
        tasks = [
            translate_with_local_http.TranslationTask("menus", ("Quit", ""), "Quit")
        ]
        with tempfile.TemporaryDirectory() as tmp:
            draft_root = Path(tmp)
            language_dir = draft_root / "simplified_chinese"
            language_dir.mkdir(parents=True)
            (language_dir / "menus.toml").write_text(
                """[[message]]
key = "Play"
[message.translations]
simplified_chinese = "开始游戏"
""",
                encoding="utf-8",
            )
            with mock.patch.object(
                translate_with_local_http, "TRANSLATIONS_DRAFT_DIR", draft_root
            ):
                out_path = translate_with_local_http.write_draft_module(
                    "simplified_chinese",
                    "menus",
                    tasks,
                    {("Quit", ""): "退出"},
                )
            content = out_path.read_text(encoding="utf-8")
            self.assertIn('key = "Play"', content)
            self.assertIn('key = "Quit"', content)

    def test_collect_tasks_only_returns_missing_entries(self):
        records = [
            mock.Mock(
                key="Play",
                source=Path("src/game/client/components/menus.cpp"),
                identity=mock.Mock(return_value=("Play", "")),
            ),
            mock.Mock(
                key="Quit",
                source=Path("src/game/client/components/menus.cpp"),
                identity=mock.Mock(return_value=("Quit", "")),
            ),
        ]
        store = {
            "menus": {
                ("Play", ""): {"simplified_chinese": "开始游戏"},
                ("Quit", ""): {"simplified_chinese": "Quit"},
            }
        }
        with mock.patch.object(
            translate_with_local_http.source_keys,
            "collect_source_key_records",
            return_value=records,
        ), mock.patch.object(
            translate_with_local_http.i18n_store,
            "load_language_store",
            return_value=store,
        ):
            tasks = translate_with_local_http.collect_tasks("simplified_chinese")
        self.assertEqual([task.identity for task in tasks], [("Quit", "")])

    def test_collect_tasks_keeps_extra_source_keys_without_source_file(self):
        records = [
            mock.Mock(
                key="Auto reply",
                source=None,
                identity=mock.Mock(return_value=("Auto reply", "")),
            )
        ]
        with mock.patch.object(
            translate_with_local_http.source_keys,
            "collect_source_key_records",
            return_value=records,
        ), mock.patch.object(
            translate_with_local_http.i18n_store,
            "load_language_store",
            return_value={},
        ):
            tasks = translate_with_local_http.collect_tasks("korean")
        self.assertEqual(len(tasks), 1)
        self.assertEqual(tasks[0].module, "misc")

    def test_apply_translations_to_store_updates_non_simplified_language(self):
        store = {
            "menus": {
                ("Play", ""): {"simplified_chinese": "开始游戏"},
            }
        }
        tasks = [
            translate_with_local_http.TranslationTask("menus", ("Play", ""), "Play")
        ]
        updated = translate_with_local_http.apply_translations_to_store(
            store,
            "korean",
            tasks,
            {("Play", ""): "플레이"},
        )
        self.assertEqual(
            updated["menus"][("Play", "")],
            {"simplified_chinese": "开始游戏", "korean": "플레이"},
        )

    def test_should_write_draft_only_for_simplified_chinese(self):
        self.assertTrue(translate_with_local_http.should_write_draft("simplified_chinese"))
        self.assertFalse(translate_with_local_http.should_write_draft("korean"))

    def test_api_key_uses_environment_fallback(self):
        with mock.patch.dict(os.environ, {"QMCLIENT_LOCAL_HTTP_API_KEY": "env-key"}):
            self.assertEqual(translate_with_local_http.resolve_api_key(""), "env-key")
            self.assertEqual(
                translate_with_local_http.resolve_api_key("cli-key"), "cli-key"
            )

    def test_validate_translations_accepts_matching_items(self):
        tasks = [
            translate_with_local_http.TranslationTask("menus", ("Play", ""), "Play"),
            translate_with_local_http.TranslationTask("menus", ("Quit", ""), "Quit"),
        ]
        translations, failures = translate_with_local_http.validate_translations(
            tasks,
            [
                {"key": "Play", "context": "", "translation": "开始游戏"},
                {"key": "Quit", "context": "", "translation": "退出"},
            ],
        )
        self.assertEqual(
            translations,
            {
                ("Play", ""): "开始游戏",
                ("Quit", ""): "退出",
            },
        )
        self.assertEqual(failures, [])

    def test_validate_translations_rejects_wrong_keys_and_bad_translations(self):
        tasks = [
            translate_with_local_http.TranslationTask("menus", ("Play", ""), "Play"),
        ]
        translations, failures = translate_with_local_http.validate_translations(
            tasks,
            [
                {"key": "Quit", "context": "", "translation": "退出"},
                {"key": "Play", "context": "", "translation": "Play"},
            ],
        )
        self.assertEqual(translations, {})
        self.assertEqual(len(failures), 3)
        self.assertIn("unexpected identity", failures[0])
        self.assertIn("translation unchanged from source", failures[1])
        self.assertIn("missing translation", failures[2])

    def test_load_existing_draft_identities_reads_existing_module_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            draft_root = Path(tmp)
            language_dir = draft_root / "simplified_chinese"
            language_dir.mkdir(parents=True)
            (language_dir / "menus.toml").write_text(
                """[[message]]
key = "Play"
[message.translations]
simplified_chinese = "开始游戏"
""",
                encoding="utf-8",
            )
            with mock.patch.object(
                translate_with_local_http, "TRANSLATIONS_DRAFT_DIR", draft_root
            ):
                identities = translate_with_local_http.load_existing_draft_identities(
                    "simplified_chinese"
                )
            self.assertEqual(identities, {("Play", "")})

    def test_parse_languages_and_modules(self):
        self.assertEqual(
            translate_with_local_http.parse_csv_values(" a, b ,,c "),
            ["a", "b", "c"],
        )

    def test_main_resume_module_and_limit_skip_existing_draft(self):
        records = [
            mock.Mock(
                key="Play",
                source=Path("src/game/client/components/menus_settings.cpp"),
                identity=mock.Mock(return_value=("Play", "")),
            ),
            mock.Mock(
                key="Quit",
                source=Path("src/game/client/components/menus_settings.cpp"),
                identity=mock.Mock(return_value=("Quit", "")),
            ),
            mock.Mock(
                key="Refresh",
                source=Path("src/game/client/components/menus_browser.cpp"),
                identity=mock.Mock(return_value=("Refresh", "")),
            ),
        ]
        store = {"menus": {}, "server_browser": {}}
        fake_client = mock.Mock()
        fake_client.chat_completion.return_value = json.dumps(
            [{"key": "Quit", "context": "", "translation": "退出"}]
        )
        with tempfile.TemporaryDirectory() as tmp:
            draft_root = Path(tmp)
            language_dir = draft_root / "simplified_chinese"
            language_dir.mkdir(parents=True)
            (language_dir / "menus.toml").write_text(
                """[[message]]
key = "Play"
[message.translations]
simplified_chinese = "开始游戏"
""",
                encoding="utf-8",
            )
            argv = [
                "translate_with_local_http.py",
                "--languages",
                "simplified_chinese",
                "--base-url",
                "http://127.0.0.1:1337/v1",
                "--model",
                "local-model",
                "--module",
                "menus",
                "--limit",
                "1",
                "--resume",
            ]
            with mock.patch.object(sys, "argv", argv), mock.patch.object(
                translate_with_local_http.source_keys,
                "collect_source_key_records",
                return_value=records,
            ), mock.patch.object(
                translate_with_local_http.i18n_store,
                "load_language_store",
                return_value=store,
            ), mock.patch.object(
                translate_with_local_http, "TRANSLATIONS_DRAFT_DIR", draft_root
            ), mock.patch.object(
                translate_with_local_http, "LocalHttpClient", return_value=fake_client
            ):
                translate_with_local_http.main()
            fake_client.chat_completion.assert_called_once()
            content = (language_dir / "menus.toml").read_text(encoding="utf-8")
            self.assertIn('key = "Quit"', content)
            self.assertNotIn('key = "Refresh"', content)

    def test_main_dry_run_does_not_write_draft(self):
        records = [
            mock.Mock(
                key="Play",
                source=Path("src/game/client/components/menus.cpp"),
                identity=mock.Mock(return_value=("Play", "")),
            ),
        ]
        store = {"menus": {}}
        with tempfile.TemporaryDirectory() as tmp:
            draft_root = Path(tmp)
            argv = [
                "translate_with_local_http.py",
                "--languages",
                "simplified_chinese",
                "--base-url",
                "http://127.0.0.1:1337/v1",
                "--model",
                "local-model",
                "--dry-run",
            ]
            with mock.patch.object(sys, "argv", argv), mock.patch.object(
                translate_with_local_http.source_keys,
                "collect_source_key_records",
                return_value=records,
            ), mock.patch.object(
                translate_with_local_http.i18n_store,
                "load_language_store",
                return_value=store,
            ), mock.patch.object(
                translate_with_local_http, "TRANSLATIONS_DRAFT_DIR", draft_root
            ):
                translate_with_local_http.main()
            self.assertFalse((draft_root / "simplified_chinese").exists())

    def test_main_batch_failure_does_not_abort_other_batches(self):
        records = [
            mock.Mock(
                key="Play",
                source=Path("src/game/client/components/menus_settings.cpp"),
                identity=mock.Mock(return_value=("Play", "")),
            ),
            mock.Mock(
                key="Quit",
                source=Path("src/game/client/components/menus_settings.cpp"),
                identity=mock.Mock(return_value=("Quit", "")),
            ),
        ]
        store = {"menus": {}}
        fake_client = mock.Mock()
        fake_client.chat_completion.side_effect = [
            "not-json",
            json.dumps([{"key": "Quit", "context": "", "translation": "退出"}]),
        ]
        with tempfile.TemporaryDirectory() as tmp:
            draft_root = Path(tmp)
            argv = [
                "translate_with_local_http.py",
                "--languages",
                "simplified_chinese",
                "--base-url",
                "http://127.0.0.1:1337/v1",
                "--model",
                "local-model",
                "--module",
                "menus",
                "--batch-size",
                "1",
            ]
            with mock.patch.object(sys, "argv", argv), mock.patch.object(
                translate_with_local_http.source_keys,
                "collect_source_key_records",
                return_value=records,
            ), mock.patch.object(
                translate_with_local_http.i18n_store,
                "load_language_store",
                return_value=store,
            ), mock.patch.object(
                translate_with_local_http, "TRANSLATIONS_DRAFT_DIR", draft_root
            ), mock.patch.object(
                translate_with_local_http, "LocalHttpClient", return_value=fake_client
            ):
                translate_with_local_http.main()
            content = (
                draft_root / "simplified_chinese" / "menus.toml"
            ).read_text(encoding="utf-8")
            self.assertIn('key = "Quit"', content)
            self.assertNotIn('key = "Play"', content)


if __name__ == "__main__":
    unittest.main()
