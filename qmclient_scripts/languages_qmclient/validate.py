#!/usr/bin/env python3
"""Validate project language sync and key coverage.

This script is read-only. It validates:
- extracted string list freshness
- generated simplified_chinese.txt covers extracted English source keys
- module-scoped i18n store parses and exposes configured translations
- phase-2 cleanup removed the legacy overlay directory
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.dirname(__file__))

import generate_all
import i18n_store
import source_keys
import twlang_qmclient as twlang

errors = []
count = 0
parsed_language_files = 0
legacy_overlay_dir = os.path.join(
    os.path.dirname(__file__), "..", "..", "data", "qmclient", "languages"
)
audit_report_path = source_keys.AUDIT_REPORT_FILE

extracted_strings = generate_all.read_strings()
current_strings = sorted(
    {
        generate_all.SourceString(record.key, record.context)
        for record in source_keys.collect_source_key_records()
    },
    key=lambda item: (item.context.casefold(), item.key.casefold()),
)
current_audit = source_keys.build_string_audit_report()
if extracted_strings != current_strings:
    errors.append(
        "extracted_strings.txt is out of date. Run "
        "python qmclient_scripts/languages_qmclient/extract_strings.py"
    )
    print(
        "  FAIL: extracted_strings.txt is out of date. Run "
        "python qmclient_scripts/languages_qmclient/extract_strings.py"
    )

if not audit_report_path.exists():
    errors.append(
        "string audit report is missing. Run "
        "python qmclient_scripts/languages_qmclient/extract_strings.py"
    )
    print(
        "  FAIL: string audit report is missing. Run "
        "python qmclient_scripts/languages_qmclient/extract_strings.py"
    )
else:
    source_keys.read_string_audit_report(audit_report_path)
    count += 1
    print("  OK: extracted_audit_report.json is readable")

active_source_keys = sorted(
    (item.key, item.context)
    for item in extracted_strings
    if not generate_all.is_chinese(item.key)
)

for language in generate_all.GENERATED_LANGUAGES:
    path = generate_all.runtime_language_path(language)
    if not path.exists():
        errors.append(f"{language}.txt is missing")
        print(f"  FAIL: {language}.txt is missing")
        continue
    try:
        parsed = twlang.translations(path)
    except Exception as e:
        errors.append(f"{language}.txt: {e}")
        print(f"  FAIL: {language}.txt: {e}")
        continue
    parsed_language_files += 1
    missing_base_keys = sorted(set(active_source_keys) - set(parsed.keys()))
    if missing_base_keys:
        errors.append(f"{language}.txt: missing_base_keys={missing_base_keys[:10]}")
        print(f"  FAIL: {language}.txt: missing_base_keys={missing_base_keys[:10]}")
    else:
        print(f"  OK: {language}.txt covers {len(active_source_keys)} source keys")

if parsed_language_files == len(generate_all.GENERATED_LANGUAGES):
    count += 1

loaded_i18n_store = i18n_store.load_language_store()
i18n_store_map = i18n_store.language_map_for(loaded_i18n_store, "simplified_chinese")
if not loaded_i18n_store:
    errors.append(
        "module i18n store is empty: qmclient_scripts/languages_qmclient/translations/i18n"
    )
    print(
        "  FAIL: module i18n store is empty: "
        "qmclient_scripts/languages_qmclient/translations/i18n"
    )
else:
    module_count = len(loaded_i18n_store)
    translation_count = len(i18n_store_map)
    print(
        "  OK: module i18n store loaded "
        f"{translation_count} simplified_chinese translations from {module_count} modules"
    )
    count += 1

missing_toml_simplified = i18n_store.missing_translations_for(
    loaded_i18n_store, active_source_keys, "simplified_chinese"
)
if missing_toml_simplified:
    print(
        "  NOTE: TOML missing simplified_chinese translations: "
        f"{len(missing_toml_simplified)}"
    )
    for key, context in missing_toml_simplified[:10]:
        print(f"    - [{context}] {key}" if context else f"    - {key}")
else:
    print("  NOTE: TOML missing simplified_chinese translations: 0")

for language in generate_all.GENERATED_LANGUAGES:
    missing_toml = i18n_store.missing_translations_for(
        loaded_i18n_store, active_source_keys, language
    )
    if missing_toml:
        errors.append(f"TOML missing {language} translations: {len(missing_toml)}")
        print(f"  FAIL: TOML missing {language} translations: {len(missing_toml)}")
        for key, context in missing_toml[:10]:
            print(f"    - [{context}] {key}" if context else f"    - {key}")

if os.path.isdir(legacy_overlay_dir):
    errors.append("legacy overlay directory still exists: data/qmclient/languages")
    print("  FAIL: legacy overlay directory still exists: data/qmclient/languages")
else:
    print("  OK: legacy overlay directory removed")
    count += 1

if current_audit.violation:
    preview = current_audit.violation[:5]
    errors.append(f"blocking violations: {len(current_audit.violation)}")
    print(f"  FAIL: blocking violations: {len(current_audit.violation)}")
    for item in preview:
        rel_file = os.path.relpath(item.file, source_keys.PROJECT_ROOT)
        print(f"    - {rel_file}:{item.line}: {item.text} ({item.reason})")
else:
    print("  OK: blocking violations: 0")
    count += 1

if current_audit.needs_review:
    print(f"  NOTE: manual review backlog: {len(current_audit.needs_review)}")
    for item in current_audit.needs_review[:5]:
        rel_file = os.path.relpath(item.file, source_keys.PROJECT_ROOT)
        print(f"    - {rel_file}:{item.line}: {item.text} ({item.reason})")
else:
    print("  NOTE: manual review backlog: 0")

print()
if errors:
    print(f"{len(errors)} files with errors!")
    sys.exit(1)
else:
    print(
        f"All validation checks passed. "
        f"Parsed {parsed_language_files} generated language files."
    )
