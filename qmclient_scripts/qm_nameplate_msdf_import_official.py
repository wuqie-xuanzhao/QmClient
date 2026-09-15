#!/usr/bin/env python3
"""Convert msdf-atlas-gen JSON to the QmClient nameplate manifest."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--image-name", required=True)
    args = parser.parse_args()

    source = json.loads(args.input.read_text(encoding="utf-8"))
    atlas = source["atlas"]
    size = float(atlas["size"])
    glyphs = {}
    for glyph in source.get("glyphs", []):
        bounds = glyph.get("atlasBounds")
        plane = glyph.get("planeBounds")
        if bounds is None or plane is None:
            continue
        left = float(bounds["left"])
        top = float(bounds["top"])
        right = float(bounds["right"])
        bottom = float(bounds["bottom"])
        glyphs[str(int(glyph["unicode"]))] = {
            "x": round(left),
            "y": round(top),
            "w": round(right - left),
            "h": round(bottom - top),
            "adv": float(glyph["advance"]) * size,
            "bx": float(plane["left"]) * size,
            "by": -float(plane["top"]) * size,
            "outline": True,
        }

    output = {
        "version": 1,
        "kind": "msdf-glyphs",
        "distance_field": "mtsdf",
        "alpha_sdf": True,
        "px_range": float(atlas["distanceRange"]),
        "em_pixels": size,
        "padding": 0,
        "source_font": "msdf-atlas-gen",
        "atlas": {"image": args.image_name, "width": int(atlas["width"]), "height": int(atlas["height"])},
        "ascent": -float(source["metrics"]["ascender"]) * size,
        "descent": float(source["metrics"]["descender"]) * size,
        "glyphs": glyphs,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
    print(f"converted {len(glyphs)} glyphs: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
