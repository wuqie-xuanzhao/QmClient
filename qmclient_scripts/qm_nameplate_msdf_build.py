#!/usr/bin/env python3
"""烘焙 QmClient 名牌 MTSDF 的符号/emoji 兜底页（离线资源管线）。

名牌 MTSDF 管线分工（覆盖取舍：只做英文与图标，汉字/假名/谚文等整条回退 FreeType）：
  1. qm_nameplate_msdf_official_build.py   克隆并构建 pinned 官方 msdf-atlas-gen
  2. qm_nameplate_msdf_batch_official.py   烤 16 个拉丁 profile 页（官方工具）
  3. 本脚本                                烤符号/emoji 兜底页（自建工具）+ 汇总 symbols profile
  4. qm_nameplate_msdf_audit.py            重烤后的引用完整性/覆盖审计

产物写入 data/qmclient/nameplate_msdf/：
  nameplate_noto_glow_emoji_XX.{png,json}  符号兜底页
  profiles/nameplate_symbols.json          symbols profile 清单

自建工具构建（正常客户端构建不依赖 msdfgen；运行需 libfreetype.dll 在 PATH，
见 ddnet-libs/freetype/windows/lib64/）：
  cmake -S qmclient_scripts/qm_nameplate_msdf_atlas -B <build> -DQM_PROJECT_ROOT=<repo>
  cmake --build <build> --config Release
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Callable, NamedTuple

# 共享脚本兜底页（所有 profile 的缺字回退）。
#
# 名牌的 MTSDF 回退链是「选中 profile → dejavu → symbols → FreeType」。
# 覆盖范围是有意取舍：MTSDF 名牌只做英文（拉丁）与图标（符号/emoji），
# 汉字/假名/谚文/泰文等一律不在图集内，由门控整条回退 FreeType——
# CJK 笔画密，MSDF 边缘收益小，而 CJK 兜底页曾占掉常驻显存的 90%（约 600 MiB）。
#
# 页名必须保持 noto_glow_* 前缀：渲染器对路径含 "noto_glow" 的页禁用 Alpha 真 SDF
# （规避历史 Alpha 伪影导致字形变实心块），改名会连带改变渲染路径。
# 符号页范围：Noto Emoji 除 1F300+ 表情外还带一批 BMP 符号（♥ ⚔ ✨ ⭐ ☟ ❤ 等），
# 一并收进来，避免这些常见符号落在 MTSDF 之外。
SYMBOL_RANGES: tuple[tuple[int, int], ...] = (
    (0x2000, 0x2BFF),  # 标点 / 箭头 / 数学 / 杂项符号 / 装饰符
    (0x1F000, 0x1FAFF),  # 麻将 / 多米诺 / 扑克 / 表情与图形符号各段
)


def prioritize_codepoints(available: list[int], tiers: tuple[list[int], ...]) -> list[int]:
    """按优先级分层重排：tiers 内先到先得，其余按原顺序（码位升序）追加在后。"""
    allowed = set(available)
    seen: set[int] = set()
    ordered: list[int] = []
    for tier in tiers:
        for codepoint in tier:
            if codepoint in allowed and codepoint not in seen:
                seen.add(codepoint)
                ordered.append(codepoint)
    ordered.extend(codepoint for codepoint in available if codepoint not in seen)
    return ordered


class SFallbackScript(NamedTuple):
    """一个兜底脚本：一种随包字体 + 一组范围 + 常用优先序 + 预算与分片。"""

    tag: str
    page_stem: str
    font: str
    face: int
    ranges: tuple[tuple[int, int], ...]
    tiers: Callable[[], tuple[list[int], ...]]
    max_glyphs: int  # 字形预算；0 = 不限（范围内能覆盖的全收）
    chunk: int = 0  # 每片字形数；0 = 用 --fallback-chunk
    page_size: int = 0  # 图集边长；0 = 用 --page-size
    # 图集是 RGBA8、无 mipmap、加载时整页上传显存，所以边长直接等于显存成本：
    # 4096² = 64 MiB，2048² = 16 MiB。字形少的脚本必须显式降尺寸。


def _no_tiers() -> tuple[list[int], ...]:
    return ()


# 兜底脚本只剩 emoji（符号/图标）：汉字/假名/谚文/泰文不做 MTSDF，缺字整条回退
# FreeType（见文件头的取舍说明）。符号页字形复杂、单个字形大：
# 1415 个字形实测在 4096² 里只占约 3745 行高，整块一片即可装下。
DEFAULT_FALLBACK_CHUNK = 1800
FALLBACK_SCRIPTS: tuple[SFallbackScript, ...] = (
    SFallbackScript("emoji", "nameplate_noto_glow_emoji", "NotoEmoji-Regular.ttf", 0, SYMBOL_RANGES, _no_tiers, 0, 0, 4096),
)
FALLBACK_PROFILE = "symbols"
# profile 里的页顺序：先加载的页在重码时优先（m_Glyphs.emplace 不覆盖），顺序固定便于复现。
FALLBACK_PAGE_ORDER = ("_emoji_",)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="烘焙名牌 MTSDF 符号/emoji 兜底页并汇总 symbols profile")
    parser.add_argument("--tool", type=Path, required=True, help="qm-nameplate-msdf-atlas 可执行文件")
    parser.add_argument("--output", type=Path, required=True, help="输出目录（data/qmclient/nameplate_msdf）")
    parser.add_argument("--data-root", type=Path, default=Path("data"), help="data 根目录，用于定位字体")
    # 使用足够的离线分辨率，避免小字号 atlas 被放大后出现圆弧折线和虫蚀。
    parser.add_argument("--em-pixels", type=int, default=64)
    parser.add_argument("--px-range", type=float, default=8.0)
    parser.add_argument("--page-size", "--cjk-size", dest="page_size", type=int, default=4096, help="兜底页图集边长")
    parser.add_argument(
        "--fallback-only-script",
        action="append",
        metavar="TAG",
        help="只烘焙指定脚本的兜底页（emoji），可重复；省略表示全部",
    )
    parser.add_argument(
        "--skip-profile-write",
        action="store_true",
        help="烘焙完不写 profile 清单（并行跑多个脚本时用，最后单独跑一次 --profile-only 汇总）",
    )
    parser.add_argument(
        "--profile-only",
        action="store_true",
        help="不烘焙，只按已落盘的页重写 symbols profile 清单",
    )
    parser.add_argument(
        "--fallback-chunk",
        type=int,
        default=DEFAULT_FALLBACK_CHUNK,
        help=f"兜底页每片字形数（默认 {DEFAULT_FALLBACK_CHUNK}）；4096 图集实测符号容量约 1900，取 1800 避免溢出",
    )
    parser.add_argument(
        "--jobs",
        type=int,
        default=0,
        help="并行烘焙进程数；0 表示 CPU 核数的一半",
    )
    return parser.parse_args()


def codepoints_in_ranges(ranges: tuple[tuple[int, int], ...]) -> list[int]:
    out: list[int] = []
    for low, high in ranges:
        out.extend(range(low, high + 1))
    return out


def font_coverage(path: Path, face_index: int = 0) -> set[int]:
    """返回字体覆盖的码位集合。"""
    from fontTools.ttLib import TTCollection, TTFont

    if path.suffix.lower() == ".ttc":
        font = TTCollection(str(path)).fonts[face_index]
    else:
        font = TTFont(str(path), fontNumber=face_index)
    return {cp for cp in font.getBestCmap() if cp > 0}


def write_charset(path: Path, codepoints: list[int]) -> None:
    path.write_text("".join(f"U+{cp:04X}\n" for cp in codepoints), encoding="utf-8")


def chunk_codepoints(codepoints: list[int], chunk_size: int) -> list[list[int]]:
    return [codepoints[index : index + chunk_size] for index in range(0, len(codepoints), chunk_size)]


def run_tool_soft(
    args: argparse.Namespace,
    font: Path,
    face_index: int,
    codepoints: list[int],
    prefix: Path,
    size: int,
) -> tuple[dict, list[int]]:
    """运行自建生成工具；返回 (manifest, 未打包的码点)。

    工具装不下时会静默截断并返回 0，这里把缺失码点回报给调用方自己决定拆分，
    而不是直接终止整批烘焙。
    """
    write_charset(prefix.with_suffix(".charset.txt"), codepoints)
    command = [
        str(args.tool),
        "--font", str(font),
        "--font-index", str(face_index),
        "--charset", str(prefix.with_suffix(".charset.txt")),
        "--output", str(prefix),
        "--em-pixels", str(args.em_pixels),
        "--px-range", str(args.px_range),
        "--size", str(size),
    ]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        raise SystemExit(f"atlas tool failed ({result.returncode}) for {prefix}")
    manifest = json.loads(prefix.with_suffix(".json").read_text(encoding="utf-8"))
    packed = {int(key) for key in manifest["glyphs"]}
    return manifest, [cp for cp in codepoints if cp not in packed]


def bake_chunk(
    args: argparse.Namespace,
    font: Path,
    face_index: int,
    codepoints: list[int],
    size: int,
    temp_dir: Path,
    tag: str,
) -> tuple[tuple[Path, dict], list[int]]:
    """烤一片，返回 (落盘的页, 没装下的码点)。

    装不下时**不再对半拆**：对半拆会把「只差几十个字形」的片裂成两页，浪费整页显存。
    溢出的码点由调用方汇总后追加一页即可。
    """
    prefix = temp_dir / tag
    manifest, missing = run_tool_soft(args, font, face_index, codepoints, prefix, size)
    if not manifest["glyphs"]:
        raise SystemExit(f"atlas page cannot hold U+{codepoints[0]:04X}")
    return (prefix, manifest), missing


def publish_page(raw_prefix: Path, manifest: dict, image_name: str, font_label: str, out_dir: Path) -> dict:
    """把临时目录里的页转成 QmClient manifest + PNG 落盘。"""
    from PIL import Image

    width = manifest["atlas"]["width"]
    height = manifest["atlas"]["height"]
    stem = Path(image_name).stem
    out_dir.mkdir(parents=True, exist_ok=True)
    raw_path = raw_prefix.with_suffix(".rgba")
    if raw_path.is_file():
        data = raw_path.read_bytes()
        if len(data) != width * height * 4:
            raise SystemExit(f"unexpected raw size for {raw_prefix}: {len(data)}")
        # 图形上传路径使用 RGBA；RGB 是 MSDF，Alpha 是真实单通道 SDF（MTSDF）。
        Image.frombytes("RGBA", (width, height), data).save(out_dir / f"{stem}.png", optimize=True)
    else:
        official_image = raw_prefix.with_suffix(".png")
        if not official_image.is_file():
            raise SystemExit(f"atlas image missing for {raw_prefix}")
        Image.open(official_image).convert("RGBA").save(out_dir / f"{stem}.png", optimize=True)

    page = {
        "version": 1,
        "kind": "msdf-glyphs",
        "distance_field": "mtsdf",
        "alpha_sdf": True,
        "px_range": manifest["px_range"],
        "em_pixels": manifest["em_pixels"],
        "padding": manifest["padding"],
        "source_font": font_label,
        "atlas": {"image": image_name, "width": width, "height": height},
        "glyphs": manifest["glyphs"],
    }
    # 字体行盒度量（可选：旧工具产物没有；运行时缺省时按 cap 高度近似）
    if manifest.get("ascent") is not None:
        page["ascent"] = manifest["ascent"]
        page["descent"] = manifest["descent"]
    (out_dir / f"{stem}.json").write_text(json.dumps(page, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return page


def write_profile_manifest(out_dir: Path, profile: str, pages: list[str]) -> None:
    profile_dir = out_dir / "profiles"
    profile_dir.mkdir(parents=True, exist_ok=True)
    (profile_dir / f"nameplate_{profile}.json").write_text(
        json.dumps({"version": 1, "kind": "msdf-profile", "profile": profile, "pages": pages}, indent=2) + "\n",
        encoding="utf-8",
    )


def fallback_page_rank(path: Path) -> tuple[int, int, str]:
    """兜底页排序键：先按 FALLBACK_PAGE_ORDER 的脚本序，再按页序号。"""
    name = path.stem
    order = len(FALLBACK_PAGE_ORDER)
    for index, token in enumerate(FALLBACK_PAGE_ORDER):
        if token in name:
            order = index
            break
    suffix = name.rsplit("_", 1)[-1]
    return (order, int(suffix) if suffix.isdigit() else -1, name)


def collect_fallback_pages(out_dir: Path) -> list[str]:
    """扫描已落盘的兜底页，按固定顺序返回 manifest 相对路径。

    用扫描而不是只用本次生成的页：分脚本增量烘焙时，本次没参与的脚本页仍要留在
    profile 里，否则下一次烘焙会把它们从 profile 里挤掉。
    """
    found: set[Path] = set()
    for entry in FALLBACK_SCRIPTS:
        found.update(out_dir.glob(f"{entry.page_stem}_*.json"))
    return [f"qmclient/nameplate_msdf/{path.name}" for path in sorted(found, key=fallback_page_rank)]


def bake_fallback_scripts(args: argparse.Namespace, fonts_dir: Path) -> int:
    """重建 symbols 兜底页：每个脚本一组页，每组只用一种随包字体。

    这是名牌 MTSDF 的最后一道 MTSDF 防线——只有这里的页也覆盖不到，才允许回退
    FreeType。页名必须保持 noto_glow_* 前缀（见文件头说明）。
    """
    requested = list(args.fallback_only_script or ())
    unknown = set(requested) - {entry.tag for entry in FALLBACK_SCRIPTS}
    if unknown:
        raise SystemExit(f"unknown fallback script(s): {', '.join(sorted(unknown))}")
    selected = [entry for entry in FALLBACK_SCRIPTS if not requested or entry.tag in requested]

    jobs = args.jobs if args.jobs > 0 else max(1, (os.cpu_count() or 4) // 2)
    baked = 0
    for entry in selected:
        font = fonts_dir / entry.font
        if not font.is_file():
            raise SystemExit(f"missing source font for '{entry.tag}': {font}")
        coverage = font_coverage(font, entry.face)
        available = [cp for cp in codepoints_in_ranges(entry.ranges) if cp in coverage]
        if not available:
            raise SystemExit(f"{entry.font} (face {entry.face}) covers none of the '{entry.tag}' ranges")

        # 常用优先 + 字形预算：预算内取最常用的那批，剩下的交给 FreeType 兜底。
        ordered = prioritize_codepoints(available, entry.tiers())
        codepoints = ordered[: entry.max_glyphs] if entry.max_glyphs > 0 else ordered
        if len(codepoints) < len(ordered):
            print(
                f"fallback '{entry.tag}': budget {len(codepoints)}/{len(ordered)} codepoints "
                f"(most-used first); rest falls back to FreeType"
            )

        chunk_size = entry.chunk if entry.chunk > 0 else args.fallback_chunk
        chunks = chunk_codepoints(codepoints, chunk_size)
        page_size = entry.page_size if entry.page_size > 0 else args.page_size
        print(f"fallback '{entry.tag}': {len(codepoints)} codepoints from {font.name} face {entry.face}")
        print(f"  baking {len(chunks)} chunk(s) of {chunk_size} with {jobs} job(s), atlas {page_size}px")

        groups: list[list[tuple[Path, dict]]] = [[] for _ in chunks]
        overflow: list[int] = []
        with tempfile.TemporaryDirectory(prefix=f"qm-nameplate-{entry.tag}-") as temp:
            temp_dir = Path(temp)
            with ThreadPoolExecutor(max_workers=jobs) as pool:
                futures = {
                    pool.submit(
                        bake_chunk,
                        args,
                        font,
                        entry.face,
                        chunk,
                        page_size,
                        temp_dir,
                        f"{entry.tag}_{index:02d}",
                    ): index
                    for index, chunk in enumerate(chunks)
                }
                for future in as_completed(futures):
                    index = futures[future]
                    page, missing = future.result()
                    if page is not None:
                        groups[index].append(page)
                    if missing:
                        overflow.extend(missing)

            # 各片溢出的字形汇总后追加成页：溢出量通常只有几十个，不该为它多裂出整页。
            extra = 0
            while overflow:
                page, remaining = bake_chunk(
                    args, font, entry.face, overflow, page_size, temp_dir, f"{entry.tag}_x{extra:02d}"
                )
                if page is not None:
                    groups.append([page])
                if len(remaining) >= len(overflow):
                    raise SystemExit(f"atlas cannot make progress on {len(remaining)} overflow codepoint(s)")
                print(f"  {entry.tag}: {len(overflow)} overflow codepoint(s) -> extra page {extra}")
                overflow = remaining
                extra += 1

            # 按首码点排序，保证页名顺序与码点顺序一致：运行时先加载的页在重码时优先。
            pages = sorted(
                (page for group in groups for page in group),
                key=lambda item: min(int(key) for key in item[1]["glyphs"]),
            )

            # 页数可能变少，先清掉同一前缀的旧产物，避免残留页被误引用。
            stale = list(args.output.glob(f"{entry.page_stem}_*.json")) + list(
                args.output.glob(f"{entry.page_stem}_*.png")
            )
            for path in stale:
                path.unlink()

            for index, (prefix, manifest) in enumerate(pages):
                stem = f"{entry.page_stem}_{index:02d}"
                image_name = f"qmclient/nameplate_msdf/{stem}.png"
                page = publish_page(prefix, manifest, image_name, font.stem, args.output)
                print(f"  {entry.tag} page {index:02d}: {len(page['glyphs'])} glyphs")
                baked += 1

    refs = collect_fallback_pages(args.output)
    if args.skip_profile_write:
        print(f"done: {baked} page(s) rebaked; profile write skipped ({len(refs)} page(s) present)")
    else:
        write_profile_manifest(args.output, FALLBACK_PROFILE, refs)
        print(f"done: {FALLBACK_PROFILE}, {len(refs)} page(s) in profile, {baked} page(s) rebaked -> {args.output}")
    return 0


def main() -> int:
    args = parse_args()
    if not args.tool.is_file():
        raise SystemExit(f"atlas tool not found: {args.tool}")

    # 只汇总 profile：并行烘焙各脚本后单独跑一次，避免多个进程同时写同一份清单。
    if args.profile_only:
        refs = collect_fallback_pages(args.output)
        write_profile_manifest(args.output, FALLBACK_PROFILE, refs)
        print(f"profile: {FALLBACK_PROFILE}, {len(refs)} page(s) -> {args.output}")
        return 0

    # 兜底页烘焙只依赖 data/fonts 下的随包字体，不碰其它 profile 页。
    return bake_fallback_scripts(args, args.data_root / "fonts")


if __name__ == "__main__":
    raise SystemExit(main())
