#!/usr/bin/env python3
"""保证 macOS 在缺少 Vulkan 工具链时仍能以默认 OpenGL 配置构建。"""

from pathlib import Path
import unittest


REPO_ROOT = Path(__file__).resolve().parents[2]


class CmakePlatformDefaultsTest(unittest.TestCase):
	def test_macos_enables_vulkan_by_default(self) -> None:
		cmake_source = (REPO_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
		block_start = cmake_source.index("set(AUTO_VULKAN_BACKEND ON)")
		block_end = cmake_source.index("option(WEBSOCKETS", block_start)
		platform_defaults = cmake_source[block_start:block_end]

		self.assertIn('if(TARGET_OS STREQUAL "windows")', platform_defaults)
		self.assertIn('elseif(TARGET_OS STREQUAL "mac")', platform_defaults)
		macos_defaults = platform_defaults.split('elseif(TARGET_OS STREQUAL "mac")', 1)[1]
		self.assertIn("set(AUTO_VULKAN_BACKEND ON)", macos_defaults)

	def test_macos_deployment_target_is_overridable(self) -> None:
		cmake_source = (REPO_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
		self.assertIn('set(DEFAULT_MACOS_DEPLOYMENT_TARGET "11.0")', cmake_source)
		self.assertIn('CACHE STRING "Minimum macOS deployment version")', cmake_source)
		self.assertNotIn('CACHE STRING "Minimum macOS deployment version" FORCE)', cmake_source)

	def test_windows_defines_update_target(self) -> None:
		cmake_source = (REPO_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
		self.assertIn('add_executable(qm-client-updater WIN32 src/qm-update/updater_main.cpp)', cmake_source)
		self.assertIn('set_property(TARGET qm-client-updater PROPERTY OUTPUT_NAME QmClient-Updater)', cmake_source)


if __name__ == "__main__":
	unittest.main()
