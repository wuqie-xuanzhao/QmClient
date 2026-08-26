#!/usr/bin/env python3
"""在构建期为 QmClient 眼睛 morph 样例生成采样几何数据。"""

from __future__ import annotations

import argparse
import math
import sys
import xml.etree.ElementTree as element_tree
from pathlib import Path

from qm_build_icon_atlas import _parse_path_polylines


SAMPLE_COUNT = 64
VIEWBOX_CENTER = 128.0
WEIGHTS = ("thin", "regular", "bold", "fill")

# Phosphor 眼睛图标由复合路径组成。每个表面用外轮廓和可选内轮廓表示；
# 缺失的一侧输出为退化轮廓，让拓扑能够从几何中出现或消失，而不是做位图淡化。
SURFACE_LAYOUT = {
    "thin": {
        "eye": [("ring", 0, 1), ("ring", 2, 3)],
        "eye-off": [("ring", 0, 1), ("fill", 2, None), ("fill", 3, None), ("fill", 4, None)],
    },
    "regular": {
        "eye": [("ring", 0, 1), ("ring", 2, 3)],
        "eye-off": [("ring", 0, 1), ("fill", 2, None), ("fill", 3, None), ("fill", 4, None)],
    },
    "bold": {
        "eye": [("ring", 0, 1), ("ring", 2, 3)],
        "eye-off": [("ring", 0, 1), ("fill", 2, None)],
    },
    "fill": {
        "eye": [("ring", 0, 1)],
        "eye-off": [("fill", 0, None), ("ring", 1, 2)],
    },
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def distance(a: tuple[float, float], b: tuple[float, float]) -> float:
    return math.hypot(a[0] - b[0], a[1] - b[1])


def centroid(points: list[tuple[float, float]]) -> tuple[float, float]:
    if not points:
        return VIEWBOX_CENTER, VIEWBOX_CENTER
    return (
        sum(point[0] for point in points) / len(points),
        sum(point[1] for point in points) / len(points),
    )


def sample_closed(points: list[tuple[float, float]]) -> list[tuple[float, float]]:
    if len(points) < 3:
        raise ValueError("eye icon subpath must be a closed polygon")
    if points[0] == points[-1]:
        points = points[:-1]
    lengths = [0.0]
    for a, b in zip(points, points[1:] + [points[0]]):
        lengths.append(lengths[-1] + distance(a, b))
    total = lengths[-1]
    if total <= 1e-12:
        return [points[0]] * SAMPLE_COUNT

    sampled: list[tuple[float, float]] = []
    for index in range(SAMPLE_COUNT):
        target = total * index / SAMPLE_COUNT
        segment = next(
            (i for i in range(len(points)) if lengths[i + 1] >= target),
            len(points) - 1,
        )
        segment_end = lengths[segment + 1]
        fraction = (target - lengths[segment]) / max(segment_end - lengths[segment], 1e-9)
        a = points[segment]
        b = points[(segment + 1) % len(points)]
        sampled.append((a[0] + (b[0] - a[0]) * fraction, a[1] + (b[1] - a[1]) * fraction))
    return sampled


def reverse_points(points: list[tuple[float, float]]) -> list[tuple[float, float]]:
    return list(reversed(points))


def rotate_points(points: list[tuple[float, float]], offset: int) -> list[tuple[float, float]]:
    return points[offset:] + points[:offset]


def signed_area(points: list[tuple[float, float]]) -> float:
    return 0.5 * sum(
        a[0] * b[1] - b[0] * a[1]
        for a, b in zip(points, points[1:] + [points[0]])
    )


def canonicalize_contour(points: list[tuple[float, float]]) -> list[tuple[float, float]]:
    # 条带顶点要求两条边界沿同一方向行走；使用稳定的最右顶点作为起点，
    # 避免独立 SVG 子路径在插值时扭转。
    if signed_area(points) > 0.0:
        points = reverse_points(points)
    center = centroid(points)
    start = max(
        range(len(points)),
        key=lambda index: (points[index][0], -abs(points[index][1] - center[1])),
    )
    return rotate_points(points, start)


def procrustes(
    source: list[tuple[float, float]],
    target: list[tuple[float, float]],
    source_center: tuple[float, float],
    target_center: tuple[float, float],
) -> tuple[float, float, float]:
    sxx = sxy = syx = syy = norm_source = norm_target = 0.0
    for (source_x, source_y), (target_x, target_y) in zip(source, target):
        source_x -= source_center[0]
        source_y -= source_center[1]
        target_x -= target_center[0]
        target_y -= target_center[1]
        sxx += source_x * target_x
        sxy += source_x * target_y
        syx += source_y * target_x
        syy += source_y * target_y
        norm_source += source_x * source_x + source_y * source_y
        norm_target += target_x * target_x + target_y * target_y

    if norm_source <= 1e-12 or norm_target <= 1e-12:
        return 0.0, 1.0, 0.0

    theta = math.atan2(sxy - syx, sxx + syy)
    numerator = math.cos(theta) * (sxx + syy) + math.sin(theta) * (sxy - syx)
    sigma = max(numerator / norm_source, 1e-6)
    residual = max(sigma * sigma * norm_source - 2.0 * sigma * numerator + norm_target, 0.0)
    return theta, sigma, math.sqrt(residual / norm_target)


def align_pair(
    source: list[tuple[float, float]],
    target: list[tuple[float, float]],
) -> tuple[list[tuple[float, float]], list[tuple[float, float]], tuple[float, float], tuple[float, float], float, float]:
    source_center = centroid(source)
    target_center = centroid(target)
    best_score = float("inf")
    best_target = target
    best_theta = 0.0
    best_sigma = 1.0
    for offset in range(SAMPLE_COUNT):
        candidate = rotate_points(target, offset)
        theta, sigma, residual = procrustes(source, candidate, source_center, target_center)
        score = residual + 0.05 * abs(theta) / math.pi
        if score < best_score:
            best_score = score
            best_target = candidate
            best_theta = theta
            best_sigma = sigma
    return source, best_target, source_center, target_center, best_theta, best_sigma


def read_icon(source_root: Path, weight: str, name: str) -> list[list[tuple[float, float]]]:
    source = source_root / f"phosphor_{weight}" / f"icon-{name}.svg"
    root = element_tree.parse(source).getroot()
    path = root.find("{http://www.w3.org/2000/svg}path")
    if path is None:
        raise ValueError(f"{source} does not contain one path")
    return [canonicalize_contour(sample_closed(polyline)) for polyline in _parse_path_polylines(path.attrib["d"])]


def degenerate_path() -> list[tuple[float, float]]:
    return [(VIEWBOX_CENTER, VIEWBOX_CENTER)] * SAMPLE_COUNT


def make_path_pair(
    source: list[tuple[float, float]] | None,
    target: list[tuple[float, float]] | None,
) -> tuple[list[tuple[float, float]], list[tuple[float, float]], tuple[float, float], tuple[float, float], float, float]:
    source = source if source is not None else degenerate_path()
    target = target if target is not None else degenerate_path()
    source, target, source_center, target_center, theta, sigma = align_pair(source, target)
    cos = math.cos(-theta)
    sin = math.sin(-theta)
    source_centered = [(x - source_center[0], y - source_center[1]) for x, y in source]
    target_transformed = []
    for x, y in target:
        x -= target_center[0]
        y -= target_center[1]
        target_transformed.append(((x * cos - y * sin) / sigma, (x * sin + y * cos) / sigma))
    return source_centered, target_transformed, source_center, target_center, theta, math.log(sigma)


def format_float(value: float) -> str:
    if abs(value) < 0.0000005:
        value = 0.0
    return f"{value:.7f}f"


def format_points(points: list[tuple[float, float]]) -> str:
    return ", ".join(format_float(component) for point in points for component in point)


def format_path_data(
    name: str,
    source: list[tuple[float, float]] | None,
    target: list[tuple[float, float]] | None,
    lines: list[str],
) -> str:
    source_data, target_data, source_center, target_center, theta, log_scale = make_path_pair(source, target)
    source_name = f"{name}_source"
    target_name = f"{name}_target"
    lines.append(f"static constexpr float {source_name}[{SAMPLE_COUNT * 2}] = {{{format_points(source_data)}}};")
    lines.append(f"static constexpr float {target_name}[{SAMPLE_COUNT * 2}] = {{{format_points(target_data)}}};")
    return (
        "{" + ", ".join(
            [
                source_name,
                target_name,
                format_float(source_center[0]),
                format_float(source_center[1]),
                format_float(target_center[0]),
                format_float(target_center[1]),
                format_float(theta),
                format_float(log_scale),
            ]
        ) + "}"
    )


def generate(source_root: Path) -> str:
    lines = [
        "// 由 qmclient_scripts/qm_build_icon_morph.py 生成，请勿手动编辑。",
        "",
    ]
    for weight in WEIGHTS:
        source_paths = read_icon(source_root, weight, "eye")
        target_paths = read_icon(source_root, weight, "eye-off")
        source_surfaces = SURFACE_LAYOUT[weight]["eye"]
        target_surfaces = SURFACE_LAYOUT[weight]["eye-off"]
        surfaces: list[str] = []
        for role in ("ring", "fill"):
            source_role = [surface for surface in source_surfaces if surface[0] == role]
            target_role = [surface for surface in target_surfaces if surface[0] == role]
            for index in range(max(len(source_role), len(target_role))):
                source_surface = source_role[index] if index < len(source_role) else None
                target_surface = target_role[index] if index < len(target_role) else None

                source_outer = source_paths[source_surface[1]] if source_surface is not None else None
                target_outer = target_paths[target_surface[1]] if target_surface is not None else None
                source_inner = source_paths[source_surface[2]] if source_surface is not None and source_surface[2] is not None else None
                target_inner = target_paths[target_surface[2]] if target_surface is not None and target_surface[2] is not None else None
                outer = format_path_data(f"s_aEyeMorph_{weight}_{role}_{index}_outer", source_outer, target_outer, lines)
                inner = format_path_data(f"s_aEyeMorph_{weight}_{role}_{index}_inner", source_inner, target_inner, lines)
                surfaces.append("\t{" + outer + ", " + inner + "},")
                lines.append("")

        lines.append(f"static constexpr SQmIconMorphSurfaceData s_aEyeMorph_{weight}_surfaces[] = {{")
        lines.extend(surfaces)
        lines.append("};")
        lines.append(
            f"static constexpr SQmIconMorphPlan s_EyeMorphPlan_{weight} = "
            f"{{s_aEyeMorph_{weight}_surfaces, static_cast<int>(sizeof(s_aEyeMorph_{weight}_surfaces) / sizeof(s_aEyeMorph_{weight}_surfaces[0]))}};"
        )
        lines.append("")
    return "\n".join(lines)


def main() -> int:
    args = parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(generate(args.source_root), encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
