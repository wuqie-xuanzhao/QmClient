# 请抬头享受阳光｜日子很好 我很我---------致咩子
from __future__ import annotations

import importlib.util
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image


REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATH = REPO_ROOT / "qmclient_scripts/qm_build_icon_atlas.py"
SPEC = importlib.util.spec_from_file_location("qm_build_icon_atlas", SCRIPT_PATH)
assert SPEC is not None and SPEC.loader is not None
ICON_ATLAS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ICON_ATLAS)


class QmBuildIconAtlasTest(unittest.TestCase):
    def test_cubic_and_smooth_cubic_commands_preserve_control_points(self) -> None:
        polylines = ICON_ATLAS._parse_path_polylines(
            "M0 0 C0 10 10 10 10 0 S20 -10 20 0"
        )
        self.assertEqual(len(polylines), 1)
        points = polylines[0]
        self.assertAlmostEqual(max(y for _, y in points), 7.5)
        self.assertAlmostEqual(min(y for _, y in points), -7.5)
        self.assertEqual(points[-1], (20.0, 0.0))

    def test_quadratic_and_smooth_quadratic_commands_preserve_control_points(self) -> None:
        polylines = ICON_ATLAS._parse_path_polylines(
            "M0 0 Q5 10 10 0 T20 0"
        )
        self.assertEqual(len(polylines), 1)
        points = polylines[0]
        self.assertAlmostEqual(max(y for _, y in points), 5.0)
        self.assertAlmostEqual(min(y for _, y in points), -5.0)
        self.assertEqual(points[-1], (20.0, 0.0))

        relative = ICON_ATLAS._parse_path_polylines("M0 0 q5 10 10 0 t10 0")
        self.assertEqual(relative[0][-1], (20.0, 0.0))

    def test_phosphor_variants_render_with_fallback(self) -> None:
        for variant in ("phosphor_thin", "phosphor_regular", "phosphor_bold", "phosphor_fill"):
            source_dir = REPO_ROOT / "datasrc/qm_icons" / variant
            sources = sorted(source_dir.glob("*.svg"))
            self.assertEqual(len(sources), 16)

            with tempfile.TemporaryDirectory(prefix="qm-icon-test-") as temp_dir:
                for source in sources:
                    with self.subTest(variant=variant, file_name=source.name):
                        root = ET.parse(source).getroot()
                        self.assertEqual(root.attrib.get("viewBox"), "0 0 256 256")
                        self.assertEqual(root.attrib.get("fill"), "currentColor")

                        output = Path(temp_dir) / f"{source.stem}.png"
                        ICON_ATLAS._render_svg_fallback(source, output, 24)
                        image = Image.open(output).convert("RGBA")
                        alpha = image.getchannel("A")
                        self.assertIsNotNone(alpha.getbbox())
                        # 24px 的细体笔画可能完全由抗锯齿覆盖，不一定存在全不透明像素。
                        self.assertGreaterEqual(alpha.getextrema()[1], 192)


if __name__ == "__main__":
    unittest.main()
