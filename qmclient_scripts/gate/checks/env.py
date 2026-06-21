#!/usr/bin/env python3
"""环境前置检查。"""

from __future__ import annotations

from pathlib import Path

from lib.report import ResultCollector

REPO_ROOT = Path(__file__).resolve().parents[3]


def run(results: ResultCollector, included: list[str], dry_run: bool = False) -> None:
    if dry_run:
        results.add("INFO", "脚本入口存在性", "DryRun，仅展示命令")
        return
    required = [
        REPO_ROOT / "qmclient_scripts" / "cmake-windows.cmd",
        REPO_ROOT / "scripts" / "check_header_guards.py",
        REPO_ROOT / "scripts" / "check_standard_headers.py",
        REPO_ROOT / "scripts" / "fix_style.py",
        REPO_ROOT / "qmclient_scripts" / "check_config_variables.py",
        REPO_ROOT / "qmclient_scripts" / "check_unused_header_files.py",
        REPO_ROOT / "qmclient_scripts" / "extract_identifiers.py",
        REPO_ROOT / "scripts" / "check_identifiers.py",
        REPO_ROOT / "qmclient_scripts" / "gate" / "check_gate.py",
        REPO_ROOT / "qmclient_scripts" / "gate" / "check_docs.py",
    ]
    ok = True
    for p in required:
        if not p.exists():
            results.add("FAIL", "脚本入口存在性", f"缺少必需路径: {p}")
            ok = False
    if ok:
        results.add("PASS", "脚本入口存在性", "核心检查脚本均已找到")
