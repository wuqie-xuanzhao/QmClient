# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""结果收集、分类、baseline allowlist 与 JSON/控制台报告。"""

from __future__ import annotations

import hashlib
import json
import re
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Literal

Level = Literal["PASS", "WARN", "FAIL", "INFO"]
Status = Literal["RUN", "SKIP", "NOT_APPLICABLE"]


@dataclass
class CheckItem:
	level: Level
	original_level: Level
	title: str
	detail: str
	category_id: str = ""
	category_label: str = ""
	allowlist_reason: str = ""
	detail_hash: str = ""
	status: Status = "RUN"


class ResultCollector:
	def __init__(self, allowlist_path: Path | None = None):
		self.items: list[CheckItem] = []
		self.pass_count = 0
		self.warn_count = 0
		self.fail_count = 0
		self.skip_count = 0
		self.not_applicable_count = 0
		self._allowlist: dict[tuple[str, str], str] = {}
		if allowlist_path and allowlist_path.exists():
			self._load_allowlist(allowlist_path)

	def _load_allowlist(self, path: Path) -> None:
		data = json.loads(path.read_text(encoding="utf-8-sig"))
		for entry in data.get("entries", []):
			title = str(entry.get("title", ""))
			detail_hash = str(entry.get("detail_hash", ""))
			reason = str(entry.get("reason", "known_baseline_debt"))
			if title and detail_hash:
				self._allowlist[(title, detail_hash)] = reason

	@staticmethod
	def _normalized_detail_hash(detail: str) -> str:
		text = detail.replace("\r\n", "\n").replace("\r", "\n")
		marker = "\n--- 原始尾部输出 ---\n"
		idx = text.find(marker)
		if idx >= 0:
			text = text[:idx]
		lines = sorted({line.strip() for line in text.split("\n") if line.strip()})
		normalized = "\n".join(lines).strip()
		return hashlib.sha256(normalized.encode("utf-8")).hexdigest()

	@staticmethod
	def _category(title: str, detail: str) -> tuple[str, str]:
		text = f"{title}\n{detail}"
		env_patterns = [
			r"环境前置检查",
			r"Python\s*前置检查",
			r"PATH\s*中未找到",
			r"未找到可用的\s*Python",
			r"WindowsApps",
			r"缺少必需路径",
			r"缺少\s*CMake\s*包装脚本",
			r"ddnet-libs",
			r"当前\s*worktree\s*的\s*DDNet\.exe\s*仍在运行",
			r"差异基线不可用",
			r"Found\s+no\s+clang-format",
			r"WinError\s+206",
			r"not\s+a\s+directory",
		]
		for p in env_patterns:
			if re.search(p, text):
				return ("environment", "环境/工具")

		debt_patterns = [
			r"配置变量使用检查",
			r"工作流文档一致性检查",
			r"头文件\s*guard\s*检查",
			r"标准头文件检查",
			r"代码格式干跑检查",
			r"标识符命名检查",
			r"未使用头文件检查",
			r"clang-format\s*附加检查",
			r"clang-format-violations",
			r"未使用配置项",
			r"头文件保护宏不正确",
			r"缺少头文件保护宏",
		]
		for p in debt_patterns:
			if re.search(p, text):
				return ("baseline_debt", "仓库基线债务")

		blocker_patterns = [
			r"严格构建与静态分析入口",
			r"Debug\s*CRT",
			r"MSVC\s*/analyze",
			r"AddressSanitizer",
			r"clang-tidy",
			r"CMake\s*run_",
			r"测试目标",
			r"JSON\s*报告",
			r"cmake-build-debug",
			r"cmake-build-analyze",
			r"run_cxx_tests",
			r"run_rust_tests",
			r"run_tests",
			r"compile_commands",
			r"仓库级检查存在失败项",
		]
		for p in blocker_patterns:
			if re.search(p, text):
				return ("active_blocker", "当前改动/构建阻断")

		return ("general", "一般项")

	def add(
		self,
		level: Level,
		title: str,
		detail: str,
		status: Status = "RUN",
	) -> None:
		category_id, category_label = self._category(title, detail)
		original_level = level
		stored_level = level
		stored_detail = detail
		allowlist_reason = ""
		detail_hash = ""

		if category_id == "baseline_debt":
			detail_hash = self._normalized_detail_hash(detail)
			if level == "FAIL":
				key = (title, detail_hash)
				if key in self._allowlist:
					allowlist_reason = self._allowlist[key]
					stored_level = "WARN"
					stored_detail = f"已按 baseline allowlist 降级为 WARN，reason={allowlist_reason}, detail_hash={detail_hash}\n{detail}"

		if stored_level == "PASS":
			self.pass_count += 1
		elif stored_level == "WARN":
			self.warn_count += 1
		elif stored_level == "FAIL":
			self.fail_count += 1
		if status == "SKIP":
			self.skip_count += 1
		elif status == "NOT_APPLICABLE":
			self.not_applicable_count += 1

		self.items.append(
			CheckItem(
				level=stored_level,
				original_level=original_level,
				title=title,
				detail=stored_detail,
				category_id=category_id,
				category_label=category_label,
				allowlist_reason=allowlist_reason,
				detail_hash=detail_hash,
				status=status,
			)
		)

	def add_skip(self, title: str, detail: str) -> None:
		self.add("INFO", title, detail, status="SKIP")

	def add_not_applicable(self, title: str, detail: str) -> None:
		self.add("INFO", title, detail, status="NOT_APPLICABLE")

	def has_failures(self) -> bool:
		return self.fail_count > 0

	def outcome(self, dry_run: bool = False) -> str:
		if self.has_failures():
			return "FAIL"
		if self.skip_count > 0:
			return "DEGRADED"
		if dry_run:
			return "DRY_RUN"
		return "PASS"

	def write_json(
		self,
		path: Path | None,
		mode: str = "",
		mode_spec: dict | None = None,
		scoped_files: list[str] | None = None,
		changed_files: list[str] | None = None,
		dry_run: bool = False,
	) -> None:
		if path is None:
			return
		path.parent.mkdir(parents=True, exist_ok=True)
		payload = {
			"Mode": mode,
			"Outcome": self.outcome(dry_run),
			"DryRun": dry_run,
			"GeneratedAt": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
			"ModeSpec": mode_spec or {},
			"Summary": {
				"Pass": self.pass_count,
				"Warn": self.warn_count,
				"Fail": self.fail_count,
				"Skip": self.skip_count,
				"NotApplicable": self.not_applicable_count,
			},
			"ChangedFiles": changed_files or [],
			"ScopedFiles": scoped_files or [],
			"Items": [
				{
					"Level": item.level,
					"OriginalLevel": item.original_level,
					"Title": item.title,
					"Detail": item.detail,
					"CategoryId": item.category_id,
					"CategoryLabel": item.category_label,
					"AllowlistReason": item.allowlist_reason,
					"DetailHash": item.detail_hash,
					"Status": item.status,
				}
				for item in self.items
			],
		}
		path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")

	def write_scope_json(
		self,
		path: Path | None,
		base_ref: str,
		base_ref_available: bool,
		base_ref_failure_reason: str,
		included: list[str],
		excluded: list[tuple[str, str]],
		changed: list[str] | None = None,
	) -> None:
		if path is None:
			return
		path.parent.mkdir(parents=True, exist_ok=True)
		payload = {
			"BaseRef": base_ref,
			"BaseRefAvailable": base_ref_available,
			"BaseRefFailureReason": base_ref_failure_reason,
			"ChangedFiles": changed or [],
			"IncludedFiles": included,
			"ExcludedFiles": [{"Path": p, "Reason": r} for p, r in excluded],
		}
		path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
		self.add("INFO", "差异范围报告", f"已写入: {path}")

	def write_summary(
		self,
		mode: str = "",
		mode_target: str = "",
		mode_expectation: str = "",
		mode_blocking_rule: str = "",
		dry_run: bool = False,
	) -> None:
		print("\n==> 检查汇总")
		print(f"[INFO] 模式: {mode}")
		if mode_target:
			print(f"[INFO] 模式目标: {mode_target}")
		if mode_expectation:
			print(f"[INFO] 模式预期: {mode_expectation}")
		if mode_blocking_rule:
			print(f"[INFO] 阻断规则: {mode_blocking_rule}")
		print(f"[INFO] 通过: {self.pass_count}")
		print(f"[INFO] 警告: {self.warn_count}")
		print(f"[INFO] 失败: {self.fail_count}")
		print(f"[INFO] 跳过: {self.skip_count}")
		print(f"[INFO] 不适用: {self.not_applicable_count}")
		print(f"[INFO] 有效结果: {self.outcome(dry_run)}")

		skips = [i for i in self.items if i.status == "SKIP"]
		if skips:
			print("\n跳过清单：")
			for item in skips:
				print(f"[SKIP] {item.title}: {item.detail[:200]}")

		warns = [i for i in self.items if i.level == "WARN"]
		if warns:
			print("\n警告清单：")
			for item in warns:
				print(f"[WARN] {item.title}: {item.detail[:200]}")

		fails = [i for i in self.items if i.level == "FAIL"]
		if fails:
			print("\n失败清单：")
			for item in fails:
				print(f"[FAIL] {item.title}: {item.detail[:200]}")
