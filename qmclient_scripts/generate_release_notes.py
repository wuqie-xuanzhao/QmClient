#!/usr/bin/env python3
"""
根据 tag / ref 区间内的 commit 生成 GitHub Release 说明。

通道：
- stable（正式版）：tag 形如 vX.Y.Z，对应 GitHub 正式 Release
- pre-release（预发布）：nightly / rc / beta 等，对应 GitHub Pre-release

输出按「功能领域」分组、中文优先，例如：

# QmClient v2.74.9 · 正式版（Stable）
> 通道：正式发布 · 建议日常使用
...

工程类 commit（ci/build/gate 等）解析但不渲染。
优先读取 commit body：

    Release-ZH: 中文发布说明

缺失时回退 subject 描述。

脚本输出是机械化草稿；正式版终稿按 docs/RELEASE_NOTE_TEMPLATE.md 人工润色。
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
COMMIT_SEPARATOR = "\x1e"
FIELD_SEPARATOR = "\x1f"

SUBJECT_RE = re.compile(
    r"^(?P<type>[A-Za-z]+)(?:\((?P<scope>[^)]+)\))?(?P<breaking>!)?[:：]\s*(?P<desc>.+)$"
)
TRAILER_RE = re.compile(r"^Release-ZH:\s*(?P<text>.+)$")
# 正式版 tag：v2.74.9 / 2.74.9（不含 rc/beta/nightly 后缀）
STABLE_TAG_RE = re.compile(r"^v?(?P<ver>\d+\.\d+(?:\.\d+)?)$")
PRE_RELEASE_HINT_RE = re.compile(
    r"(?i)(nightly|rc\d*|alpha|beta|pre|preview|snapshot|dev)"
)

PLAYER_TYPES = {"feat", "fix", "perf", "refactor", "improve", "revert"}

TYPE_PRIORITY = {
    "feat": 0,
    "improve": 1,
    "fix": 2,
    "perf": 3,
    "refactor": 4,
    "revert": 5,
}

DOMAIN_ORDER = [
    "界面与视觉",
    "聊天与社交",
    "翻译与语言",
    "语音",
    "输入与操作",
    "媒体与渲染",
    "设置页",
    "平台适配",
    "客户端与核心",
    "其他",
]

INTERNAL_DOMAIN = "开发者与构建"
FALLBACK_DOMAIN = "其他"

SCOPE_TO_DOMAIN: dict[str, str] = {
    "ui": "界面与视觉",
    "hud": "界面与视觉",
    "qmhud": "界面与视觉",
    "hud-editor": "界面与视觉",
    "nameplate": "界面与视觉",
    "nameplates": "界面与视觉",
    "scoreboard": "界面与视觉",
    "menus": "界面与视觉",
    "menu": "界面与视觉",
    "menus_demo": "界面与视觉",
    "pie_menu": "界面与视觉",
    "particles": "界面与视觉",
    "skins": "界面与视觉",
    "entity-bg": "界面与视觉",
    "gores": "界面与视觉",
    "image": "界面与视觉",
    "assets": "界面与视觉",
    "资源页面": "界面与视觉",
    "chat": "聊天与社交",
    "聊天交互": "聊天与社交",
    "friends": "聊天与社交",
    "spectator": "聊天与社交",
    "console": "聊天与社交",
    "通知栏": "聊天与社交",
    "translate": "翻译与语言",
    "i18n": "翻译与语言",
    "翻译模块": "翻译与语言",
    "macos与翻译": "翻译与语言",
    "ime": "翻译与语言",
    "agents": "翻译与语言",
    "voice": "语音",
    "语音": "语音",
    "语音模块": "语音",
    "voicesrv": "语音",
    "input": "输入与操作",
    "input-overlay": "输入与操作",
    "bind": "输入与操作",
    "fast-practice": "输入与操作",
    "practice": "输入与操作",
    "lyrics": "媒体与渲染",
    "sound": "媒体与渲染",
    "vulkan": "媒体与渲染",
    "live": "媒体与渲染",
    "game": "媒体与渲染",
    "modes": "媒体与渲染",
    "collision": "媒体与渲染",
    "trajectory": "媒体与渲染",
    "player_points": "媒体与渲染",
    "settings": "设置页",
    "settings-ui": "设置页",
    "config": "设置页",
    "spec": "设置页",
    "macos": "平台适配",
    "linux": "平台适配",
    "windows": "平台适配",
    "android": "平台适配",
    "qmclient": "客户端与核心",
    "client": "客户端与核心",
    "客户端": "客户端与核心",
    "tclient": "客户端与核心",
    "core": "客户端与核心",
    "qimeng": "客户端与核心",
    "qmrt": "客户端与核心",
    "section-loader": "客户端与核心",
    "sync": "客户端与核心",
    "demo": "客户端与核心",
    "editor": "客户端与核心",
    "perf": "客户端与核心",
    "ci": INTERNAL_DOMAIN,
    "build": INTERNAL_DOMAIN,
    "gate": INTERNAL_DOMAIN,
    "clang-tidy": INTERNAL_DOMAIN,
    "git": INTERNAL_DOMAIN,
    "workflow": INTERNAL_DOMAIN,
    "version": INTERNAL_DOMAIN,
    "tooling": INTERNAL_DOMAIN,
    "tidy": INTERNAL_DOMAIN,
    "strict-build": INTERNAL_DOMAIN,
    "asan": INTERNAL_DOMAIN,
    "nightly": INTERNAL_DOMAIN,
    "eol": INTERNAL_DOMAIN,
    "gitignore": INTERNAL_DOMAIN,
    "ddnet-libs": INTERNAL_DOMAIN,
    "github": INTERNAL_DOMAIN,
    "wiki": INTERNAL_DOMAIN,
    "pr": INTERNAL_DOMAIN,
    "pr156": INTERNAL_DOMAIN,
    "merge": INTERNAL_DOMAIN,
    "master": INTERNAL_DOMAIN,
    "main": INTERNAL_DOMAIN,
    "monitoring": INTERNAL_DOMAIN,
    "logging": INTERNAL_DOMAIN,
    "explore": INTERNAL_DOMAIN,
    "plan": INTERNAL_DOMAIN,
    "backlog": INTERNAL_DOMAIN,
    "test": INTERNAL_DOMAIN,
    "ai": INTERNAL_DOMAIN,
    "scripts": INTERNAL_DOMAIN,
    "timeout": INTERNAL_DOMAIN,
    "all": INTERNAL_DOMAIN,
}

_WARNINGS: list[tuple[str, str]] = []


@dataclass
class CommitNote:
    commit_hash: str
    commit_type: str
    scope: str
    description: str
    release_zh: str
    domain: str

    def format_prefix(self) -> str:
        if self.scope:
            return f"{self.commit_type}({self.scope})"
        return self.commit_type


def run_git(args: list[str]) -> str:
    result = subprocess.run(
        ["git", *args],
        cwd=REPO_ROOT,
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    return result.stdout.strip()


def git_ref_exists(ref: str) -> bool:
    result = subprocess.run(
        ["git", "rev-parse", "--verify", "--quiet", ref],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
    )
    return result.returncode == 0


def detect_channel(tag: str) -> str:
    """返回 stable 或 pre-release。"""
    name = tag.strip()
    if not name:
        return "pre-release"
    if name == "nightly" or name.startswith("nightly/"):
        return "pre-release"
    if PRE_RELEASE_HINT_RE.search(name):
        return "pre-release"
    if STABLE_TAG_RE.match(name):
        return "stable"
    # 无法识别时按预发布处理，避免误标正式版
    return "pre-release"


def resolve_domain(scope: str, commit_hash: str) -> str:
    key = scope.strip().lower()
    if not key:
        return FALLBACK_DOMAIN
    if key in SCOPE_TO_DOMAIN:
        return SCOPE_TO_DOMAIN[key]
    _WARNINGS.append((scope, commit_hash))
    return FALLBACK_DOMAIN


def parse_trailers(body: str) -> str:
    for line in body.splitlines():
        match = TRAILER_RE.match(line.strip())
        if match:
            return match.group("text").strip()
    return ""


def parse_commit(commit_hash: str, subject: str, body: str) -> CommitNote | None:
    match = SUBJECT_RE.match(subject.strip())
    if not match:
        return None
    commit_type = match.group("type").lower()
    if commit_type not in PLAYER_TYPES:
        return None
    scope = (match.group("scope") or "").strip()
    description = match.group("desc").strip()
    release_zh = parse_trailers(body) or description
    domain = resolve_domain(scope, commit_hash)
    if domain == INTERNAL_DOMAIN:
        return None
    return CommitNote(
        commit_hash=commit_hash,
        commit_type=commit_type,
        scope=scope,
        description=description,
        release_zh=release_zh,
        domain=domain,
    )


def list_tags_by_creatordate() -> list[str]:
    raw = run_git(["tag", "--sort=-creatordate"])
    return [line.strip() for line in raw.splitlines() if line.strip()]


def list_stable_tags() -> list[str]:
    return [tag for tag in list_tags_by_creatordate() if STABLE_TAG_RE.match(tag)]


def resolve_previous_tag(current_tag: str, channel: str) -> str | None:
    """正式版：同通道上一 tag；预发布：相对最近正式版，便于玩家看「相对 stable 多了什么」。"""
    if channel == "pre-release":
        stables = list_stable_tags()
        for tag in stables:
            if tag != current_tag:
                return tag
        return None

    tags = run_git(["tag", "--sort=-creatordate", "--merged", current_tag]).splitlines()
    for tag in tags:
        tag = tag.strip()
        if tag and tag != current_tag and STABLE_TAG_RE.match(tag):
            return tag
    for tag in tags:
        tag = tag.strip()
        if tag and tag != current_tag:
            return tag
    return None


def collect_notes(current_tag: str, previous_tag: str | None) -> list[CommitNote]:
    revspec = f"{previous_tag}..{current_tag}" if previous_tag else current_tag
    raw = run_git(
        [
            "log",
            "--reverse",
            f"--format=%H{FIELD_SEPARATOR}%s{FIELD_SEPARATOR}%b{COMMIT_SEPARATOR}",
            revspec,
        ]
    )
    notes: list[CommitNote] = []
    for entry in raw.split(COMMIT_SEPARATOR):
        if not entry.strip():
            continue
        parts = entry.split(FIELD_SEPARATOR, 2)
        if len(parts) != 3:
            continue
        note = parse_commit(parts[0].strip(), parts[1].strip(), parts[2])
        if note is not None:
            notes.append(note)
    return notes


def render_domain(domain: str, notes: list[CommitNote]) -> list[str]:
    grouped = [note for note in notes if note.domain == domain]
    if not grouped:
        return []
    grouped.sort(key=lambda note: TYPE_PRIORITY.get(note.commit_type, 99))
    lines = [f"## {domain}"]
    for note in grouped:
        lines.append(f"- {note.format_prefix()}: {note.release_zh}")
    lines.append("")
    return lines


def render_header(
    *,
    channel: str,
    version: str,
    current_tag: str,
    previous_tag: str | None,
    commit: str | None,
    built_at: str | None,
    branch: str | None,
) -> list[str]:
    lines: list[str] = []
    if channel == "stable":
        lines.append(f"# QmClient {version} · 正式版（Stable）")
        lines.append("")
        lines.append("> **通道**：正式发布 · 建议日常使用")
        lines.append(f"> **Tag**：`{current_tag}`")
    else:
        title = "Nightly" if current_tag == "nightly" else version
        lines.append(f"# QmClient {title} · 预发布（Pre-release）")
        lines.append("")
        lines.append("> **通道**：预发布 / 内部测试")
        lines.append("> **注意**：可能不稳定；`nightly` 会被下一次构建覆盖，**不建议当主力客户端**")
        lines.append(f"> **Tag**：`{current_tag}`")
        if branch:
            lines.append(f"> **分支**：`{branch}`")
        if commit:
            lines.append(f"> **Commit**：`{commit}`")
        if built_at:
            lines.append(f"> **构建时间**：{built_at}")

    if previous_tag:
        lines.append(f"> **范围**：`{previous_tag}..{current_tag}`")
    else:
        lines.append(f"> **范围**：`{current_tag}`")
    lines.append("")
    lines.append("---")
    lines.append("")
    return lines


def render_markdown(
    *,
    version: str,
    current_tag: str,
    previous_tag: str | None,
    notes: list[CommitNote],
    channel: str,
    commit: str | None = None,
    built_at: str | None = None,
    branch: str | None = None,
) -> str:
    lines = render_header(
        channel=channel,
        version=version,
        current_tag=current_tag,
        previous_tag=previous_tag,
        commit=commit,
        built_at=built_at,
        branch=branch,
    )
    if not notes:
        if channel == "pre-release":
            lines.append("## 说明")
            lines.append("- 本构建相对最近正式版未解析到玩家向提交，或历史深度不足；请以安装包与 commit 为准。")
            lines.append("")
        else:
            lines.append("## 说明")
            lines.append("- 本区间未解析到面向玩家的提交（或仅有工程类变更）。")
            lines.append("")
    else:
        for domain in DOMAIN_ORDER:
            lines.extend(render_domain(domain, notes))

    lines.append("## 完整变更")
    if previous_tag:
        lines.append(
            f"- 对比：`{previous_tag}...{current_tag}`（GitHub Compare / `git log`）"
        )
    else:
        lines.append(f"- 起点：`{current_tag}`")
    lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def emit_warnings() -> None:
    if not _WARNINGS:
        return
    print(
        "[release-notes] 以下 scope 未在映射表中，已归入「其他」，"
        "可考虑补充 SCOPE_TO_DOMAIN：",
        file=sys.stderr,
    )
    for scope, commit_hash in _WARNINGS:
        print(f"  - {scope} (commit {commit_hash[:12]})", file=sys.stderr)


def resolve_repo_path(path: Path | None) -> Path | None:
    if path is None:
        return None
    if path.is_absolute():
        return path
    return REPO_ROOT / path


def main() -> int:
    parser = argparse.ArgumentParser(description="生成 GitHub Release 说明（正式版 / 预发布）")
    parser.add_argument("--version", default="UNRELEASED", help="展示用版本号")
    parser.add_argument("--current-tag", required=True, help="当前 tag/ref，如 v2.74.9 或 nightly")
    parser.add_argument("--previous-tag", default=None, help="上一个 tag；不传则按通道自动推断")
    parser.add_argument(
        "--channel",
        choices=("auto", "stable", "pre-release"),
        default="auto",
        help="发布通道；auto 根据 tag 名推断（vX.Y.Z=stable，nightly/rc=pre-release）",
    )
    parser.add_argument("--commit", default=None, help="预发布展示用完整 commit SHA")
    parser.add_argument("--branch", default=None, help="预发布展示用分支名")
    parser.add_argument(
        "--built-at",
        default=None,
        help="预发布构建时间（UTC 字符串）；不传则不写",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="输出 Markdown 路径；不传则打印到 stdout",
    )
    args = parser.parse_args()

    channel = detect_channel(args.current_tag) if args.channel == "auto" else args.channel

    if not git_ref_exists(args.current_tag):
        print(
            f"错误：当前 tag/ref 不存在：{args.current_tag}。"
            "CI 场景请传真实 tag；本地预演请改用仓库里已存在的 tag。",
            file=sys.stderr,
        )
        return 2
    if args.previous_tag and not git_ref_exists(args.previous_tag):
        print(f"错误：上一个 tag/ref 不存在：{args.previous_tag}", file=sys.stderr)
        return 2

    previous_tag = args.previous_tag or resolve_previous_tag(args.current_tag, channel)
    output_path = resolve_repo_path(args.output)
    notes = collect_notes(args.current_tag, previous_tag)
    markdown = render_markdown(
        version=args.version,
        current_tag=args.current_tag,
        previous_tag=previous_tag,
        notes=notes,
        channel=channel,
        commit=args.commit,
        built_at=args.built_at,
        branch=args.branch,
    )

    if output_path is not None:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(markdown, encoding="utf-8")
        print(f"已写入发布说明：{output_path}（通道={channel}）")
    else:
        print(markdown, end="")

    emit_warnings()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
