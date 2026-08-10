from __future__ import annotations

import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
FIND_CRYPTO = REPO_ROOT / "cmake/FindCrypto.cmake"
BORINGSSL = REPO_ROOT / "ddnet-libs/boringssl"


class AndroidCryptoDependencyTest(unittest.TestCase):
    def test_android_uses_bundled_boringssl(self) -> None:
        cmake = FIND_CRYPTO.read_text(encoding="utf-8")
        self.assertIn('TARGET_OS STREQUAL "android"', cmake)
        self.assertIn("set_extra_dirs_lib(CRYPTO boringssl)", cmake)
        self.assertIn("set_extra_dirs_include(CRYPTO boringssl", cmake)
        self.assertIn("openssl/base.h", cmake)
        self.assertNotIn("set_extra_dirs_lib(CRYPTO openssl)", cmake)

    def test_android_boringssl_assets_cover_all_abis(self) -> None:
        for directory in ("libarm", "libarm64", "lib32", "lib64"):
            with self.subTest(directory=directory):
                self.assertTrue(
                    (BORINGSSL / "android" / directory / "libcrypto.a").is_file()
                )
                self.assertTrue(
                    (BORINGSSL / "android" / directory / "libssl.a").is_file()
                )
        for header in ("ssl.h", "base.h", "hkdf.h", "opensslconf.h"):
            with self.subTest(header=header):
                self.assertTrue(
                    (BORINGSSL / "include/android/openssl" / header).is_file()
                )


if __name__ == "__main__":
    unittest.main()
