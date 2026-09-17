#!/usr/bin/env python3
"""Generate published MTSDF profiles for the selected bundled fonts only."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

from fontTools.ttLib import TTFont


RANGES = (
    (0x0020, 0x024F),  # English, German and Latin extensions
    (0x0370, 0x03FF),  # Greek used by names/symbols
    (0x0400, 0x052F),  # Russian and Cyrillic extensions
    (0x2000, 0x206F),  # punctuation
    (0x20A0, 0x20CF),  # currency
    (0x2100, 0x214F),
    (0x2190, 0x21FF),
    (0x2500, 0x257F),
    (0x2600, 0x27BF),  # common symbols
    (0x3000, 0x303F),  # CJK punctuation
    (0x3040, 0x30FF),  # Japanese kana
    (0x31F0, 0x31FF),
    (0x3400, 0x4DBF),  # CJK extension A
    (0x4E00, 0x9FFF),  # CJK unified ideographs
    (0xAC00, 0xD7AF),  # Korean Hangul
    (0xF900, 0xFAFF),  # compatibility ideographs
    (0xFF00, 0xFFEF),  # fullwidth forms
)

LATIN_RANGES = ((0x20, 0x24F), (0x370, 0x3FF), (0x400, 0x52F), (0x2000, 0x27BF))
# 注意：CJK_RANGES 含 2 万多个汉字，只适合「汉字兜底页」那种有页数预算的用途。
# 用在 profile 页上会一次烤出十几页（每页 4096² = 64 MiB 显存），见 KANA_RANGES 的说明。
CJK_RANGES = ((0x3000, 0x303F), (0x3040, 0x30FF), (0x31F0, 0x31FF), (0x3400, 0x4DBF), (0x4E00, 0x9FFF), (0xAC00, 0xD7AF), (0xF900, 0xFAFF), (0xFF00, 0xFFEF))
# glow_sans_j 只负责假名 / CJK 标点 / 全角形式：汉字由 noto_glow_cn 兜底页提供
# （见 qm_nameplate_msdf_build.py 的 FALLBACK_SCRIPTS 注释）。用 CJK_RANGES 会连
# 2.8 万个汉字一起烤成 16 页（约 1 GB 显存），与已发布产物（425 字形）完全不符。
KANA_RANGES = ((0x3000, 0x303F), (0x3040, 0x30FF), (0x31F0, 0x31FF), (0xFF00, 0xFFEF))
GENERATOR = "msdf-atlas-gen@6148900d59423059bafde2f51a0cb303184404bd"

# 图集是 RGBA8、无 mipmap、加载时整页上传显存，所以边长直接等于显存成本：
# 4096² = 64 MiB、2048² = 16 MiB、1024² = 4 MiB。同一个 profile 的字形越集中，
# 常驻显存越低；能一页装下就不要拆成两页。
PAGE_SIZE_LADDER = (1024, 2048, 4096)
# 估算起始尺寸用的每字形预算（px²，含打包空隙）。只用来决定「从哪一档开始试」，
# 试错失败会自动往上走，所以偏保守只会多花一次烘焙时间，不会影响正确性。
GLYPH_AREA_BUDGET = 5000
# 打包器是扫描线填充，同一行内的字形会填得比较满，按这个系数估算所需边长。
PACK_EFFICIENCY = 0.85
FONTS = (
    ("dejavu", "data/fonts/DejaVuSans.ttf", "DejaVu Sans", LATIN_RANGES),
    ("cabin", "data/qmclient/fonts/Cabin-Regular.ttf", "Cabin", LATIN_RANGES),
    ("freesans", "data/qmclient/fonts/FreeSans-Bold.ttf", "FreeSans Bold", LATIN_RANGES),
    ("google_sans", "data/qmclient/fonts/GoogleSans-Regular.ttf", "Google Sans", LATIN_RANGES),
    ("inter_regular", "data/qmclient/fonts/Inter/Inter_24pt-Regular.ttf", "Inter", LATIN_RANGES),
    ("inter_semibold", "data/qmclient/fonts/Inter/Inter_24pt-SemiBold.ttf", "Inter SemiBold", LATIN_RANGES),
    ("maple_mono_regular", "data/qmclient/fonts/Maple Mono Normal/MapleMonoNormal-CN-Regular.ttf", "Maple Mono Normal", LATIN_RANGES),
    ("maple_mono_medium", "data/qmclient/fonts/Maple Mono Normal/MapleMonoNormal-CN-Medium.ttf", "Maple Mono Normal Medium", LATIN_RANGES),
    ("maple_mono_bold", "data/qmclient/fonts/Maple Mono Normal/MapleMonoNormal-CN-Bold.ttf", "Maple Mono Normal Bold", LATIN_RANGES),
    ("minecraft", "data/qmclient/fonts/Minecraft.ttf", "Minecraft", LATIN_RANGES),
    ("montserrat", "data/qmclient/fonts/Montserrat-Regular.ttf", "Montserrat", LATIN_RANGES),
    ("nunito", "data/qmclient/fonts/Nunito-Black.ttf", "Nunito Black", LATIN_RANGES),
    ("poppins_regular", "data/qmclient/fonts/Poppins/Poppins-Regular.ttf", "Poppins", LATIN_RANGES),
    ("poppins_medium", "data/qmclient/fonts/Poppins/Poppins-Medium.ttf", "Poppins Medium", LATIN_RANGES),
    ("poppins_bold", "data/qmclient/fonts/Poppins/Poppins-Bold.ttf", "Poppins Bold", LATIN_RANGES),
    ("rubik", "data/qmclient/fonts/Rubik-Regular.ttf", "Rubik", LATIN_RANGES),
    ("times_new_roman", "data/qmclient/fonts/Times New Roman.TTF", "Times New Roman", LATIN_RANGES),
    ("glow_sans_j", "data/fonts/GlowSansJ-Compressed-Book.otf", "Glow Sans J Compressed Book", KANA_RANGES),
    # 下面三个条目目前**没有已发布的页、也没有 profile 引用它们**（脚本只写过
    # glow_sans_j 与 Latin/Phosphor 页）。留着是为了让「随包字体覆盖表」完整，
    # 但直接跑会把 noto_sans_sc 按 CJK_RANGES 烤成十几页孤儿产物——真要用它们，
    # 请先给 noto_sans_sc 加上页数预算（参照 qm_nameplate_msdf_build.py 的
    # PROFILE_CJK_LIMITS），并让某个 profile 引用产出的页。
    ("noto_sans_sc", "data/fonts/NotoSansSC-VF.ttf", "Noto Sans SC", CJK_RANGES),
    ("noto_emoji", "data/fonts/NotoEmoji-Regular.ttf", "Noto Emoji", ((0x1F300, 0x1FAFF),)),
    ("noto_thai", "data/fonts/NotoSansThai-Regular.ttf", "Noto Sans Thai", ((0x0E00, 0x0E7F),)),
    # Phosphor is bundled under qmclient/fonts and is intentionally retained.
    ("phosphor_regular", "data/qmclient/fonts/Phosphor/Phosphor-Regular.ttf", "Phosphor", ((0xE000, 0xF8FF),)),
    ("phosphor_bold", "data/qmclient/fonts/Phosphor/Phosphor-Bold.ttf", "Phosphor Bold", ((0xE000, 0xF8FF),)),
    ("phosphor_duotone", "data/qmclient/fonts/Phosphor/Phosphor-Duotone.ttf", "Phosphor Duotone", ((0xE000, 0xF8FF),)),
    ("phosphor_fill", "data/qmclient/fonts/Phosphor/Phosphor-Fill.ttf", "Phosphor Fill", ((0xE000, 0xF8FF),)),
    ("phosphor_light", "data/qmclient/fonts/Phosphor/Phosphor-Light.ttf", "Phosphor Light", ((0xE000, 0xF8FF),)),
)


def coverage(path: Path) -> set[int]:
    font = TTFont(str(path))
    return set().union(*[set(table.cmap) for table in font["cmap"].tables])


def target_codepoints(cmap: set[int], ranges) -> list[int]:
    return [cp for lo, hi in ranges for cp in range(lo, hi + 1) if cp in cmap]


def write_charset(path: Path, codepoints: list[int]) -> None:
    # 官方解析器支持十六进制数值，避免引号、反斜杠等 Unicode 字符破坏字符串语法。
    path.write_text("\n".join(f"0x{cp:X}" for cp in codepoints) + "\n", encoding="utf-8")


def run_page(adapter: Path, tool: Path, font: Path, name: str, codepoints: list[int], output: Path, width: int, height: int) -> None:
    with tempfile.TemporaryDirectory(prefix="qm-msdf-page-") as temp:
        temp = Path(temp)
        charset = temp / "charset.txt"
        write_charset(charset, codepoints)
        subprocess.run([
            sys.executable, str(adapter), "--tool", str(tool), "--font", str(font),
            "--charset", str(charset), "--output", str(temp / "page.json"),
            "--image", str(temp / "page.png"), "--font-name", name,
            "--size", "64", "--px-range", "8", "--padding", "9",
            "--width", str(width), "--height", str(height),
        ], check=True)
        manifest = json.loads((temp / "page.json").read_text(encoding="utf-8"))
        stem = output.stem
        output.parent.mkdir(parents=True, exist_ok=True)
        (output.parent / f"{stem}.png").write_bytes((temp / "page.png").read_bytes())
        manifest["atlas"]["image"] = f"qmclient/nameplate_msdf/{stem}.png"
        output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"  {stem}: {len(manifest['glyphs'])} glyphs @ {width}px")


def starting_size(codepoint_count: int) -> int:
    """按每字形预算估算起始边长：偏小只会浪费一次烘焙，偏大会多占显存。"""
    needed = codepoint_count * GLYPH_AREA_BUDGET / PACK_EFFICIENCY
    for size in PAGE_SIZE_LADDER:
        if size * size >= needed:
            return size
    return PAGE_SIZE_LADDER[-1]


def bake_best_fit(
    adapter: Path,
    tool: Path,
    font: Path,
    label: str,
    codepoints: list[int],
    output: Path,
) -> int | None:
    """从估算尺寸起逐档放大，用第一个装得下的边长烤一页，返回实际边长；都装不下返回 None。

    官方生成器装不下时**不会静默截断**：它会打印 "Could not fit N out of M glyphs"
    并以非 0 退出（无轮廓的字形如空格才会被静默跳过）。所以「试小尺寸、失败就放大」
    是可靠的，而且这是唯一能同时压住显存与页数的做法：同一个 profile 的字形集中在
    一页里，比按固定片大小切开要省一整页（4096² 一页就是 64 MiB）。
    """
    ladder = [size for size in PAGE_SIZE_LADDER if size >= starting_size(len(codepoints))]
    for size in ladder:
        try:
            run_page(adapter, tool, font, label, codepoints, output, size, size)
        except subprocess.CalledProcessError:
            print(f"  {output.stem}: {len(codepoints)} codepoints do not fit {size}px, trying larger")
            continue
        return size
    return None


def bake_profile_pages(
    adapter: Path,
    args: argparse.Namespace,
    font: Path,
    label: str,
    profile: str,
    codepoints: list[int],
) -> list[str]:
    """烤一个 profile 的页：优先整批一页，装不下才按 --chunk 拆。

    dejavu 的 2294 个字形实测能装进一页 4096²（填充率 63.5%），但按 1800 分片会被
    切成两页、白占 64 MiB 常驻显存——名牌的兜底链每次都加载 dejavu，这份浪费是
    每个用户都要付的。
    """
    stems: list[str] = []
    if not args.no_merge and len(codepoints) > args.chunk:
        stem = f"nameplate_{profile}_00"
        page = args.output / f"{stem}.json"
        if bake_best_fit(adapter, args.tool, font, label, codepoints, page) is not None:
            print(f"  {profile}: whole set packed into a single page")
            return [f"qmclient/nameplate_msdf/{stem}.json"]
        print(f"  {profile}: {len(codepoints)} codepoints do not fit one page, splitting by {args.chunk}")

    for index in range(0, len(codepoints), args.chunk):
        stem = f"nameplate_{profile}_{index // args.chunk:02d}"
        page = args.output / f"{stem}.json"
        if bake_best_fit(adapter, args.tool, font, label, codepoints[index:index + args.chunk], page) is None:
            raise SystemExit(
                f"no atlas size in {PAGE_SIZE_LADDER} can hold {len(codepoints[index:index + args.chunk])} "
                f"codepoints for {page.name}; lower --chunk"
            )
        stems.append(stem)
    return [f"qmclient/nameplate_msdf/{name}.json" for name in stems]


def prune_stale_pages(out_dir: Path, profile: str, keep: list[str]) -> None:
    """删掉同一 profile 前缀下、本次没有产出的旧页（页数变少时尤其重要）。"""
    keep_names = {Path(ref).stem for ref in keep}
    for path in sorted(out_dir.glob(f"nameplate_{profile}_*")):
        if path.suffix not in (".json", ".png") or path.stem in keep_names:
            continue
        path.unlink()
        print(f"  removed stale page: {path.name}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("data/qmclient/nameplate_msdf"))
    parser.add_argument("--chunk", type=int, default=1800)
    parser.add_argument(
        "--no-merge",
        action="store_true",
        help="不做「先整批试一页」的合并（调试用；合并能省下一整页 64 MiB 显存）",
    )
    parser.add_argument("--only", action="append", default=[], help="Generate only the named profile(s)")
    args = parser.parse_args()
    adapter = Path(__file__).with_name("qm_nameplate_msdf_official.py")
    profiles = args.output / "profiles"
    for profile, filename, label, ranges in FONTS:
        if args.only and profile not in args.only:
            continue
        font = Path(filename)
        if not font.is_file():
            print(f"skip missing bundled font: {font}")
            continue
        cps = target_codepoints(coverage(font), ranges)
        if not cps:
            print(f"skip {label}: no requested codepoints")
            continue
        print(f"{label}: {len(cps)} requested codepoints")
        pages = bake_profile_pages(adapter, args, font, label, profile, cps)
        prune_stale_pages(args.output, profile, pages)
        profiles.mkdir(parents=True, exist_ok=True)
        (profiles / f"nameplate_{profile}.json").write_text(json.dumps({
            "version": 1,
            "profile": profile,
            "generator": GENERATOR,
            "pages": pages,
        }, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
