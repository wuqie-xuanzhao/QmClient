from __future__ import annotations

import importlib.util
import math
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATH = REPO_ROOT / "qmclient_scripts/qm_build_icon_morph.py"
sys.path.insert(0, str(SCRIPT_PATH.parent))
SPEC = importlib.util.spec_from_file_location("qm_build_icon_morph", SCRIPT_PATH)
assert SPEC is not None and SPEC.loader is not None
MORPH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MORPH)


class QmBuildIconMorphTest(unittest.TestCase):
    def test_all_weight_variants_have_stable_sampled_surface_data(self) -> None:
        for weight in MORPH.WEIGHTS:
            with self.subTest(weight=weight):
                source = MORPH.read_icon(REPO_ROOT / "datasrc/qm_icons", weight, "eye")
                target = MORPH.read_icon(REPO_ROOT / "datasrc/qm_icons", weight, "eye-off")
                self.assertTrue(source)
                self.assertTrue(target)
                self.assertTrue(all(len(path) == MORPH.SAMPLE_COUNT for path in source))
                self.assertTrue(all(len(path) == MORPH.SAMPLE_COUNT for path in target))

    def test_surface_layout_indexes_match_the_source_svg_topology(self) -> None:
        for weight in MORPH.WEIGHTS:
            for name in ("eye", "eye-off"):
                paths = MORPH.read_icon(REPO_ROOT / "datasrc/qm_icons", weight, name)
                for _, outer, inner in MORPH.SURFACE_LAYOUT[weight][name]:
                    with self.subTest(weight=weight, name=name, outer=outer, inner=inner):
                        self.assertLess(outer, len(paths))
                        if inner is not None:
                            self.assertLess(inner, len(paths))

    def test_endpoints_reconstruct_the_original_path(self) -> None:
        source = MORPH.read_icon(REPO_ROOT / "datasrc/qm_icons", "regular", "eye")[0]
        target = MORPH.read_icon(REPO_ROOT / "datasrc/qm_icons", "regular", "eye-off")[0]
        _, aligned_target, _, _, _, _ = MORPH.align_pair(source, target)
        source_data, target_data, source_center, target_center, theta, log_scale = MORPH.make_path_pair(source, target)
        scale = math.exp(log_scale)
        cos_angle = math.cos(theta)
        sin_angle = math.sin(theta)
        for index, expected in enumerate(source):
            actual = (source_center[0] + source_data[index][0], source_center[1] + source_data[index][1])
            self.assertAlmostEqual(actual[0], expected[0], places=4)
            self.assertAlmostEqual(actual[1], expected[1], places=4)
        for index, expected in enumerate(aligned_target):
            x, y = target_data[index]
            actual = (
                target_center[0] + (x * cos_angle - y * sin_angle) * scale,
                target_center[1] + (x * sin_angle + y * cos_angle) * scale,
            )
            self.assertAlmostEqual(actual[0], expected[0], places=4)
            self.assertAlmostEqual(actual[1], expected[1], places=4)

    def test_midpoint_contains_polar_transform_not_raw_coordinate_lerp(self) -> None:
        source = MORPH.read_icon(REPO_ROOT / "datasrc/qm_icons", "regular", "eye")[0]
        target = MORPH.read_icon(REPO_ROOT / "datasrc/qm_icons", "regular", "eye-off")[0]
        source_data, target_data, source_center, target_center, theta, log_scale = MORPH.make_path_pair(source, target)
        index = 17
        progress = 0.5
        raw_lerp = (
            source[index][0] + (target[index][0] - source[index][0]) * progress,
            source[index][1] + (target[index][1] - source[index][1]) * progress,
        )
        sx, sy = source_data[index]
        tx, ty = target_data[index]
        residual_x = sx + (tx - sx) * progress
        residual_y = sy + (ty - sy) * progress
        scale = math.exp(log_scale * progress)
        angle = theta * progress
        actual = (
            source_center[0] + (target_center[0] - source_center[0]) * progress + (residual_x * math.cos(angle) - residual_y * math.sin(angle)) * scale,
            source_center[1] + (target_center[1] - source_center[1]) * progress + (residual_x * math.sin(angle) + residual_y * math.cos(angle)) * scale,
        )
        self.assertGreater(math.dist(actual, raw_lerp), 0.01)

    def test_generator_emits_all_runtime_plans(self) -> None:
        with tempfile.TemporaryDirectory(prefix="qm-icon-morph-test-") as temp_dir:
            output = Path(temp_dir) / "qm_eye_morph.inc"
            generated = MORPH.generate(REPO_ROOT / "datasrc/qm_icons")
            output.write_text(generated, encoding="utf-8")
            text = output.read_text(encoding="utf-8")
            self.assertIn("s_EyeMorphPlan_thin", text)
            self.assertIn("s_EyeMorphPlan_regular", text)
            self.assertIn("s_EyeMorphPlan_bold", text)
            self.assertIn("s_EyeMorphPlan_fill", text)
            self.assertIn("sizeof(s_aEyeMorph_bold_surfaces)", text)
            self.assertNotIn("SEyeMorphPair", text)

    def test_input_field_keeps_morph_before_the_existing_icon_fallback(self) -> None:
        source = (REPO_ROOT / "src/game/client/QmUi/UiForms.cpp").read_text(encoding="utf-8")
        morph = source.index("RenderQmEyeMorph")
        fallback = source.index("Ctx.m_pIconManager->RenderIcon", morph)
        self.assertIn("QmIcon == static_cast<int>(EQmIcon::EYE)", source)
        self.assertIn("QmIcon == static_cast<int>(EQmIcon::EYE_OFF)", source)
        self.assertIn("g_Config.m_QmUiMotionLevel > 0", source)
        self.assertIn("ui_token::motion::TOGGLE", source)
        self.assertIn("ResolveUiAnimSpringValue", source)
        self.assertIn("HasActiveAnimation(MorphNodeKey", source)
        self.assertLess(morph, fallback)
        self.assertIn("Options.m_pTrailingActionId", source[morph:])


if __name__ == "__main__":
    unittest.main()
