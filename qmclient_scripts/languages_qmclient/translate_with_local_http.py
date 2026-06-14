#!/usr/bin/env python3
"""Fill translation TOML entries from a local HTTP chat-completions API."""

from __future__ import annotations

import argparse
import json
import os
from dataclasses import dataclass
from pathlib import Path

try:
    from . import i18n_store
    from . import source_keys
    from .local_http_client import ChatMessage, LocalHttpClient
except ImportError:  # pragma: no cover - script entrypoint fallback
    import i18n_store
    import source_keys
    from local_http_client import ChatMessage, LocalHttpClient

SCRIPT_DIR = Path(__file__).resolve().parent
TRANSLATIONS_DRAFT_DIR = SCRIPT_DIR / "translations_draft"
PROMPT_ASSETS_DIR = SCRIPT_DIR / "prompt_assets"


@dataclass(frozen=True)
class TranslationTask:
    module: str
    identity: tuple[str, str]
    source_text: str
    existing_translation: str = ""


def read_text(path: Path, fallback: str = "") -> str:
    if not path.exists():
        return fallback
    return path.read_text(encoding="utf-8")


def load_system_prompt() -> str:
    return read_text(PROMPT_ASSETS_DIR / "system_prompt.md", "# Translation task\n")


def load_terminology() -> str:
    return read_text(PROMPT_ASSETS_DIR / "terminology.toml", "")


def load_few_shots() -> str:
    return read_text(PROMPT_ASSETS_DIR / "few_shots.toml", "")


def parse_csv_values(value: str) -> list[str]:
    return [item.strip() for item in value.split(",") if item.strip()]


def should_write_draft(language: str) -> bool:
    return language == "simplified_chinese"


def resolve_api_key(cli_value: str) -> str:
    return cli_value or os.environ.get("QMCLIENT_LOCAL_HTTP_API_KEY", "")


def collect_tasks(
    language: str,
    *,
    modules: set[str] | None = None,
    limit: int | None = None,
    existing_draft_identities: set[tuple[str, str]] | None = None,
) -> list[TranslationTask]:
    records = source_keys.collect_source_key_records()
    store = i18n_store.load_language_store()
    translations = i18n_store.language_map_for(store, language)
    skipped = existing_draft_identities or set()
    tasks: list[TranslationTask] = []
    for record in records:
        identity = record.identity()
        module = i18n_store.module_name_for_source(record.source)
        if modules and module not in modules:
            continue
        if identity in skipped:
            continue
        existing = translations.get(identity, "")
        if existing and existing != record.key:
            continue
        tasks.append(TranslationTask(module, identity, record.key, existing))
        if limit is not None and len(tasks) >= limit:
            break
    return tasks


def neighboring_translations(
    language: str,
    module: str,
    *,
    max_items: int = 8,
) -> list[dict[str, str]]:
    store = i18n_store.load_language_store()
    entries = store.get(module, {})
    samples: list[dict[str, str]] = []
    for (key, context), translations in sorted(entries.items()):
        translation = translations.get(language, "")
        if not translation:
            continue
        samples.append(
            {
                "key": key,
                "context": context,
                "translation": translation,
            }
        )
        if len(samples) >= max_items:
            break
    return samples


def render_prompt(language: str, module: str, tasks: list[TranslationTask]) -> str:
    lines = [
        load_system_prompt().strip(),
        "",
        f"Target language: {language}",
        f"Module: {module}",
        "",
        "Terminology:",
        load_terminology().strip(),
        "",
        "Few shots:",
        load_few_shots().strip(),
        "",
        "Existing neighboring translations:",
        json.dumps(
            neighboring_translations(language, module),
            ensure_ascii=False,
            indent=2,
        ),
        "",
        "Translate these entries and return JSON only.",
        json.dumps(
            [
                {
                    "key": task.identity[0],
                    "context": task.identity[1],
                    "source": task.source_text,
                }
                for task in tasks
            ],
            ensure_ascii=False,
            indent=2,
        ),
    ]
    return "\n".join(line for line in lines if line != "")


def parse_response(text: str) -> list[dict]:
    payload = json.loads(text)
    if not isinstance(payload, list):
        raise ValueError("translation response must be a JSON array")
    for item in payload:
        if not isinstance(item, dict):
            raise ValueError("translation response must contain JSON objects")
    return payload


def validate_translations(
    tasks: list[TranslationTask], response_items: list[dict]
) -> tuple[dict[tuple[str, str], str], list[str]]:
    requested = {task.identity: task for task in tasks}
    remaining = set(requested)
    translated: dict[tuple[str, str], str] = {}
    failures: list[str] = []
    for item in response_items:
        key = item.get("key")
        context = item.get("context", "")
        translation = item.get("translation", "")
        identity = (key, context)
        if not isinstance(key, str) or not isinstance(context, str):
            failures.append(f"invalid identity in response item: {item!r}")
            continue
        if identity not in requested:
            failures.append(f"unexpected identity returned: {identity!r}")
            continue
        if identity in translated:
            failures.append(f"duplicate identity returned: {identity!r}")
            continue
        if not isinstance(translation, str) or not translation.strip():
            failures.append(f"empty translation returned: {identity!r}")
            continue
        task = requested[identity]
        if translation == task.source_text:
            failures.append(f"translation unchanged from source: {identity!r}")
            continue
        translated[identity] = translation
        remaining.discard(identity)
    for identity in sorted(remaining):
        failures.append(f"missing translation for requested identity: {identity!r}")
    return translated, failures


def load_existing_draft_identities(language: str) -> set[tuple[str, str]]:
    draft_dir = TRANSLATIONS_DRAFT_DIR / language
    identities: set[tuple[str, str]] = set()
    if not draft_dir.exists():
        return identities
    import tomllib

    for path in sorted(draft_dir.glob("*.toml")):
        with path.open("rb") as file:
            parsed = tomllib.load(file)
        for entry in parsed.get("message", []):
            if not isinstance(entry, dict):
                continue
            key = entry.get("key", "")
            context = entry.get("context", "")
            if isinstance(key, str) and key:
                identities.add((key, context if isinstance(context, str) else ""))
    return identities


def load_existing_draft_module(
    language: str, module: str
) -> dict[tuple[str, str], dict[str, str]]:
    path = TRANSLATIONS_DRAFT_DIR / language / f"{module}.toml"
    if not path.exists():
        return {}
    import tomllib

    with path.open("rb") as file:
        data = tomllib.load(file)
    entries: dict[tuple[str, str], dict[str, str]] = {}
    for entry in data.get("message", []):
        if not isinstance(entry, dict):
            continue
        key = entry.get("key", "")
        context = entry.get("context", "")
        translations = entry.get("translations", {})
        if not isinstance(key, str) or not key or not isinstance(translations, dict):
            continue
        normalized = {
            item_language: value
            for item_language, value in translations.items()
            if isinstance(item_language, str) and isinstance(value, str) and value
        }
        if normalized:
            entries[(key, context if isinstance(context, str) else "")] = normalized
    return entries


def write_draft_module(
    language: str,
    module: str,
    tasks: list[TranslationTask],
    translations: dict[tuple[str, str], str],
    *,
    resume: bool = False,
) -> Path:
    draft_dir = TRANSLATIONS_DRAFT_DIR / language
    draft_dir.mkdir(parents=True, exist_ok=True)
    out_path = draft_dir / f"{module}.toml"
    existing = load_existing_draft_module(language, module)
    merged = dict(existing)
    for task in tasks:
        translation = translations.get(task.identity, "")
        if not translation:
            continue
        entry = dict(merged.get(task.identity, {}))
        entry[language] = translation
        merged[task.identity] = entry
    messages = [
        (i18n_store.Message(key, context), translation_map)
        for (key, context), translation_map in merged.items()
    ]
    out_path.write_text(i18n_store.dump_module(messages), encoding="utf-8", newline="\n")
    return out_path


def apply_translations_to_store(
    store: dict[str, dict[tuple[str, str], dict[str, str]]],
    language: str,
    tasks: list[TranslationTask],
    translations: dict[tuple[str, str], str],
) -> dict[str, dict[tuple[str, str], dict[str, str]]]:
    updated = {
        module: {
            identity: dict(translation_map)
            for identity, translation_map in module_entries.items()
        }
        for module, module_entries in store.items()
    }
    for task in tasks:
        translation = translations.get(task.identity, "")
        if not translation:
            continue
        entry = dict(updated.setdefault(task.module, {}).get(task.identity, {}))
        entry[language] = translation
        updated[task.module][task.identity] = entry
    return updated


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("--languages", default="simplified_chinese")
    parser.add_argument("--base-url", required=True)
    parser.add_argument("--model", required=True)
    parser.add_argument("--api-key", default="")
    parser.add_argument("--module", default="")
    parser.add_argument("--modules", default="")
    parser.add_argument("--limit", type=int)
    parser.add_argument("--batch-size", type=int, default=32)
    parser.add_argument("--resume", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    return parser


def selected_modules(args: argparse.Namespace) -> set[str]:
    values = parse_csv_values(args.modules)
    if args.module:
        values.extend(parse_csv_values(args.module))
    return set(values)


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    client = LocalHttpClient(
        base_url=args.base_url,
        model=args.model,
        api_key=resolve_api_key(args.api_key),
    )
    modules = selected_modules(args)
    languages = parse_csv_values(args.languages)
    for language in languages:
        existing_draft = (
            load_existing_draft_identities(language)
            if args.resume and should_write_draft(language)
            else set()
        )
        tasks = collect_tasks(
            language,
            modules=modules or None,
            limit=args.limit,
            existing_draft_identities=existing_draft,
        )
        if not tasks:
            print(f"{language}: nothing to translate")
            continue
        grouped: dict[str, list[TranslationTask]] = {}
        for task in tasks:
            grouped.setdefault(task.module, []).append(task)
        for module, module_tasks in sorted(grouped.items()):
            translated: dict[tuple[str, str], str] = {}
            failures: list[str] = []
            for index in range(0, len(module_tasks), args.batch_size):
                batch = module_tasks[index : index + args.batch_size]
                prompt = render_prompt(language, module, batch)
                if args.dry_run:
                    print(prompt)
                    continue
                try:
                    response = client.chat_completion(
                        [
                            ChatMessage("system", load_system_prompt()),
                            ChatMessage("user", prompt),
                        ]
                    )
                    batch_translations, batch_failures = validate_translations(
                        batch, parse_response(response)
                    )
                except (RuntimeError, ValueError, json.JSONDecodeError) as exc:
                    batch_translations = {}
                    batch_failures = [f"batch {module}#{index // args.batch_size + 1}: {exc}"]
                translated.update(batch_translations)
                failures.extend(batch_failures)
            if args.dry_run:
                continue
            if should_write_draft(language):
                out_path = write_draft_module(
                    language,
                    module,
                    module_tasks,
                    translated,
                    resume=args.resume,
                )
                print(
                    f"{language}: wrote draft {out_path} "
                    f"({len(translated)}/{len(module_tasks)} translated, {len(failures)} failed)"
                )
            else:
                store = i18n_store.load_language_store()
                updated_store = apply_translations_to_store(
                    store, language, module_tasks, translated
                )
                i18n_store.write_language_store(updated_store)
                print(
                    f"{language}: updated translations/i18n/{module}.toml "
                    f"({len(translated)}/{len(module_tasks)} translated, {len(failures)} failed)"
                )
            for failure in failures:
                print(f"  - {failure}")


if __name__ == "__main__":
    main()
