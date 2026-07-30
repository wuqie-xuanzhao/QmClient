# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""Git 范围收集与文件过滤。"""

from __future__ import annotations

import re
import subprocess
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]


def _git(*args: str, cwd: Path | None = None, check: bool = False) -> str:
	cmd = ["git", "-c", "core.safecrlf=false", *args]
	result = subprocess.run(
		cmd,
		cwd=str(cwd or REPO_ROOT),
		capture_output=True,
		text=True,
		check=False,
	)
	if check and result.returncode != 0:
		raise subprocess.CalledProcessError(result.returncode, cmd, output=result.stdout, stderr=result.stderr)
	return result.stdout


def normalize_path(path: str) -> str:
	return path.replace("\\", "/")


def is_scoped_first_party_file(path: str) -> bool:
	norm = normalize_path(path)
	if not re.match(r"^src/.+\.(c|cc|cpp|h|hpp)$", norm):
		return False
	if norm.startswith(("src/engine/external/", "src/game/generated/", "src/rust-bridge/base/")):
		return False
	return True


def _unique_lines(text: str) -> list[str]:
	seen: set[str] = set()
	out: list[str] = []
	for line in text.splitlines():
		line = line.rstrip("\r")
		if not line or line in seen:
			continue
		seen.add(line)
		out.append(line)
	return out


def _exclude_reason(path: str) -> str:
	norm = normalize_path(path)
	if norm.startswith("src/engine/external/"):
		return "third-party-external"
	if norm.startswith("src/game/generated/"):
		return "generated"
	if norm.startswith("src/rust-bridge/base/"):
		return "rust-bridge-base"
	if norm.startswith("src/") and not re.search(r"\.(c|cc|cpp|h|hpp)$", norm):
		return "extension-not-supported"
	return "not-in-src"


@dataclass(frozen=True)
class ScopeResult:
	branch: list[str]
	unstaged: list[str]
	staged: list[str]
	untracked: list[str]
	changed: list[str]
	included: list[str]
	excluded: list[tuple[str, str]]
	base_ref: str
	base_ref_available: bool
	base_ref_failure_reason: str


def _ref_exists(ref: str) -> bool:
	result = subprocess.run(
		["git", "rev-parse", "--verify", "--quiet", ref],
		cwd=str(REPO_ROOT),
		capture_output=True,
		text=True,
		check=False,
	)
	return result.returncode == 0


def default_base_ref() -> str:
	origin_head = _git("symbolic-ref", "--quiet", "--short", "refs/remotes/origin/HEAD").strip()
	if origin_head and _ref_exists(origin_head):
		return origin_head
	if _ref_exists("origin/master"):
		return "origin/master"
	if _ref_exists("origin/main"):
		return "origin/main"
	if _ref_exists("master"):
		return "master"
	return "main"


def collect_scope(
	base_ref: str | None = None,
	branch_scope_only: bool = False,
	explicit_files: list[str] | None = None,
) -> ScopeResult:
	if base_ref is None:
		base_ref = default_base_ref()

	if explicit_files:
		changed = sorted({normalize_path(f) for f in explicit_files})
		included = []
		excluded = []
		for f in changed:
			if is_scoped_first_party_file(f):
				included.append(f)
			else:
				excluded.append((f, _exclude_reason(f)))
		return ScopeResult(
			branch=[],
			unstaged=[],
			staged=[],
			untracked=[],
			changed=changed,
			included=sorted(set(included)),
			excluded=sorted(set(excluded)),
			base_ref=base_ref,
			base_ref_available=True,
			base_ref_failure_reason="",
		)

	# merge-base
	base_ref_available = True
	base_ref_failure_reason = ""
	branch_files: list[str] = []
	try:
		merge_base = _git("merge-base", base_ref, "HEAD", check=True).strip().splitlines()[0]
		out = _git("diff", "--name-only", "--diff-filter=ACMRD", f"{merge_base}...HEAD")
		branch_files = _unique_lines(out)
	except subprocess.CalledProcessError as exc:
		base_ref_available = False
		base_ref_failure_reason = exc.stderr.strip().replace("\r", " ").replace("\n", " ") if exc.stderr else "git merge-base 返回空结果"
		branch_files = []

	unstaged: list[str] = []
	staged: list[str] = []
	untracked: list[str] = []

	if not branch_scope_only:
		out = _git(
			"diff",
			"--name-only",
			"--diff-filter=ACMRD",
		)
		unstaged = _unique_lines(out)

		out = _git(
			"diff",
			"--cached",
			"--name-only",
			"--diff-filter=ACMRD",
		)
		staged = _unique_lines(out)

		out = _git(
			"ls-files",
			"--others",
			"--exclude-standard",
		)
		untracked = _unique_lines(out)

	combined = branch_files + unstaged + staged + untracked
	changed = sorted(set(normalize_path(path) for path in combined if path))
	included = []
	excluded = []
	for p in combined:
		if not p:
			continue
		norm = normalize_path(p)
		if is_scoped_first_party_file(norm):
			included.append(norm)
		else:
			excluded.append((norm, _exclude_reason(norm)))

	included = sorted(set(included))
	excluded = sorted(set(excluded))

	return ScopeResult(
		branch=branch_files,
		unstaged=unstaged,
		staged=staged,
		untracked=untracked,
		changed=changed,
		included=included,
		excluded=excluded,
		base_ref=base_ref,
		base_ref_available=base_ref_available,
		base_ref_failure_reason=base_ref_failure_reason,
	)
