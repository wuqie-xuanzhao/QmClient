from __future__ import annotations

import platform
import subprocess
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


class ReleaseVulkanConfigTest(unittest.TestCase):
    def test_windows_graphics_release_overrides_cached_vulkan_off(self) -> None:
        if platform.system() != "Windows":
            self.skipTest("Windows release policy is host-specific")

        cmake_script = REPO_ROOT / "qmclient_scripts" / "cmake-windows.cmd"
        if not cmake_script.exists():
            self.skipTest("Windows CMake wrapper is not available")

        with tempfile.TemporaryDirectory(prefix="qm-release-vulkan-") as temp_dir:
            build_dir = Path(temp_dir) / "build"
            result = subprocess.run(
                [
                    "cmd.exe",
                    "/c",
                    str(cmake_script),
                    "-G",
                    "Ninja",
                    "-S",
                    str(REPO_ROOT),
                    "-B",
                    str(build_dir),
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DCLIENT=ON",
                    "-DHEADLESS_CLIENT=OFF",
                    "-DSERVER=OFF",
                    "-DTOOLS=OFF",
                    "-DDOWNLOAD_GTEST=OFF",
                    "-DDEV=ON",
                    "-DVULKAN=OFF",
                ],
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                check=False,
            )
            self.assertEqual(
                result.returncode,
                0,
                msg=f"CMake configure failed:\n{result.stdout}\n{result.stderr}",
            )
            cache = (build_dir / "CMakeCache.txt").read_text(encoding="utf-8")
            self.assertIn("VULKAN:BOOL=ON", cache)


if __name__ == "__main__":
    unittest.main()
