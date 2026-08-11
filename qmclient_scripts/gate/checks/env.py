# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""环境前置检查。"""

from __future__ import annotations

from pathlib import Path
import subprocess

from lib.report import ResultCollector

REPO_ROOT = Path(__file__).resolve().parents[3]


def uninitialized_submodules(output: str) -> list[str]:
	paths = []
	for line in output.splitlines():
		if not line.startswith("-"):
			continue
		parts = line[1:].strip().split(maxsplit=1)
		if len(parts) == 2:
			paths.append(parts[1].split(" ", 1)[0])
	return paths


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
	]
	ok = True
	for p in required:
		if not p.exists():
			results.add("FAIL", "脚本入口存在性", f"缺少必需路径: {p}")
			ok = False
	if ok:
		results.add("PASS", "脚本入口存在性", "核心检查脚本均已找到")

	proc = subprocess.run(
		["git", "submodule", "status", "--recursive"],
		cwd=REPO_ROOT,
		capture_output=True,
		text=True,
		encoding="utf-8",
		errors="replace",
		check=False,
	)
	if proc.returncode != 0:
		results.add("FAIL", "Git 子模块前置检查", proc.stderr or proc.stdout)
		return
	missing = uninitialized_submodules(proc.stdout)
	if missing:
		results.add(
			"FAIL",
			"Git 子模块前置检查",
			"以下子模块未初始化: " + ", ".join(missing) + "\n运行: git submodule update --init --recursive",
		)
		return
	results.add("PASS", "Git 子模块前置检查", "递归子模块均已初始化")
