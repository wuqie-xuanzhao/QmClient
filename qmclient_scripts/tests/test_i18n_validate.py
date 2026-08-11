from __future__ import annotations

import subprocess
import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


class I18nValidateTest(unittest.TestCase):
    def test_incremental_validation_completes(self) -> None:
        result = subprocess.run(
            [
                sys.executable,
                "qmclient_scripts/languages_qmclient/validate.py",
                "--incremental",
            ],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("All validation checks passed.", result.stdout)


if __name__ == "__main__":
    unittest.main()
