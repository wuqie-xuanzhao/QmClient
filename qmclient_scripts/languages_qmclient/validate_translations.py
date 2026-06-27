#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path

try:
    from . import i18n_store
    from . import source_keys
    from . import translate_with_local_http
except ImportError:  # pragma: no cover
    import i18n_store
    import source_keys
    import translate_with_local_http


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("--json", dest="json_path", default="")
    parser.add_argument("--languages", nargs="*", default=[])
    parser.add_argument("--limit", type=int, default=20)
    return parser


def _filter_store_languages(
    store: dict[str, dict[tuple[str, str], dict[str, str]]], languages: set[str]
) -> dict[str, dict[tuple[str, str], dict[str, str]]]:
    if not languages:
        return store
    filtered: dict[str, dict[tuple[str, str], dict[str, str]]] = {}
    for module_name, module_entries in store.items():
        for identity, translations in module_entries.items():
            kept_translations = {
                language: translation
                for language, translation in translations.items()
                if language in languages
            }
            if kept_translations:
                filtered.setdefault(module_name, {})[identity] = kept_translations
    return filtered


def main() -> None:
    args = build_parser().parse_args()
    languages = set(args.languages)
    store = _filter_store_languages(i18n_store.load_language_store(), languages)
    source_records, _changed_files, _full_scan = (
        source_keys.collect_incremental_source_key_records()
    )
    active_module_identities = {
        (
            i18n_store.module_name_for_source(record.source),
            record.key,
            record.context,
        )
        for record in source_records
    }
    terminology_by_language = {}
    terminology_path = i18n_store.SCRIPT_DIR / "prompt_assets" / "terminology.toml"
    if terminology_path.exists():
        parsed_terminology = translate_with_local_http.parse_terminology_terms(
            terminology_path.read_text(encoding="utf-8")
        )
        terminology_by_language = {
            "simplified_chinese": parsed_terminology.get("simplified_chinese", {})
        }
    report = i18n_store.translation_quality_report(
        store,
        active_module_identities=active_module_identities,
        terminology_by_language=terminology_by_language,
        limit=args.limit,
    )

    for item in report.warnings:
        print(f"warning: {item}")
    for item in report.errors:
        print(f"error: {item}")

    if args.json_path:
        Path(args.json_path).write_text(
            json.dumps(
                {"errors": report.errors, "warnings": report.warnings},
                ensure_ascii=False,
                indent=2,
            ),
            encoding="utf-8",
        )
    if report.errors:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
