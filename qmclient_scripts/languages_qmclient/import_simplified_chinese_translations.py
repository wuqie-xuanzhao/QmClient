# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""Import Simplified Chinese translations into module-scoped i18n TOML files."""

from __future__ import annotations

from pathlib import Path

try:
    from . import i18n_store
    from . import source_keys
    from . import twlang_qmclient as twlang
except ImportError:  # pragma: no cover - script entrypoint fallback
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


def simplified_chinese_patch_entries(
    sc_store: dict[str, dict[tuple[str, str], dict[str, str]]],
) -> dict[str, dict[tuple[str, str], dict[str, str]]]:
    """Build per-module patch maps with only simplified_chinese values."""

    patches: dict[str, dict[tuple[str, str], dict[str, str]]] = {}
    for module, entries in sc_store.items():
        module_patch: dict[tuple[str, str], dict[str, str]] = {}
        for identity, translations in entries.items():
            sc = translations.get("simplified_chinese", "").strip()
            if not sc:
                continue
            module_patch[identity] = {"simplified_chinese": sc}
        if module_patch:
            patches[module] = module_patch
    return patches


def merge_simplified_chinese_into_store(
    existing: dict[str, dict[tuple[str, str], dict[str, str]]],
    sc_store: dict[str, dict[tuple[str, str], dict[str, str]]],
) -> dict[str, dict[tuple[str, str], dict[str, str]]]:
    """Merge SC values into an existing multi-language store (in place) and return patches."""

    patches = simplified_chinese_patch_entries(sc_store)
    for module, entries in patches.items():
        module_store = existing.setdefault(module, {})
        for identity, translations in entries.items():
            module_store.setdefault(identity, {}).update(translations)
    return patches


def main() -> None:
    sc_store = import_translations()
    existing = i18n_store.load_language_store()
    patches = merge_simplified_chinese_into_store(existing, sc_store)
    count = 0
    for module, entries in sorted(patches.items()):
        i18n_store.patch_module_store(module, entries)
        count += len(entries)
    print(
        f"Patched {count} Simplified Chinese translations into "
        f"{i18n_store.TRANSLATIONS_DIR.relative_to(PROJECT_ROOT)}"
    )


if __name__ == "__main__":
    main()
