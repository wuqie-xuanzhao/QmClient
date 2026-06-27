#!/usr/bin/env python3
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path


def _find_build_dir(argv: list[str]) -> Path | None:
    # 同时兼容 configure 的 -B 和 build 的 --build，两条命令都共用这一份修复入口。
    for i, arg in enumerate(argv):
        if arg == "-B" and i + 1 < len(argv):
            return Path(argv[i + 1])
        if arg == "--build" and i + 1 < len(argv):
            return Path(argv[i + 1])
        if arg.startswith("-B") and len(arg) > 2:
            return Path(arg[2:])
        if arg.startswith("--build="):
            return Path(arg.split("=", 1)[1])
    return None


def _read_cache_value(cache_file: Path, name: str) -> str | None:
    # 这里只读取单个 cache 项，避免为了一次修复再引入额外的 CMake 解析依赖。
    for line in cache_file.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith(f"{name}:"):
            return line.split("=", 1)[1]
    return None


def _extract_showincludes_prefix(build_dir: Path) -> str | None:
    # 不信任历史 configure 日志，直接让当前机器上的 cl.exe 编一次最小探针，
    # 从真实的 /showIncludes 输出里反推出前缀，才能避开“日志里既有正常值也有乱码值”的旧状态。
    cache_file = build_dir / "CMakeCache.txt"
    compiler = _read_cache_value(cache_file, "CMAKE_C_COMPILER")
    if not compiler:
        return None

    show_dir = build_dir / "CMakeFiles" / "ShowIncludes"
    show_dir.mkdir(parents=True, exist_ok=True)
    (show_dir / "foo.h").write_text("\n", encoding="utf-8")
    (show_dir / "main.c").write_text(
        '#include "foo.h"\nint main(void) { return 0; }\n', encoding="utf-8"
    )
    include_path = str((show_dir / "foo.h").resolve())

    result = subprocess.run(
        [compiler, "/nologo", "/showIncludes", "/c", "main.c"],
        cwd=show_dir,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        return None

    for encoding in ("utf-8", "utf-8-sig", "mbcs"):
        try:
            stdout = result.stdout.decode(encoding, errors="replace")
        except LookupError:
            continue

        # 直接用探针头文件路径反推出前缀，避免历史日志里正常值和乱码值混在一起时取错。
        for line in stdout.splitlines():
            if include_path in line:
                return line[: line.index(include_path)]
    return None


def _read_rules_prefix(rules_file: Path) -> str | None:
    text = rules_file.read_text(encoding="utf-8", errors="replace")
    match = re.search(r"^msvc_deps_prefix = (.*)$", text, re.MULTILINE)
    if not match:
        return None
    return match.group(1)


def _repair_rules_file(rules_file: Path, expected_prefix: str) -> bool:
    # Ninja 依赖跟踪靠 rules.ninja 里的 msvc_deps_prefix 去识别 /showIncludes 输出。
    # 这里只做最小替换：只有当前前缀和真实前缀不一致时才改文件。
    text = rules_file.read_text(encoding="utf-8", errors="replace")
    match = re.search(r"^msvc_deps_prefix = (.*)$", text, re.MULTILINE)
    if not match:
        return False

    current_prefix = match.group(1)
    if current_prefix == expected_prefix:
        return False

    # rules.ninja 里全局前缀和 cmcldeps 参数共用同一份文本，统一替换能一起修正。
    text = text.replace(current_prefix, expected_prefix)
    rules_file.write_text(text, encoding="utf-8", newline="\n")
    return True


def main() -> int:
    argv = sys.argv[1:]

    build_dir = _find_build_dir(argv)
    if build_dir is None:
        return 1

    if not build_dir.is_absolute():
        build_dir = Path.cwd() / build_dir

    cache_file = build_dir / "CMakeCache.txt"
    rules_file = build_dir / "CMakeFiles" / "rules.ninja"
    if not rules_file.is_file() or not cache_file.is_file():
        return 1

    prefix = _extract_showincludes_prefix(build_dir)
    if not prefix:
        return 1

    if _repair_rules_file(rules_file, prefix):
        print(f"Patched Ninja MSVC deps prefix: {build_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
