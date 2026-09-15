import importlib.util
import unittest
from pathlib import Path

import numpy as np


MODULE_PATH = Path(__file__).parents[1] / "qm_nameplate_msdf_compare.py"
SPEC = importlib.util.spec_from_file_location("qm_nameplate_msdf_compare", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class NameplateMsdfCompareTest(unittest.TestCase):
    def test_identical_masks_pass(self):
        image = np.zeros((12, 12), dtype=np.float32)
        image[3:9, 4:8] = 1.0
        metrics, _ = MODULE.compare(image, image, search=2)
        self.assertEqual(metrics["translation"], {"x": 0, "y": 0})
        self.assertEqual(metrics["tolerant_f1"], 1.0)
        self.assertEqual(metrics["mae"], 0.0)

    def test_one_pixel_edge_shift_is_tolerated_but_reported(self):
        reference = np.zeros((12, 12), dtype=np.float32)
        reference[3:9, 4:8] = 1.0
        actual = np.zeros_like(reference)
        actual[3:9, 5:9] = 1.0
        metrics, _ = MODULE.compare(reference, actual, search=0)
        self.assertLess(metrics["iou"], 1.0)
        self.assertEqual(metrics["tolerant_f1"], 1.0)

    def test_large_shape_change_is_not_hidden_by_tolerance(self):
        reference = np.zeros((12, 12), dtype=np.float32)
        reference[2:10, 2:10] = 1.0
        actual = np.zeros_like(reference)
        actual[5:7, 5:7] = 1.0
        metrics, _ = MODULE.compare(reference, actual, search=0)
        self.assertLess(metrics["tolerant_f1"], 0.92)

    def test_msdf_decode_tracks_runtime_scale(self):
        manifest = {"px_range": 4.0, "glyphs": {"65": {"outline": True, "x": 0, "y": 0, "w": 2, "h": 2}}}
        atlas = np.zeros((2, 2, 4), dtype=np.float32)
        atlas[:, :, :3] = 0.5
        atlas[:, :, 3] = 0.5
        decoded, kind = MODULE.msdf_tile(manifest, atlas, 65, 0.5)
        self.assertEqual(kind, "outlined")
        self.assertEqual(decoded.shape, (1, 1))
        self.assertAlmostEqual(float(decoded[0, 0]), 0.5, delta=0.01)


if __name__ == "__main__":
    unittest.main()
