#!/usr/bin/env python3
"""生成 QmClient 名牌专用 MSDF 字形图集（离线资源管线）。

产物写入 data/qmclient/nameplate_msdf/：
  <page>.png / <page>.json   图集页与字形清单（运行时按 manifest 加载）

字形来源与分层（每页只用一种字体，度量口径统一，不做跨页拼补）：
  page 0  base：DejaVuSans  —— 拉丁/拉丁扩展/希腊/西里尔/常用标点符号
  page 1  cjk ：SourceHanSans SC —— CJK 统一表意文字 + 假名 + CJK 标点，按码位升序截取

CJK 按码位升序截取而非按词频：U+4E00..U+9FFF 本身即康熙部首序，常用字集中在低段，
在有限图集预算下这是确定性强且覆盖「常用优先」的取法。

需要一次性构建的生成工具（正常客户端构建不依赖 msdfgen）：
  cmake -S qmclient_scripts/qm_nameplate_msdf_atlas -B <build> -DQM_PROJECT_ROOT=<repo>
  cmake --build <build> --config Release
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

# 基础脚本区：拉丁、希腊、西里尔、常用标点与符号
BASE_RANGES: tuple[tuple[int, int], ...] = (
    (0x0020, 0x007E),  # Basic Latin
    (0x00A0, 0x00FF),  # Latin-1 Supplement
    (0x0100, 0x017F),  # Latin Extended-A
    (0x0180, 0x024F),  # Latin Extended-B
    (0x0370, 0x03FF),  # Greek
    (0x0400, 0x04FF),  # Cyrillic
    (0x2010, 0x203A),  # General Punctuation（常用段）
    (0x20AC, 0x20AC),  # Euro
    (0x2116, 0x2116),  # №
    (0x2190, 0x2193),  # 箭头
    (0x25A0, 0x25CF),  # 几何图形（常用）
    (0x2605, 0x2606),  # ★☆
    (0x2665, 0x2665),  # ♥
)

CJK_BLOCKS: tuple[tuple[int, int], ...] = (
    (0x4E00, 0x9FFF),  # CJK 统一表意文字
    (0x3000, 0x303F),  # CJK 符号与标点
    (0x3040, 0x309F),  # 平假名
    (0x30A0, 0x30FF),  # 片假名
)

# 目标语言的完整脚本范围。它们与汉字一起进入共享 fallback 页，
# 这样每个 profile 都能覆盖中文、日文、韩文，而不是只覆盖 U+4E00 的前 8000 个字。
JAPANESE_RANGES: tuple[tuple[int, int], ...] = (
    (0x3040, 0x309F),  # Hiragana
    (0x30A0, 0x30FF),  # Katakana
    (0x31F0, 0x31FF),  # Katakana Phonetic Extensions
    (0xFF66, 0xFF9D),  # Halfwidth Katakana
)
KOREAN_RANGES: tuple[tuple[int, int], ...] = (
    (0x1100, 0x11FF),  # Hangul Jamo
    (0x3130, 0x318F),  # Hangul Compatibility Jamo
    (0xA960, 0xA97F),  # Hangul Jamo Extended-A
    (0xAC00, 0xD7AF),  # Hangul Syllables
    (0xD7B0, 0xD7FF),  # Hangul Jamo Extended-B
)

THAI_RANGES: tuple[tuple[int, int], ...] = ((0x0E00, 0x0E7F),)
EMOJI_RANGES: tuple[tuple[int, int], ...] = ((0x1F300, 0x1FAFF),)

# 汉字区段：预算优先给汉字，假名/标点只在有余量时补
HAN_BLOCK = (0x4E00, 0x9FFF)

SOURCE_HAN_SC_FACE = 2  # SourceHanSans.ttc: 0 通用 / 2 简体

BUILTIN_PROFILES = (
    ("dejavu", "DejaVuSans.ttf", 0, "DejaVu Sans", ()),
    ("glow_sans_j", "GlowSansJ-Compressed-Book.otf", 0, "Glow Sans J Compressed Book", ()),
    ("source_han", "SourceHanSans.ttc", 0, "Source Han Sans", ()),
    ("source_han_sc", "SourceHanSans.ttc", 2, "Source Han Sans SC", ()),
    ("source_han_tc", "SourceHanSans.ttc", 3, "Source Han Sans TC", ()),
    ("source_han_k", "SourceHanSans.ttc", 1, "Source Han Sans K", ()),
    ("source_han_hc", "SourceHanSans.ttc", 4, "Source Han Sans HC", ()),
    ("noto_sans_sc", "NotoSansSC-VF.ttf", 0, "Noto Sans SC", ()),
    ("noto_sans_thai", "NotoSansThai-Regular.ttf", 0, "Noto Sans Thai", THAI_RANGES),
    ("noto_emoji", "NotoEmoji-Regular.ttf", 0, "Noto Emoji", ()),
)

COMPARE_DEFAULT_CODEPOINTS = (0x0041, 0x0067, 0x4E00, 0x4E2D, 0x56FD, 0x65E5, 0x0E01, 0x1F600)
# Noto Sans SC 的 CJK 页先固定为常用字子集，剩余汉字由共享 Source Han
# fallback 页覆盖；这样新增字体不会在每次全量重建时额外烤 8000 个重复字形。
PROFILE_CJK_LIMITS = {"noto_sans_sc": 856}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool", type=Path, required=True, help="qm-nameplate-msdf-atlas 可执行文件")
    parser.add_argument("--official-tool", type=Path, help="官方 msdf-atlas-gen 可执行文件；指定后替代自定义生成器")
    parser.add_argument("--output", type=Path, required=True, help="输出目录（data/qmclient/nameplate_msdf）")
    parser.add_argument("--data-root", type=Path, default=Path("data"), help="data 根目录，用于定位字体")
    # 使用足够的离线分辨率，避免小字号 atlas 被放大后出现圆弧折线和虫蚀。
    # CJK 批量烘焙可显式传回较低参数，但验收 profile 默认走高质量档。
    parser.add_argument("--em-pixels", type=int, default=64)
    parser.add_argument("--px-range", type=float, default=8.0)
    parser.add_argument("--base-size", type=int, default=2048)
    parser.add_argument("--cjk-size", type=int, default=4096)
    parser.add_argument("--cjk-max", type=int, default=8000, help="CJK 页最多烤多少字形（优先汉字）")
    parser.add_argument("--profiles-only", action="store_true", help="只生成多字体 profile 页，不重烤已有基础/CJK 页")
    parser.add_argument("--shared-scripts-only", action="store_true", help="只生成共享汉字/假名/Hangul 页并更新 profile 引用")
    parser.add_argument("--compare-output", type=Path, help="生成 MSDF/FreeType 对照报告的目录")
    parser.add_argument("--compare-scale", type=float, action="append", help="对照缩放比例，可重复；默认 1.0")
    parser.add_argument("--compare-codepoint", action="append", help="对照码点，例如 U+4E2D；可重复")
    parser.add_argument("--compare-strict", action="store_true", help="对照出现 fail/missing 时让构建失败")
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


def run_tool(args: argparse.Namespace, font: Path, face_index: int, codepoints: list[int], prefix: Path, size: int) -> dict:
    charset = prefix.with_suffix(".charset.txt")
    if args.official_tool is not None:
        charset.write_text('"' + "".join(chr(cp) for cp in codepoints) + '"\n', encoding="utf-8")
        image = prefix.with_suffix(".png")
        command = [
            sys.executable,
            str(Path(__file__).with_name("qm_nameplate_msdf_official.py")),
            "--tool", str(args.official_tool),
            "--font", str(font),
            "--charset", str(charset),
            "--output", str(prefix.with_suffix(".json")),
            "--image", str(image),
            "--font-name", font.stem,
            "--font-index", str(face_index),
            "--size", str(args.em_pixels),
            "--px-range", str(args.px_range),
            "--padding", str(max(1, int(args.px_range) + 1)),
            "--width", str(size),
            "--height", str(size),
        ]
        print(f"  run official: {font.name} chars={len(codepoints)} size={size}")
        result = subprocess.run(command, capture_output=True, text=True)
        if result.stdout.strip():
            sys.stdout.write("  " + result.stdout.strip().replace("\n", "\n  ") + "\n")
        if result.returncode != 0:
            sys.stderr.write(result.stderr)
            raise SystemExit(f"official atlas tool failed ({result.returncode}) for {prefix}")
        manifest = json.loads(prefix.with_suffix(".json").read_text(encoding="utf-8"))
        packed = {int(key) for key in manifest["glyphs"]}
        missing = [cp for cp in codepoints if cp not in packed]
        if missing:
            raise SystemExit(f"official atlas page missing U+{missing[0]:04X} for {prefix}")
        return manifest

    write_charset(charset, codepoints)
    command = [
        str(args.tool),
        "--font",
        str(font),
        "--font-index",
        str(face_index),
        "--charset",
        str(charset),
        "--output",
        str(prefix),
        "--em-pixels",
        str(args.em_pixels),
        "--px-range",
        str(args.px_range),
        "--size",
        str(size),
    ]
    print(f"  run: {font.name} face={face_index} chars={len(codepoints)} size={size}")
    result = subprocess.run(command, capture_output=True, text=True)
    if result.stdout.strip():
        sys.stdout.write("  " + result.stdout.strip().replace("\n", "\n  ") + "\n")
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        raise SystemExit(f"atlas tool failed ({result.returncode}) for {prefix}")
    manifest = json.loads(prefix.with_suffix(".json").read_text(encoding="utf-8"))
    # 页装不下会静默截断；这里显式失败，避免发布一个覆盖不全的图集
    packed = {int(key) for key in manifest["glyphs"]}
    missing = [cp for cp in codepoints if cp not in packed]
    if missing:
        raise SystemExit(
            f"atlas page too small for {font.name}: {len(missing)} codepoints unpacked "
            f"(first U+{missing[0]:04X}); increase --base-size/--cjk-size"
        )
    return manifest


def compare_page(args: argparse.Namespace, page: dict, image_name: str, out_dir: Path, font_path: Path, face_index: int) -> None:
    if args.compare_output is None:
        return
    compare_script = Path(__file__).with_name("qm_nameplate_msdf_compare.py")
    if not compare_script.is_file():
        raise SystemExit(f"compare tool not found: {compare_script}")
    requested = args.compare_codepoint or [f"U+{Codepoint:04X}" for Codepoint in COMPARE_DEFAULT_CODEPOINTS]
    available = [Value for Value in requested if str(int(Value[2:] if Value.upper().startswith("U+") else Value, 16)) in page["glyphs"]]
    if not available:
        return
    scales = args.compare_scale or [1.0]
    manifest_path = out_dir / f"{Path(image_name).stem}.json"
    image_path = out_dir / f"{Path(image_name).stem}.png"
    for scale in scales:
        if scale <= 0.0:
            raise SystemExit("--compare-scale must be positive")
        scale_name = f"{scale:g}".replace(".", "_")
        report_dir = args.compare_output / Path(image_name).stem / f"scale-{scale_name}"
        command = [
            sys.executable,
            str(compare_script),
            "--font",
            str(font_path),
            "--font-index",
            str(face_index),
            "--manifest",
            str(manifest_path),
            "--image",
            str(image_path),
            "--output",
            str(report_dir),
            "--scale",
            str(scale),
        ]
        for Codepoint in available:
            command.extend(("--codepoint", Codepoint))
        result = subprocess.run(command, capture_output=True, text=True)
        if result.stdout.strip():
            print(f"  compare {Path(image_name).stem} scale={scale:g}: {result.stdout.strip()}")
        if result.returncode != 0:
            message = f"MSDF compare reported differences for {image_name} scale={scale:g}"
            if args.compare_strict:
                raise SystemExit(message)
            print(f"  warning: {message}")


def publish_page(
    args: argparse.Namespace,
    raw_prefix: Path,
    manifest: dict,
    image_name: str,
    font_label: str,
    out_dir: Path,
    font_path: Path | None = None,
    face_index: int = 0,
) -> dict:
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
    if font_path is not None:
        compare_page(args, page, image_name, out_dir, font_path, face_index)
    return page


def write_profile_manifest(out_dir: Path, profile: str, pages: list[str]) -> None:
    profile_dir = out_dir / "profiles"
    profile_dir.mkdir(parents=True, exist_ok=True)
    (profile_dir / f"nameplate_{profile}.json").write_text(
        json.dumps({"version": 1, "kind": "msdf-profile", "profile": profile, "pages": pages}, indent=2) + "\n",
        encoding="utf-8",
    )


def profile_cjk_codepoints(coverage: set[int], cjk_max: int) -> list[int]:
    """Select CJK codepoints in the same deterministic order as the shared page."""
    han = sorted(cp for cp in coverage if HAN_BLOCK[0] <= cp <= HAN_BLOCK[1])
    other = sorted(
        cp
        for cp in coverage
        if any(low <= cp <= high for low, high in CJK_BLOCKS if (low, high) != HAN_BLOCK)
    )
    return (han + other)[:cjk_max]


def chunk_codepoints(codepoints: list[int], chunk_size: int) -> list[list[int]]:
    return [codepoints[index : index + chunk_size] for index in range(0, len(codepoints), chunk_size)]


def main() -> int:
    args = parse_args()
    if not args.tool.is_file():
        raise SystemExit(f"atlas tool not found: {args.tool}")

    fonts_dir = args.data_root / "fonts"
    dejavu = fonts_dir / "DejaVuSans.ttf"
    source_han = fonts_dir / "SourceHanSans.ttc"
    for font in (dejavu, source_han):
        if not font.is_file():
            raise SystemExit(f"missing source font: {font}")

    print("resolving coverage...")
    # 优先级顺序：ASCII → Latin-1 → Latin Ext-A → 标点符号 → 希腊/西里尔 → 其余。
    # 图集放不下时，工具按此顺序先满足靠前的字形。
    base_codepoints = [cp for cp in codepoints_in_ranges(BASE_RANGES) if cp in font_coverage(dejavu)]
    han_coverage = font_coverage(source_han, SOURCE_HAN_SC_FACE)
    han_codepoints = sorted(cp for cp in han_coverage if HAN_BLOCK[0] <= cp <= HAN_BLOCK[1])
    other_codepoints = sorted(
        cp
        for cp in han_coverage
        if any(low <= cp <= high for low, high in CJK_BLOCKS if (low, high) != HAN_BLOCK)
    )
    # 汉字页保持可控大小；日文假名与韩文 Hangul 单独分片，避免被 8000 汉字预算挤掉。
    cjk_codepoints = (han_codepoints + other_codepoints)[: args.cjk_max]
    baked_han = [cp for cp in cjk_codepoints if HAN_BLOCK[0] <= cp <= HAN_BLOCK[1]]
    print(f"  base: DejaVu covers {len(base_codepoints)} codepoints")
    print(
        f"  cjk : font covers {len(han_codepoints)} han; baking {len(cjk_codepoints)} "
        f"({len(baked_han)} han)"
    )

    pages: list[dict] = []
    shared_cjk_pages: list[str] = []
    common_cjk_codepoints: list[int] = []
    if not args.profiles_only:
        with tempfile.TemporaryDirectory(prefix="qm-nameplate-msdf-") as temp:
            temp_dir = Path(temp)

            # 共享脚本增量模式只更新 CJK/假名/Hangul 页；基础拉丁页属于各
            # profile 的主字体资源，不应在该模式下被重复重烤。
            if base_codepoints and not args.shared_scripts_only:
                prefix = temp_dir / "base"
                manifest = run_tool(args, dejavu, 0, base_codepoints, prefix, args.base_size)
                page = publish_page(
                    args, prefix, manifest, "qmclient/nameplate_msdf/nameplate_base_msdf.png", "DejaVuSans", args.output, dejavu, 0
                )
                pages.append(page)
                print(f"  base page: {len(page['glyphs'])} glyphs {page['atlas']['width']}x{page['atlas']['height']}")

            for PageIndex, CodepointChunk in enumerate(chunk_codepoints(cjk_codepoints, args.cjk_max)):
                prefix = temp_dir / f"cjk_{PageIndex:02d}"
                manifest = run_tool(args, source_han, SOURCE_HAN_SC_FACE, CodepointChunk, prefix, args.cjk_size)
                stem = "nameplate_cjk_msdf" if PageIndex == 0 else f"nameplate_cjk_msdf_{PageIndex:02d}"
                image_name = f"qmclient/nameplate_msdf/{stem}.png"
                page = publish_page(
                    args,
                    prefix,
                    manifest,
                    image_name,
                    f"SourceHanSansSC#{SOURCE_HAN_SC_FACE}",
                    args.output,
                    source_han,
                    SOURCE_HAN_SC_FACE,
                )
                pages.append(page)
                shared_cjk_pages.append(f"qmclient/nameplate_msdf/{stem}.json")
                print(f"  cjk page {PageIndex + 1}: {len(page['glyphs'])} glyphs {page['atlas']['width']}x{page['atlas']['height']}")

            # 日文假名独立页：完整覆盖 Hiragana/Katakana 及半角片假名。
            KanaCodepoints = sorted(cp for cp in han_coverage if any(low <= cp <= high for low, high in JAPANESE_RANGES))
            if KanaCodepoints:
                prefix = temp_dir / "kana"
                manifest = run_tool(args, source_han, SOURCE_HAN_SC_FACE, KanaCodepoints, prefix, args.cjk_size)
                page = publish_page(
                    args, prefix, manifest, "qmclient/nameplate_msdf/nameplate_kana_msdf.png",
                    f"SourceHanSansSC#{SOURCE_HAN_SC_FACE}", args.output, source_han, SOURCE_HAN_SC_FACE
                )
                pages.append(page)
                shared_cjk_pages.append("qmclient/nameplate_msdf/nameplate_kana_msdf.json")
                print(f"  kana page: {len(page['glyphs'])} glyphs {page['atlas']['width']}x{page['atlas']['height']}")

            # 韩文 Hangul 独立分片，完整覆盖现代音节与 Jamo。
            HangulCodepoints = sorted(cp for cp in han_coverage if any(low <= cp <= high for low, high in KOREAN_RANGES))
            for PageIndex, CodepointChunk in enumerate(chunk_codepoints(HangulCodepoints, args.cjk_max)):
                prefix = temp_dir / f"hangul_{PageIndex:02d}"
                manifest = run_tool(args, source_han, SOURCE_HAN_SC_FACE, CodepointChunk, prefix, args.cjk_size)
                stem = "nameplate_hangul_msdf" if PageIndex == 0 else f"nameplate_hangul_msdf_{PageIndex:02d}"
                page = publish_page(
                    args, prefix, manifest, f"qmclient/nameplate_msdf/{stem}.png",
                    f"SourceHanSansSC#{SOURCE_HAN_SC_FACE}", args.output, source_han, SOURCE_HAN_SC_FACE
                )
                pages.append(page)
                shared_cjk_pages.append(f"qmclient/nameplate_msdf/{stem}.json")
                print(f"  hangul page {PageIndex + 1}: {len(page['glyphs'])} glyphs {page['atlas']['width']}x{page['atlas']['height']}")

    if not pages and not args.profiles_only:
        raise SystemExit("no glyph pages were produced")

    if args.shared_scripts_only:
        # 共享脚本页生成后，增量更新现有 profile manifest，避免重新烤每套主字体页。
        profile_dir = args.output / "profiles"
        for manifest_path in profile_dir.glob("nameplate_*.json"):
            try:
                profile_manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                continue
            changed = False
            for page_path in shared_cjk_pages:
                if page_path not in profile_manifest.get("pages", []):
                    profile_manifest.setdefault("pages", []).append(page_path)
                    changed = True
            if changed:
                manifest_path.write_text(json.dumps(profile_manifest, indent=2) + "\n", encoding="utf-8")
        print(f"done: shared script pages, {len(shared_cjk_pages)} page reference(s) updated")
        return 0

    # 多字体 profile：主字体先加载，随后加载客户端默认图集作为缺字 fallback。
    # 这样运行时的码点优先级与 FreeType 的 selected/default/fallback 顺序一致，
    # 而不需要把所有字体塞进一张巨大图集。
    # profiles-only 模式不重烤共享页，但仍要引用补充 CJK 页。
    if not shared_cjk_pages:
        shared_cjk_pages = ["qmclient/nameplate_msdf/nameplate_cjk_msdf.json"]
    shared_pages = shared_cjk_pages + [
        "qmclient/nameplate_msdf/nameplate_noto_thai_msdf.json",
        "qmclient/nameplate_msdf/nameplate_noto_emoji_base_msdf.json",
    ]
    for Profile, FontName, FaceIndex, FontLabel, ExtraRanges in BUILTIN_PROFILES:
        FontPath = fonts_dir / FontName
        Coverage = font_coverage(FontPath, FaceIndex)
        RequestedRanges = BASE_RANGES + ExtraRanges
        ProfileCodepoints = [cp for cp in codepoints_in_ranges(RequestedRanges) if cp in Coverage]
        ProfileCjkCodepoints = profile_cjk_codepoints(Coverage, PROFILE_CJK_LIMITS.get(Profile, args.cjk_max))
        ProfileEmojiCodepoints = [cp for cp in codepoints_in_ranges(EMOJI_RANGES) if cp in Coverage]
        if not ProfileCodepoints and not ProfileCjkCodepoints and not ProfileEmojiCodepoints:
            continue
        ProfilePages: list[str] = []
        with tempfile.TemporaryDirectory(prefix=f"qm-nameplate-msdf-{Profile}-") as ProfileTemp:
            ProfileTempPath = Path(ProfileTemp)
            if ProfileCodepoints:
                Prefix = ProfileTempPath / "base"
                Manifest = run_tool(args, FontPath, FaceIndex, ProfileCodepoints, Prefix, args.base_size)
                ImageName = f"qmclient/nameplate_msdf/nameplate_{Profile}_base_msdf.png"
                publish_page(args, Prefix, Manifest, ImageName, FontLabel, args.output, FontPath, FaceIndex)
                ProfilePages.append(f"qmclient/nameplate_msdf/nameplate_{Profile}_base_msdf.json")
            if ProfileCjkCodepoints:
                Prefix = ProfileTempPath / "cjk"
                Manifest = run_tool(args, FontPath, FaceIndex, ProfileCjkCodepoints, Prefix, args.cjk_size)
                ImageName = f"qmclient/nameplate_msdf/nameplate_{Profile}_cjk_msdf.png"
                publish_page(args, Prefix, Manifest, ImageName, FontLabel, args.output, FontPath, FaceIndex)
                ProfilePages.append(f"qmclient/nameplate_msdf/nameplate_{Profile}_cjk_msdf.json")
            if ProfileEmojiCodepoints:
                Prefix = ProfileTempPath / "emoji"
                Manifest = run_tool(args, FontPath, FaceIndex, ProfileEmojiCodepoints, Prefix, args.cjk_size)
                ImageName = f"qmclient/nameplate_msdf/nameplate_{Profile}_emoji_msdf.png"
                publish_page(args, Prefix, Manifest, ImageName, FontLabel, args.output, FontPath, FaceIndex)
                ProfilePages.append(f"qmclient/nameplate_msdf/nameplate_{Profile}_emoji_msdf.json")

        # 除默认 DejaVu 外，其余主字体按 FreeType 的 fallback 顺序补默认拉丁页。
        if Profile != "dejavu":
            ProfilePages.append("qmclient/nameplate_msdf/nameplate_base_msdf.json")
        ProfilePages.extend(shared_pages)
        write_profile_manifest(args.output, Profile, ProfilePages)

    # 共享 fallback 页只生成一次，所有 profile 通过 manifest 引用。
    for Profile, FontName, FaceIndex, FontLabel, Ranges in (
        ("noto_thai", "NotoSansThai-Regular.ttf", 0, "Noto Sans Thai", THAI_RANGES),
    ):
        FontPath = fonts_dir / FontName
        Coverage = font_coverage(FontPath, FaceIndex)
        Codepoints = [cp for cp in codepoints_in_ranges(Ranges) if cp in Coverage]
        with tempfile.TemporaryDirectory(prefix=f"qm-nameplate-msdf-{Profile}-") as ProfileTemp:
            Prefix = Path(ProfileTemp) / "page"
            Manifest = run_tool(args, FontPath, FaceIndex, Codepoints, Prefix, args.base_size)
            publish_page(args, Prefix, Manifest, f"qmclient/nameplate_msdf/nameplate_{Profile}_msdf.png", FontLabel, args.output, FontPath, FaceIndex)

    # Emoji 是独立 fallback 页，避免把 1,000+ 个彩色符号挤进基础拉丁页。
    EmojiFontPath = fonts_dir / "NotoEmoji-Regular.ttf"
    EmojiCoverage = font_coverage(EmojiFontPath, 0)
    EmojiCodepoints = [cp for cp in codepoints_in_ranges(EMOJI_RANGES) if cp in EmojiCoverage]
    if EmojiCodepoints:
        with tempfile.TemporaryDirectory(prefix="qm-nameplate-msdf-noto-emoji-") as ProfileTemp:
            Prefix = Path(ProfileTemp) / "emoji"
            Manifest = run_tool(args, EmojiFontPath, 0, EmojiCodepoints, Prefix, args.cjk_size)
            publish_page(args, Prefix, Manifest, "qmclient/nameplate_msdf/nameplate_noto_emoji_base_msdf.png", "Noto Emoji", args.output, EmojiFontPath, 0)

    total = sum(len(page["glyphs"]) for page in pages)
    print(f"done: {len(pages)} page(s), {total} glyphs -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
