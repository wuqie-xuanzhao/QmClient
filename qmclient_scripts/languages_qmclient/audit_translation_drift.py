#!/usr/bin/env python3
"""Compare current TOML translations against a historical simplified_chinese baseline."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

import i18n_store
import twlang_qmclient as twlang


def load_historical_map(git_ref: str) -> dict[tuple[str, str], str]:
    content = subprocess.run(
        ["git", "show", f"{git_ref}:data/languages/simplified_chinese.txt"],
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    ).stdout
    temp_path = Path(".git") / "tmp_hist_simplified_chinese_for_audit.txt"
    temp_path.write_text(content, encoding="utf-8", newline="\n")
    try:
        return {identity: values[1] for identity, values in twlang.translations(temp_path).items()}
    finally:
        temp_path.unlink(missing_ok=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--git-ref", default="HEAD")
    parser.add_argument("--output", default="qmclient_scripts/languages_qmclient/translation_drift_report.txt")
    args = parser.parse_args()

    historical = load_historical_map(args.git_ref)
    current_store = i18n_store.load_language_store()
    current = i18n_store.language_map_for(current_store, "simplified_chinese")

    rows: list[tuple[str, str, str, str]] = []
    for (key, context), translation in sorted(
        current.items(), key=lambda item: (item[0][1].casefold(), item[0][0].casefold())
    ):
        old = historical.get((key, context))
        if old and old != translation:
            rows.append((context, key, old, translation))

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    lines = [f"git_ref={args.git_ref}", f"diff_count={len(rows)}", ""]
    for context, key, old, new in rows:
        if context:
            lines.append(f"[{context}] {key}")
        else:
            lines.append(key)
        lines.append(f"OLD={old}")
        lines.append(f"NEW={new}")
        lines.append("")
    output.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    print(f"Wrote {len(rows)} drift rows to {output}")


if __name__ == "__main__":
    main()
