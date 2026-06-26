#!/usr/bin/env python3
"""
根据 tag 区间内的 commit 生成 GitHub Release 说明。

输出按「功能领域」分组、纯中文，例如：

# 更新日志
> Version: vX.Y.Z
> Range: vPrev..vCurr

## 界面与视觉
- feat(nameplate): 中文文本
- perf(hud): 中文文本

## 设置页
- fix(settings): 中文文本

## 其他
- feat: 无 scope 的提交

分组依据是 commit 的 scope 字段（见 SCOPE_TO_DOMAIN 映射表）。
工程类 commit（ci/build/gate 等，对玩家无意义）解析但不渲染。
优先从 commit body 读取更稳定的中文说明：

    Release-ZH: 中文发布说明

缺失时回退到 commit subject 的 description。

注意：脚本输出是机械化草稿，终稿需按 docs/RELEASE_NOTE_TEMPLATE.md 人工润色。
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
COMMIT_SEPARATOR = "\x1e"
FIELD_SEPARATOR = "\x1f"

SUBJECT_RE = re.compile(
    r"^(?P<type>[A-Za-z]+)(?:\((?P<scope>[^)]+)\))?(?P<breaking>!)?[:：]\s*(?P<desc>.+)$"
)
TRAILER_RE = re.compile(r"^Release-ZH:\s*(?P<text>.+)$")

# 玩家可感知的 commit type；其余（ci/build/docs/test/chore/style 等）直接丢弃
PLAYER_TYPES = {"feat", "fix", "perf", "refactor", "improve", "revert"}

# 组内排序：feat 在前，revert 在后；同 type 内时序由 git log --reverse 保底
TYPE_PRIORITY = {
    "feat": 0,
    "improve": 1,
    "fix": 2,
    "perf": 3,
    "refactor": 4,
    "revert": 5,
}

# 渲染顺序（INTERNAL_DOMAIN 不在其中 → 归入它的 commit 解析但不渲染）
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

# scope(小写) -> 领域。未命中 -> FALLBACK_DOMAIN + 警告。
# INTERNAL_DOMAIN 的条目会被识别但不渲染，避免它们污染兜底组。
SCOPE_TO_DOMAIN: dict[str, str] = {
    # 界面与视觉
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
    # 聊天与社交
    "chat": "聊天与社交",
    "聊天交互": "聊天与社交",
    "friends": "聊天与社交",
    "spectator": "聊天与社交",
    "console": "聊天与社交",
    "通知栏": "聊天与社交",
    # 翻译与语言
    "translate": "翻译与语言",
    "i18n": "翻译与语言",
    "翻译模块": "翻译与语言",
    "macos与翻译": "翻译与语言",
    "ime": "翻译与语言",
    # 语音
    "voice": "语音",
    "语音": "语音",
    "语音模块": "语音",
    "voicesrv": "语音",
    # 输入与操作
    "input": "输入与操作",
    "input-overlay": "输入与操作",
    "bind": "输入与操作",
    "fast-practice": "输入与操作",
    "practice": "输入与操作",
    # 媒体与渲染
    "lyrics": "媒体与渲染",
    "sound": "媒体与渲染",
    "vulkan": "媒体与渲染",
    "live": "媒体与渲染",
    "game": "媒体与渲染",
    "modes": "媒体与渲染",
    "collision": "媒体与渲染",
    "trajectory": "媒体与渲染",
    "player_points": "媒体与渲染",
    # 设置页（spec 历史多为 qm_spec_* 配置项）
    "settings": "设置页",
    "config": "设置页",
    "spec": "设置页",
    # 平台适配
    "macos": "平台适配",
    "linux": "平台适配",
    # 客户端与核心
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
    # 开发者与构建（INTERNAL，映射但不渲染）
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

# 模块级警告收集：未命中映射表的 (scope, commit_hash)
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
    )
    return result.stdout.strip()


def git_ref_exists(ref: str) -> bool:
    result = subprocess.run(
        ["git", "rev-parse", "--verify", "--quiet", ref],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    return result.returncode == 0


def resolve_domain(scope: str, commit_hash: str) -> str:
    """scope 归一化后查映射表；未命中返回兜底领域并记录警告。"""
    if not scope:
        return FALLBACK_DOMAIN
    # 多 scope（如 i18n,nameplate）：取首个归类，format_prefix 仍保留原始完整 scope
    primary = scope.split(",", 1)[0].strip()
    domain = SCOPE_TO_DOMAIN.get(primary.lower())
    if domain is not None:
        return domain
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
    domain = resolve_domain(scope, commit_hash)

    release_zh = parse_trailers(body) or description

    return CommitNote(
        commit_hash=commit_hash,
        commit_type=commit_type,
        scope=scope,
        description=description,
        release_zh=release_zh,
        domain=domain,
    )


def resolve_previous_tag(current_tag: str) -> str | None:
    tags = run_git(["tag", "--sort=-creatordate", "--merged", current_tag]).splitlines()
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


def render_markdown(
    version: str, current_tag: str, previous_tag: str | None, notes: list[CommitNote]
) -> str:
    lines: list[str] = []
    lines.append("# 更新日志")
    lines.append(f"> Version: {version}")
    if previous_tag:
        lines.append(f"> Range: {previous_tag}..{current_tag}")
    else:
        lines.append(f"> Range: {current_tag}")
    lines.append("")
    for domain in DOMAIN_ORDER:
        lines.extend(render_domain(domain, notes))
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
    parser = argparse.ArgumentParser(description="生成 GitHub Release 说明")
    parser.add_argument("--version", default="UNRELEASED", help="展示用版本号")
    parser.add_argument("--current-tag", required=True, help="当前发布 tag，如 v2.52.3")
    parser.add_argument(
        "--previous-tag", default=None, help="上一个 tag；不传则自动推断"
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="输出 Markdown 文件路径；不传则打印到 stdout",
    )
    args = parser.parse_args()

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

    previous_tag = args.previous_tag or resolve_previous_tag(args.current_tag)
    output_path = resolve_repo_path(args.output)
    notes = collect_notes(args.current_tag, previous_tag)
    markdown = render_markdown(args.version, args.current_tag, previous_tag, notes)

    if output_path is not None:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(markdown, encoding="utf-8")
        print(f"已写入发布说明：{output_path}")
    else:
        print(markdown, end="")

    emit_warnings()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
