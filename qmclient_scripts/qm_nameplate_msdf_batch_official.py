#!/usr/bin/env python3
"""Generate published MTSDF profiles for the selected bundled fonts only."""

from __future__ import annotations

import argparse
import json
import subprocess
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
CJK_RANGES = ((0x3000, 0x303F), (0x3040, 0x30FF), (0x31F0, 0x31FF), (0x3400, 0x4DBF), (0x4E00, 0x9FFF), (0xAC00, 0xD7AF), (0xF900, 0xFAFF), (0xFF00, 0xFFEF))
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
    ("glow_sans_j", "data/fonts/GlowSansJ-Compressed-Book.otf", "Glow Sans J Compressed Book", CJK_RANGES),
    ("noto_sans_sc", "data/fonts/NotoSansSC-VF.ttf", "Noto Sans SC", CJK_RANGES),
    ("lxgw_wenkai_regular", "data/qmclient/fonts/霞鹜文楷/LXGWWenKai-Regular.ttf", "LXGW WenKai", CJK_RANGES),
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
            "python", str(adapter), "--tool", str(tool), "--font", str(font),
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
        print(f"  {stem}: {len(manifest['glyphs'])} glyphs")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("data/qmclient/nameplate_msdf"))
    parser.add_argument("--chunk", type=int, default=1800)
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
        pages = []
        print(f"{label}: {len(cps)} requested codepoints")
        for index in range(0, len(cps), args.chunk):
            stem = f"nameplate_{profile}_{index // args.chunk:02d}"
            page = args.output / f"{stem}.json"
            run_page(adapter, args.tool, font, label, cps[index:index + args.chunk], page, 4096, 4096)
            pages.append(f"qmclient/nameplate_msdf/{stem}.json")
        profiles.mkdir(parents=True, exist_ok=True)
        (profiles / f"nameplate_{profile}.json").write_text(json.dumps({
            "version": 1,
            "profile": profile,
            "generator": "msdf-atlas-gen@6148900d59423059bafde2f51a0cb303184404bd",
            "pages": pages,
        }, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
