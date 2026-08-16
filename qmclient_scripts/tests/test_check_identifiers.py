from __future__ import annotations

import importlib.util
from pathlib import Path
import unittest


REPO_ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("check_identifiers", REPO_ROOT / "scripts" / "check_identifiers.py")
assert SPEC is not None and SPEC.loader is not None
check_identifiers = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(check_identifiers)


class CheckIdentifiersTest(unittest.TestCase):
	def test_structs_use_s_prefix(self):
		self.assertIsNone(check_identifiers.check_name("struct", "", "", "SQmAxiomScore"))
		self.assertEqual(
			check_identifiers.check_name("struct", "", "", "CQmAxiomScore"),
			"should start with 'S'",
		)

	def test_classes_still_use_c_or_i_prefix(self):
		self.assertIsNone(check_identifiers.check_name("class", "", "", "CQmAxiomScores"))
		self.assertIsNone(check_identifiers.check_name("class", "", "", "IQmAxiomHttp"))

	def test_vector_members_use_v_prefix(self):
		self.assertIsNone(check_identifiers.check_variable_name("m", "v", "m_vDifficulties"))
		self.assertEqual(
			check_identifiers.check_variable_name("m", "v", "m_Difficulties"),
			"should start with 'm_v'",
		)


if __name__ == "__main__":
	unittest.main()
