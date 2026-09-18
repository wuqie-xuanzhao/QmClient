import json
from pathlib import Path
import tempfile
import unittest
from types import SimpleNamespace

try:
	from qmclient_scripts.integration.e2e_qmclient import (
		E2E_TESTS,
		_assert_nameplate_msdf_page_totals,
		_expected_nameplate_msdf_totals,
	)
except ModuleNotFoundError:
	from e2e_qmclient import (  # type: ignore[no-redef]
		E2E_TESTS,
		_assert_nameplate_msdf_page_totals,
		_expected_nameplate_msdf_totals,
	)


def _write_fake_atlas(build_dir: Path) -> None:
	"""三页的最小图集：dejavu 两页 + 兜底一页，故意让码点 2/3 跨页重复。"""
	atlas = build_dir / "data" / "qmclient" / "nameplate_msdf"
	(atlas / "profiles").mkdir(parents=True)
	pages = {
		"nameplate_dejavu_00.json": ["1", "2"],
		"nameplate_dejavu_01.json": ["2", "3"],
		"nameplate_noto_glow_cn_00.json": ["3", "4"],
	}
	for name, codepoints in pages.items():
		(atlas / name).write_text(
			json.dumps({"glyphs": {codepoint: {} for codepoint in codepoints}}),
			encoding="utf-8",
		)
	(atlas / "profiles" / "nameplate_dejavu.json").write_text(
		json.dumps(
			{
				"pages": [
					"qmclient/nameplate_msdf/nameplate_dejavu_00.json",
					"qmclient/nameplate_msdf/nameplate_dejavu_01.json",
				]
			}
		),
		encoding="utf-8",
	)


class QmClientE2ERunnerTest(unittest.TestCase):
	def test_required_e2e_scenarios_are_registered(self):
		self.assertEqual(
			set(E2E_TESTS),
			{
				"assert_dialog_no_false_hang",
				"connection_failure_recovery",
				"demo_recording",
				"hang_watchdog_reports_stall",
				"invalid_statistics_preserved",
				"perf_log_persistence",
				"qm_lifecycle_persistence",
				"recording_without_connection",
				"vector_font_and_icon_resources",
			},
		)

	def test_registered_scenarios_are_callable(self):
		self.assertTrue(all(callable(scenario) for scenario in E2E_TESTS.values()))

	def test_nameplate_msdf_totals_sum_pages_and_union_glyphs(self):
		"""页数是相加，字形数是并集：跨页重码不能被重复计数（运行时用 emplace 先到先得）。"""
		with tempfile.TemporaryDirectory() as temp:
			build_dir = Path(temp)
			_write_fake_atlas(build_dir)
			atlas = build_dir / "data" / "qmclient" / "nameplate_msdf"
			(atlas / "profiles" / "nameplate_symbols.json").write_text(
				json.dumps({"pages": ["qmclient/nameplate_msdf/nameplate_noto_glow_cn_00.json"]}),
				encoding="utf-8",
			)
			self.assertEqual(_expected_nameplate_msdf_totals(SimpleNamespace(build_dir=build_dir)), (3, 4))

	def test_missing_fallback_profile_page_is_counted_as_zero(self):
		"""兜底 profile 尚未发布时只算主 profile 的页，不抛异常。"""
		with tempfile.TemporaryDirectory() as temp:
			build_dir = Path(temp)
			_write_fake_atlas(build_dir)
			atlas = build_dir / "data" / "qmclient" / "nameplate_msdf"
			(atlas / "profiles" / "nameplate_symbols.json").write_text(
				json.dumps({"pages": []}), encoding="utf-8"
			)
			self.assertEqual(_expected_nameplate_msdf_totals(SimpleNamespace(build_dir=build_dir)), (2, 3))

	def test_skipped_page_fails_the_totals_assertion(self):
		"""运行时少加载一页必须报错 —— 这正是日文页曾因 manifest image 指向 tmp/ 被静默跳过的形态。"""
		with tempfile.TemporaryDirectory() as temp:
			build_dir = Path(temp)
			_write_fake_atlas(build_dir)
			atlas = build_dir / "data" / "qmclient" / "nameplate_msdf"
			(atlas / "profiles" / "nameplate_symbols.json").write_text(
				json.dumps({"pages": ["qmclient/nameplate_msdf/nameplate_noto_glow_cn_00.json"]}),
				encoding="utf-8",
			)
			client = SimpleNamespace(
				_lines=["2026-09-16 23:06:37 I nameplate_msdf: Nameplate MSDF ready: 2 page(s), 3 glyphs, refEm=64"]
			)
			env = SimpleNamespace(build_dir=build_dir, client=client)
			with self.assertRaises(AssertionError) as caught:
				_assert_nameplate_msdf_page_totals(env)
			self.assertIn("a page was skipped", str(caught.exception))

	def test_matching_totals_pass(self):
		with tempfile.TemporaryDirectory() as temp:
			build_dir = Path(temp)
			_write_fake_atlas(build_dir)
			atlas = build_dir / "data" / "qmclient" / "nameplate_msdf"
			(atlas / "profiles" / "nameplate_symbols.json").write_text(
				json.dumps({"pages": ["qmclient/nameplate_msdf/nameplate_noto_glow_cn_00.json"]}),
				encoding="utf-8",
			)
			client = SimpleNamespace(
				_lines=["2026-09-16 23:06:37 I nameplate_msdf: Nameplate MSDF ready: 3 page(s), 4 glyphs, refEm=64"]
			)
			_assert_nameplate_msdf_page_totals(SimpleNamespace(build_dir=build_dir, client=client))


if __name__ == "__main__":
	unittest.main()
