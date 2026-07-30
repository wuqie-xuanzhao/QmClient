# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""Python 代码风格检查：ruff format + ruff check。"""

from __future__ import annotations

import shutil

from lib import runner
from lib.report import ResultCollector

RUFF_EXCLUDES = ["ddnet_libs"]


def run(results: ResultCollector, included: list[str], dry_run: bool = False) -> None:
    if not shutil.which("ruff"):
        results.add("WARN", "Python 代码风格检查 (ruff)", "PATH 中未找到 ruff，已跳过")
        return
    if dry_run:
        results.add("INFO", "Python 代码风格检查 (ruff)", "DryRun，仅展示命令")
        return
    ruff_format_cmd = ["ruff", "format", "--check"]
    ruff_check_cmd = ["ruff", "check"]
    for path in RUFF_EXCLUDES:
        ruff_format_cmd.extend(["--exclude", path])
        ruff_check_cmd.extend(["--exclude", path])
    code, out = runner.run(ruff_format_cmd, title="ruff format", check=False)
    if code != 0:
        results.add("FAIL", "ruff format", out)
        return
    results.add("PASS", "ruff format", "通过")
    code, out = runner.run(ruff_check_cmd, title="ruff check", check=False)
    if code != 0:
        results.add("FAIL", "ruff check", out)
        return
    results.add("PASS", "ruff check", "通过")
