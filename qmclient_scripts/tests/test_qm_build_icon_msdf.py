from __future__ import annotations

import json
import unittest
from pathlib import Path

from PIL import Image


REPO_ROOT = Path(__file__).resolve().parents[2]
ICON_ROOT = REPO_ROOT / "data/qmclient/icons"


def pixels(image: Image.Image) -> list[tuple[int, int, int]]:
    return [image.getpixel((x, y)) for y in range(image.height) for x in range(image.width)]


class QmBuildIconMsdfTest(unittest.TestCase):
    def test_committed_msdf_atlases_are_rgb_distance_fields_with_padding(self) -> None:
        manifests = {}
        for weight in ("regular", "bold"):
            manifest_path = ICON_ROOT / f"qm_icons_{weight}_msdf.json"
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifests[weight] = manifest

            self.assertEqual(manifest["version"], 2)
            self.assertEqual(manifest["kind"], "msdf")
            self.assertEqual(manifest["px_range"], 6)
            self.assertIn("msdfgen", manifest["source"])

            atlas_info = manifest["atlas"]
            padding = atlas_info["padding"]
            image_path = REPO_ROOT / "data" / atlas_info["image"]
            with Image.open(image_path) as image:
                self.assertIn(image.mode, ("RGB", "RGBA"))
                self.assertEqual(image.size, (atlas_info["width"], atlas_info["height"]))

                atlas_pixels = pixels(image.convert("RGB"))
                self.assertTrue(any(red != green or green != blue for red, green, blue in atlas_pixels))

                for name, entry in manifest["icons"].items():
                    x = entry["x"]
                    y = entry["y"]
                    width = entry["w"]
                    height = entry["h"]
                    self.assertGreaterEqual(x, padding, name)
                    self.assertGreaterEqual(y, padding, name)
                    self.assertLessEqual(x + width + padding, image.width, name)
                    self.assertLessEqual(y + height + padding, image.height, name)

                    icon = image.crop((x, y, x + width, y + height)).convert("RGB")
                    self.assertIsNotNone(icon.getbbox(), name)
                    self.assertGreater(len(set(pixels(icon))), 1, name)

                    padding_regions = (
                        (x - padding, y, x, y + height),
                        (x, y - padding, x + width, y),
                        (x + width, y, x + width + padding, y + height),
                        (x, y + height, x + width, y + height + padding),
                    )
                    for region in padding_regions:
                        self.assertIsNone(image.crop(region).getbbox(), name)

        self.assertEqual(manifests["regular"]["icons"], manifests["bold"]["icons"])


if __name__ == "__main__":
    unittest.main()
