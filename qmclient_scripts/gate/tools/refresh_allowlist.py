# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""根据 check-gate JSON 报告刷新 baseline debt allowlist。"""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[3]


def load_allowlist(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {"version": 1, "entries": []}
    return json.loads(path.read_text(encoding="utf-8-sig"))


def build_entry_key(entry: dict[str, str]) -> tuple[str, str]:
    return entry["title"], entry["detail_hash"]


def resolve_repo_path(path: Path) -> Path:
    if path.is_absolute():
        return path
    return REPO_ROOT / path


def main() -> int:
    parser = argparse.ArgumentParser(description="刷新 baseline debt allowlist")
    parser.add_argument(
        "--report", required=True, type=Path, help="check-gate JSON 报告路径"
    )
    parser.add_argument(
        "--output", required=True, type=Path, help="baseline debt allowlist 输出路径"
    )
    parser.add_argument(
        "--rewrite", action="store_true", help="按当前报告全量重写 allowlist"
    )
    parser.add_argument(
        "--dry-run", action="store_true", help="只打印 diff，不写回文件"
    )
    args = parser.parse_args()

    report_path = resolve_repo_path(args.report)
    output_path = resolve_repo_path(args.output)

    with report_path.open("r", encoding="utf-8-sig") as file:
        report = json.load(file)

    report_base_ref = report.get("BaseRef", "")
    report_generated_at = report.get("GeneratedAt", "")

    report_entries: list[dict[str, str]] = []
    for item in report.get("Items", []):
        if item.get("CategoryId") != "baseline_debt":
            continue
        if item.get("OriginalLevel") != "FAIL" and item.get("Level") != "FAIL":
            continue
        title = item.get("Title")
        detail_hash = item.get("DetailHash")
        if not title or not detail_hash:
            continue
        report_entries.append(
            {
                "title": title,
                "detail_hash": detail_hash,
                "reason": "known_baseline_debt",
                "base_ref": report_base_ref,
                "added_at": datetime.now(timezone.utc)
                .replace(microsecond=0)
                .isoformat()
                .replace("+00:00", "Z"),
                "source_report": report_path.as_posix(),
                "source_generated_at": report_generated_at,
            }
        )

    existing_allowlist = load_allowlist(output_path)
    existing_entries = list(existing_allowlist.get("entries", []))
    existing_by_key = {
        build_entry_key(entry): entry
        for entry in existing_entries
        if entry.get("title") and entry.get("detail_hash")
    }
    report_by_key = {build_entry_key(entry): entry for entry in report_entries}

    added_keys = sorted(set(report_by_key) - set(existing_by_key))
    removed_keys = sorted(set(existing_by_key) - set(report_by_key))

    if args.rewrite:
        merged_entries = [report_by_key[key] for key in sorted(report_by_key)]
    else:
        merged_entries = list(existing_entries)
        for key in added_keys:
            merged_entries.append(report_by_key[key])

    print(f"报告中的 baseline debt 条目: {len(report_entries)}")
    print(f"现有 allowlist 条目: {len(existing_entries)}")
    print(f"新增条目: {len(added_keys)}")
    for title, detail_hash in added_keys:
        print(f"  + {title} [{detail_hash}]")
    if args.rewrite:
        print(f"将被移除的旧条目: {len(removed_keys)}")
        for title, detail_hash in removed_keys:
            print(f"  - {title} [{detail_hash}]")
    elif removed_keys:
        print(f"未自动移除的旧条目: {len(removed_keys)}")
        for title, detail_hash in removed_keys:
            print(f"  = 保留 {title} [{detail_hash}]")

    allowlist = {
        "version": 1,
        "entries": merged_entries,
    }

    if args.dry_run:
        print("Dry-run：未写回 allowlist 文件。")
        return 0

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        json.dumps(allowlist, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    mode = "全量重写" if args.rewrite else "增量合并"
    print(f"已写入 baseline debt allowlist（{mode}）：{output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
