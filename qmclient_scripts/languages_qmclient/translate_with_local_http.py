#!/usr/bin/env python3
"""Fill translation TOML entries from an OpenAI-compatible HTTP API."""

from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

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
INDEXED_LINE_RE = re.compile(r"^\s*(\d+)[\.)]\s*(.*)\s*$")
MAX_PARALLEL_REQUESTS = 10
SAME_SOURCE_ALLOWED_BY_LANGUAGE = {
    "german": {
        "%c Team %d",
        "Alpha",
        "Animation",
        "Audio",
        "Auto",
        "Chat",
        "Clan",
        "Classic Easy",
        "Classic Next",
        "Classic Nut",
        "Classic Pro",
        "Client",
        "Combo",
        "Community",
        "Controller",
        "Credits",
        "Dummy",
        "Editor",
        "Emoticon",
        "Emoticons",
        "Event",
        "Extras",
        "Hammer",
        "Info",
        "Internet",
        "Jitter",
        "Kaomoji",
        "Laser",
        "Layout",
        "Live",
        "Name",
        "Name:",
        "Name: %s",
        "Normal",
        "Offline",
        "Ohhhhhhhhhhhhhhhh",
        "Pause",
        "Position:",
        "Regex",
        "Region",
        "Renderer",
        "Ring",
        "Screenshot",
        "Screenshots",
        "Server",
        "Skin",
        "Skin: %s",
        "Solo",
        "Status",
        "System",
        "Tags",
        "Team",
        "Team %d",
        "Team %d (%d/%d)",
        "Teams",
        "Tele",
        "Text",
        "Tutorial",
        "Update",
        "Version",
        "z = Zoom",
        "Clan: %s",
        "Hammer: %s",
    },
    "spanish": {
        " min",
        "%d dummies",
        "%d dummy",
        "%s min.",
        "Audio",
        "Auto",
        "Chat",
        "Clan",
        "Color",
        "Combo",
        "Demos",
        "Editor",
        "Dummy",
        "Endpoint",
        "Error",
        "Extras",
        "General",
        "Internet",
        "Jitter",
        "Kaomoji",
        "Literal",
        "Local",
        "Local + Dummy",
        "Manual",
        "No",
        "Normal",
        "Ohhhhhhhhhhhhhhhh",
        "Regex",
        "Simple",
        "Skin",
        "Social",
        "Solo",
        "solo",
        "Tele",
        "Total",
        "Tutorial",
        "Visual",
        "z = Zoom",
        "Clan: %s",
    },
    "french": {
        " min",
        "%d dummies",
        "%d dummy",
        "%s min.",
        "Animation",
        "Audio",
        "Auto",
        "Chat",
        "Clan",
        "Classic Easy",
        "Classic Next",
        "Classic Nut",
        "Classic Pro",
        "Client",
        "Combo",
        "Compact",
        "Console",
        "Cube",
        "Date",
        "Dummy",
        "Frags",
        "Gameplay",
        "Interface",
        "Internet",
        "Kaomoji",
        "Local",
        "Local + Dummy",
        "Alpha",
        "Laser",
        "Mention",
        "Messages",
        "Microphone",
        "Minutes",
        "Mode",
        "Mute",
        "Net",
        "Normal",
        "Note",
        "Notifications",
        "Points",
        "Ratio",
        "Regex",
        "Sat.",
        "Score",
        "Simple",
        "Skin",
        "Social",
        "Solo",
        "Suicides",
        "Total",
        "Type",
        "Types",
        "Version",
        "maximum",
        "minimum",
        "Pause",
        "Rectangle",
        "solo",
        "z = Zoom",
    },
    "brazilian_portuguese": {
        " min",
        "%d dummies",
        "%d dummy",
        "%s min.",
        "Chat",
        "Combo",
        "Config",
        "Console",
        "Dummy",
        "Editor",
        "Emoticon",
        "Emoticons",
        "Endpoint",
        "Extras",
        "Interface",
        "Internet",
        "Jitter",
        "Kaomoji",
        "Laser",
        "Layout",
        "Literal",
        "Local",
        "Local + Dummy",
        "Manual",
        "Mouse",
        "Normal",
        "Offline",
        "Ok",
        "Regex",
        "Skin",
        "Social",
        "Solo",
        "solo",
        "Status",
        "Tags",
        "Tele",
        "Total",
        "Tutorial",
        "Visual",
        "auto",
        "Ohhhhhhhhhhhhhhhh",
        "tile",
        "z = Zoom",
    },
    "portuguese": {
        " min",
        "%d dummies",
        "%d dummy",
        "%s min.",
        "Auto",
        "Chat",
        "Classic Easy",
        "Classic Next",
        "Classic Nut",
        "Classic Pro",
        "Combo",
        "Dummy",
        "Editor",
        "Emoticon",
        "Emoticons",
        "Endpoint",
        "Extras",
        "Frags",
        "Interface",
        "Internet",
        "Jitter",
        "Kaomoji",
        "Laser",
        "Literal",
        "Local",
        "Local + Dummy",
        "Manual",
        "Normal",
        "Offline",
        "Ok",
        "Regex",
        "Shotgun",
        "Skin",
        "Social",
        "Solo",
        "solo",
        "Tele",
        "Total",
        "Tutorial",
        "Visual",
        "Ohhhhhhhhhhhhhhhh",
        "tile",
        "z = Zoom",
    },
    "turkish": {
        "Dummy",
        "Kaomoji",
        "Model",
        "Net",
        "Normal",
        "Skin",
        "Solo",
        "solo",
        "minimum",
        "Ohhhhhhhhhhhhhhhh",
    },
    "polish": {
        " min",
        "%s min.",
        "Audio",
        "Auto",
        "Folder",
        "Internet",
        "Kaomoji",
        "Laser",
        "Model",
        "Offline",
        "Regex",
        "Region",
        "Restart",
        "Solo",
        "solo",
        "Status",
        "System",
    },
}


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


def load_prompt_assets() -> tuple[str, str, str]:
    return (
        read_text(PROMPT_ASSETS_DIR / "system_prompt.md", "# Translation task\n"),
        read_text(PROMPT_ASSETS_DIR / "terminology.toml", ""),
        read_text(PROMPT_ASSETS_DIR / "few_shots.toml", ""),
    )


def parse_csv_values(value: str) -> list[str]:
    return [item.strip() for item in value.split(",") if item.strip()]


def should_write_draft(language: str, write_back: bool = False) -> bool:
    return not write_back


def resolve_api_key(cli_value: str) -> str:
    return cli_value or os.environ.get("QMCLIENT_LOCAL_HTTP_API_KEY", "")


def parse_chat_extra_json(value: str) -> dict:
    if not value:
        return {}
    payload = json.loads(value)
    if not isinstance(payload, dict):
        raise ValueError("--chat-extra-json must be a JSON object")
    return payload


def build_chat_extra_body(chat_extra_json: str, reasoning_effort: str) -> dict:
    extra = parse_chat_extra_json(chat_extra_json)
    if reasoning_effort:
        extra["reasoning_effort"] = reasoning_effort
    return extra


def collect_source_text_by_identity(records) -> dict[tuple[str, str], str]:
    source_texts: dict[tuple[str, str], str] = {}
    for record in records:
        source_texts.setdefault(record.identity(), record.key)
    return source_texts


def collect_tasks(
    language: str,
    *,
    modules: set[str] | None = None,
    limit: int | None = None,
    existing_draft_identities: set[tuple[str, str]] | None = None,
    rewrite_existing: bool = False,
    records=None,
    store=None,
) -> list[TranslationTask]:
    records = (
        records if records is not None else source_keys.collect_source_key_records()
    )
    store = store if store is not None else i18n_store.load_language_store()
    translations = i18n_store.language_map_for(store, language)
    skipped = existing_draft_identities or set()
    tasks: list[TranslationTask] = []
    seen: set[tuple[str, str]] = set()
    for record in records:
        identity = record.identity()
        if identity in seen:
            continue
        seen.add(identity)
        module = i18n_store.module_name_for_source(record.source)
        if modules and module not in modules:
            continue
        if identity in skipped:
            continue
        existing = translations.get(identity, "")
        if (
            existing
            and not rewrite_existing
            and not language_quality_failure(language, record.key, existing)
        ):
            continue
        tasks.append(TranslationTask(module, identity, record.key, existing))
        if limit is not None and len(tasks) >= limit:
            break
    return tasks


def neighboring_translations(
    language: str,
    module: str,
    *,
    store: dict[str, dict[tuple[str, str], dict[str, str]]] | None = None,
    max_items: int = 8,
) -> list[dict[str, str]]:
    store = store if store is not None else i18n_store.load_language_store()
    samples: list[dict[str, str]] = []
    for (key, context), translations in sorted(store.get(module, {}).items()):
        translation = translations.get(language, "")
        if not translation:
            continue
        samples.append({"key": key, "context": context, "translation": translation})
        if len(samples) >= max_items:
            break
    return samples


def simplified_reference(
    task: TranslationTask,
    store: dict[str, dict[tuple[str, str], dict[str, str]]],
) -> str:
    return (
        store.get(task.module, {}).get(task.identity, {}).get("simplified_chinese", "")
    )


def render_prompt(
    language: str,
    module: str,
    tasks: list[TranslationTask],
    *,
    prompt_assets: tuple[str, str, str] | None = None,
    store: dict[str, dict[tuple[str, str], dict[str, str]]] | None = None,
) -> str:
    prompt_assets = prompt_assets or load_prompt_assets()
    store = store if store is not None else i18n_store.load_language_store()
    target_names = {
        "simplified_chinese": "Simplified Chinese",
        "traditional_chinese": "Traditional Chinese",
        "korean": "Korean",
        "japanese": "Japanese",
        "russian": "Russian",
        "german": "German",
        "spanish": "Spanish",
        "french": "French",
        "brazilian_portuguese": "Brazilian Portuguese",
        "portuguese": "Portuguese",
        "turkish": "Turkish",
        "polish": "Polish",
    }
    lines = [
        "# QmClient translation task",
        prompt_assets[0].strip(),
        "",
        f"Target language: {target_names.get(language, language)}",
        f"Module: {module}",
        "",
        "Terminology:",
        prompt_assets[1].strip(),
        "",
        "Few shots:",
        prompt_assets[2].strip(),
        "",
        "Existing neighboring translations:",
        json.dumps(
            neighboring_translations(language, module, store=store),
            ensure_ascii=False,
            indent=2,
        ),
        "",
        "Input:",
    ]
    for index, task in enumerate(tasks, start=1):
        reference = simplified_reference(task, store)
        if reference:
            lines.append(
                f"{index}. English: {task.source_text} | Simplified Chinese reference: {reference}"
            )
        else:
            lines.append(f"{index}. English: {task.source_text}")
    lines.extend(
        [
            "",
            "Output:",
            "Return only the following numbered translation lines. Keep every number exactly once:",
            *[f"{index}." for index in range(1, len(tasks) + 1)],
        ]
    )
    return "\n".join(lines)


def extract_placeholders(text: str) -> list[str]:
    return re.findall(
        r"%(?:\d+\$)?[+#0\- ]?(?:\d+|\*)?(?:\.\d+|\.\*)?[hljztL]*[diuoxXfFeEgGaAcspn%]",
        text,
    )


def extract_digits(text: str) -> list[str]:
    digits = re.findall(r"\d+(?:\.\d+)?", text)
    word_digits = {"five": "5"}
    for word in re.findall(r"\b[a-zA-Z]+\b", text):
        digit = word_digits.get(word.lower())
        if digit is not None:
            digits.append(digit)
    return sorted(digits, key=lambda value: (float(value), value))


def contains_hangul(text: str) -> bool:
    return any("\uac00" <= char <= "\ud7af" for char in text)


def contains_kana(text: str) -> bool:
    return any(
        "\u3040" <= char <= "\u30ff" or "\u31f0" <= char <= "\u31ff" for char in text
    )


def contains_cjk(text: str) -> bool:
    return any("\u4e00" <= char <= "\u9fff" for char in text)


def contains_cyrillic(text: str) -> bool:
    return any(
        "\u0400" <= char <= "\u04ff" or "\u0500" <= char <= "\u052f" for char in text
    )


def contains_latin_letter(text: str) -> bool:
    return any("a" <= char.lower() <= "z" for char in text)


def may_keep_source_text(source: str) -> bool:
    if extract_placeholders(source):
        without_placeholders = source
        for placeholder in extract_placeholders(source):
            without_placeholders = without_placeholders.replace(placeholder, "")
        without_placeholders = without_placeholders.replace("\\n", "")
        if re.fullmatch(r"[\W\d_%.:/\\-]*", without_placeholders):
            return True
    if re.fullmatch(r"[\W\d_%.:/\\-]+", source):
        return True
    if source in {
        "DDNet",
        "QmClient",
        "TClient",
        "OpenAI",
        "API",
        "HUD",
        "Hz",
        "FPS",
        "Ping",
        "Super",
        "Demo",
        "DDmaX",
        "DeepSeek",
        "Discord",
        "Github",
        "LibreTranslate",
        "SecretId",
        "SecretKey",
        "Tee",
        "Tee 0.7",
        "Qm",
        "DDRace HUD",
        "Lenny:",
        "my_%s",
        "entity_bg (Workshop)",
        "Tencent Cloud",
        "Tencent Cloud SecretId",
        "Tencent Cloud SecretKey",
        "Zhipu AI",
        "V-Sync",
        "DDmaX Easy",
        "DDmaX Next",
        "DDmaX Nut",
        "DDmaX Pro",
        "Brutal",
        "Fun",
        "Insane",
        "Moderate",
        "Novice",
        "Oldschool",
    }:
        return True
    if re.fullmatch(r"%[^\n]*\s(?:KiB|MiB)(?:/[a-zA-Z]+|\s*\([^)]*\))?", source):
        return True
    if re.fullmatch(r"[A-Z0-9_./% -]{1,24}", source):
        return True
    if source.startswith(("http://", "https://", "/")):
        return True
    return False


def may_keep_source_text_for_language(language: str, source: str) -> bool:
    return source in SAME_SOURCE_ALLOWED_BY_LANGUAGE.get(
        language, set()
    ) or may_keep_source_text(source)


def language_quality_failure(language: str, source: str, translation: str) -> str:
    translation = translation.strip()
    if not translation:
        return "empty translation returned"
    if extract_placeholders(source) != extract_placeholders(translation):
        return (
            f"placeholder mismatch: expected {extract_placeholders(source)!r}, "
            f"got {extract_placeholders(translation)!r}"
        )
    if extract_digits(source) != extract_digits(translation):
        return f"digit mismatch: expected {extract_digits(source)!r}, got {extract_digits(translation)!r}"
    if source.strip() == translation and not may_keep_source_text_for_language(
        language, source
    ):
        return "suspect prompt echo or unchanged source"
    if language == "traditional_chinese":
        simplified_only_chars = set("们这为没个队战图连实时后声显项击启级")
        if sum(1 for char in translation if char in simplified_only_chars) >= 2:
            return "traditional_chinese output contains too many simplified characters"
    if (
        language == "korean"
        and contains_latin_letter(source)
        and not contains_hangul(translation)
    ):
        if not may_keep_source_text_for_language(language, source):
            return "korean output does not contain Hangul"
    if (
        language == "japanese"
        and contains_latin_letter(source)
        and not contains_kana(translation)
        and not contains_cjk(translation)
    ):
        if not may_keep_source_text_for_language(language, source):
            return "japanese output does not contain Japanese text"
    if (
        language == "russian"
        and contains_latin_letter(source)
        and not contains_cyrillic(translation)
    ):
        if not may_keep_source_text_for_language(language, source):
            return "russian output does not contain cyrillic text"
    return ""


def parse_indexed_response_lines(text: str, expected_count: int) -> list[str] | None:
    indexed: dict[int, str] = {}
    saw_indexed = False
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("```"):
            continue
        match = INDEXED_LINE_RE.match(line)
        if not match:
            continue
        saw_indexed = True
        index = int(match.group(1))
        if 1 <= index <= expected_count and index not in indexed:
            indexed[index] = match.group(2).strip()
    if not saw_indexed:
        return None
    if set(indexed) != set(range(1, expected_count + 1)):
        return None
    return [indexed[index] for index in range(1, expected_count + 1)]


def validate_json_translations(
    tasks: list[TranslationTask],
    response_items: list[dict],
    language: str,
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
        if identity not in requested:
            failures.append(f"unexpected identity returned: {identity!r}")
            continue
        reason = language_quality_failure(
            language, requested[identity].source_text, translation
        )
        if reason:
            failures.append(f"{reason}: {identity!r}")
            continue
        translated[identity] = translation.strip()
        remaining.discard(identity)
    for identity in sorted(remaining):
        failures.append(f"missing translation for requested identity: {identity!r}")
    return translated, failures


def parse_response(text: str) -> list[dict]:
    payload = json.loads(text)
    if not isinstance(payload, list):
        raise ValueError("response must be a JSON array")
    if not all(isinstance(item, dict) for item in payload):
        raise ValueError("response array items must be objects")
    return payload


def validate_translations(
    tasks: list[TranslationTask],
    response_items: list[dict],
    language: str = "simplified_chinese",
) -> tuple[dict[tuple[str, str], str], list[str]]:
    return validate_json_translations(tasks, response_items, language)


def apply_translations_to_store(
    store: dict[str, dict[tuple[str, str], dict[str, str]]],
    language: str,
    tasks: list[TranslationTask],
    translations: dict[tuple[str, str], str],
) -> dict[str, dict[tuple[str, str], dict[str, str]]]:
    updated = {
        module: {
            identity: dict(language_map)
            for identity, language_map in module_entries.items()
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


def translation_patch_for_language(
    language: str,
    tasks: list[TranslationTask],
    translations: dict[tuple[str, str], str],
) -> dict[tuple[str, str], dict[str, str]]:
    patch: dict[tuple[str, str], dict[str, str]] = {}
    for task in tasks:
        translation = translations.get(task.identity, "")
        if translation:
            patch[task.identity] = {language: translation}
    return patch


def parse_translation_output(
    text: str, tasks: list[TranslationTask], language: str
) -> tuple[dict[tuple[str, str], str], list[str]]:
    try:
        payload = json.loads(text)
    except json.JSONDecodeError:
        payload = None
    if isinstance(payload, list) and all(isinstance(item, dict) for item in payload):
        if len(payload) == len(tasks) and all(
            isinstance(item.get("translation"), str) for item in payload
        ):
            translated: dict[tuple[str, str], str] = {}
            failures: list[str] = []
            for task, item in zip(tasks, payload):
                if "key" in item or "context" in item:
                    identity = (item.get("key"), item.get("context", ""))
                    if identity != task.identity:
                        failures.append(
                            f"unexpected identity order: expected {task.identity!r}, got {identity!r}"
                        )
                        continue
                translation = item.get("translation", "")
                reason = language_quality_failure(
                    language, task.source_text, translation
                )
                if reason:
                    failures.append(f"{reason}: {task.identity!r}")
                    continue
                translated[task.identity] = translation.strip()
            return translated, failures
        return validate_json_translations(tasks, payload, language)
    lines = parse_indexed_response_lines(text, len(tasks))
    if lines is None:
        return {}, ["numbered response is incomplete or malformed"]
    translated: dict[tuple[str, str], str] = {}
    failures: list[str] = []
    for task, translation in zip(tasks, lines):
        reason = language_quality_failure(language, task.source_text, translation)
        if reason:
            failures.append(f"{reason}: {task.identity!r}")
            continue
        translated[task.identity] = translation.strip()
    return translated, failures


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


def load_existing_valid_draft_identities(
    language: str, source_texts: dict[tuple[str, str], str]
) -> set[tuple[str, str]]:
    valid: set[tuple[str, str]] = set()
    draft_dir = TRANSLATIONS_DRAFT_DIR / language
    if not draft_dir.exists():
        return valid
    for path in sorted(draft_dir.glob("*.toml")):
        for identity, translations in load_existing_draft_module(
            language, path.stem
        ).items():
            translation = translations.get(language, "")
            source = source_texts.get(identity, identity[0])
            if translation and not language_quality_failure(
                language, source, translation
            ):
                valid.add(identity)
    return valid


def load_valid_draft_translations(
    language: str,
    source_texts: dict[tuple[str, str], str],
    modules: set[str] | None = None,
) -> dict[str, dict[tuple[str, str], str]]:
    by_module: dict[str, dict[tuple[str, str], str]] = {}
    draft_dir = TRANSLATIONS_DRAFT_DIR / language
    if not draft_dir.exists():
        return by_module
    for path in sorted(draft_dir.glob("*.toml")):
        module = path.stem
        if modules and module not in modules:
            continue
        for identity, translations in load_existing_draft_module(
            language, module
        ).items():
            translation = translations.get(language, "")
            source = source_texts.get(identity, identity[0])
            if translation and not language_quality_failure(
                language, source, translation
            ):
                by_module.setdefault(module, {})[identity] = translation
    return by_module


def load_existing_draft_identities(language: str) -> set[tuple[str, str]]:
    identities: set[tuple[str, str]] = set()
    draft_dir = TRANSLATIONS_DRAFT_DIR / language
    if not draft_dir.exists():
        return identities
    for path in sorted(draft_dir.glob("*.toml")):
        identities.update(load_existing_draft_module(language, path.stem))
    return identities


def write_draft_module(
    language: str,
    module: str,
    tasks: list[TranslationTask],
    translations: dict[tuple[str, str], str],
    *,
    source_texts: dict[tuple[str, str], str] | None = None,
) -> Path:
    draft_dir = TRANSLATIONS_DRAFT_DIR / language
    draft_dir.mkdir(parents=True, exist_ok=True)
    out_path = draft_dir / f"{module}.toml"
    source_texts = source_texts or {}
    merged = {
        identity: translation_map
        for identity, translation_map in load_existing_draft_module(
            language, module
        ).items()
        if not language_quality_failure(
            language,
            source_texts.get(identity, identity[0]),
            translation_map.get(language, ""),
        )
    }
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
    out_path.write_text(
        i18n_store.dump_module(messages), encoding="utf-8", newline="\n"
    )
    return out_path


def write_back_module(
    language: str,
    module: str,
    tasks: list[TranslationTask],
    translations: dict[tuple[str, str], str],
    store: dict[str, dict[tuple[str, str], dict[str, str]]],
) -> None:
    patch_entries = translation_patch_for_language(language, tasks, translations)
    if not patch_entries:
        return
    updated = apply_translations_to_store(store, language, tasks, translations)
    store.clear()
    store.update(updated)
    i18n_store.patch_module_store(module, patch_entries)


def write_back_draft(
    language: str,
    *,
    modules: set[str] | None,
    source_records,
    source_texts: dict[tuple[str, str], str],
    store: dict[str, dict[tuple[str, str], dict[str, str]]],
    rewrite_existing: bool = False,
) -> tuple[int, list[str]]:
    draft_by_module = load_valid_draft_translations(language, source_texts, modules)
    if not draft_by_module:
        return 0, [f"no valid draft translations found for {language}"]

    active_by_module: dict[str, list[TranslationTask]] = {}
    seen: set[tuple[str, str]] = set()
    for record in source_records:
        identity = record.identity()
        if identity in seen:
            continue
        seen.add(identity)
        module = i18n_store.module_name_for_source(record.source)
        if modules and module not in modules:
            continue
        translation = draft_by_module.get(module, {}).get(identity, "")
        if not translation:
            continue
        existing = store.get(module, {}).get(identity, {}).get(language, "")
        if existing and not rewrite_existing:
            continue
        active_by_module.setdefault(module, []).append(
            TranslationTask(module, identity, record.key, existing)
        )

    written = 0
    for module, tasks in sorted(active_by_module.items()):
        translations = {
            task.identity: draft_by_module[module][task.identity] for task in tasks
        }
        write_back_module(language, module, tasks, translations, store)
        written += len(translations)
        print(
            f"{language}: wrote back translations/i18n/{module}.toml "
            f"({len(translations)} translated)"
        )
    return written, []


def translate_batch(
    *,
    client: LocalHttpClient,
    language: str,
    module: str,
    batch: list[TranslationTask],
    batch_number: int,
    prompt_assets: tuple[str, str, str],
    store: dict[str, dict[tuple[str, str], dict[str, str]]],
) -> tuple[dict[tuple[str, str], str], list[str]]:
    prompt = render_prompt(
        language,
        module,
        batch,
        prompt_assets=prompt_assets,
        store=store,
    )
    try:
        response = client.chat_completion(
            [ChatMessage("system", prompt_assets[0]), ChatMessage("user", prompt)]
        )
        return parse_translation_output(response, batch, language)
    except (RuntimeError, ValueError, json.JSONDecodeError) as exc:
        return {}, [f"batch {module}#{batch_number}: {exc}"]


def translate_task_batches(
    *,
    client: LocalHttpClient,
    language: str,
    tasks_by_module: dict[str, list[TranslationTask]],
    batch_size: int,
    prompt_assets: tuple[str, str, str],
    store: dict[str, dict[tuple[str, str], dict[str, str]]],
    parallel_requests: int = 1,
    progress: Callable[[str, int, int, int], None] | None = None,
) -> tuple[dict[str, dict[tuple[str, str], str]], dict[str, list[str]]]:
    parallel_requests = max(1, min(parallel_requests, MAX_PARALLEL_REQUESTS))
    jobs: list[tuple[str, int, list[TranslationTask]]] = []
    for module, module_tasks in sorted(tasks_by_module.items()):
        for index in range(0, len(module_tasks), batch_size):
            jobs.append(
                (
                    module,
                    index // batch_size + 1,
                    module_tasks[index : index + batch_size],
                )
            )
    translated_by_module = {module: {} for module in tasks_by_module}
    failures_by_module = {module: [] for module in tasks_by_module}

    def run(job: tuple[str, int, list[TranslationTask]]):
        module, batch_number, batch = job
        batch_translations, batch_failures = translate_batch(
            client=client,
            language=language,
            module=module,
            batch=batch,
            batch_number=batch_number,
            prompt_assets=prompt_assets,
            store=store,
        )
        return module, batch_number, batch_translations, batch_failures

    if parallel_requests > 1 and len(jobs) > 1:
        with concurrent.futures.ThreadPoolExecutor(
            max_workers=parallel_requests
        ) as executor:
            futures = [executor.submit(run, job) for job in jobs]
            for future in concurrent.futures.as_completed(futures):
                module, batch_number, batch_translations, batch_failures = (
                    future.result()
                )
                translated_by_module[module].update(batch_translations)
                failures_by_module[module].extend(batch_failures)
                if progress:
                    progress(
                        module,
                        batch_number,
                        len(batch_translations),
                        len(batch_failures),
                    )
    else:
        for job in jobs:
            module, batch_number, batch_translations, batch_failures = run(job)
            translated_by_module[module].update(batch_translations)
            failures_by_module[module].extend(batch_failures)
            if progress:
                progress(
                    module, batch_number, len(batch_translations), len(batch_failures)
                )
    return translated_by_module, failures_by_module


def selected_modules(args: argparse.Namespace) -> set[str]:
    values = parse_csv_values(args.modules)
    if args.module:
        values.extend(parse_csv_values(args.module))
    return set(values)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("--languages", default="simplified_chinese")
    parser.add_argument("--base-url", default="")
    parser.add_argument("--model", default="")
    parser.add_argument("--api-key", default="")
    parser.add_argument("--module", default="")
    parser.add_argument("--modules", default="")
    parser.add_argument("--limit", type=int)
    parser.add_argument("--batch-size", type=int, default=1024)
    parser.add_argument("--parallel-requests", type=int, default=1)
    parser.add_argument("--max-tokens", type=int)
    parser.add_argument("--timeout", type=float, default=120.0)
    parser.add_argument("--chat-extra-json", default="")
    parser.add_argument("--reasoning-effort", default="")
    parser.add_argument("--resume", action="store_true")
    parser.add_argument("--rewrite", action="store_true")
    parser.add_argument("--write-back", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    return parser


def main() -> None:
    args = build_parser().parse_args()
    modules = selected_modules(args)
    languages = parse_csv_values(args.languages)
    source_records = source_keys.collect_source_key_records()
    source_texts = collect_source_text_by_identity(source_records)
    store = i18n_store.load_language_store()
    prompt_assets = load_prompt_assets()
    client = None
    if not args.write_back:
        if not args.base_url or not args.model:
            raise SystemExit(
                "--base-url and --model are required unless --write-back is used"
            )
        client = LocalHttpClient(
            base_url=args.base_url,
            model=args.model,
            api_key=resolve_api_key(args.api_key),
            timeout_seconds=args.timeout,
            max_tokens=args.max_tokens,
            chat_extra_body=build_chat_extra_body(
                args.chat_extra_json, args.reasoning_effort
            ),
        )
    for language in languages:
        if args.write_back:
            written, failures = write_back_draft(
                language,
                modules=modules or None,
                source_records=source_records,
                source_texts=source_texts,
                store=store,
                rewrite_existing=args.rewrite,
            )
            if written == 0:
                print(f"{language}: no draft translations written back")
            for failure in failures:
                print(f"  - {failure}")
            continue
        existing_draft = (
            load_existing_valid_draft_identities(language, source_texts)
            if args.resume and should_write_draft(language, args.write_back)
            else set()
        )
        tasks = collect_tasks(
            language,
            modules=modules or None,
            limit=args.limit,
            existing_draft_identities=existing_draft,
            rewrite_existing=args.rewrite,
            records=source_records,
            store=store,
        )
        if not tasks:
            print(f"{language}: nothing to translate")
            continue
        grouped: dict[str, list[TranslationTask]] = {}
        for task in tasks:
            grouped.setdefault(task.module, []).append(task)
        if args.dry_run:
            for module, module_tasks in sorted(grouped.items()):
                for index in range(0, len(module_tasks), args.batch_size):
                    print(
                        render_prompt(
                            language,
                            module,
                            module_tasks[index : index + args.batch_size],
                            prompt_assets=prompt_assets,
                            store=store,
                        )
                    )
            continue
        translated_by_module, failures_by_module = translate_task_batches(
            client=client,
            language=language,
            tasks_by_module=grouped,
            batch_size=args.batch_size,
            prompt_assets=prompt_assets,
            store=store,
            parallel_requests=args.parallel_requests,
            progress=lambda module, batch_number, translated_count, failure_count: (
                print(
                    f"{language}: completed {module} batch {batch_number} "
                    f"({translated_count} translated, {failure_count} failed)",
                    flush=True,
                )
            ),
        )
        for module, module_tasks in sorted(grouped.items()):
            translated = translated_by_module.get(module, {})
            failures = failures_by_module.get(module, [])
            if should_write_draft(language, args.write_back):
                out_path = write_draft_module(
                    language,
                    module,
                    module_tasks,
                    translated,
                    source_texts=source_texts,
                )
                print(
                    f"{language}: wrote draft {out_path} "
                    f"({len(translated)}/{len(module_tasks)} translated, {len(failures)} failed)"
                )
            else:
                write_back_module(language, module, module_tasks, translated, store)
                print(
                    f"{language}: updated translations/i18n/{module}.toml "
                    f"({len(translated)}/{len(module_tasks)} translated, {len(failures)} failed)"
                )
            for failure in failures:
                print(f"  - {failure}")


if __name__ == "__main__":
    main()
