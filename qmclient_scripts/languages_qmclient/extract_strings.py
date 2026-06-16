#!/usr/bin/env python3
"""Extract localization source keys for reporting/generation only."""

from __future__ import annotations

import os
from pathlib import Path

import source_keys

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parents[1]
STRINGS_FILE = SCRIPT_DIR / "extracted_strings.txt"
AUDIT_REPORT_FILE = source_keys.AUDIT_REPORT_FILE


def collect_strings() -> list[str]:
    return source_keys.collect_source_keys()


def format_record(record: source_keys.SourceKeyRecord) -> str:
    if record.context:
        return f"[{record.context}]\t{record.key}"
    return record.key


def main() -> None:
    records = source_keys.collect_source_key_records()
    audit_report = source_keys.build_string_audit_report()
    summary = source_keys.summarize_source_key_records(records)
    unique_identities = sorted(
        {(record.key, record.context) for record in records},
        key=lambda item: (item[1].casefold(), item[0].casefold(), item[1], item[0]),
    )
    sorted_strings = [
        format_record(source_keys.SourceKeyRecord(key, "", None, context))
        for key, context in unique_identities
    ]
    with STRINGS_FILE.open("w", encoding="utf-8", newline="\n") as file:
        for key in sorted_strings:
            file.write(key + "\n")
    source_keys.write_string_audit_report(AUDIT_REPORT_FILE, audit_report)

    rel_outpath = os.path.relpath(STRINGS_FILE, PROJECT_ROOT)
    rel_audit_path = os.path.relpath(AUDIT_REPORT_FILE, PROJECT_ROOT)
    print(
        f"Extracted {len(sorted_strings)} unique localization strings to {rel_outpath}"
    )
    print(f"Wrote string audit report to {rel_audit_path}")
    print("Category summary:")
    print(f"  Localize/Localizable records: {summary.localize_or_localizable}")
    print(f"  Register help records: {summary.register_help}")
    print(f"  Indirect records: {summary.indirect}")
    print(f"  Extra records: {summary.extra}")
    print(f"  CJK unique strings: {summary.cjk}")
    print(f"  Total records before dedupe: {summary.total_records}")
    print(f"  Total unique strings: {summary.total_unique}")
    print("Audit summary:")
    for category, count in audit_report.summary().items():
        print(f"  {category}: {count}")
    for index, key in enumerate(sorted_strings, start=1):
        print(f"  {index:3d}. {key}")


if __name__ == "__main__":
    main()
