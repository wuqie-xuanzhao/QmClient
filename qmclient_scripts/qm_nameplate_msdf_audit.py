#!/usr/bin/env python3
"""名牌 MTSDF 资源审计：重烤或整理资源后跑一次，防止死资源与断链进入发布包。

子命令：
  references [--data-root DIR]   引用完整性与可达性（默认命令）
    - 渲染器硬编码回退链（dejavu / symbols）的 profile manifest 必须存在；
    - gate.h 的字体映射表指向的 profile 必须存在（防「映射了却没烤」）；
    - 每个 profile 引用的页（.json + .png）必须存在；
    - 磁盘上每个页必须被至少一个 profile 引用（防孤儿页白占发布包）；
    - 每个 profile manifest 必须被回退链或 gate 映射引用（防 phosphor 式死 profile）。
  coverage OLD_ROOT NEW_ROOT     逐 profile 码点比对，判据「不得丢失」（重烤前后各一份产物目录）
  extent [--data-root DIR] [PATTERN ...]
                                 各页真实占用范围与最小可行 2 的幂尺寸（判断还能不能降显存）

用法示例：
  py -3 qmclient_scripts/qm_nameplate_msdf_audit.py references
  py -3 qmclient_scripts/qm_nameplate_msdf_audit.py coverage <旧产物目录> data
  py -3 qmclient_scripts/qm_nameplate_msdf_audit.py extent emoji cabin
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

# 与 qm_nameplate_msdf_renderer.cpp 的 pFallbackProfiles 硬编码回退链保持一致。
RENDERER_FALLBACK_CHAIN = ("dejavu", "symbols")
GATE_HEADER = Path("src/game/client/components/qmclient/nameplate_msdf/qm_nameplate_msdf_gate.h")


def load_gate_profiles(repo_root: Path) -> set[str]:
    """从 gate.h 提取字体映射表指向的 profile 名（静态提取，仅供审计用）。"""
    text = (repo_root / GATE_HEADER).read_text(encoding="utf-8")
    # 匹配 s_aProfiles 表里的 {"Family", "profile"} 条目
    return set(re.findall(r'\{\s*"[^"]+"\s*,\s*"([a-z0-9_]+)"\s*\}', text))


def profile_manifests(repo_root: Path, data_root: Path) -> dict[str, dict]:
    atlas = repo_root / data_root
    out: dict[str, dict] = {}
    for path in sorted((atlas / "profiles").glob("nameplate_*.json")):
        out[path.stem.removeprefix("nameplate_")] = json.loads(path.read_text(encoding="utf-8"))
    return out


def check_references(repo_root: Path, data_root: Path) -> int:
    atlas = repo_root / data_root
    failures = 0

    def fail(message: str) -> None:
        nonlocal failures
        failures += 1
        print(f"FAIL: {message}")

    manifests = profile_manifests(repo_root, data_root)
    if not manifests:
        print(f"FAIL: no profile manifests under {atlas / 'profiles'}")
        return 1

    # 1) 渲染器回退链与 gate 映射指向的 profile 必须都存在
    reachable = set(RENDERER_FALLBACK_CHAIN) | load_gate_profiles(repo_root)
    for profile in sorted(reachable):
        if profile not in manifests:
            fail(f"profile '{profile}' is referenced (fallback chain / gate.h) but has no manifest")
    print(f"reachable profiles: {sorted(reachable)}")

    # 2) 每个 profile 引用的页必须存在；收集被引用的页
    referenced: set[str] = set()
    for profile, manifest in sorted(manifests.items()):
        pages = manifest.get("pages", [])
        if not pages:
            fail(f"profile '{profile}' references no pages")
        for ref in pages:
            page = Path(ref).name
            referenced.add(page)
            for suffix in ("json", "png"):
                if not (atlas / f"{page.removesuffix('.json')}.{suffix}").is_file():
                    fail(f"profile '{profile}' references missing page artifact: {page} ({suffix})")
        print(f"profile '{profile}': {len(pages)} page(s)")

    # 3) 磁盘上的页必须被引用（孤儿页会经 CMake GLOB 进发布包）
    for path in sorted(atlas.glob("nameplate_*.json")):
        if path.name not in referenced:
            fail(f"orphan page (not referenced by any profile): {path.name}")

    # 4) 每个 profile 必须可达（回退链或 gate 映射），否则是死资源
    for profile in sorted(manifests):
        if profile not in reachable:
            fail(f"unreachable profile (no gate.h mapping, not in fallback chain): '{profile}'")

    if failures:
        print(f"\n{failures} problem(s) found")
        return 1
    print("\nall references consistent")
    return 0


def profile_codepoints(atlas: Path, profile: str) -> set[int]:
    manifest = json.loads((atlas / "profiles" / f"nameplate_{profile}.json").read_text(encoding="utf-8"))
    out: set[int] = set()
    for ref in manifest["pages"]:
        page = json.loads((atlas / Path(ref).name).read_text(encoding="utf-8"))
        out.update(int(key) for key in page["glyphs"])
    return out


def check_coverage(old_root: Path, new_root: Path) -> int:
    """逐 profile 比对新旧产物码点集合；判据是「不得丢失」，新增只报告不失败。"""
    profiles = sorted(p.stem.removeprefix("nameplate_") for p in new_root.glob("profiles/nameplate_*.json"))
    failures = 0
    print(f"{'profile':26s} {'glyphs':>15s}  覆盖")
    for profile in profiles:
        try:
            old_cps = profile_codepoints(old_root, profile)
        except FileNotFoundError:
            print(f"{profile:26s} {'-':>15s}  (旧产物不存在，跳过对比)")
            continue
        new_cps = profile_codepoints(new_root, profile)
        missing = old_cps - new_cps
        added = new_cps - old_cps
        status = "OK" if not missing else f"丢失{len(missing)}  首个丢失=U+{min(missing):04X}"
        if added:
            status += f"  (+{len(added)} 新增)"
        if missing:
            failures += 1
        print(f"{profile:26s} {len(old_cps):6d}->{len(new_cps):-6d}  {status}")
    if failures:
        print(f"\n覆盖不一致的 profile: {failures}")
        return 1
    print("\n全部 profile 覆盖一致")
    return 0


def pow2_ceil(value: int) -> int:
    size = 1
    while size < value:
        size *= 2
    return size


def check_extent(atlas: Path, patterns: list[str]) -> int:
    targets = sorted(atlas.glob("nameplate_*.json"))
    if patterns:
        targets = [p for p in targets if any(pattern in p.name for pattern in patterns)]
    if not targets:
        print("no pages matched")
        return 1
    for page in targets:
        data = json.loads(page.read_text(encoding="utf-8"))
        glyphs = data["glyphs"]
        if not glyphs:
            print(f"{page.name}: EMPTY")
            continue
        atlas_meta = data["atlas"]
        width, height = int(atlas_meta["width"]), int(atlas_meta["height"])
        right = max(int(g["x"]) + int(g["w"]) for g in glyphs.values())
        bottom = max(int(g["y"]) + int(g["h"]) for g in glyphs.values())
        pad = int(data.get("padding", 0))
        need = max(right + pad, bottom + pad)
        min_side = pow2_ceil(need)
        used_ratio = (right * bottom) / float(width * height)
        print(
            f"{page.name:44s} canvas={width}x{height}  extent={right}x{bottom} "
            f"need={need}  min_pow2={min_side}  bbox_fill={used_ratio * 100:5.1f}%  "
            f"glyphs={len(glyphs)}  "
            f"{'-> 可降尺寸' if min_side < width else 'OK'}"
        )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="名牌 MTSDF 资源审计")
    parser.add_argument("command", nargs="?", default="references", choices=["references", "coverage", "extent"])
    parser.add_argument("--data-root", type=Path, default=Path("data/qmclient/nameplate_msdf"), help="图集目录（相对仓库根）")
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).parents[1], help="仓库根目录")
    parser.add_argument("args", nargs="*", help="coverage: OLD_ROOT NEW_ROOT；extent: 页名过滤子串")
    parsed = parser.parse_args()

    if parsed.command == "references":
        return check_references(parsed.repo_root, parsed.data_root)
    if parsed.command == "coverage":
        if len(parsed.args) != 2:
            parser.error("coverage 需要 OLD_ROOT NEW_ROOT 两个参数")
        return check_coverage(Path(parsed.args[0]), Path(parsed.args[1]))
    if parsed.command == "extent":
        return check_extent(parsed.repo_root / parsed.data_root, parsed.args)
    parser.error(f"unknown command: {parsed.command}")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
