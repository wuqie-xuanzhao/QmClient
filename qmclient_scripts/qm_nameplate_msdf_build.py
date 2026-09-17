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
import os
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Callable, NamedTuple

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

# 共享脚本兜底页（所有 profile 的缺字回退）。
#
# 名牌的 MTSDF 回退链是「选中 profile → dejavu → noto_glow_cjk → FreeType」，
# 所以 noto_glow_cjk 是唯一的脚本兜底入口：随包字体能覆盖的脚本都要在这里补齐，
# MTSDF 全部落空才允许回退 FreeType。每个脚本只用一种随包字体，同一批字形不会
# 在多套 profile 里重复烘焙。
#
# 这套页历史上是临时命令的产物，仓库里没有脚本能复现它，结果是汉字只覆盖到
# U+6F3F（8512/20976 ≈ 40%），常用字如「的/球/茶/空/王/爱」全部缺席；名牌是整条
# 回退，命中一个缺失字就整条退回 FreeType。--fallback-scripts-only 固定了这个入口。
#
# 页名必须保持 noto_glow_* 前缀：渲染器对路径含 "noto_glow" 的页禁用 Alpha 真 SDF
# （规避历史 Alpha 伪影导致字形变实心块），改名会连带改变渲染路径。
HAN_RANGES: tuple[tuple[int, int], ...] = (
    (0x4E00, 0x9FFF),  # CJK 统一表意文字（排在扩展 A 前：预算的剩余部分按此顺序填充，
    (0x3400, 0x4DBF),  # CJK 扩展 A         罕用的扩展 A 因此落在最后）
)
HANGUL_RANGES: tuple[tuple[int, int], ...] = (
    (0xAC00, 0xD7AF),  # Hangul Syllables（同理：音节优先于字母）
    (0x3130, 0x318F),  # Hangul Compatibility Jamo
    (0x1100, 0x11FF),  # Hangul Jamo
)
# 符号页范围：Noto Emoji 除 1F300+ 表情外还带一批 BMP 符号（♥ ⚔ ✨ ⭐ ☟ ❤ 等），
# 一并收进来，避免这些常见符号落在 MTSDF 之外。
SYMBOL_RANGES: tuple[tuple[int, int], ...] = (
    (0x2000, 0x2BFF),  # 标点 / 箭头 / 数学 / 杂项符号 / 装饰符
    (0x1F000, 0x1FAFF),  # 麻将 / 多米诺 / 扑克 / 表情与图形符号各段
)


def gb2312_han_order() -> list[int]:
    """GB2312 汉字序：一级字表（3755 常用）→ 二级字表（3008 次常用）。

    这是「常用优先」的离线依据。按码位升序取字会把 U+4E00..U+6F3F 当成常用段，
    而「的」(U+7684)、茶(0x8336)、空(0x7A7A)、爱(0x7231)、球(0x7403) 全在截断线
    之上——最常用的字反而缺席，实测项目自身简中语料只覆盖 59.4%。
    """
    order: list[int] = []
    for high in range(0xB0, 0xF8):
        for low in range(0xA1, 0xFF):
            try:
                text = bytes((high, low)).decode("gb2312")
            except UnicodeDecodeError:
                continue
            if len(text) == 1:
                order.append(ord(text))
    return order


def ksx1001_hangul_order() -> list[int]:
    """KS X 1001 谚文序（2350 个常用音节），作为韩文的常用优先依据。"""
    order: list[int] = []
    for high in range(0xB0, 0xC9):
        for low in range(0xA1, 0xFF):
            try:
                text = bytes((high, low)).decode("euc_kr")
            except UnicodeDecodeError:
                continue
            if len(text) == 1:
                order.append(ord(text))
    return order


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
    page_size: int = 0  # 图集边长；0 = 用 --cjk-size
    # 图集是 RGBA8、无 mipmap、加载时整页上传显存，所以边长直接等于显存成本：
    # 4096² = 64 MiB，2048² = 16 MiB。字形少的脚本必须显式降尺寸，否则一页
    # 泰文（87 字形、填充率 1.2%）也要占掉 64 MiB。


def _han_tiers() -> tuple[list[int], ...]:
    return (gb2312_han_order(),)


def _hangul_tiers() -> tuple[list[int], ...]:
    return (ksx1001_hangul_order(),)


def _no_tiers() -> tuple[list[int], ...]:
    return ()


# 假名、CJK 标点与全角形式由 nameplate_noto_glow_jp（Glow Sans J）提供，这里不重烤。
# 预算是显式取舍：全量汉字 27558 字按 4096²/约 1900 字每页要 15 页（约 165MB），与
# 「体积成本」冲突。改为常用优先后，4 页即覆盖项目自身简中语料 1125/1126 = 99.9%
# （旧的码位升序只覆盖 669/1126 = 59.4%），这里给到 6 页，余量留给玩家昵称里的
# 中频字。符号页字形复杂、页数少，用更小的分片换并行。
DEFAULT_FALLBACK_CHUNK = 1800
FALLBACK_SCRIPTS: tuple[SFallbackScript, ...] = (
    SFallbackScript(
        "cn", "nameplate_noto_glow_cn", "NotoSansSC-VF.ttf", 0, HAN_RANGES, _han_tiers, 6 * 1800, 1800, 4096
    ),
    SFallbackScript(
        "kr", "nameplate_noto_glow_kr", "SourceHanSans.ttc", 1, HANGUL_RANGES, _hangul_tiers, 2 * 1900, 1900, 4096
    ),
    # 泰文只有 87 个字形，4096² 的填充率是 1.2%（实测占用 2627x645）；2048² 能装下
    # 且填充率约 4.8%，直接省 48 MiB 显存，覆盖范围一点不少。
    SFallbackScript("thai", "nameplate_noto_glow_thai", "NotoSansThai-Regular.ttf", 0, THAI_RANGES, _no_tiers, 0, 0, 2048),
    # 符号页字形复杂、单个字形大：1415 个字形实测在 4096² 里只占约 3745 行高，
    # 原来的 800/片会裂成两页（各 64 MiB）。改成整块一片即可装下，省 64 MiB。
    SFallbackScript("emoji", "nameplate_noto_glow_emoji", "NotoEmoji-Regular.ttf", 0, SYMBOL_RANGES, _no_tiers, 0, 0, 4096),
)
FALLBACK_PROFILE = "noto_glow_cjk"
# profile 里的页顺序：先加载的页在重码时优先（m_Glyphs.emplace 不覆盖），顺序固定便于复现。
FALLBACK_PAGE_ORDER = ("_cn_", "_jp", "_kr_", "_thai_", "_emoji_")


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
    parser.add_argument(
        "--fallback-scripts-only",
        action="store_true",
        help="只重建 noto_glow_cjk 兜底页（汉字/谚文/泰文/emoji，页名 nameplate_noto_glow_<脚本>_XX）",
    )
    parser.add_argument(
        "--fallback-only-script",
        action="append",
        metavar="TAG",
        help="只烘焙指定脚本的兜底页（cn/kr/thai/emoji），可重复；省略表示全部",
    )
    parser.add_argument(
        "--skip-profile-write",
        action="store_true",
        help="烘焙完不写 profile 清单（并行跑多个脚本时用，最后单独跑一次 --profile-only 汇总）",
    )
    parser.add_argument(
        "--profile-only",
        action="store_true",
        help="不烘焙，只按已落盘的页重写 noto_glow_cjk profile 清单",
    )
    parser.add_argument(
        "--fallback-chunk",
        type=int,
        default=DEFAULT_FALLBACK_CHUNK,
        help=f"兜底页每片字形数（默认 {DEFAULT_FALLBACK_CHUNK}）；4096 图集实测汉字容量 1859~1952 浮动，取 1800 避免溢出",
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


def run_tool_soft(
    args: argparse.Namespace,
    font: Path,
    face_index: int,
    codepoints: list[int],
    prefix: Path,
    size: int,
) -> tuple[dict, list[int]]:
    """运行自建生成工具；返回 (manifest, 未打包的码点)。

    与 run_tool 的区别：工具装不下时只会静默截断并返回 0，这里把缺失码点回报给调用方
    自己决定拆分，而不是直接终止整批烘焙。
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

    装不下时**不再对半拆**：对半拆会把「只差几十个字」的片裂成两页，实测 6 页变 10 页
    （约 110MB）。溢出的码点由调用方汇总后追加一页即可。
    """
    prefix = temp_dir / tag
    manifest, missing = run_tool_soft(args, font, face_index, codepoints, prefix, size)
    if not manifest["glyphs"]:
        raise SystemExit(f"atlas page cannot hold U+{codepoints[0]:04X}")
    return (prefix, manifest), missing


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
    jp_page = out_dir / "nameplate_noto_glow_jp.json"
    if jp_page.is_file():
        found.add(jp_page)
    return [f"qmclient/nameplate_msdf/{path.name}" for path in sorted(found, key=fallback_page_rank)]


def bake_fallback_scripts(args: argparse.Namespace, fonts_dir: Path) -> int:
    """重建 noto_glow_cjk 兜底页：每个脚本一组页，每组只用一种随包字体。

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
        page_size = entry.page_size if entry.page_size > 0 else args.cjk_size
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
                page = publish_page(args, prefix, manifest, image_name, font.stem, args.output, font, entry.face)
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

    fonts_dir = args.data_root / "fonts"

    # 只汇总 profile：并行烘焙各脚本后单独跑一次，避免多个进程同时写同一份清单。
    if args.profile_only:
        refs = collect_fallback_pages(args.output)
        write_profile_manifest(args.output, FALLBACK_PROFILE, refs)
        print(f"profile: {FALLBACK_PROFILE}, {len(refs)} page(s) -> {args.output}")
        return 0

    # 兜底页模式只依赖 data/fonts 下的随包字体，不碰其它 profile 页。
    if args.fallback_scripts_only:
        return bake_fallback_scripts(args, fonts_dir)

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
