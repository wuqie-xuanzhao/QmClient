# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""clang-format 附加检查（逐文件）。"""

from __future__ import annotations

import shutil
from pathlib import Path

from lib import runner
from lib.report import ResultCollector

REPO_ROOT = Path(__file__).resolve().parents[3]


def run(results: ResultCollector, included: list[str], dry_run: bool = False) -> None:
    if not shutil.which("clang-format"):
        results.add(
            "WARN", "clang-format 附加检查", "PATH 中未找到 clang-format，已跳过"
        )
        return
    for file in included:
        code, out = runner.run(
            ["clang-format", "--dry-run", "--Werror", str(REPO_ROOT / file)],
            title=f"clang-format 附加检查: {file}",
            check=False,
        )
        if code != 0:
            results.add("WARN", f"clang-format 附加检查: {file}", out)
        else:
            results.add("PASS", f"clang-format 附加检查: {file}", "通过")
