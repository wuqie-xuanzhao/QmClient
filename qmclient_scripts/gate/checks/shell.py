# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""Shell 脚本检查：shellcheck。"""

from __future__ import annotations

import shutil
from pathlib import Path

from lib import runner
from lib.report import ResultCollector

REPO_ROOT = Path(__file__).resolve().parents[3]


def run(results: ResultCollector, included: list[str], dry_run: bool = False) -> None:
    sh_files = []
    for p in REPO_ROOT.rglob("*.sh"):
        rel = p.relative_to(REPO_ROOT)
        rel_str = str(rel).replace("\\", "/")
        if rel_str.startswith("qmclient_scripts/") or rel_str.startswith("scripts/"):
            sh_files.append(str(p))
    if not sh_files:
        results.add("PASS", "Shell 脚本检查", "未找到 shell 脚本")
        return
    if dry_run:
        results.add("INFO", "Shell 脚本检查", "DryRun，仅展示命令")
        return
    if shutil.which("shellcheck"):
        code, out = runner.run(
            ["shellcheck", "--exclude=SC1017", "--severity=error"] + sh_files,
            title="shellcheck",
            check=False,
        )
        if code != 0:
            results.add("FAIL", "shellcheck", out)
            return
        results.add("PASS", "shellcheck", "通过")
    else:
        results.add("WARN", "shellcheck", "PATH 中未找到 shellcheck，已跳过")
