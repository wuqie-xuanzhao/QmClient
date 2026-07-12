# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""统一更新 QmClient 仓库内的版本定义。"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
VERSION_H_PATH = REPO_ROOT / "src/game/version.h"
DOCS_INFO_PATH = REPO_ROOT / "docs/info.json"
VERSION_RE = re.compile(r"^\d+\.\d+(?:\.\d+)?$")
VERSION_DEFINE_RE = re.compile(
    r'^(#define\s+QMCLIENT_VERSION\s+)"[^"]+"$', re.MULTILINE
)

def configure_stdio() -> None:
    for stream in (sys.stdout, sys.stderr):
        reconfigure = getattr(stream, "reconfigure", None)
        if reconfigure is not None:
            reconfigure(encoding="utf-8", errors="replace")



def normalize_version(version: str | None, tag: str | None) -> str:
    if bool(version) == bool(tag):
        raise ValueError("必须且只能提供 --version 或 --tag 其中之一。")

    raw = version if version is not None else tag
    assert raw is not None
    normalized = raw[1:] if raw[:1] in {"v", "V"} else raw
    if not VERSION_RE.fullmatch(normalized):
        raise ValueError(
            f"版本格式非法：{raw}。期望格式为 X.Y 或 X.Y.Z，tag 可写成 vX.Y.Z。"
        )
    return normalized


def update_version_h(version: str) -> None:
    content = VERSION_H_PATH.read_text(encoding="utf-8")
    updated, count = VERSION_DEFINE_RE.subn(rf'\1"{version}"', content, count=1)
    if count != 1:
        raise RuntimeError("未找到 QMCLIENT_VERSION 宏，无法更新 src/game/version.h。")
    VERSION_H_PATH.write_text(updated, encoding="utf-8")


def update_docs_info(version: str) -> None:
    data = json.loads(DOCS_INFO_PATH.read_text(encoding="utf-8-sig"))
    data["version"] = version
    DOCS_INFO_PATH.write_text(
        json.dumps(data, ensure_ascii=False, indent=4) + "\n", encoding="utf-8"
    )


def latest_tag_version() -> str | None:
    """读取最近的 v* tag，返回去掉 v 前缀的版本号；无 tag 或非 git 仓库返回 None。"""
    try:
        result = subprocess.run(
            ["git", "tag", "--list", "v*", "--sort=-v:refname"],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            encoding="utf-8",
            check=False,
        )
    except (FileNotFoundError, OSError):
        return None
    for line in result.stdout.splitlines():
        tag = line.strip()
        if not tag:
            continue
        normalized = tag[1:] if tag[:1] in {"v", "V"} else tag
        # 只接受纯 X.Y[.Z]，跳过上游遗留的 v16.5-headless、nightly 等干扰 tag
        if VERSION_RE.fullmatch(normalized):
            return normalized
    return None


def _version_key(version: str) -> list[int]:
    parts: list[int] = []
    for piece in version.split("."):
        try:
            parts.append(int(piece))
        except ValueError:
            parts.append(0)
    return parts


def warn_if_not_progressing(version: str) -> None:
    """目标版本未高于最近 tag 时，打印 stderr 警告（不阻断）。"""
    latest = latest_tag_version()
    if latest is None:
        return
    target = _version_key(version)
    current = _version_key(latest)
    width = max(len(target), len(current))
    target += [0] * (width - len(target))
    current += [0] * (width - len(current))
    if target <= current:
        print(
            f"[bump-version] 警告：目标版本 {version} 未高于最近 tag v{latest}，"
            "可能是重复或倒退版本。",
            file=sys.stderr,
        )


def main() -> int:
    configure_stdio()

    parser = argparse.ArgumentParser(description="统一更新 QmClient 版本号")
    parser.add_argument("--version", help="目标版本号，如 2.58.1")
    parser.add_argument("--tag", help="目标 tag，如 v2.58.1")
    parser.add_argument(
        "--dry-run", action="store_true", help="只打印解析后的版本，不写回文件"
    )
    args = parser.parse_args()

    normalized = normalize_version(args.version, args.tag)
    print(f"目标版本：{normalized}")
    warn_if_not_progressing(normalized)
    if args.dry_run:
        print("Dry-run：未写回文件。")
        return 0

    update_version_h(normalized)
    update_docs_info(normalized)
    print(f"已更新：{VERSION_H_PATH}")
    print(f"已更新：{DOCS_INFO_PATH}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
