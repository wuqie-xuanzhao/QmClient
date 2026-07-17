# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""
Build QmClient Tabler icon PNG atlases.

This script is intentionally build-time only: runtime code loads the generated
PNG atlas and JSON manifest, never SVG. It expects source SVG files in
datasrc/qm_icons/tabler and writes data/qmclient/icons/qm_icons_{scale}x.*
"""

from __future__ import annotations

import argparse
import json
import math
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path


ICON_ALIASES = {
    "x": "close",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sizes", type=int, nargs="+", default=[1, 2, 4])
    parser.add_argument("--base-size", type=int, default=24)
    parser.add_argument("--padding", type=int, default=4)
    return parser.parse_args()


def find_renderer() -> str | None:
    for name in ("resvg", "inkscape", "magick"):
        path = shutil.which(name)
        if path:
            return path
    return None


def _float_attr(node: ET.Element, name: str, default: float = 0.0) -> float:
    value = node.attrib.get(name)
    if value is None:
        return default
    try:
        return float(value)
    except ValueError:
        return default


def _parse_points(points: str) -> list[tuple[float, float]]:
    values = [float(part) for part in re.split(r"[\s,]+", points.strip()) if part]
    return list(zip(values[0::2], values[1::2]))


_PATH_TOKEN_RE = re.compile(r"[AaHhLlMmVvZz]|[-+]?(?:\d*\.\d+|\d+\.?)(?:[eE][-+]?\d+)?")


def _vector_angle(ux: float, uy: float, vx: float, vy: float) -> float:
    dot = ux * vx + uy * vy
    length = math.hypot(ux, uy) * math.hypot(vx, vy)
    if length <= 1e-12:
        return 0.0
    angle = math.acos(max(-1.0, min(1.0, dot / length)))
    return -angle if ux * vy - uy * vx < 0.0 else angle


def _sample_svg_arc(
    start: tuple[float, float],
    rx: float,
    ry: float,
    rotation: float,
    large_arc: bool,
    sweep: bool,
    end: tuple[float, float],
) -> list[tuple[float, float]]:
    x1, y1 = start
    x2, y2 = end
    rx = abs(rx)
    ry = abs(ry)
    if rx <= 1e-12 or ry <= 1e-12 or (x1 == x2 and y1 == y2):
        return [end]

    phi = math.radians(rotation % 360.0)
    cos_phi = math.cos(phi)
    sin_phi = math.sin(phi)
    dx = (x1 - x2) * 0.5
    dy = (y1 - y2) * 0.5
    x1p = cos_phi * dx + sin_phi * dy
    y1p = -sin_phi * dx + cos_phi * dy

    radius_scale = x1p * x1p / (rx * rx) + y1p * y1p / (ry * ry)
    if radius_scale > 1.0:
        scale = math.sqrt(radius_scale)
        rx *= scale
        ry *= scale

    numerator = max(
        0.0,
        rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p,
    )
    denominator = rx * rx * y1p * y1p + ry * ry * x1p * x1p
    coefficient = 0.0
    if denominator > 1e-12:
        coefficient = math.sqrt(numerator / denominator)
        if large_arc == sweep:
            coefficient = -coefficient

    cxp = coefficient * rx * y1p / ry
    cyp = -coefficient * ry * x1p / rx
    cx = cos_phi * cxp - sin_phi * cyp + (x1 + x2) * 0.5
    cy = sin_phi * cxp + cos_phi * cyp + (y1 + y2) * 0.5

    ux = (x1p - cxp) / rx
    uy = (y1p - cyp) / ry
    vx = (-x1p - cxp) / rx
    vy = (-y1p - cyp) / ry
    start_angle = _vector_angle(1.0, 0.0, ux, uy)
    delta_angle = _vector_angle(ux, uy, vx, vy)
    if not sweep and delta_angle > 0.0:
        delta_angle -= 2.0 * math.pi
    elif sweep and delta_angle < 0.0:
        delta_angle += 2.0 * math.pi

    segments = max(2, math.ceil(abs(delta_angle) / (math.pi / 16.0)))
    points: list[tuple[float, float]] = []
    for index in range(1, segments + 1):
        angle = start_angle + delta_angle * index / segments
        cos_angle = math.cos(angle)
        sin_angle = math.sin(angle)
        points.append(
            (
                cx + cos_phi * rx * cos_angle - sin_phi * ry * sin_angle,
                cy + sin_phi * rx * cos_angle + cos_phi * ry * sin_angle,
            )
        )
    points[-1] = end
    return points


def _parse_path_polylines(path_data: str) -> list[list[tuple[float, float]]]:
    tokens = _PATH_TOKEN_RE.findall(path_data.replace(",", " "))
    polylines: list[list[tuple[float, float]]] = []
    current_polyline: list[tuple[float, float]] = []
    current = (0.0, 0.0)
    subpath_start = (0.0, 0.0)
    command = ""
    index = 0

    def number() -> float:
        nonlocal index
        if index >= len(tokens) or tokens[index].isalpha():
            raise ValueError(f"Missing path parameter in {path_data!r}")
        value = float(tokens[index])
        index += 1
        return value

    def append(point: tuple[float, float]) -> None:
        nonlocal current
        current = point
        if not current_polyline or current_polyline[-1] != point:
            current_polyline.append(point)

    def flush() -> None:
        nonlocal current_polyline
        if len(current_polyline) >= 2:
            polylines.append(current_polyline)
        current_polyline = []

    while index < len(tokens):
        if tokens[index].isalpha():
            command = tokens[index]
            index += 1
        if not command:
            raise ValueError(f"Path data starts without command: {path_data!r}")

        relative = command.islower()
        upper = command.upper()
        if upper == "M":
            x = number()
            y = number()
            if relative:
                x += current[0]
                y += current[1]
            flush()
            current = (x, y)
            subpath_start = current
            current_polyline = [current]
            command = "l" if relative else "L"
        elif upper == "L":
            x = number()
            y = number()
            if relative:
                x += current[0]
                y += current[1]
            append((x, y))
        elif upper == "H":
            x = number()
            if relative:
                x += current[0]
            append((x, current[1]))
        elif upper == "V":
            y = number()
            if relative:
                y += current[1]
            append((current[0], y))
        elif upper == "A":
            rx = number()
            ry = number()
            rotation = number()
            large_arc = number() != 0.0
            sweep = number() != 0.0
            x = number()
            y = number()
            if relative:
                x += current[0]
                y += current[1]
            end = (x, y)
            for point in _sample_svg_arc(
                current, rx, ry, rotation, large_arc, sweep, end
            ):
                append(point)
        elif upper == "Z":
            append(subpath_start)
            flush()
            current = subpath_start
            command = ""
        else:
            raise ValueError(f"Unsupported SVG path command {command!r}")

    flush()
    return polylines


def _local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def _draw_round_line(
    draw, points: list[tuple[float, float]], width: int, fill: tuple[int, int, int, int]
) -> None:
    if len(points) < 2:
        return
    draw.line(points, fill=fill, width=width, joint="curve")
    radius = width / 2.0
    for x, y in points:
        draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=fill)


def _render_svg_fallback(source: Path, output: Path, size: int) -> None:
    try:
        from PIL import Image, ImageDraw
    except ImportError as exc:
        raise SystemExit("Pillow is required to render Qm icon SVG fallback") from exc

    root = ET.parse(source).getroot()
    view_box = root.attrib.get("viewBox", "0 0 24 24").split()
    if len(view_box) != 4:
        raise SystemExit(f"Unsupported SVG viewBox in {source}")

    min_x, min_y, view_w, view_h = map(float, view_box)
    oversample = 4
    canvas_size = size * oversample
    scale = canvas_size / max(view_w, view_h)
    stroke_width = max(1, round(float(root.attrib.get("stroke-width", "2")) * scale))
    fill = (255, 255, 255, 255)

    image = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    def sx(value: float) -> float:
        return (value - min_x) * scale

    def sy(value: float) -> float:
        return (value - min_y) * scale

    for node in root.iter():
        tag = _local_name(node.tag)
        if tag in {"svg", "title", "desc"}:
            continue
        if tag == "line":
            points = [
                (sx(_float_attr(node, "x1")), sy(_float_attr(node, "y1"))),
                (sx(_float_attr(node, "x2")), sy(_float_attr(node, "y2"))),
            ]
            _draw_round_line(draw, points, stroke_width, fill)
        elif tag == "polyline":
            points = [
                (sx(x), sy(y)) for x, y in _parse_points(node.attrib.get("points", ""))
            ]
            _draw_round_line(draw, points, stroke_width, fill)
        elif tag == "polygon":
            points = [
                (sx(x), sy(y)) for x, y in _parse_points(node.attrib.get("points", ""))
            ]
            if len(points) >= 2:
                _draw_round_line(draw, points + [points[0]], stroke_width, fill)
        elif tag == "circle":
            cx = sx(_float_attr(node, "cx"))
            cy = sy(_float_attr(node, "cy"))
            radius = _float_attr(node, "r") * scale
            draw.ellipse(
                (cx - radius, cy - radius, cx + radius, cy + radius),
                outline=fill,
                width=stroke_width,
            )
        elif tag == "rect":
            x = sx(_float_attr(node, "x"))
            y = sy(_float_attr(node, "y"))
            w = _float_attr(node, "width") * scale
            h = _float_attr(node, "height") * scale
            radius = _float_attr(node, "rx") * scale
            draw.rounded_rectangle(
                (x, y, x + w, y + h), radius=radius, outline=fill, width=stroke_width
            )
        elif tag == "path":
            path_data = node.attrib.get("d", "")
            if not path_data:
                continue
            try:
                polylines = _parse_path_polylines(path_data)
            except ValueError as exc:
                raise SystemExit(
                    f"Unsupported SVG path data in {source}: {exc}"
                ) from exc
            for points in polylines:
                _draw_round_line(
                    draw,
                    [(sx(x), sy(y)) for x, y in points],
                    stroke_width,
                    fill,
                )
        else:
            raise SystemExit(
                f"Fallback SVG renderer does not support <{tag}> in {source}"
            )

    image = image.resize((size, size), Image.Resampling.LANCZOS)
    image.save(output)


def render_svg(renderer, source: Path, output: Path, size: int) -> None:
    if renderer is None:
        _render_svg_fallback(source, output, size)
        return

    exe = Path(renderer).name.lower()
    if exe == "resvg":
        cmd = [
            renderer,
            "--width",
            str(size),
            "--height",
            str(size),
            str(source),
            str(output),
        ]
    elif exe == "inkscape":
        cmd = [
            renderer,
            str(source),
            "--export-type=png",
            f"--export-filename={output}",
            "-w",
            str(size),
            "-h",
            str(size),
        ]
    else:
        cmd = [
            renderer,
            "-background",
            "none",
            str(source),
            "-resize",
            f"{size}x{size}",
            str(output),
        ]
    subprocess.run(cmd, check=True)


def dilate_image(image):
    """Match DDNet's dilate tool: copy neighbor colors into transparent pixels."""
    source = image.convert("RGBA")
    original = source.copy()
    work = source.copy()
    width, height = work.size
    alpha_threshold = 10

    for _ in range(11):
        previous = work.load()
        current = work.copy()
        pixels = current.load()
        for y in range(height):
            for x in range(width):
                if previous[x, y][3] > alpha_threshold:
                    continue
                for dx, dy in ((0, -1), (-1, 0), (1, 0), (0, 1)):
                    nx = min(max(x + dx, 0), width - 1)
                    ny = min(max(y + dy, 0), height - 1)
                    neighbor = previous[nx, ny]
                    if neighbor[3] > alpha_threshold:
                        pixels[x, y] = (neighbor[0], neighbor[1], neighbor[2], 255)
                        break
        work = current

    out = original.copy()
    out_pixels = out.load()
    work_pixels = work.load()
    for y in range(height):
        for x in range(width):
            if out_pixels[x, y][3] == 0:
                color = work_pixels[x, y]
                out_pixels[x, y] = (color[0], color[1], color[2], 0)
    return out


def icon_id(path: Path) -> str:
    stem = path.stem
    if stem.startswith("icon-"):
        stem = stem[5:]
    return ICON_ALIASES.get(stem, stem)


def build_scale(
    source_dir: Path, output_dir: Path, scale: int, base_size: int, padding: int
) -> None:
    try:
        from PIL import Image
    except ImportError as exc:
        raise SystemExit("Pillow is required to compose the Qm icon atlas") from exc

    renderer = find_renderer()

    svg_files = sorted(source_dir.glob("*.svg"))
    if not svg_files:
        raise SystemExit(f"No SVG files found in {source_dir}")

    icon_size = base_size * scale
    pad = padding * scale
    tile = icon_size + pad * 2
    columns = max(1, math.ceil(math.sqrt(len(svg_files))))
    rows = math.ceil(len(svg_files) / columns)
    atlas_w = columns * tile
    atlas_h = rows * tile

    atlas = Image.new("RGBA", (atlas_w, atlas_h), (0, 0, 0, 0))
    icons: dict[str, dict[str, int]] = {}

    with tempfile.TemporaryDirectory(prefix="qm-icons-") as tmp:
        tmp_dir = Path(tmp)
        for index, svg in enumerate(svg_files):
            rendered = tmp_dir / f"{svg.stem}.png"
            render_svg(renderer, svg, rendered, icon_size)
            image = Image.open(rendered).convert("RGBA")
            if image.size != (icon_size, icon_size):
                image = image.resize((icon_size, icon_size), Image.Resampling.LANCZOS)

            col = index % columns
            row = index // columns
            x = col * tile + pad
            y = row * tile + pad
            atlas.alpha_composite(image, (x, y))
            icons[icon_id(svg)] = {"x": x, "y": y, "w": icon_size, "h": icon_size}

    output_dir.mkdir(parents=True, exist_ok=True)
    image_name = f"qm_icons_{scale}x.png"
    manifest_name = f"qm_icons_{scale}x.json"
    atlas = dilate_image(atlas)
    atlas.save(output_dir / image_name)
    manifest = {
        "version": 1,
        "scale": scale,
        "source": "QmClient SVG icon sources, rendered at build time",
        "atlas": {
            "image": f"qmclient/icons/{image_name}",
            "width": atlas_w,
            "height": atlas_h,
            "padding": pad,
        },
        "icons": icons,
    }
    with (output_dir / manifest_name).open(
        "w", encoding="utf-8", newline="\n"
    ) as manifest_file:
        manifest_file.write(json.dumps(manifest, indent=2, sort_keys=True))


def main() -> int:
    args = parse_args()
    for scale in args.sizes:
        if scale <= 0:
            raise SystemExit("sizes must be positive")
        build_scale(args.source, args.output, scale, args.base_size, args.padding)
    return 0


if __name__ == "__main__":
    sys.exit(main())
