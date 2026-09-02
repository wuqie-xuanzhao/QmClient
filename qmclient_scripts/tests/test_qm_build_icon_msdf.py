from __future__ import annotations

import json
import importlib.util
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image


REPO_ROOT = Path(__file__).resolve().parents[2]
ICON_ROOT = REPO_ROOT / "data/qmclient/icons"
BUILD_SCRIPT = REPO_ROOT / "qmclient_scripts/qm_build_icon_msdf.py"


def pixels(image: Image.Image) -> list[tuple[int, int, int]]:
    return [image.getpixel((x, y)) for y in range(image.height) for x in range(image.width)]


class QmBuildIconMsdfTest(unittest.TestCase):
    def test_builder_generates_rgba_atlas(self) -> None:
        with tempfile.TemporaryDirectory(prefix="qm-build-icon-msdf-test-") as temp_dir:
            temp_path = Path(temp_dir)
            source_dir = temp_path / "source"
            shared_source_dir = temp_path / "shared-source"
            output_dir = temp_path / "output"
            source_dir.mkdir()
            shared_source_dir.mkdir()
            (source_dir / "icon-test.svg").write_text(
                '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1 1"><path d="M0 0h1v1H0z"/></svg>',
                encoding="utf-8",
            )
            (shared_source_dir / "icon-shared.svg").write_text(
                '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1 1"><path d="M0 0h1v1H0z"/></svg>',
                encoding="utf-8",
            )

            fake_msdfgen_impl = temp_path / "fake_msdfgen.py"
            fake_msdfgen_args = temp_path / "msdfgen-args.txt"
            fake_msdfgen_impl.write_text(
                "\n".join(
                    (
                        "import sys",
                        "from pathlib import Path",
                        f"Path({str(fake_msdfgen_args)!r}).write_text('\\n'.join(sys.argv[1:]), encoding='utf-8')",
                        "from PIL import Image",
                        "output = sys.argv[sys.argv.index('-o') + 1]",
                        "Image.new('RGB', (48, 48), (17, 29, 71)).save(output)",
                    )
                )
                + "\n",
                encoding="utf-8",
            )
            if os.name == "nt":
                fake_msdfgen = temp_path / "msdfgen.cmd"
                fake_msdfgen.write_text(
                    f'@"{sys.executable}" "%~dp0fake_msdfgen.py" %*\n',
                    encoding="utf-8",
                    newline="\r\n",
                )
            else:
                fake_msdfgen = temp_path / "msdfgen"
                fake_msdfgen.write_text(
                    f"#!{sys.executable}\n"
                    "from pathlib import Path\n"
                    "import runpy\n"
                    "runpy.run_path(str(Path(__file__).with_name('fake_msdfgen.py')), run_name='__main__')\n",
                    encoding="utf-8",
                )
                fake_msdfgen.chmod(0o755)

            env = os.environ.copy()
            env["PATH"] = f"{temp_path}{os.pathsep}{env['PATH']}"
            subprocess.run(
                [
                    sys.executable,
                    str(BUILD_SCRIPT),
                    "--source",
                    str(source_dir),
                    "--source",
                    str(shared_source_dir),
                    "--output",
                    str(output_dir),
                    "--atlas-name",
                    "test_icons",
                ],
                check=True,
                cwd=REPO_ROOT,
                env=env,
            )

            with Image.open(output_dir / "test_icons_msdf.png") as atlas:
                self.assertEqual(atlas.mode, "RGBA")
                self.assertEqual(atlas.getpixel((8, 8)), (17, 29, 71, 255))
                self.assertEqual(atlas.getpixel((0, 0)), (0, 0, 0, 255))

            manifest = json.loads((output_dir / "test_icons_msdf.json").read_text(encoding="utf-8"))
            self.assertEqual(set(manifest["icons"]), {"test", "shared"})
            self.assertNotIn("-yflip", fake_msdfgen_args.read_text(encoding="utf-8").splitlines())

    def test_committed_satellite_check_keeps_svg_vertical_orientation(self) -> None:
        for weight in ("regular", "bold", "thin", "fill"):
            with self.subTest(weight=weight):
                alpha_manifest = json.loads((ICON_ROOT / f"qm_icons_{weight}_1x.json").read_text(encoding="utf-8"))
                msdf_manifest = json.loads((ICON_ROOT / f"qm_icons_{weight}_msdf.json").read_text(encoding="utf-8"))
                alpha_entry = alpha_manifest["icons"]["satellite-check"]
                msdf_entry = msdf_manifest["icons"]["satellite-check"]

                with Image.open(ICON_ROOT / f"qm_icons_{weight}_1x.png") as alpha_atlas:
                    alpha_icon = alpha_atlas.crop(
                        (
                            alpha_entry["x"],
                            alpha_entry["y"],
                            alpha_entry["x"] + alpha_entry["w"],
                            alpha_entry["y"] + alpha_entry["h"],
                        )
                    ).getchannel("A")
                with Image.open(ICON_ROOT / f"qm_icons_{weight}_msdf.png") as msdf_atlas:
                    msdf_icon = msdf_atlas.crop(
                        (
                            msdf_entry["x"],
                            msdf_entry["y"],
                            msdf_entry["x"] + msdf_entry["w"],
                            msdf_entry["y"] + msdf_entry["h"],
                        )
                    ).convert("RGB")

                alpha_icon = alpha_icon.resize(msdf_icon.size, Image.Resampling.NEAREST)
                alpha_mask = [value >= 128 for value in alpha_icon.get_flattened_data()]
                flipped_alpha_mask = [
                    value
                    for y in range(alpha_icon.height - 1, -1, -1)
                    for value in alpha_mask[y * alpha_icon.width : (y + 1) * alpha_icon.width]
                ]
                msdf_mask = [sorted(pixel)[1] >= 128 for pixel in pixels(msdf_icon)]
                normal_mismatches = sum(alpha != msdf for alpha, msdf in zip(alpha_mask, msdf_mask))
                flipped_mismatches = sum(alpha != msdf for alpha, msdf in zip(flipped_alpha_mask, msdf_mask))

                self.assertLess(normal_mismatches, flipped_mismatches)

    def test_committed_msdf_atlases_are_rgba_distance_fields_with_padding(self) -> None:
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
                self.assertEqual(image.mode, "RGBA")
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
                        self.assertIsNone(image.crop(region).convert("RGB").getbbox(), name)

        self.assertEqual(manifests["regular"]["icons"], manifests["bold"]["icons"])

    def test_multishape_svg_uses_raster_sdf(self) -> None:
        with tempfile.TemporaryDirectory(prefix="qm-build-icon-msdf-test-") as temp_dir:
            temp_path = Path(temp_dir)
            source = temp_path / "icon-multishape.svg"
            source.write_text(
                '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256">'
                '<circle cx="64" cy="128" r="40" fill="currentColor"/>'
                '<path d="M128 96h96v64h-96z" fill="currentColor"/>'
                '</svg>',
                encoding="utf-8",
            )
            output = temp_path / "field.png"

            msdf_module = importlib.util.spec_from_file_location("qm_build_icon_msdf", BUILD_SCRIPT)
            assert msdf_module is not None and msdf_module.loader is not None
            module = importlib.util.module_from_spec(msdf_module)
            msdf_module.loader.exec_module(module)
            module.render_raster_sdf(source, output)

            image = Image.open(output).convert("RGBA")
            self.assertGreater(len(set(pixels(image.convert("RGB")))), 1)
            self.assertEqual(image.getpixel((0, 0))[3], 255)


if __name__ == "__main__":
    unittest.main()
