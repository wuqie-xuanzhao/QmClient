import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
CLIENT_SOURCE = ROOT / "src/engine/client/client.cpp"
PATCH_SOURCE = (
    ROOT
    / "qmclient_scripts/third_party/sdl2/patches/0001-qm-skip-uiless-uielement-processing.diff"
)
HINT = "SDL_QM_IME_SKIP_UILESS_UIELEMENT_PROCESSING"


class Sdl2ImePatchTests(unittest.TestCase):
    def test_client_enables_patched_hint_before_sdl_initialization(self):
        source = CLIENT_SOURCE.read_text(encoding="utf-8")

        self.assertLess(source.index(HINT), source.index("SDL_Init(0)"))

    def test_patch_skips_all_uiless_uielement_callbacks(self):
        source = PATCH_SOURCE.read_text(encoding="utf-8")

        self.assertIn(HINT, source)
        self.assertEqual(
            source.count("+    if (WIN_ShouldSkipUIElementProcessing())"), 3
        )
        self.assertIn("UIElementSink_BeginUIElement", source)
        self.assertIn("UIElementSink_UpdateUIElement", source)
        self.assertIn("UIElementSink_EndUIElement", source)

        begin = source.index("UIElementSink_BeginUIElement")
        show = source.index("+    *pbShow = FALSE;", begin)
        skip = source.index("+    if (WIN_ShouldSkipUIElementProcessing())", begin)
        lookup = source.index("UILess_GetUIElement", skip)
        self.assertLess(show, skip)
        self.assertLess(skip, lookup)


if __name__ == "__main__":
    unittest.main()
