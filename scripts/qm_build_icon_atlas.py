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


def _local_name(tag: str) -> str:
	return tag.rsplit("}", 1)[-1]


def _draw_round_line(draw, points: list[tuple[float, float]], width: int, fill: tuple[int, int, int, int]) -> None:
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
			points = [(sx(x), sy(y)) for x, y in _parse_points(node.attrib.get("points", ""))]
			_draw_round_line(draw, points, stroke_width, fill)
		elif tag == "polygon":
			points = [(sx(x), sy(y)) for x, y in _parse_points(node.attrib.get("points", ""))]
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
			draw.rounded_rectangle((x, y, x + w, y + h), radius=radius, outline=fill, width=stroke_width)
		elif tag == "path" and not node.attrib.get("d"):
			continue
		elif tag == "path":
			raise SystemExit(f"Fallback SVG renderer does not support path data in {source}")
		else:
			raise SystemExit(f"Fallback SVG renderer does not support <{tag}> in {source}")

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


def icon_id(path: Path) -> str:
	stem = path.stem
	if stem.startswith("icon-"):
		stem = stem[5:]
	return ICON_ALIASES.get(stem, stem)


def build_scale(source_dir: Path, output_dir: Path, scale: int, base_size: int, padding: int) -> None:
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
	atlas.save(output_dir / image_name)
	manifest = {
		"version": 1,
		"scale": scale,
		"source": "Tabler Icons SVG, rendered at build time",
		"atlas": {
			"image": f"qmclient/icons/{image_name}",
			"width": atlas_w,
			"height": atlas_h,
			"padding": pad,
		},
		"icons": icons,
	}
	(output_dir / manifest_name).write_text(json.dumps(manifest, indent=2, sort_keys=True), encoding="utf-8")


def main() -> int:
	args = parse_args()
	for scale in args.sizes:
		if scale <= 0:
			raise SystemExit("sizes must be positive")
		build_scale(args.source, args.output, scale, args.base_size, args.padding)
	return 0


if __name__ == "__main__":
	sys.exit(main())
