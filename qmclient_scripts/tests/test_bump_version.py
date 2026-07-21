# 请抬头享受阳光｜日子很好 我很我---------致咩子
from __future__ import annotations

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATH = REPO_ROOT / "qmclient_scripts/bump_version.py"
WORKFLOW_PATH = REPO_ROOT / ".github/workflows/build.yml"
SPEC = importlib.util.spec_from_file_location("bump_version", SCRIPT_PATH)
assert SPEC is not None and SPEC.loader is not None
BUMP_VERSION = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUMP_VERSION)


class BumpVersionTest(unittest.TestCase):
    def test_tag_updates_authoritative_version_without_removed_docs_info(self) -> None:
        with tempfile.TemporaryDirectory(prefix="qm-version-test-") as temp_dir:
            version_h_path = Path(temp_dir) / "version.h"
            for line_ending in (b"\n", b"\r\n"):
                with self.subTest(line_ending=line_ending):
                    version_h_path.write_bytes(
                        b'#define QMCLIENT_VERSION "2.76.17"' + line_ending
                    )

                    with (
                        mock.patch.object(
                            BUMP_VERSION, "VERSION_H_PATH", version_h_path
                        ),
                        mock.patch.object(
                            sys, "argv", ["bump_version.py", "--tag", "v2.76.18"]
                        ),
                    ):
                        self.assertEqual(BUMP_VERSION.main(), 0)

                    self.assertEqual(
                        version_h_path.read_bytes(),
                        b'#define QMCLIENT_VERSION "2.76.18"' + line_ending,
                    )

    def test_tag_workflow_does_not_reference_removed_docs_info(self) -> None:
        workflow = WORKFLOW_PATH.read_text(encoding="utf-8")
        self.assertNotIn("docs/info.json", workflow)


if __name__ == "__main__":
    unittest.main()
