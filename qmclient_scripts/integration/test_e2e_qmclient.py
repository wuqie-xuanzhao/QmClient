import unittest

try:
	from qmclient_scripts.integration.e2e_qmclient import E2E_TESTS
except ModuleNotFoundError:
	from e2e_qmclient import E2E_TESTS  # type: ignore[no-redef]


class QmClientE2ERunnerTest(unittest.TestCase):
	def test_required_e2e_scenarios_are_registered(self):
		self.assertEqual(
			set(E2E_TESTS),
			{
				"connection_failure_recovery",
				"demo_recording",
				"invalid_statistics_preserved",
				"perf_log_persistence",
				"qm_lifecycle_persistence",
				"recording_without_connection",
			},
		)

	def test_registered_scenarios_are_callable(self):
		self.assertTrue(all(callable(scenario) for scenario in E2E_TESTS.values()))


if __name__ == "__main__":
	unittest.main()
