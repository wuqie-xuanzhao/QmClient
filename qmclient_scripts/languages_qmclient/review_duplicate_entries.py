# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""Review duplicate and similar entries in DDNet language files.

This script is intentionally read-only. It prints a terminal report with
line numbers so an AI or maintainer can decide what to edit manually.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path

import source_keys

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parents[1]
DEFAULT_LANGUAGE_FILE = PROJECT_ROOT / "data" / "languages" / "simplified_chinese.txt"
LANGUAGE_DIR = PROJECT_ROOT / "data" / "languages"
STRINGS_FILE = SCRIPT_DIR / "extracted_strings.txt"
SIMILAR_TEXT_RE = re.compile(r"[\W_]+", re.UNICODE)
UI_PUNCTUATION_ONLY_RE = re.compile(r"[:：()\[\]{}]")


@dataclass(frozen=True)
class LanguageEntry:
    file: Path
    key: str
    context: str
    translation: str
    line: int
    translation_line: int | None


@dataclass(frozen=True)
class EntryGroup:
    title: str
    entries: list[LanguageEntry]


@dataclass(frozen=True)
class AuditReport:
    entries: list[LanguageEntry]
    duplicate_keys: list[EntryGroup]
    similar_keys: list[EntryGroup]
    blank_translations: list[LanguageEntry]
    duplicate_translations: list[EntryGroup]
    similar_translations: list[EntryGroup]
    unused: list[LanguageEntry]


def parse_language_file(path: Path) -> list[LanguageEntry]:
    entries: list[LanguageEntry] = []
    current_context = ""
    pending: tuple[str, int] | None = None

    with path.open(encoding="utf-8-sig") as fileobj:
        for line_no, raw_line in enumerate(fileobj, start=1):
            line = raw_line.rstrip("\r\n")
            if not line or line.startswith("#"):
                if pending is not None:
                    key, key_line = pending
                    entries.append(
                        LanguageEntry(path, key, current_context, "", key_line, None)
                    )
                    pending = None
                if not line:
                    current_context = ""
                continue
            if line.startswith("[") and not line.startswith("[%"):
                if pending is not None:
                    key, key_line = pending
                    entries.append(
                        LanguageEntry(path, key, current_context, "", key_line, None)
                    )
                    pending = None
                if line.endswith("]"):
                    current_context = line[1:-1]
                continue
            if line.startswith("== "):
                if pending is not None:
                    key, key_line = pending
                    entries.append(
                        LanguageEntry(
                            path,
                            key,
                            current_context,
                            line[3:],
                            key_line,
                            line_no,
                        )
                    )
                    pending = None
                continue

            if pending is not None:
                key, key_line = pending
                entries.append(
                    LanguageEntry(path, key, current_context, "", key_line, None)
                )
            pending = (line, line_no)

    if pending is not None:
        key, key_line = pending
        entries.append(LanguageEntry(path, key, current_context, "", key_line, None))
    return entries


def normalize_similar_text(text: str) -> str:
    return SIMILAR_TEXT_RE.sub("", text).casefold()


def only_ui_punctuation_diff(values: set[str]) -> bool:
    if len(values) < 2:
        return False
    stripped = {UI_PUNCTUATION_ONLY_RE.sub("", value).strip() for value in values}
    return len(stripped) == 1


def _groups_by(
    entries: list[LanguageEntry],
    key_fn,
    title_fn,
    *,
    min_normalized_length: int = 2,
) -> list[EntryGroup]:
    grouped: dict[str, list[LanguageEntry]] = {}
    titles: dict[str, str] = {}
    for entry in entries:
        group_key = key_fn(entry)
        if not group_key or len(group_key) < min_normalized_length:
            continue
        grouped.setdefault(group_key, []).append(entry)
        titles.setdefault(group_key, title_fn(entry))
    return [
        EntryGroup(titles[group_key], group_entries)
        for group_key, group_entries in sorted(grouped.items())
        if len(group_entries) > 1
    ]


def find_duplicate_keys(entries: list[LanguageEntry]) -> list[EntryGroup]:
    return _groups_by(
        entries,
        lambda entry: f"{entry.context}\0{entry.key}",
        lambda entry: entry.key,
        min_normalized_length=1,
    )


def find_similar_keys(entries: list[LanguageEntry]) -> list[EntryGroup]:
    groups = _groups_by(
        entries,
        lambda entry: f"{entry.context}\0{normalize_similar_text(entry.key)}",
        lambda entry: normalize_similar_text(entry.key),
    )
    return [
        group
        for group in groups
        if len({entry.key for entry in group.entries}) > 1
        and not only_ui_punctuation_diff({entry.key for entry in group.entries})
    ]


def find_duplicate_translations(entries: list[LanguageEntry]) -> list[EntryGroup]:
    translated_entries = [entry for entry in entries if entry.translation]
    return _groups_by(
        translated_entries,
        lambda entry: f"{entry.context}\0{entry.translation}",
        lambda entry: entry.translation,
        min_normalized_length=1,
    )


def find_similar_translations(entries: list[LanguageEntry]) -> list[EntryGroup]:
    translated_entries = [entry for entry in entries if entry.translation]
    groups = _groups_by(
        translated_entries,
        lambda entry: f"{entry.context}\0{normalize_similar_text(entry.translation)}",
        lambda entry: normalize_similar_text(entry.translation),
    )
    return [
        group
        for group in groups
        if len({entry.translation for entry in group.entries}) > 1
        and not only_ui_punctuation_diff({entry.translation for entry in group.entries})
    ]


def parse_extracted_string_identity(line: str) -> tuple[str, str]:
    if line.startswith("[") and "]\t" in line:
        context, key = line.split("]\t", 1)
        return key, context[1:]
    return line, ""


def collect_used_keys() -> set[tuple[str, str]]:
    if not STRINGS_FILE.exists():
        return source_keys.collect_source_key_identities()
    with STRINGS_FILE.open("r", encoding="utf-8") as file:
        return {
            parse_extracted_string_identity(line.rstrip("\n"))
            for line in file
            if line.strip()
        }


def find_unused(
    entries: list[LanguageEntry], used_keys: set[tuple[str, str]]
) -> list[LanguageEntry]:
    return [entry for entry in entries if (entry.key, entry.context) not in used_keys]


def find_blank_translations(entries: list[LanguageEntry]) -> list[LanguageEntry]:
    return [entry for entry in entries if not entry.translation]


def review_language_files(
    paths: list[Path], used_keys: set[tuple[str, str]] | None = None
) -> AuditReport:
    entries: list[LanguageEntry] = []
    for path in paths:
        entries.extend(parse_language_file(path))
    if used_keys is None:
        used_keys = collect_used_keys()
    return AuditReport(
        entries,
        find_duplicate_keys(entries),
        find_similar_keys(entries),
        find_blank_translations(entries),
        find_duplicate_translations(entries),
        find_similar_translations(entries),
        find_unused(entries, used_keys),
    )


def _entry_location(entry: LanguageEntry) -> str:
    rel_file = os.path.relpath(entry.file, PROJECT_ROOT)
    if entry.translation_line is None:
        return f"{rel_file}:{entry.line}"
    return f"{rel_file}:{entry.line}/{entry.translation_line}"


def _print_groups(title: str, groups: list[EntryGroup], limit: int) -> None:
    print(title)
    if not groups:
        print("  none")
        return
    for group in groups[:limit]:
        print(f"  - {group.title}")
        for entry in group.entries:
            context = f" context=[{entry.context}]" if entry.context else ""
            print(f"    {_entry_location(entry)}{context}")
            print(f"      key: {entry.key}")
            print(f"      == {entry.translation}")
    if len(groups) > limit:
        print(f"  ... {len(groups) - limit} more groups")


def _print_entries(title: str, entries: list[LanguageEntry], limit: int) -> None:
    print(title)
    if not entries:
        print("  none")
        return
    for entry in entries[:limit]:
        context = f" context=[{entry.context}]" if entry.context else ""
        print(f"  {_entry_location(entry)}{context}")
        print(f"    key: {entry.key}")
        print(f"    == {entry.translation}")
    if len(entries) > limit:
        print(f"  ... {len(entries) - limit} more entries")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Review duplicate and similar entries in DDNet language files"
    )
    parser.add_argument("--language-file", type=Path, default=DEFAULT_LANGUAGE_FILE)
    parser.add_argument(
        "--all-languages",
        action="store_true",
        help="Review every data/languages/*.txt language file except index.txt.",
    )
    parser.add_argument("--show-groups", type=int, default=30)
    parser.add_argument("--show-unused", type=int, default=80)
    parser.add_argument("--fail-on-duplicates", action="store_true")
    parser.add_argument("--fail-on-unused", action="store_true")
    args = parser.parse_args()

    if args.all_languages:
        language_files = sorted(
            path for path in LANGUAGE_DIR.glob("*.txt") if path.name != "index.txt"
        )
    else:
        language_file = args.language_file
        if not language_file.is_absolute():
            language_file = PROJECT_ROOT / language_file
        language_files = [language_file]

    report = review_language_files(language_files)

    print("Language files:")
    for language_file in language_files:
        print(f"  {os.path.relpath(language_file, PROJECT_ROOT)}")
    print(f"Entries: {len(report.entries)}")
    print(f"Duplicate key groups: {len(report.duplicate_keys)}")
    print(f"Similar key groups: {len(report.similar_keys)}")
    print(f"Blank translations: {len(report.blank_translations)}")
    print(f"Duplicate translation groups: {len(report.duplicate_translations)}")
    print(f"Similar translation groups: {len(report.similar_translations)}")
    print(f"Candidate unused entries: {len(report.unused)}")
    print()

    _print_groups("Duplicate keys:", report.duplicate_keys, args.show_groups)
    print()
    _print_groups("Similar keys:", report.similar_keys, args.show_groups)
    print()
    _print_entries("Blank translations:", report.blank_translations, args.show_unused)
    print()
    _print_groups(
        "Duplicate translations:", report.duplicate_translations, args.show_groups
    )
    print()
    _print_groups(
        "Similar translations:", report.similar_translations, args.show_groups
    )
    print()
    _print_entries("Candidate unused entries:", report.unused, args.show_unused)

    if args.fail_on_duplicates and (
        report.duplicate_keys
        or report.similar_keys
        or report.blank_translations
        or report.duplicate_translations
        or report.similar_translations
    ):
        return 1
    if args.fail_on_unused and report.unused:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
