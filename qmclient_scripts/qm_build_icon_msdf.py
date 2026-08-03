#!/usr/bin/env python3
"""Build deterministic Phosphor MSDF icon atlases for QmClient."""

from __future__ import annotations

import argparse
import json
import math
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ICON_ALIASES = {"x": "close"}
FIELD_SIZE = 48
PX_RANGE = 6
PADDING = 8
CELL_SIZE = FIELD_SIZE + PADDING * 2


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--atlas-name", required=True)
    return parser.parse_args()


def icon_id(path: Path) -> str:
    stem = path.stem.removeprefix("icon-")
    return ICON_ALIASES.get(stem, stem)


def main() -> int:
    args = parse_args()
    msdfgen = shutil.which("msdfgen")
    if msdfgen is None:
        raise SystemExit("msdfgen is required to build QmClient MSDF icon atlases")

    try:
        from PIL import Image
    except ImportError as exc:
        raise SystemExit("Pillow is required to compose the QmClient MSDF icon atlas") from exc

    svg_files = sorted(args.source.glob("*.svg"))
    if not svg_files:
        raise SystemExit(f"No SVG files found in {args.source}")

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
            subprocess.run(
                [
                    msdfgen,
                    "msdf",
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
                    "-yflip",
                    "-o",
                    str(field_path),
                ],
                check=True,
            )
            field = Image.open(field_path).convert("RGB")
            if field.size != (FIELD_SIZE, FIELD_SIZE):
                raise SystemExit(f"msdfgen returned unexpected size for {svg}")

            column = index % columns
            row = index // columns
            x = column * CELL_SIZE + PADDING
            y = row * CELL_SIZE + PADDING
            atlas.paste(field.convert("RGBA"), (x, y))
            icons[icon_id(svg)] = {"x": x, "y": y, "w": FIELD_SIZE, "h": FIELD_SIZE}

    args.output.mkdir(parents=True, exist_ok=True)
    image_name = f"{args.atlas_name}_msdf.png"
    manifest_name = f"{args.atlas_name}_msdf.json"
    atlas.save(args.output / image_name)
    manifest = {
        "version": 2,
        "kind": "msdf",
        "px_range": PX_RANGE,
        "source": "Phosphor SVG sources generated with msdfgen",
        "atlas": {
            "image": f"qmclient/icons/{image_name}",
            "width": atlas_width,
            "height": atlas_height,
            "padding": PADDING,
        },
        "icons": icons,
    }
    (args.output / manifest_name).write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
