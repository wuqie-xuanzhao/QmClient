# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""头文件相关检查：guard、标准头文件、未使用头文件。"""

from __future__ import annotations

from pathlib import Path

from lib import runner
from lib.report import ResultCollector

REPO_ROOT = Path(__file__).resolve().parents[3]


def _run_script(
    results: ResultCollector, title: str, script: Path, dry_run: bool
) -> None:
    if dry_run:
        results.add("INFO", title, "DryRun，仅展示命令")
        return
    code, out = runner.run_python(
        str(script), title=title, fail_level="FAIL", check=False
    )
    if code != 0:
        results.add("FAIL", title, out)
    else:
        results.add("PASS", title, "执行通过")


def run(results: ResultCollector, included: list[str], dry_run: bool = False) -> None:
    _run_script(
        results,
        "头文件 guard 检查",
        REPO_ROOT / "qmclient_scripts" / "check_header_guards.py",
        dry_run,
    )
    _run_script(
        results,
        "标准头文件检查",
        REPO_ROOT / "scripts" / "check_standard_headers.py",
        dry_run,
    )
    _run_script(
        results,
        "未使用头文件检查",
        REPO_ROOT / "qmclient_scripts" / "check_unused_header_files.py",
        dry_run,
    )
