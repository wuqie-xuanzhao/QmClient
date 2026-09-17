#!/usr/bin/env python3
"""Build deterministic QmClient MSDF icon atlases from one or more SVG sources."""

from __future__ import annotations

import argparse
import json
import math
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path


ICON_ALIASES = {"x": "close"}
FIELD_SIZE = 48
PX_RANGE = 6
PADDING = 8
CELL_SIZE = FIELD_SIZE + PADDING * 2


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, action="append", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--atlas-name", required=True)
    return parser.parse_args()


def icon_id(path: Path) -> str:
    stem = path.stem.removeprefix("icon-")
    return ICON_ALIASES.get(stem, stem)


def collect_svg_files(source_dirs: list[Path]) -> list[Path]:
    svg_files = sorted(
        (svg for source_dir in source_dirs for svg in source_dir.glob("*.svg")),
        key=lambda svg: svg.name,
    )
    if not svg_files:
        raise SystemExit(f"No SVG files found in {', '.join(str(source_dir) for source_dir in source_dirs)}")

    icon_names: set[str] = set()
    for svg in svg_files:
        name = icon_id(svg)
        if name in icon_names:
            raise SystemExit(f"Duplicate icon name: {name}")
        icon_names.add(name)
    return svg_files


def requires_raster_sdf(source: Path) -> bool:
    # Duotone SVGs may contain compound path commands that msdfgen cannot
    # robustly classify; flatten them through the raster SDF fallback so the
    # complete style remains loadable instead of aborting atlas generation.
    if "phosphor_duotone" in source.parts:
        return True
    root = ET.parse(source).getroot()
    path_count = 0
    for element in root.iter():
        tag = element.tag.rsplit("}", 1)[-1]
        if tag not in {"path", "circle", "ellipse", "line", "polygon", "polyline", "rect"}:
            continue
        if tag != "path" or element.attrib.get("stroke") not in (None, "none") or element.attrib.get("fill") == "none":
            return True
        path_count += 1
    return path_count != 1


def distance_transform_1d(values: list[float]) -> list[float]:
    count = len(values)
    finite_positions = [position for position, value in enumerate(values) if math.isfinite(value)]
    if not finite_positions:
        return [float("inf")] * count

    sites = [0] * count
    boundaries = [0.0] * (count + 1)
    result = [0.0] * count
    site_count = 0
    sites[0] = finite_positions[0]
    boundaries[0] = float("-inf")
    boundaries[1] = float("inf")
    for position in finite_positions[1:]:
        intersection = ((values[position] + position * position) - (values[sites[site_count]] + sites[site_count] * sites[site_count])) / (2.0 * (position - sites[site_count]))
        while intersection <= boundaries[site_count]:
            site_count -= 1
            intersection = ((values[position] + position * position) - (values[sites[site_count]] + sites[site_count] * sites[site_count])) / (2.0 * (position - sites[site_count]))
        site_count += 1
        sites[site_count] = position
        boundaries[site_count] = intersection
        boundaries[site_count + 1] = float("inf")

    site_count = 0
    for position in range(count):
        while boundaries[site_count + 1] < position:
            site_count += 1
        distance = position - sites[site_count]
        result[position] = distance * distance + values[sites[site_count]]
    return result


def distance_transform(mask: list[bool], width: int, height: int, feature: bool) -> list[float]:
    infinity = float(width * width + height * height)
    rows = [0.0] * (width * height)
    for y in range(height):
        values = [0.0 if mask[y * width + x] == feature else infinity for x in range(width)]
        rows[y * width : (y + 1) * width] = distance_transform_1d(values)

    result = [0.0] * (width * height)
    for x in range(width):
        values = [rows[y * width + x] for y in range(height)]
        transformed = distance_transform_1d(values)
        for y, value in enumerate(transformed):
            result[y * width + x] = value
    return result


def _raster_to_sdf(raster: Path, *, required: bool) -> "Image.Image":
    """把一层的光栅覆盖转成 0.5 + signed/PX_RANGE 编码的距离场（单通道 L）。

    required=True 时空覆盖视为生成失败；否则返回全 0（等价于「远离字形」），
    因为部分 Phosphor duotone 图标本身就没有 secondary 层。
    """
    from PIL import Image

    alpha = Image.open(raster).convert("RGBA").getchannel("A")
    mask = [value >= 128 for value in alpha.get_flattened_data()]
    if not any(mask):
        if required:
            raise SystemExit(f"Unable to create SDF from {raster}")
        return Image.new("L", (FIELD_SIZE, FIELD_SIZE), 0)
    if all(mask):
        return Image.new("L", (FIELD_SIZE, FIELD_SIZE), 255)

    distance_to_inside = distance_transform(mask, FIELD_SIZE, FIELD_SIZE, True)
    distance_to_outside = distance_transform(mask, FIELD_SIZE, FIELD_SIZE, False)
    field = Image.new("L", (FIELD_SIZE, FIELD_SIZE))
    pixels = field.load()
    for y in range(FIELD_SIZE):
        for x in range(FIELD_SIZE):
            index = y * FIELD_SIZE + x
            signed_distance = math.sqrt(distance_to_outside[index]) - math.sqrt(distance_to_inside[index])
            pixels[x, y] = round(255.0 * max(0.0, min(1.0, 0.5 + signed_distance / PX_RANGE)))
    return field


def render_raster_sdf(source: Path, output: Path) -> None:
    from PIL import Image

    try:
        from qmclient_scripts.qm_build_icon_atlas import _render_svg_fallback
    except ModuleNotFoundError:
        from qm_build_icon_atlas import _render_svg_fallback

    raster_path = output.with_name(f"{output.stem}-source.png")
    _render_svg_fallback(source, raster_path, FIELD_SIZE)
    field = _raster_to_sdf(raster_path, required=True)
    # 单层图集沿用 MTSDF 约定：RGB 与 Alpha 都是同一条真 SDF。
    Image.merge("RGBA", (field, field, field, field)).save(output)
    raster_path.unlink()


def _write_layer_svg(source: Path, output: Path, *, keep_secondary: bool) -> None:
    """把 duotone SVG 拆成单层：secondary 是 opacity < 0.5 的填充层，其余为 primary。

    primary 必须单独渲染。若直接用两层并集当 primary，secondary 区域会同时落在
    primary 覆盖里，着色器算出的 Opacity 恒为 1，secondary 颜色永远显示不出来。
    """
    root = ET.parse(source).getroot()
    for element in list(root):
        opacity = float(element.attrib.get("opacity", "1"))
        if (opacity < 0.5) != keep_secondary:
            root.remove(element)
    ET.ElementTree(root).write(output, encoding="unicode")


def render_duotone_field(source: Path, output: Path) -> None:
    """Build primary-layer distance field in RGB plus secondary-layer distance field in Alpha.

    Phosphor duotone 用 opacity="0.2" 标注 secondary 填充层。两层各自是独立距离场，
    共享同一 PX_RANGE，因此着色器可以用同一个 ScreenPxRange 解码两者，
    在任意缩放尺寸下都保持锐利边缘（而不是把光栅覆盖当遮罩用）。
    """
    from PIL import Image

    try:
        from qmclient_scripts.qm_build_icon_atlas import _render_svg_fallback
    except ModuleNotFoundError:
        from qm_build_icon_atlas import _render_svg_fallback

    primary_svg = output.with_suffix(".primary.svg")
    secondary_svg = output.with_suffix(".secondary.svg")
    primary_raster = output.with_name(f"{output.stem}-primary.png")
    secondary_raster = output.with_name(f"{output.stem}-secondary.png")
    _write_layer_svg(source, primary_svg, keep_secondary=False)
    _write_layer_svg(source, secondary_svg, keep_secondary=True)
    _render_svg_fallback(primary_svg, primary_raster, FIELD_SIZE)
    _render_svg_fallback(secondary_svg, secondary_raster, FIELD_SIZE)

    primary_field = _raster_to_sdf(primary_raster, required=True)
    secondary_field = _raster_to_sdf(secondary_raster, required=False)
    Image.merge("RGBA", (primary_field, primary_field, primary_field, secondary_field)).save(output)

    for path in (primary_svg, secondary_svg, primary_raster, secondary_raster):
        path.unlink()


def main() -> int:
    args = parse_args()
    msdfgen = shutil.which("msdfgen")
    if msdfgen is None:
        raise SystemExit("msdfgen is required to build QmClient MSDF icon atlases")

    try:
        from PIL import Image
    except ImportError as exc:
        raise SystemExit("Pillow is required to compose the QmClient MSDF icon atlas") from exc

    svg_files = collect_svg_files(args.source)

    columns = math.ceil(math.sqrt(len(svg_files)))
    rows = math.ceil(len(svg_files) / columns)
    atlas_width = columns * CELL_SIZE
    atlas_height = rows * CELL_SIZE
    # The graphics upload path uses RGBA while the MSDF distance field remains in RGB.
    atlas = Image.new("RGBA", (atlas_width, atlas_height), (0, 0, 0, 255))
    icons: dict[str, dict[str, int]] = {}

    with tempfile.TemporaryDirectory(prefix="qm-icons-msdf-") as temp:
        temp_dir = Path(temp)
        for index, svg in enumerate(svg_files):
            field_path = temp_dir / f"{svg.stem}.png"
            if "phosphor_duotone" in svg.parts:
                render_duotone_field(svg, field_path)
            elif requires_raster_sdf(svg):
                render_raster_sdf(svg, field_path)
            else:
                subprocess.run(
                    [
                        msdfgen,
                        "mtsdf",
                        "-svg",
                        str(svg),
                        "-dimensions",
                        str(FIELD_SIZE),
                        str(FIELD_SIZE),
                        "-pxrange",
                        str(PX_RANGE),
                        "-autoframe",
                        "-coloringstrategy",
                        "inktrap",
                        "-guesswinding",
                        "-o",
                        str(field_path),
                    ],
                    check=True,
                )
            # 保留 MTSDF 的 Alpha 真 SDF；丢弃 Alpha 会把资源退化成普通 MSDF。
            field = Image.open(field_path).convert("RGBA")
            if field.size != (FIELD_SIZE, FIELD_SIZE):
                raise SystemExit(f"msdfgen returned unexpected size for {svg}")

            column = index % columns
            row = index // columns
            x = column * CELL_SIZE + PADDING
            y = row * CELL_SIZE + PADDING
            atlas.paste(field, (x, y))
            icons[icon_id(svg)] = {"x": x, "y": y, "w": FIELD_SIZE, "h": FIELD_SIZE}

    args.output.mkdir(parents=True, exist_ok=True)
    image_name = f"{args.atlas_name}_msdf.png"
    manifest_name = f"{args.atlas_name}_msdf.json"
    atlas.save(args.output / image_name)
    manifest = {
        "version": 2,
        "kind": "mtsdf",
        "distance_field": "mtsdf",
        "alpha_sdf": True,
        "px_range": PX_RANGE,
        "source": "QmClient SVG sources generated with msdfgen MTSDF",
        "atlas": {
            "image": f"qmclient/icons/{image_name}",
            "width": atlas_width,
            "height": atlas_height,
            "padding": PADDING,
        },
        "icons": icons,
    }
    if "duotone" in args.atlas_name:
        manifest["secondary_mask"] = "alpha"
    (args.output / manifest_name).write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
