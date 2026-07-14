#!/usr/bin/env python3
"""配置变量使用检查。"""

from __future__ import annotations

from pathlib import Path

from lib import runner
from lib.report import ResultCollector

REPO_ROOT = Path(__file__).resolve().parents[3]


def run(results: ResultCollector, included: list[str], dry_run: bool = False) -> None:
	if dry_run:
		results.add("INFO", "配置变量使用检查（Qm/Tc/栖梦）", "DryRun，仅展示命令")
		return
	code, out = runner.run_python(
		str(REPO_ROOT / "qmclient_scripts" / "check_config_variables.py"),
		"--qm",
		title="配置变量使用检查（Qm/Tc/栖梦）",
		check=False,
	)
	if code != 0:
		results.add(
			"FAIL",
			"配置变量使用检查（Qm/Tc/栖梦）",
			f"命令: check_config_variables.py --qm\n{out}",
		)
	else:
		results.add("PASS", "配置变量使用检查（Qm/Tc/栖梦）", "执行通过")
