#!/usr/bin/env python3
"""Render a nameplate MSDF/MTSDF glyph offline without starting the client."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


def parse_codepoints(values: list[str]) -> list[int]:
    result = []
    for value in values:
        value = value.strip().upper()
        result.append(int(value[2:] if value.startswith("U+") else value, 16))
    if not result:
        raise SystemExit("provide --codepoint")
    return result


def parse_color(value: str) -> tuple[int, int, int, int]:
    value = value.lstrip("#")
    if "," not in value:
        if len(value) not in (6, 8):
            raise SystemExit("colors must be RRGGBB or RRGGBBAA")
        parts = [int(value[index : index + 2], 16) for index in range(0, len(value), 2)]
    else:
        parts = [int(part, 16) for part in value.split(",")]
    if len(parts) == 3:
        parts.append(255)
    if len(parts) != 4:
        raise SystemExit("colors must be RRGGBB or RRGGBBAA")
    return tuple(parts)  # type: ignore[return-value]


def render_glyph(tile: np.ndarray, px_range: float, scale: float, outline_px: float, channel: str, uv_inset: int) -> np.ndarray:
    if uv_inset > 0 and tile.shape[0] > uv_inset * 2 and tile.shape[1] > uv_inset * 2:
        tile = tile[uv_inset:-uv_inset, uv_inset:-uv_inset]
    image = Image.fromarray(np.clip(tile * 255.0 + 0.5, 0, 255).astype(np.uint8), mode="RGBA")
    width = max(1, round(image.width * scale))
    height = max(1, round(image.height * scale))
    image = image.resize((width, height), Image.Resampling.BILINEAR)
    sample = np.asarray(image, dtype=np.float32) / 255.0
    distance = (np.median(sample[:, :, :3], axis=2) if channel == "msdf" else sample[:, :, 3]) - 0.5
    screen_px_range = max(px_range * scale, 1.0)
    fill = np.clip(distance * screen_px_range + 0.5, 0.0, 1.0)
    outer = np.clip(distance * screen_px_range + outline_px + 0.5, 0.0, 1.0)
    outline = np.maximum(outer - fill, 0.0)
    return np.stack((fill, outline), axis=2)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--image", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--codepoint", action="append", default=[])
    parser.add_argument("--scale", type=float, default=4.0)
    parser.add_argument("--outline-width", type=float, default=3.0)
    parser.add_argument("--channel", choices=("msdf", "alpha"), default="msdf", help="distance channel to preview")
    parser.add_argument("--uv-inset", type=int, default=1)
    parser.add_argument("--text-color", default="FFFFFFFF")
    parser.add_argument("--outline-color", default="303030FF")
    parser.add_argument("--background", default="808080FF")
    args = parser.parse_args()
    if args.scale <= 0.0 or args.outline_width < 0.0:
        raise SystemExit("scale must be positive and outline width cannot be negative")

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    image_path = args.image or args.manifest.parent / Path(manifest["atlas"]["image"]).name
    atlas = np.asarray(Image.open(image_path).convert("RGBA"), dtype=np.float32) / 255.0
    codepoints = parse_codepoints(args.codepoint)
    panels = []
    labels = []
    for codepoint in codepoints:
        entry = manifest.get("glyphs", {}).get(str(codepoint))
        if entry is None or not entry.get("outline", False):
            continue
        x, y = int(entry["x"]), int(entry["y"])
        w, h = int(entry["w"]), int(entry["h"])
        rendered = render_glyph(atlas[y : y + h, x : x + w], float(manifest["px_range"]), args.scale, args.outline_width, args.channel, args.uv_inset)
        background = np.array(parse_color(args.background), dtype=np.float32) / 255.0
        text = np.array(parse_color(args.text_color), dtype=np.float32) / 255.0
        outline_color = np.array(parse_color(args.outline_color), dtype=np.float32) / 255.0
        rgba = np.empty((*rendered.shape[:2], 4), dtype=np.float32)
        alpha = rendered[:, :, 1] * outline_color[3]
        alpha = np.maximum(alpha, rendered[:, :, 0] * text[3])
        rgb = background[:3][None, None, :] * (1.0 - alpha[:, :, None])
        rgb += outline_color[:3][None, None, :] * (rendered[:, :, 1] * outline_color[3])[:, :, None]
        text_alpha = rendered[:, :, 0] * text[3]
        rgb = rgb * (1.0 - text_alpha[:, :, None]) + text[:3][None, None, :] * text_alpha[:, :, None]
        rgba[:, :, :3] = rgb
        rgba[:, :, 3] = 1.0
        panels.append(Image.fromarray(np.clip(rgba * 255.0 + 0.5, 0, 255).astype(np.uint8), mode="RGBA"))
        labels.append(f"U+{codepoint:04X} {chr(codepoint)}")

    if not panels:
        raise SystemExit("no outlined glyphs found")
    gap = 24
    label_height = 36
    canvas = Image.new("RGBA", (sum(panel.width for panel in panels) + gap * (len(panels) - 1), max(panel.height for panel in panels) + label_height), parse_color(args.background))
    draw = ImageDraw.Draw(canvas)
    x = 0
    for panel, label in zip(panels, labels):
        canvas.alpha_composite(panel, (x, label_height))
        draw.text((x + 4, 8), label, fill=(255, 255, 255, 255))
        x += panel.width + gap
    args.output.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(args.output)
    print(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
