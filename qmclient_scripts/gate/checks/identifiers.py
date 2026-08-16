# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""标识符命名检查。"""

from __future__ import annotations

import csv
import os
import re
import subprocess
import tempfile
from pathlib import Path

from checks.strict_build import _changed_line_ranges
from lib import runner
from lib import scope
from lib.report import ResultCollector

REPO_ROOT = Path(__file__).resolve().parents[3]
SOURCE_SUFFIXES = (".c", ".cc", ".cpp")


def _summarize_identifier_output(out: str) -> str:
	lines = [line for line in out.splitlines() if line.strip()]
	violation_lines = [line for line in lines if re.match(r"^[A-Za-z]:\\.*:\d+:\d+: ", line)]
	preview = "\n".join(violation_lines[:20])
	suffix = ""
	if len(violation_lines) > 20:
		suffix = f"\n... 另有 {len(violation_lines) - 20} 条命名提示未展开"
	if preview:
		return f"check_identifiers.py 报告 {len(violation_lines)} 条命名提示；该阶段是 full gate 高噪音附加命名检查，已降级为 WARN。\n" + preview + suffix
	return "check_identifiers.py 返回非零退出码；该阶段是 full gate 高噪音附加命名检查，已降级为 WARN。\n" + out


def _existing_source_files(included: list[str]) -> list[str]:
	source_files = []
	for file in included:
		normalized_file = file.replace("\\", "/")
		source_path = Path(normalized_file)
		if not source_path.is_absolute():
			source_path = REPO_ROOT / source_path
		if normalized_file.startswith("src/test/"):
			continue
		if normalized_file.endswith(SOURCE_SUFFIXES) and source_path.is_file():
			source_files.append(normalized_file)
	return source_files


def _existing_analysis_files(included: list[str]) -> list[str]:
	analysis_files = []
	for file in included:
		normalized_file = scope.normalize_path(file)
		if normalized_file.startswith("src/test/"):
			continue
		path = Path(normalized_file)
		if not path.is_absolute():
			path = REPO_ROOT / path
		if path.is_file():
			analysis_files.append(normalized_file)
	return analysis_files


def _filter_changed_rows(rows: list[dict[str, str]], changed_ranges: dict[str, list[tuple[int, int]]]) -> list[dict[str, str]]:
	filtered_rows = []
	for row in rows:
		row_file = scope.normalize_path(row.get("file", "")).lower()
		try:
			row_line = int(row.get("line", "0"))
		except ValueError:
			continue
		for file, ranges in changed_ranges.items():
			normalized_file = scope.normalize_path(file).lower()
			if (row_file == normalized_file or row_file.endswith("/" + normalized_file)) and any(start <= row_line <= end for start, end in ranges):
				filtered_rows.append(row)
				break
	return filtered_rows


def run(
	results: ResultCollector,
	included: list[str],
	dry_run: bool = False,
	base_ref: str | None = None,
) -> None:
	source_files = [str(REPO_ROOT / file) for file in _existing_source_files(included)]
	if not source_files:
		results.add(
			"WARN",
			"标识符命名检查",
			"未找到改动范围内可供 extract_identifiers.py 分析的首方源文件",
		)
		return
	if dry_run:
		results.add("INFO", "标识符命名检查", "DryRun")
		return
	if base_ref is None:
		base_ref = scope.default_base_ref()
	changed_ranges = _changed_line_ranges(_existing_analysis_files(included), base_ref)
	runner.print_section("标识符命名检查")
	with tempfile.NamedTemporaryFile(mode="w+", suffix=".txt", delete=False) as tmp:
		tmp_path = tmp.name
	try:
		py = runner.resolve_python_cmd()
		if not py:
			results.add("FAIL", "标识符命名检查", "未找到 Python")
			return
		with open(tmp_path, "w", encoding="utf-8") as f:
			proc = subprocess.run(
				[
					py,
					str(REPO_ROOT / "qmclient_scripts" / "extract_identifiers.py"),
					*source_files,
				],
				stdout=f,
				stderr=subprocess.PIPE,
				text=True,
				encoding="utf-8",
				errors="replace",
			)
		if proc.returncode != 0:
			results.add(
				"WARN",
				"标识符命名检查解析失败",
				"extract_identifiers.py 未能解析当前编译环境；该阶段是 full gate 附加命名检查，已降级为 WARN。\n" + proc.stderr,
			)
			return
		with open(tmp_path, encoding="utf-8", newline="") as f:
			reader = csv.DictReader(f)
			fieldnames = reader.fieldnames
			rows = _filter_changed_rows(list(reader), changed_ranges)
		if not fieldnames:
			results.add("WARN", "标识符命名检查解析失败", "extract_identifiers.py 未生成 CSV 表头")
			return
		with open(tmp_path, "w", encoding="utf-8", newline="") as f:
			writer = csv.DictWriter(f, fieldnames=fieldnames)
			writer.writeheader()
			writer.writerows(rows)
		with open(tmp_path, "r", encoding="utf-8") as f:
			code2, out = runner.run(
				[py, str(REPO_ROOT / "scripts" / "check_identifiers.py")],
				title="检查标识符",
				check=False,
				stdin=f,
			)
		if code2 != 0:
			results.add("WARN", "标识符命名检查", _summarize_identifier_output(out))
			return
		results.add("PASS", "标识符命名检查", "命名风格检查通过")
	finally:
		if os.path.exists(tmp_path):
			os.unlink(tmp_path)
