# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""代码格式干跑检查（fix_style.py）。"""

from __future__ import annotations

import re
from pathlib import Path

from lib import runner
from lib.report import ResultCollector

REPO_ROOT = Path(__file__).resolve().parents[3]


def run(results: ResultCollector, included: list[str], dry_run: bool = False) -> None:
    if dry_run:
        results.add("INFO", "代码格式检查", "DryRun，仅展示命令")
        return
    if not included:
        results.add(
            "WARN",
            "代码格式检查",
            "未收集到改动范围内可供 fix_style.py 检查的首方 C/C++ 文件",
        )
        return
    results.add(
        "INFO", "代码格式检查范围", f"按收敛后的首方源码范围传入 {len(included)} 个文件"
    )
    fix_style = REPO_ROOT / "qmclient_scripts" / "fix_style.py"
    py = runner.resolve_python_cmd()
    if not py:
        results.add("FAIL", "代码格式检查", "未找到可用的 Python")
        return

    def _run_batch(batch_files: list[str]) -> bool:
        """运行一批文件的格式检查，返回 True 表示通过。"""
        if not batch_files:
            return True
        args = [str(fix_style), "-n"] + batch_files
        code, out = runner.run(
            [py]
            + (
                ["-3"]
                if re.search(r"py\.exe|py$", py, re.IGNORECASE)
                and not py.lower().endswith("python.exe")
                else []
            )
            + args,
            title="代码格式干跑检查",
            check=False,
        )
        if code != 0:
            results.add("FAIL", "代码格式干跑检查", out)
            return False
        return True

    existing = [file for file in included if (REPO_ROOT / file).exists()]
    skipped = len(included) - len(existing)
    if not existing:
        results.add(
            "WARN",
            "代码格式检查",
            "改动范围内首方 C/C++ 文件都已删除，跳过 fix_style.py 检查",
        )
        return
    if skipped:
        results.add("INFO", "代码格式检查范围", f"跳过 {skipped} 个已删除文件")

    batch: list[str] = []
    batch_len = 0
    for file in existing:
        file_len = len(file) + 3
        if batch and batch_len + file_len > 6000:
            if not _run_batch(batch):
                return
            batch = []
            batch_len = 0
        batch.append(file)
        batch_len += file_len
    if not _run_batch(batch):
        return
    results.add("PASS", "代码格式检查", "命名风格检查通过")
