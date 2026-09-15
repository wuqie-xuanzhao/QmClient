import argparse
import json
import tempfile
import unittest
from pathlib import Path

from qmclient_scripts.qm_nameplate_msdf_build import compare_page


ROOT = Path(__file__).parents[2]


class NameplateMsdfBuildCompareTest(unittest.TestCase):
    @unittest.skipUnless(
        (ROOT / "data/qmclient/nameplate_msdf/nameplate_base_msdf.json").is_file()
        and (ROOT / "data/qmclient/nameplate_msdf/nameplate_base_msdf.png").is_file(),
        "no published built-in MSDF profile is present yet",
    )
    def test_compare_page_runs_against_published_cjk_page(self):
        manifest_path = ROOT / "data/qmclient/nameplate_msdf/nameplate_base_msdf.json"
        page = json.loads(manifest_path.read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory(prefix="qm-msdf-compare-test-") as temp:
            args = argparse.Namespace(
                compare_output=Path(temp),
                compare_codepoint=["U+4E2D"],
                compare_scale=[1.0],
                compare_strict=True,
            )
            compare_page(
                args,
                page,
                "qmclient/nameplate_msdf/nameplate_base_msdf.png",
                ROOT / "data/qmclient/nameplate_msdf",
                ROOT / "data/fonts/DejaVuSans.ttf",
                0,
            )
            report = Path(temp) / "nameplate_base_msdf/scale-1/report.json"
            self.assertTrue(report.is_file())
            summary = json.loads(report.read_text(encoding="utf-8"))["summary"]
            self.assertEqual(summary["fail"], 0)
            self.assertEqual(summary["missing"], 0)


if __name__ == "__main__":
    unittest.main()
