#!/usr/bin/env python3
"""
校验 commit message subject 是否符合 <type>(<scope>): <简述> 规范。

规范见 .agents/skills/qmclient-git-commit/SKILL.md。

用法：
  # 直接校验一段文本
  python qmclient_scripts/check_commit_msg.py "fix(hud): 修复通知栏位置"

  # 从 stdin 读
  git log -1 --format=%s | python qmclient_scripts/check_commit_msg.py

  # 作为 git commit-msg hook：校验本次提交的消息文件
  python qmclient_scripts/check_commit_msg.py --file "$1" --strict

安装为 hook（提示不阻断，去掉 --strict 即纯提示）：
  # Windows PowerShell（管理员或开启开发者模式后符号链接可用）
  New-Item -ItemType SymbolicLink -Path .git/hooks/commit-msg `
    -Target "$PWD/qmclient_scripts/check_commit_msg.py"
  # POSIX
  ln -sf ../../qmclient_scripts/check_commit_msg.py .git/hooks/commit-msg

豁免（不校验）：
  - Merge commit（Merge / 合并 / Automatic merge / Fast-forward 开头）
  - Revert（Revert 开头）
  - 版本号提交（bump version / chore(build): bump version）

未命中规范：默认 exit 0（stderr 提示，不阻断提交）；--strict 时 exit 1（阻断）。
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


SUBJECT_RE = re.compile(
    r"^(?P<type>[A-Za-z]+)(?:\((?P<scope>[^)]+)\))?(?P<breaking>!)?[:：]\s*(?P<desc>.+)$"
)

# 允许的 type（覆盖 git-workflow 分组类型 + PR 模板 + 现状出现的 build/style/improve）
KNOWN_TYPES = {
    "feat",
    "fix",
    "perf",
    "refactor",
    "docs",
    "test",
    "chore",
    "ci",
    "build",
    "revert",
    "style",
    "improve",
}

# 豁免前缀：merge / revert / 版本号
EXEMPT_PATTERNS = [
    re.compile(r"^(Merge|合并|Automatic merge|Fast-forward|自动合并)", re.IGNORECASE),
    re.compile(r"^Revert\b", re.IGNORECASE),
    re.compile(r"^bump\s+version", re.IGNORECASE),
    re.compile(
        r"^(chore|build)(\([\w.\-]+\))?\s*[:：]\s*bump\s+version", re.IGNORECASE
    ),
]


def first_line(message: str) -> str:
    stripped = message.strip()
    return stripped.splitlines()[0].strip() if stripped else ""


def is_exempt(subject: str) -> bool:
    return any(pattern.search(subject) for pattern in EXEMPT_PATTERNS)


def check(subject: str) -> tuple[bool, str]:
    if not subject:
        return False, "subject 为空"
    if is_exempt(subject):
        return True, "豁免（merge / revert / 版本号提交）"

    match = SUBJECT_RE.match(subject)
    if not match:
        return False, "不符合 <type>(<scope>): <简述> 格式"

    commit_type = match.group("type").lower()
    if commit_type not in KNOWN_TYPES:
        return (
            False,
            f"未知 type '{match.group('type')}'，可选：{', '.join(sorted(KNOWN_TYPES))}",
        )

    desc = match.group("desc").strip()
    if len(desc) < 4:
        return False, "简述过短，缺少有效信息"

    return True, "OK"


def main() -> int:
    parser = argparse.ArgumentParser(description="校验 commit message subject 格式")
    parser.add_argument("message", nargs="?", help="待校验文本；不传则从 stdin 读")
    parser.add_argument("--file", help="commit-msg hook 模式：校验该文件内容")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="未命中规范时退出码 1（默认 0，仅提示不阻断）",
    )
    args = parser.parse_args()

    if args.file:
        text = Path(args.file).read_text(encoding="utf-8")
    elif args.message:
        text = args.message
    else:
        text = sys.stdin.read()

    subject = first_line(text)
    ok, reason = check(subject)
    if ok:
        return 0

    hint = (
        f"[commit-msg] subject 不符合规范：{subject!r}\n"
        f"  原因：{reason}\n"
        f"  期望：<type>(<scope>): <中文简述>，如 fix(hud): 修复通知栏位置\n"
        f"  type 可选：{', '.join(sorted(KNOWN_TYPES))}\n"
        f"  规范见 .agents/skills/qmclient-git-commit/SKILL.md"
    )
    print(hint, file=sys.stderr)
    return 1 if args.strict else 0


if __name__ == "__main__":
    raise SystemExit(main())
