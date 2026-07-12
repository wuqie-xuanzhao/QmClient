# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""Normalize simplified Chinese typography in i18n TOML maintenance sources."""

from __future__ import annotations

try:
    from . import i18n_store
except ImportError:  # pragma: no cover - script entrypoint fallback
    import i18n_store


def main() -> int:
    store = i18n_store.load_language_store()
    changed_entries = 0
    changed_modules = 0

    for module_name, entries in sorted(store.items()):
        module_patch: dict[tuple[str, str], dict[str, str]] = {}
        for identity, translations in sorted(entries.items()):
            translation = translations.get("simplified_chinese", "")
            if not translation:
                continue
            normalized = i18n_store.normalize_translation(
                "simplified_chinese", translation
            )
            if normalized != translation:
                module_patch[identity] = {"simplified_chinese": normalized}

        if module_patch:
            i18n_store.patch_module_store(module_name, module_patch)
            changed_entries += len(module_patch)
            changed_modules += 1

    print(
        "Normalized simplified_chinese translations: "
        f"{changed_entries} entries across {changed_modules} modules"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
