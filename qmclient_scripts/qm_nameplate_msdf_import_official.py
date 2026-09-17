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
    parser.add_argument("--image-name", help="覆盖默认的 image 字段；basename 必须与 manifest 同名")
    args = parser.parse_args()

    # 运行时只取 image 的 basename，再与 manifest 所在目录拼接（见
    # qm_nameplate_msdf_renderer.cpp::ParseManifest），所以 basename 必须与 manifest 同名。
    # 这里直接由输出名推导：历史上 --image-name tmp/glow_jp.png 让日文页在运行时被静默跳过
    # （图集文件其实就在同目录，只是名字对不上），整页 425 个假名字形从未生效。
    expected_image = f"qmclient/nameplate_msdf/{args.output.stem}.png"
    if args.image_name is not None and Path(args.image_name).name != Path(expected_image).name:
        raise SystemExit(
            f"--image-name basename must match the manifest name ({Path(expected_image).name}), got {args.image_name}"
        )

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
        "atlas": {"image": expected_image, "width": int(atlas["width"]), "height": int(atlas["height"])},
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
