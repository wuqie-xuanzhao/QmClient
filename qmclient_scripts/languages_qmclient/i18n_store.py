#!/usr/bin/env python3
"""Shared i18n store for module-scoped translation TOML files."""

from __future__ import annotations

import re
import tomllib
from dataclasses import dataclass
from pathlib import Path

try:
    from . import chinese_text_style, source_keys
except ImportError:  # pragma: no cover - script entrypoint fallback
    import chinese_text_style
    import source_keys

SCRIPT_DIR = Path(__file__).resolve().parent
TRANSLATIONS_DIR = SCRIPT_DIR / "translations" / "i18n"
LANGUAGE_ORDER = (
    "simplified_chinese",
    "traditional_chinese",
    "japanese",
    "korean",
    "russian",
    "german",
    "spanish",
    "french",
    "brazilian_portuguese",
    "portuguese",
    "turkish",
    "polish",
)
CHINESE_LANGUAGES = {"simplified_chinese", "traditional_chinese"}
CJK_TOLERANT_LANGUAGES = CHINESE_LANGUAGES | {"japanese", "korean"}


@dataclass(frozen=True)
class Message:
    key: str
    context: str = ""

    def identity(self) -> tuple[str, str]:
        return (self.key, self.context)


def toml_quote(value: str) -> str:
    escaped = (
        value.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )
    return f'"{escaped}"'


def module_name_for_source(source: Path | None) -> str:
    if source is None:
        return "misc"

    normalized = source.as_posix()
    if normalized.endswith("src/engine/shared/config_variables_qmclient.h"):
        return "qmclient"
    if "/menus_browser." in normalized:
        return "server_browser"
    if "/menus_demo." in normalized:
        return "demo"
    if (
        "/menus_ingame_touch_controls." in normalized
        or "/touch_controls." in normalized
    ):
        return "touch_controls"
    if "/chat." in normalized or "/translate/" in normalized:
        return "chat"
    if "/gameclient.cpp" in normalized or "/menus.cpp" in normalized:
        return "loading"
    if "/components/qmclient/" in normalized:
        return "qmclient"
    if "/components/tclient/" in normalized:
        return "tclient"
    if "/menus_" in normalized or "/menus." in normalized:
        return "menus"
    return "misc"


def sorted_records(
    records: list[tuple[Message, dict[str, str]]],
) -> list[tuple[Message, dict[str, str]]]:
    return sorted(
        records,
        key=lambda item: (
            item[0].context.casefold(),
            item[0].key.casefold(),
            item[0].context,
            item[0].key,
        ),
    )


def sorted_translation_languages(translations: dict[str, str]) -> list[str]:
    known = [language for language in LANGUAGE_ORDER if language in translations]
    extra = sorted(
        (language for language in translations if language not in LANGUAGE_ORDER),
        key=str.casefold,
    )
    return known + extra


def normalize_translation(language: str, translation: str) -> str:
    if language == "simplified_chinese":
        return chinese_text_style.normalize_simplified_chinese_text(translation)
    return translation


def translation_quality_errors(
    store: dict[str, dict[tuple[str, str], dict[str, str]]],
    *,
    active_module_identities: set[tuple[str, str, str]] | None = None,
    active_identities: set[tuple[str, str]] | None = None,
    terminology_by_language: dict[str, dict[str, str]] | None = None,
    limit: int | None = None,
) -> list[str]:
    errors: list[str] = []
    for module_name, module_entries in sorted(store.items()):
        for (key, context), translations in sorted(module_entries.items()):
            if (
                active_module_identities is not None
                and (module_name, key, context) not in active_module_identities
            ):
                continue
            if (
                active_identities is not None
                and (key, context) not in active_identities
            ):
                continue
            for language, translation in sorted(translations.items()):
                normalized = normalize_translation(language, translation)
                if language == "simplified_chinese" and translation != normalized:
                    errors.append(
                        f"{module_name}: [{context}] {key}: {language} typography "
                        f"should be {toml_quote(normalized)}"
                    )
                if (
                    language not in CHINESE_LANGUAGES
                    and source_keys.has_cjk(key)
                    and translation == key
                ):
                    errors.append(
                        f"{module_name}: [{context}] {key}: {language} repeats CJK source key"
                    )
                if (
                    language not in CHINESE_LANGUAGES
                    and _looks_like_english_placeholder_key(key)
                    and translation.strip() == key.strip()
                ):
                    errors.append(
                        f"{module_name}: [{context}] {key}: {language} repeats English source key"
                    )
                if (
                    language not in CJK_TOLERANT_LANGUAGES
                    and source_keys.has_cjk(translation)
                    and _looks_like_cjk_fallback(translation)
                ):
                    errors.append(
                        f"{module_name}: [{context}] {key}: {language} contains CJK text "
                        f"{toml_quote(translation)}"
                    )
                terminology_reason = _terminology_quality_failure(
                    key,
                    translation,
                    (terminology_by_language or {}).get(language, {}),
                )
                if terminology_reason:
                    errors.append(
                        f"{module_name}: [{context}] {key}: {language} "
                        f"{terminology_reason}"
                    )
                if limit is not None and len(errors) >= limit:
                    return errors
    return errors


def _terminology_quality_failure(
    source: str, translation: str, terminology: dict[str, object]
) -> str:
    if not terminology:
        return ""
    if source in terminology:
        expected, enforce = _terminology_value_parts(terminology[source], "exact")
        if enforce == "prompt_only":
            return ""
        if expected and expected not in translation:
            return (
                f"terminology mismatch: expected {toml_quote(expected)} "
                f"for {toml_quote(source)}"
            )
        return ""
    for term_source, expected in sorted(
        terminology.items(), key=lambda item: len(item[0]), reverse=True
    ):
        expected_translation, enforce = _terminology_value_parts(expected, "pattern")
        if enforce != "pattern":
            continue
        if not _source_contains_term(source, term_source):
            continue
        if expected_translation and expected_translation not in translation:
            return (
                f"terminology mismatch: expected {toml_quote(expected_translation)} "
                f"for {toml_quote(term_source)}"
            )
        return ""
    return ""


def _terminology_value_parts(value: object, default_enforce: str) -> tuple[str, str]:
    translation = getattr(value, "translation", value)
    enforce = getattr(value, "enforce", default_enforce)
    if not isinstance(translation, str):
        translation = ""
    if enforce not in {"exact", "pattern", "prompt_only"}:
        enforce = default_enforce
    return translation, enforce


def _source_contains_term(source: str, term_source: str) -> bool:
    if len(term_source) <= 2:
        return False
    if term_source == "Hook":
        return source.strip().casefold() == "hook"
    if term_source == "Hook collision line":
        return source.strip().casefold() == "hook collision line"
    if term_source == "Grenade":
        lowered = source.strip().casefold()
        return (
            lowered == "grenade"
            or lowered.startswith("switch ")
            and " to grenade" in lowered
        )
    return (
        re.search(
            rf"(?<![A-Za-z0-9]){re.escape(term_source)}(?![A-Za-z0-9])",
            source,
            flags=re.IGNORECASE,
        )
        is not None
    )


def toml_format_errors(path: Path) -> list[str]:
    errors: list[str] = []
    lines = path.read_text(encoding="utf-8").splitlines()
    previous_message_line: int | None = None
    for index, line in enumerate(lines):
        if line.strip() != "[[message]]":
            continue
        line_number = index + 1
        if previous_message_line is not None and index > 0 and lines[index - 1].strip():
            errors.append(
                f"{path}: line {line_number}: missing blank line before [[message]]"
            )
        previous_message_line = line_number
    return errors


def _looks_like_cjk_fallback(translation: str) -> bool:
    cjk_count = sum(1 for char in translation if source_keys.has_cjk(char))
    if cjk_count == 0:
        return False
    non_space_count = sum(1 for char in translation if not char.isspace())
    if non_space_count == 0:
        return False
    return cjk_count >= 4 and cjk_count / non_space_count >= 0.35


def _looks_like_english_placeholder_key(key: str) -> bool:
    if source_keys.has_cjk(key) or "%" in key:
        return False
    if _may_keep_source_text(key):
        return False
    words = [word.strip("()[],:;.!?") for word in key.strip().split()]
    if not any(any(char.isalpha() for char in word) for word in words):
        return False
    return any(word[:1].islower() for word in words)


def _may_keep_source_text(source: str) -> bool:
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
        "DeepSeek API Key",
        "Discord",
        "Glitch",
        "Gradient",
        "Github",
        "Linear",
        "LibreTranslate",
        "LibreTranslate API Key",
        "OpenAI API Key",
        "SecretId",
        "SecretKey",
        "Zhipu AI API Key",
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
        "Ease in out quad",
        "Ease out cubic",
        "Brutal",
        "Fun",
        "Insane",
        " min",
        "Moderate",
        "Novice",
        "Oldschool",
        "z = Zoom",
    }:
        return True
    if re.fullmatch(r"[\W\d_%.:/\\-]+", source):
        return True
    if re.fullmatch(r"%[^\n]*\s(?:KiB|MiB)(?:/[a-zA-Z]+|\s*\([^)]*\))?", source):
        return True
    if re.fullmatch(r"[A-Z0-9_./% -]{1,24}", source):
        return True
    if re.fullmatch(r"[a-z0-9_./%-]{1,32}", source):
        return True
    if source.startswith(("http://", "https://", "/")):
        return True
    return False


def dump_message_block(message: Message, translations: dict[str, str]) -> str:
    lines = ["[[message]]", f"key = {toml_quote(message.key)}"]
    if message.context:
        lines.append(f"context = {toml_quote(message.context)}")
    lines.append("[message.translations]")
    for language in sorted_translation_languages(translations):
        translation = normalize_translation(language, translations.get(language, ""))
        if translation:
            lines.append(f"{language} = {toml_quote(translation)}")
    return "\n".join(lines)


def load_language_store() -> dict[str, dict[tuple[str, str], dict[str, str]]]:
    store: dict[str, dict[tuple[str, str], dict[str, str]]] = {}
    if not TRANSLATIONS_DIR.exists():
        return store

    for path in sorted(TRANSLATIONS_DIR.glob("*.toml")):
        with path.open("rb") as file:
            data = tomllib.load(file)
        entries = data.get("message", [])
        module_entries: dict[tuple[str, str], dict[str, str]] = {}
        for entry in entries:
            if not isinstance(entry, dict):
                continue
            key = entry.get("key", "")
            context = entry.get("context", "")
            translations = entry.get("translations", {})
            if not key or not isinstance(translations, dict):
                continue
            normalized = {
                language: translation
                for language, translation in translations.items()
                if isinstance(language, str)
                and isinstance(translation, str)
                and translation
            }
            if normalized:
                module_entries[(key, context)] = normalized
        store[path.stem] = module_entries
    return store


def language_map_for(
    store: dict[str, dict[tuple[str, str], dict[str, str]]], language: str
) -> dict[tuple[str, str], str]:
    flattened: dict[tuple[str, str], str] = {}
    for module_entries in store.values():
        for identity, translations in module_entries.items():
            translation = translations.get(language, "")
            if translation:
                flattened[identity] = translation
    return flattened


def missing_translations_for(
    store: dict[str, dict[tuple[str, str], dict[str, str]]],
    identities: list[tuple[str, str]] | tuple[tuple[str, str], ...],
    language: str,
) -> list[tuple[str, str]]:
    flattened = language_map_for(store, language)
    if language == "simplified_chinese":
        return [
            identity
            for identity in identities
            if not flattened.get(identity, "")
            and not source_keys.has_cjk(identity[0])
            and not _may_keep_source_text(identity[0])
        ]
    return [
        identity
        for identity in identities
        if not flattened.get(identity, "") and not _may_keep_source_text(identity[0])
    ]


def dump_module(messages: list[tuple[Message, dict[str, str]]]) -> str:
    return (
        "\n\n".join(
            dump_message_block(message, translations)
            for message, translations in sorted_records(messages)
        ).rstrip()
        + "\n"
    )


def write_language_store(
    store: dict[str, dict[tuple[str, str], dict[str, str]]],
) -> None:
    TRANSLATIONS_DIR.mkdir(parents=True, exist_ok=True)
    for existing in TRANSLATIONS_DIR.glob("*.toml"):
        if existing.stem not in store:
            existing.unlink()
    for module_name, entries in sorted(store.items()):
        path = TRANSLATIONS_DIR / f"{module_name}.toml"
        messages = [
            (Message(key, context), translations)
            for (key, context), translations in entries.items()
        ]
        path.write_text(dump_module(messages), encoding="utf-8", newline="\n")


def _parse_assignment_value(line: str, name: str) -> str | None:
    stripped = line.strip()
    prefix = f"{name} = "
    if not stripped.startswith(prefix):
        return None
    try:
        import tomllib

        data = tomllib.loads(stripped)
    except tomllib.TOMLDecodeError:
        return None
    value = data.get(name)
    return value if isinstance(value, str) else None


def _block_translations(lines: list[str]) -> dict[str, str]:
    translations: dict[str, str] = {}
    in_translations = False
    for line in lines:
        stripped = line.strip()
        if stripped == "[message.translations]":
            in_translations = True
            continue
        if not in_translations or " = " not in stripped:
            continue
        language = stripped.split(" = ", 1)[0]
        value = _parse_assignment_value(stripped, language)
        if value:
            translations[language] = value
    return translations


def _block_identity(lines: list[str]) -> tuple[str, str] | None:
    key = ""
    context = ""
    for line in lines:
        if not key:
            parsed_key = _parse_assignment_value(line, "key")
            if parsed_key is not None:
                key = parsed_key
                continue
        parsed_context = _parse_assignment_value(line, "context")
        if parsed_context is not None:
            context = parsed_context
    if not key:
        return None
    return (key, context)


def _patch_message_block(lines: list[str], entries: dict[str, str]) -> list[str]:
    identity = _block_identity(lines)
    if identity is None:
        return lines
    translations = _block_translations(lines)
    for language, translation in entries.items():
        if translation:
            translations[language] = translation
    return dump_message_block(Message(*identity), translations).splitlines()


def patch_module_store(
    module_name: str,
    entries: dict[tuple[str, str], dict[str, str]],
) -> None:
    """Patch one module TOML without rewriting unrelated message blocks."""

    path = TRANSLATIONS_DIR / f"{module_name}.toml"
    TRANSLATIONS_DIR.mkdir(parents=True, exist_ok=True)
    if not path.exists():
        messages = [
            (Message(key, context), translations)
            for (key, context), translations in entries.items()
        ]
        path.write_text(dump_module(messages), encoding="utf-8", newline="\n")
        return

    original = path.read_text(encoding="utf-8")
    has_trailing_newline = original.endswith(("\n", "\r"))
    lines = original.splitlines()
    output: list[str] = []
    index = 0
    patched_identities: set[tuple[str, str]] = set()

    while index < len(lines):
        if lines[index].strip() != "[[message]]":
            output.append(lines[index])
            index += 1
            continue

        start = index
        index += 1
        while index < len(lines) and lines[index].strip() != "[[message]]":
            index += 1
        block = lines[start:index]
        identity = _block_identity(block)
        if identity is None or identity not in entries:
            if output and output[-1] != "" and output[-1].strip() != "":
                output.append("")
            output.extend(block)
            continue

        patched_block = _patch_message_block(block, entries[identity])
        patched_identities.add(identity)
        if output and output[-1] != "" and output[-1].strip() != "":
            output.append("")
        output.extend(patched_block)

    missing = [
        (Message(key, context), translations)
        for (key, context), translations in entries.items()
        if (key, context) not in patched_identities
    ]
    if missing:
        if output and output[-1] != "":
            output.append("")
        output.extend(dump_module(missing).rstrip("\n").splitlines())

    text = "\n".join(output)
    if has_trailing_newline or missing:
        text += "\n"
    path.write_text(text, encoding="utf-8", newline="\n")


def build_module_store_from_records(
    records: list[source_keys.SourceKeyRecord],
    translations_by_identity: dict[tuple[str, str], dict[str, str]],
) -> dict[str, dict[tuple[str, str], dict[str, str]]]:
    store: dict[str, dict[tuple[str, str], dict[str, str]]] = {}
    assigned: set[tuple[str, str]] = set()
    for record in records:
        identity = record.identity()
        if identity in assigned:
            continue
        assigned.add(identity)
        module_name = module_name_for_source(record.source)
        translations = translations_by_identity.get(identity, {})
        store.setdefault(module_name, {})[identity] = dict(sorted(translations.items()))
    return {module: entries for module, entries in store.items() if entries}
