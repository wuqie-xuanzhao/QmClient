#!/usr/bin/env python3
"""Compare an MSDF glyph atlas against a FreeType/Pillow reference render.

This is an offline diagnostic tool. It does not change the client rendering path.
The comparison is intentionally tolerant of small placement differences: it searches
for the best translation in a small window, then reports both the alignment offset and
the actual shape error. ``--scale`` approximates the runtime quad scale: the atlas
sample is bilinearly resized and the shader's effective screen pixel range becomes
``px_range * scale``.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont


def parse_codepoints(args: argparse.Namespace) -> list[int]:
    values: list[int] = []
    for value in args.codepoint:
        value = value.strip().upper()
        values.append(int(value[2:] if value.startswith("U+") else value, 16))
    if args.charset:
        for line in Path(args.charset).read_text(encoding="utf-8").splitlines():
            line = line.split("#", 1)[0].strip().upper()
            if not line:
                continue
            values.append(int(line[2:] if line.startswith("U+") else line, 16))
    result = sorted(set(cp for cp in values if 0 < cp <= 0x10FFFF))
    if not result:
        raise SystemExit("provide --codepoint or --charset")
    return result


def load_atlas(manifest_path: Path, image_override: Path | None) -> tuple[dict, np.ndarray]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    atlas = manifest["atlas"]
    image_path = image_override
    if image_path is None:
        image_path = manifest_path.parent / Path(atlas["image"]).name
    if not image_path.is_file():
        raise SystemExit(f"atlas image not found: {image_path}")
    image = np.asarray(Image.open(image_path).convert("RGBA"), dtype=np.float32) / 255.0
    expected = (int(atlas["height"]), int(atlas["width"]), 4)
    if image.shape != expected:
        raise SystemExit(f"atlas size mismatch: expected {expected[:2]}, got {image.shape[:2]}")
    return manifest, image


def msdf_tile(manifest: dict, atlas: np.ndarray, codepoint: int, scale: float) -> tuple[np.ndarray | None, str]:
    entry = manifest.get("glyphs", {}).get(str(codepoint))
    if entry is None:
        return None, "missing"
    if not entry.get("outline", False):
        return None, "empty"
    x, y = int(entry["x"]), int(entry["y"])
    w, h = int(entry["w"]), int(entry["h"])
    tile = atlas[y : y + h, x : x + w]
    if tile.shape[:2] != (h, w):
        return None, "invalid"
    if scale != 1.0:
        resized = Image.fromarray(np.clip(tile * 255.0 + 0.5, 0, 255).astype(np.uint8), mode="RGBA")
        scaled_w = max(1, round(w * scale))
        scaled_h = max(1, round(h * scale))
        tile = np.asarray(resized.resize((scaled_w, scaled_h), Image.Resampling.BILINEAR), dtype=np.float32) / 255.0
    # MTSDF pages use the alpha true-SDF channel in the runtime path. Keep the
    # RGB median fallback for legacy pages without alpha_sdf metadata.
    if manifest.get("alpha_sdf", False):
        signed_distance = tile[:, :, 3] - 0.5
    else:
        signed_distance = np.median(tile[:, :, :3], axis=2) - 0.5
    px_range = float(manifest["px_range"]) * scale
    return np.clip(signed_distance * px_range + 0.5, 0.0, 1.0), "outlined"


def reference_mask(font: ImageFont.FreeTypeFont, char: str, size: tuple[int, int]) -> np.ndarray:
    width, height = size
    canvas = Image.new("L", (width + 32, height + 32), 0)
    draw = ImageDraw.Draw(canvas)
    bbox = draw.textbbox((0, 0), char, font=font)
    draw.text((16 - bbox[0], 16 - bbox[1]), char, font=font, fill=255)
    image = np.asarray(canvas, dtype=np.float32) / 255.0
    ys, xs = np.nonzero(image > 0.01)
    if len(xs) == 0:
        return np.zeros((height, width), dtype=np.float32)
    cropped = image[ys.min() : ys.max() + 1, xs.min() : xs.max() + 1]
    out = np.zeros((height, width), dtype=np.float32)
    y = max(0, (height - cropped.shape[0]) // 2)
    x = max(0, (width - cropped.shape[1]) // 2)
    h = min(cropped.shape[0], height - y)
    w = min(cropped.shape[1], width - x)
    out[y : y + h, x : x + w] = cropped[:h, :w]
    return out


def translated(image: np.ndarray, dx: int, dy: int) -> np.ndarray:
    out = np.zeros_like(image)
    src_x0, src_x1 = max(0, -dx), min(image.shape[1], image.shape[1] - dx)
    src_y0, src_y1 = max(0, -dy), min(image.shape[0], image.shape[0] - dy)
    dst_x0, dst_x1 = max(0, dx), min(image.shape[1], image.shape[1] + dx)
    dst_y0, dst_y1 = max(0, dy), min(image.shape[0], image.shape[0] + dy)
    if src_x1 > src_x0 and src_y1 > src_y0:
        out[dst_y0:dst_y1, dst_x0:dst_x1] = image[src_y0:src_y1, src_x0:src_x1]
    return out


def compare(reference: np.ndarray, actual: np.ndarray, search: int) -> tuple[dict, np.ndarray]:
    best: tuple[float, int, int, np.ndarray] | None = None
    for dy in range(-search, search + 1):
        for dx in range(-search, search + 1):
            candidate = translated(reference, dx, dy)
            binary_a = actual >= 0.5
            binary_b = candidate >= 0.5
            intersection = np.count_nonzero(binary_a & binary_b)
            union = np.count_nonzero(binary_a | binary_b)
            iou = intersection / union if union else 1.0
            mae = float(np.mean(np.abs(actual - candidate)))
            score = mae + (1.0 - iou)
            if best is None or score < best[0]:
                best = (score, dx, dy, candidate)
    assert best is not None
    _, dx, dy, aligned = best
    diff = np.abs(actual - aligned)
    binary_a = actual >= 0.5
    binary_b = aligned >= 0.5
    union = np.count_nonzero(binary_a | binary_b)
    intersection = np.count_nonzero(binary_a & binary_b)

    # FreeType and MSDF use different antialiasing kernels. Treat a one-pixel
    # contour displacement as a rasterization difference, but keep the strict
    # IoU in the report so large shape errors remain visible.
    padded_a = np.pad(binary_a, 1)
    padded_b = np.pad(binary_b, 1)
    dilated_a = np.zeros_like(binary_a)
    dilated_b = np.zeros_like(binary_b)
    for y in range(3):
        for x in range(3):
            dilated_a |= padded_a[y : y + binary_a.shape[0], x : x + binary_a.shape[1]]
            dilated_b |= padded_b[y : y + binary_b.shape[0], x : x + binary_b.shape[1]]
    tolerant_precision = float(np.count_nonzero(binary_a & dilated_b) / np.count_nonzero(binary_a)) if np.count_nonzero(binary_a) else 1.0
    tolerant_recall = float(np.count_nonzero(binary_b & dilated_a) / np.count_nonzero(binary_b)) if np.count_nonzero(binary_b) else 1.0
    tolerant_f1 = 2.0 * tolerant_precision * tolerant_recall / (tolerant_precision + tolerant_recall) if tolerant_precision + tolerant_recall else 1.0
    metrics = {
        "translation": {"x": dx, "y": dy},
        "mae": float(np.mean(diff)),
        "max_error": float(np.max(diff)),
        "iou": float(intersection / union if union else 1.0),
        "tolerant_f1": tolerant_f1,
        "actual_coverage": float(np.mean(actual)),
        "reference_coverage": float(np.mean(aligned)),
    }
    return metrics, diff


def save_gray(path: Path, image: np.ndarray) -> None:
    Image.fromarray(np.clip(image * 255.0 + 0.5, 0, 255).astype(np.uint8), mode="L").save(path)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--font", type=Path, required=True)
    parser.add_argument("--font-index", type=int, default=0)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--image", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--codepoint", action="append", default=[])
    parser.add_argument("--charset", type=Path)
    parser.add_argument("--search", type=int, default=4)
    parser.add_argument("--scale", type=float, default=1.0, help="runtime glyph scale relative to manifest em_pixels")
    parser.add_argument("--min-iou", type=float, default=0.90, help="strict binary IoU, retained for reporting")
    parser.add_argument("--min-tolerant-f1", type=float, default=0.92)
    parser.add_argument("--max-mae", type=float, default=0.12)
    args = parser.parse_args()
    if args.scale <= 0.0:
        raise SystemExit("--scale must be positive")

    manifest, atlas = load_atlas(args.manifest, args.image)
    codepoints = parse_codepoints(args)
    em_pixels = int(manifest["em_pixels"])
    font = ImageFont.truetype(str(args.font), max(1, round(em_pixels * args.scale)), index=args.font_index)
    args.output.mkdir(parents=True, exist_ok=True)

    report = {
        "font": str(args.font),
        "font_index": args.font_index,
        "manifest": str(args.manifest),
        "em_pixels": em_pixels,
        "px_range": manifest["px_range"],
        "scale": args.scale,
        "effective_px_range": float(manifest["px_range"]) * args.scale,
        "reference_mode": "Pillow FreeType raster reference; small-size hinting/AA may differ",
        "thresholds": {"min_iou": args.min_iou, "min_tolerant_f1": args.min_tolerant_f1, "max_mae": args.max_mae},
        "glyphs": [],
    }
    for codepoint in codepoints:
        actual, glyph_kind = msdf_tile(manifest, atlas, codepoint, args.scale)
        item = {"codepoint": f"U+{codepoint:04X}", "char": chr(codepoint)}
        if actual is None:
            item.update({"status": "skip" if glyph_kind == "empty" else "missing", "reason": "empty glyph" if glyph_kind == "empty" else "no outlined glyph in atlas"})
            report["glyphs"].append(item)
            continue
        reference = reference_mask(font, chr(codepoint), (actual.shape[1], actual.shape[0]))
        metrics, diff = compare(reference, actual, args.search)
        item.update(metrics)
        item["status"] = "pass" if metrics["tolerant_f1"] >= args.min_tolerant_f1 and metrics["mae"] <= args.max_mae else "fail"
        stem = f"U+{codepoint:04X}"
        save_gray(args.output / f"{stem}.reference.png", reference)
        save_gray(args.output / f"{stem}.msdf.png", actual)
        save_gray(args.output / f"{stem}.diff.png", diff)
        report["glyphs"].append(item)

    report["summary"] = {
        "pass": sum(item["status"] == "pass" for item in report["glyphs"]),
        "fail": sum(item["status"] == "fail" for item in report["glyphs"]),
        "missing": sum(item["status"] == "missing" for item in report["glyphs"]),
        "skip": sum(item["status"] == "skip" for item in report["glyphs"]),
    }
    if report["effective_px_range"] < 2.0:
        report["warning"] = "effective pxRange is below 2 screen pixels; MSDF antialiasing may degrade at this scale"
    (args.output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report["summary"], ensure_ascii=False))
    return 1 if report["summary"]["fail"] or report["summary"]["missing"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
