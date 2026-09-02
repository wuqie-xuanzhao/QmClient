from __future__ import annotations

import json
import re
import unittest
from pathlib import Path

from PIL import Image


REPO_ROOT = Path(__file__).resolve().parents[2]
CONFIG_PATH = REPO_ROOT / "data/qmclient/InputOverlay/input_overlay.json"
EDITOR_PATH = REPO_ROOT / "data/qmclient/input_overlay_editor.html"
FORMAT_HEADER = REPO_ROOT / "src/game/client/components/qmclient/input_overlay_format.h"
FORMAT_SOURCE = REPO_ROOT / "src/game/client/components/qmclient/input_overlay_format.cpp"
RUNTIME_SOURCE = REPO_ROOT / "src/game/client/components/qmclient/input_overlay.cpp"


class InputOverlayObsContractTest(unittest.TestCase):
    def test_default_assets_are_official_wrapper_and_presets(self) -> None:
        # 默认配置是 OBS 官方 layouts[] wrapper，引用官方 Zac 预设（键盘 + 鼠标）。
        wrapper = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
        self.assertNotIn("format", wrapper)
        self.assertNotIn("version", wrapper)
        self.assertNotIn("elements", wrapper)
        layouts = wrapper["layouts"]
        self.assertEqual(len(layouts), 2)
        self.assertEqual(layouts[0]["layout"], "wasd.json")
        self.assertEqual(layouts[0]["pressed_offset_y"], 61)
        self.assertEqual(layouts[1]["layout"], "mouse.json")
        self.assertEqual(layouts[1]["offset"], {"x": 467, "y": 0})
        self.assertEqual(layouts[1]["pressed_offset_y"], 0)
        wasd = json.loads((REPO_ROOT / "data/qmclient/InputOverlay/wasd.json").read_text(encoding="utf-8"))
        mouse = json.loads((REPO_ROOT / "data/qmclient/InputOverlay/mouse.json").read_text(encoding="utf-8"))
        self.assertNotIn("version", wasd)
        self.assertNotIn("image", wasd)
        self.assertEqual(wasd["overlay_width"], 705)
        self.assertEqual(wasd["overlay_height"], 526)
        self.assertEqual(len(wasd["elements"]), 17)
        self.assertEqual(mouse["overlay_width"], 285)
        self.assertEqual(mouse["overlay_height"], 421)
        with Image.open(REPO_ROOT / "data/qmclient/InputOverlay/wasd.png") as image:
            self.assertEqual(image.size, (1725, 1050))
        with Image.open(REPO_ROOT / "data/qmclient/InputOverlay/mouse.png") as image:
            self.assertEqual(image.size, (715, 353))

    def test_default_elements_use_only_official_types(self) -> None:
        for name in ("wasd.json", "mouse.json"):
            layout = json.loads((REPO_ROOT / f"data/qmclient/InputOverlay/{name}").read_text(encoding="utf-8"))
            self.assertTrue(all(0 <= element["type"] <= 9 for element in layout["elements"]))
            self.assertFalse(any("normal_frames" in element for element in layout["elements"]))
        wasd = json.loads((REPO_ROOT / "data/qmclient/InputOverlay/wasd.json").read_text(encoding="utf-8"))
        self.assertEqual({element["type"] for element in wasd["elements"]}, {1})
        mouse = json.loads((REPO_ROOT / "data/qmclient/InputOverlay/mouse.json").read_text(encoding="utf-8"))
        self.assertEqual({element["type"] for element in mouse["elements"]}, {0, 3, 4, 9})

    def test_editor_exports_json_png_and_qm_extension(self) -> None:
        html = EDITOR_PATH.read_text(encoding="utf-8")
        self.assertIn("导入 JSON + PNG", html)
        self.assertIn("导出 JSON + PNG", html)
        self.assertIn("_qm_editor", html)
        self.assertIn("resources", html)
        self.assertIn("atlas", html)
        self.assertIn("OBS_VERSION=507", html)
        self.assertIn("mapping_press", html)
        self.assertNotIn("manifest.json", html)
        self.assertNotIn("editor.json", html)
        self.assertNotIn("CompressionStream", html)
        self.assertNotRegex(html, r"<script[^>]+src=")
        self.assertNotRegex(html, r'<link[^>]+href=["\']https?://')

    def test_editor_supports_all_official_element_types_and_editor_state(self) -> None:
        html = EDITOR_PATH.read_text(encoding="utf-8")
        for token in (
            'value="0">0 纹理',
            'value="1">1 键盘按键',
            'value="2">2 手柄按钮',
            'value="3">3 鼠标按键',
            'value="4">4 滚轮',
            'value="5">5 摇杆',
            'value="6">6 扳机',
            'value="7">7 手柄编号',
            'value="8">8 方向键',
            'value="9">9 鼠标移动',
            "stickRadius",
            "mouseRadius",
            "mouseType",
            "triggerMode",
            "frameCount",
            "style",
        ):
            self.assertIn(token, html)

    def test_editor_keeps_full_editor_state_contract(self) -> None:
        html = EDITOR_PATH.read_text(encoding="utf-8")
        for token in (
            "normal_mappings",
            "pressed_mappings",
            "ensureFrameMappings",
            "setFrameCount",
            "多布局 OBS wrapper 需要导入",
            "wrapper",
            "root",
            "resources",
            "atlas",
        ):
            self.assertIn(token, html)

    def test_editor_has_no_legacy_profile_or_archive_symbols(self) -> None:
        html = EDITOR_PATH.read_text(encoding="utf-8")
        for token in ("input_overlay_v3", "qm_input_overlay", "exportZip", "importEditorArchive", "readZip"):
            self.assertNotIn(token, html)

    def test_parser_and_runtime_use_new_resource_directory(self) -> None:
        header = FORMAT_HEADER.read_text(encoding="utf-8")
        source = FORMAT_SOURCE.read_text(encoding="utf-8")
        runtime = RUNTIME_SOURCE.read_text(encoding="utf-8")
        self.assertIn('CONFIGURATION_PATH = "qmclient/InputOverlay/input_overlay.json"', header)
        self.assertIn('IMAGE_PATH = "wasd.png"', header)
        self.assertIn("ET_MOUSE_MOVEMENT = 9", header)
        self.assertIn("mapping_press", source)
        self.assertIn("Legacy QmClient input overlay profiles are not supported", runtime)
        self.assertIn("GlobalMousePos", runtime)
        self.assertNotIn("ImportProfileFromZip", runtime)
        self.assertNotIn("QmInputOverlayProfile", runtime)

    def test_runtime_demo_gamepad_guard_and_path_helpers(self) -> None:
        runtime = RUNTIME_SOURCE.read_text(encoding="utf-8")
        gameclient_header = (REPO_ROOT / "src/game/client/gameclient.h").read_text(encoding="utf-8")
        gameclient_source = (REPO_ROOT / "src/game/client/gameclient.cpp").read_text(encoding="utf-8")
        for token in ("StickPressed", "JoinPath", "pressed_offset_y", "ResolvedImagePath"):
            self.assertIn(token, runtime)
        # per-tick gamepad 录制 guard（不再依赖 m_LastDemoInputRecordTick）
        self.assertIn("m_LastDemoGamepadRecordTick", gameclient_header)
        self.assertIn("m_LastDemoGamepadRecordTick", gameclient_source)
        # Demo 播放期间使用 Demo 手柄状态，不回退实时手柄
        self.assertIn("STATE_DEMOPLAYBACK", runtime)
        self.assertIn("m_GamepadValid", runtime)

    def test_legacy_symbols_only_appear_as_rejection_strings(self) -> None:
        html = EDITOR_PATH.read_text(encoding="utf-8")
        runtime = RUNTIME_SOURCE.read_text(encoding="utf-8")
        format_sources = FORMAT_SOURCE.read_text(encoding="utf-8") + FORMAT_HEADER.read_text(encoding="utf-8")
        archive_symbols = ("manifest.json", "editor.json", "exportZip", "readZip", "QmInputOverlayProfile", "ImportProfileFromZip")
        for token in archive_symbols:
            self.assertNotIn(token, runtime + format_sources)
        for token in archive_symbols + ("input_overlay_v3", "qm_input_overlay"):
            self.assertNotIn(token, html)
        # 运行时必须保留旧格式拒绝字符串（只用于报错，不提供读取/导出）
        self.assertIn("qm_input_overlay", runtime)
        self.assertIn("input_overlay_v3", runtime)

    def test_cmake_packages_only_the_new_overlay_assets(self) -> None:
        cmake = (REPO_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        for token in ("qmclient/InputOverlay/input_overlay.json", "qmclient/InputOverlay/wasd.json", "qmclient/InputOverlay/wasd.png", "qmclient/InputOverlay/mouse.json", "qmclient/InputOverlay/mouse.png"):
            self.assertIn(token, cmake)
        self.assertNotIn("qmclient/InputOverlay/input_overlay.png", cmake)
        self.assertNotIn("input_overlay_v3", cmake)
        self.assertNotIn("data/input_overlay.json", cmake)


if __name__ == "__main__":
    unittest.main()
