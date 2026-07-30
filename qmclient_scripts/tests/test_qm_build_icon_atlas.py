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

    def test_unsupported_path_command_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "Unsupported SVG path command 'Q'"):
            ICON_ATLAS._parse_path_polylines("M0 0 Q5 5 10 0")

    def test_selected_phosphor_fill_sources_render_with_fallback(self) -> None:
        selected_icons = {
            "icon-eye.svg": "M176,32c-20.61",
            "icon-satellite-swap-incoming.svg": "H107.31l18.35,18.34",
            "icon-satellite-swap-outgoing.svg": "L148.69,136H88",
            "icon-satellite-switch.svg": "M208,96a12,12,0,1,1,12,12",
        }
        source_dir = REPO_ROOT / "datasrc/qm_icons/tabler"

        with tempfile.TemporaryDirectory(prefix="qm-icon-test-") as temp_dir:
            for file_name, geometry_fingerprint in selected_icons.items():
                with self.subTest(file_name=file_name):
                    source = source_dir / file_name
                    root = ET.parse(source).getroot()
                    self.assertEqual(root.attrib.get("fill"), "currentColor")
                    self.assertNotIn("stroke", root.attrib)
                    self.assertIn(geometry_fingerprint, source.read_text("utf-8"))

                    output = Path(temp_dir) / f"{source.stem}.png"
                    ICON_ATLAS._render_svg_fallback(source, output, 96)
                    alpha = Image.open(output).convert("RGBA").getchannel("A")
                    self.assertIsNotNone(alpha.getbbox())
                    self.assertEqual(alpha.getextrema(), (0, 255))

    def test_nonzero_fill_keeps_arrow_cutout_transparent(self) -> None:
        source = REPO_ROOT / "datasrc/qm_icons/tabler/icon-satellite-swap-incoming.svg"
        with tempfile.TemporaryDirectory(prefix="qm-icon-test-") as temp_dir:
            output = Path(temp_dir) / "incoming.png"
            ICON_ATLAS._render_svg_fallback(source, output, 96)
            alpha = Image.open(output).convert("RGBA").getchannel("A")

        self.assertEqual(alpha.getpixel((48, 48)), 0)
        self.assertGreater(alpha.getpixel((48, 12)), 240)

    def test_nonzero_fill_keeps_clock_dots_filled_and_hands_transparent(
        self,
    ) -> None:
        source = REPO_ROOT / "datasrc/qm_icons/tabler/icon-satellite-switch.svg"
        with tempfile.TemporaryDirectory(prefix="qm-icon-test-") as temp_dir:
            output = Path(temp_dir) / "switch.png"
            ICON_ATLAS._render_svg_fallback(source, output, 96)
            alpha = Image.open(output).convert("RGBA").getchannel("A")

        self.assertGreater(alpha.getpixel((74, 23)), 240)
        self.assertGreater(alpha.getpixel((83, 36)), 240)
        self.assertEqual(alpha.getpixel((48, 48)), 0)

    def test_selected_cutouts_remain_visible_at_runtime_base_size(self) -> None:
        source_dir = REPO_ROOT / "datasrc/qm_icons/tabler"
        file_names = (
            "icon-satellite-swap-incoming.svg",
            "icon-satellite-swap-outgoing.svg",
            "icon-satellite-switch.svg",
        )
        with tempfile.TemporaryDirectory(prefix="qm-icon-test-") as temp_dir:
            for file_name in file_names:
                with self.subTest(file_name=file_name):
                    output = Path(temp_dir) / f"{Path(file_name).stem}.png"
                    ICON_ATLAS._render_svg_fallback(source_dir / file_name, output, 24)
                    alpha = Image.open(output).convert("RGBA").getchannel("A")
                    self.assertLess(alpha.getpixel((12, 12)), 32)

    def test_hand_drawn_spectator_eye_pair_stays_distinct_at_runtime_size(self) -> None:
        source_dir = REPO_ROOT / "datasrc/qm_icons/tabler"
        with tempfile.TemporaryDirectory(prefix="qm-icon-test-") as temp_dir:
            rendered = {}
            for state in ("eye", "eye-closed"):
                source = source_dir / f"icon-satellite-spectator-{state}.svg"
                root = ET.parse(source).getroot()
                self.assertEqual(root.attrib.get("fill"), "none")
                output = Path(temp_dir) / f"{state}.png"
                ICON_ATLAS._render_svg_fallback(source, output, 24)
                rendered[state] = Image.open(output).convert("RGBA").getchannel("A")

        self.assertGreater(rendered["eye"].getpixel((12, 12)), 160)
        self.assertLess(rendered["eye-closed"].getpixel((12, 12)), 32)
        self.assertGreater(rendered["eye-closed"].getpixel((12, 15)), 240)


if __name__ == "__main__":
    unittest.main()
