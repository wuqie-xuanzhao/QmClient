#!/usr/bin/env python3

import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from qmclient_scripts import repair_ninja_msvc_prefix


class RepairNinjaMsvcPrefixTest(unittest.TestCase):
    def test_prepare_build_argument_does_not_run_cmake_configure(self):
        with tempfile.TemporaryDirectory() as tmp:
            build_dir = Path(tmp) / "build"
            rules_dir = build_dir / "CMakeFiles"
            rules_dir.mkdir(parents=True)
            (build_dir / "CMakeCache.txt").write_text(
                "CMAKE_C_COMPILER:FILEPATH=cl.exe\n"
                "CMAKE_HOME_DIRECTORY:INTERNAL=E:/Coding/DDNet/QmClient\n",
                encoding="utf-8",
            )
            rules_file = rules_dir / "rules.ninja"
            rules_file.write_text(
                "msvc_deps_prefix = old prefix\n"
                "rule CXX_COMPILER__game\n"
                "  deps = msvc\n",
                encoding="utf-8",
            )

            with (
                mock.patch.object(
                    sys,
                    "argv",
                    [
                        "repair_ninja_msvc_prefix.py",
                        "--prepare-build",
                        "--build",
                        str(build_dir),
                    ],
                ),
                mock.patch.object(
                    repair_ninja_msvc_prefix,
                    "_extract_showincludes_prefix",
                    return_value="new prefix",
                ),
                mock.patch.object(
                    repair_ninja_msvc_prefix.subprocess,
                    "run",
                    side_effect=AssertionError("must not configure before build"),
                ),
            ):
                self.assertEqual(repair_ninja_msvc_prefix.main(), 0)

            self.assertIn(
                "msvc_deps_prefix = new prefix",
                rules_file.read_text(encoding="utf-8"),
            )


if __name__ == "__main__":
    unittest.main()
