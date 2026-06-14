#!/usr/bin/env python3
"""Import Simplified Chinese translations into module-scoped i18n TOML files."""

from __future__ import annotations

from pathlib import Path

import i18n_store
import source_keys
import twlang_qmclient as twlang

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parents[1]
CURRENT_SIMPLIFIED = PROJECT_ROOT / "data" / "languages" / "simplified_chinese.txt"


def read_language_translations(path: Path) -> dict[tuple[str, str], str]:
    if not path.exists():
        return {}
    pairs = twlang.translations(path)
    translations: dict[tuple[str, str], str] = {}
    for identity, values in pairs.items():
        translation = values[1]
        if translation and translation != identity[0]:
            translations[identity] = translation
    return translations


def import_translations() -> dict[str, dict[tuple[str, str], dict[str, str]]]:
    records = source_keys.collect_source_key_records()
    current = read_language_translations(CURRENT_SIMPLIFIED)
    translations_by_identity = {
        identity: {"simplified_chinese": translation}
        for identity, translation in current.items()
    }
    return i18n_store.build_module_store_from_records(records, translations_by_identity)


def main() -> None:
    store = import_translations()
    i18n_store.write_language_store(store)
    count = sum(len(entries) for entries in store.values())
    print(
        f"Wrote {count} Simplified Chinese translations to "
        f"{i18n_store.TRANSLATIONS_DIR.relative_to(PROJECT_ROOT)}"
    )


if __name__ == "__main__":
    main()
