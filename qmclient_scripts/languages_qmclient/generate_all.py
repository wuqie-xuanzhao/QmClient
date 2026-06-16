#!/usr/bin/env python3
"""Generate DDNet language files from the module-scoped i18n store."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

try:
    from . import i18n_store
except ImportError:  # pragma: no cover - script entrypoint fallback
    import i18n_store

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parents[1]
STRINGS_FILE = SCRIPT_DIR / "extracted_strings.txt"
BASE_LANGUAGES_DIR = PROJECT_ROOT / "data" / "languages"
BASE_SIMPLIFIED_CHINESE = BASE_LANGUAGES_DIR / "simplified_chinese.txt"
GENERATED_LANGUAGES = (
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
SIMPLIFIED_CHINESE_PASSTHROUGH_KEYS = {
    "%.2f KiB",
    "%.2f MiB",
    "DDmaX",
    "DDmaX Easy",
    "DDmaX Next",
    "DDmaX Nut",
    "DDmaX Pro",
    "DDNet",
    "DDRace HUD",
    "DeepSeek",
    "DF",
    "Discord",
    "FTAPI",
    "FPS",
    "Github",
    "HDF",
    "HUD",
    "LLM API",
    "OpenAI",
    "TClient",
    "Tee",
    "Tee 0.7",
}
SIMPLIFIED_CHINESE_PASSTHROUGH_IDENTITIES = {
    ("Hz", "Hertz"),
    ("%d\\n(%d/%d)", "Team and size"),
}


@dataclass(frozen=True)
class SourceString:
    key: str
    context: str = ""

    def identity(self) -> tuple[str, str]:
        return (self.key, self.context)


def is_chinese(value: str) -> bool:
    return any("\u4e00" <= ch <= "\u9fff" or "\u3400" <= ch <= "\u4dbf" for ch in value)


def parse_extracted_string(line: str) -> SourceString:
    if line.startswith("[") and "]\t" in line:
        context, key = line.split("]\t", 1)
        return SourceString(key, context[1:])
    return SourceString(line)


def read_strings() -> list[SourceString]:
    with STRINGS_FILE.open("r", encoding="utf-8") as file:
        return sorted(
            {
                parse_extracted_string(line.rstrip("\n"))
                for line in file
                if line.strip()
            },
            key=lambda item: (item.context.casefold(), item.key.casefold()),
        )


def format_language_entry(key: str, context: str, translation: str) -> str:
    translation = translation.replace("\r", "\\r").replace("\n", "\\n")
    if context:
        return f"[{context}]\n{key}\n== {translation}"
    return f"{key}\n== {translation}"


def read_existing_language_entries(path: Path) -> dict[tuple[str, str], str]:
    try:
        from . import twlang_qmclient as twlang
    except ImportError:  # pragma: no cover - script entrypoint fallback
        import twlang_qmclient as twlang

    if not path.exists():
        return {}
    parsed = twlang.translations(path)
    return {identity: values[1] for identity, values in parsed.items()}


def generate_language_entries(
    strings: list[SourceString], language: str
) -> list[tuple[tuple[str, str], str]]:
    store = i18n_store.load_language_store()
    translations = i18n_store.language_map_for(store, language)
    entries: list[tuple[tuple[str, str], str]] = []
    missing: list[tuple[str, str]] = []

    for source in strings:
        if is_chinese(source.key):
            continue
        identity = source.identity()
        translation = i18n_store.normalize_translation(
            language, translations.get(identity, source.key)
        )
        entries.append((identity, translation))
        if language == "simplified_chinese" and translation == source.key:
            if (
                source.key in SIMPLIFIED_CHINESE_PASSTHROUGH_KEYS
                or identity in SIMPLIFIED_CHINESE_PASSTHROUGH_IDENTITIES
            ):
                continue
            missing.append(identity)

    if missing and language == "simplified_chinese":
        print("  WARNING: untranslated Simplified Chinese placeholders:")
        for key, context in missing:
            print(f"    - [{context}] {key}" if context else f"    - {key}")

    return entries


def runtime_language_path(language: str) -> Path:
    return BASE_LANGUAGES_DIR / f"{language}.txt"


def generate_configured_languages(
    strings: list[SourceString],
    languages: list[str] | tuple[str, ...] = GENERATED_LANGUAGES,
) -> int:
    for language in languages:
        print(f"\n--- Syncing {language}.txt ---")
        entries = generate_language_entries(strings, language)
        write_language_file(runtime_language_path(language), entries, language)
    return len(languages)


def write_language_file(
    path: Path, entries: list[tuple[tuple[str, str], str]], language: str
) -> None:
    merged_entries = dict(entries)
    sorted_entries = sorted(
        merged_entries.items(),
        key=lambda item: (item[0][1].casefold(), item[0][0].casefold()),
    )
    path.write_text(
        "\n\n".join(
            format_language_entry(key, context, translation)
            for (key, context), translation in sorted_entries
        )
        + ("\n" if sorted_entries else ""),
        encoding="utf-8",
        newline="\n",
    )
    print(f"  Generated {len(sorted_entries)} entries to {path}")


def main() -> None:
    print("=" * 60)
    print("QmClient Translation Sync")
    print("=" * 60)

    strings = read_strings()
    print(f"\nLoaded {len(strings)} unique strings")

    updated_count = generate_configured_languages(strings)

    print(f"\n{'=' * 60}")
    print("Done! QmClient translations synced into:")
    for language in GENERATED_LANGUAGES:
        print(f"  {runtime_language_path(language)}")
    print(f"{'=' * 60}")

    chinese_keys = sum(1 for item in strings if is_chinese(item.key))
    english_keys = len(strings) - chinese_keys
    print("\nSummary:")
    print(f"  Source strings containing CJK: {chinese_keys}")
    print(f"  English source keys: {english_keys}")
    print(f"  Total unique strings: {len(strings)}")
    print(f"  Base language files updated: {updated_count}")


if __name__ == "__main__":
    main()
