from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


GATE_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GATE_DIR))

from lib import docs_harness  # noqa: E402


AI_WORKFLOW_FILES = {
    "docs/ai-workflow/meta.md",
    "docs/ai-workflow/ddnet-development.md",
    "docs/ai-workflow/verification.md",
    "docs/ai-workflow/review.md",
    "docs/ai-workflow/git-workflow.md",
    "docs/ai-workflow/advanced/README.md",
    "docs/ai-workflow/advanced/feature-introduction.md",
    "docs/ai-workflow/advanced/memory-lifetime.md",
    "docs/ai-workflow/advanced/observability-debugging.md",
    "docs/ai-workflow/advanced/performance-workflow.md",
    "docs/ai-workflow/advanced/perf-system-workflow.md",
    "docs/ai-workflow/advanced/refactor-workflow.md",
    "docs/ai-workflow/advanced/regression-prevention.md",
    "docs/ai-workflow/advanced/safety-security.md",
    "docs/ai-workflow/advanced/threading-jobs.md",
}


def frontmatter(status: str = "active") -> str:
    return f"---\ntitle: Test document\nstatus: {status}\n---\n\n# Test\n"


class DocsHarnessTest(unittest.TestCase):
    def setUp(self) -> None:
        self._temp_dir = tempfile.TemporaryDirectory()
        self.repo_root = Path(self._temp_dir.name)

    def tearDown(self) -> None:
        self._temp_dir.cleanup()

    def write(self, relative_path: str, content: str = "") -> None:
        path = self.repo_root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8", newline="\n")

    def create_ai_workflow(self) -> None:
        for path in AI_WORKFLOW_FILES:
            self.write(path, "# Current rule\n")

    def create_superpowers(self) -> None:
        self.write(
            "docs/superpowers/plans/current.md",
            frontmatter("active") + "\nSee [draft spec](../specs/draft.md).\n",
        )
        self.write("docs/superpowers/specs/draft.md", frontmatter("draft"))
        self.write(
            "docs/superpowers/README.md",
            frontmatter("active")
            + "\n- [Current plan](plans/current.md): Current plan.\n"
            + "- [Draft spec](specs/draft.md): Draft spec.\n",
        )

    def test_ai_workflow_manifest_accepts_exact_set_and_rejects_extra_file(
        self,
    ) -> None:
        self.create_ai_workflow()

        result = docs_harness.check_ai_workflow_manifest(self.repo_root)
        self.assertTrue(result.ok, result.detail)

        self.write("docs/ai-workflow/obsolete.md", "# Old rule\n")
        result = docs_harness.check_ai_workflow_manifest(self.repo_root)
        self.assertFalse(result.ok)
        self.assertIn("docs/ai-workflow/obsolete.md", result.detail)

        (self.repo_root / "docs/ai-workflow/obsolete.md").unlink()
        missing_path = self.repo_root / "docs/ai-workflow/review.md"
        missing_path.unlink()
        result = docs_harness.check_ai_workflow_manifest(self.repo_root)
        self.assertFalse(result.ok)
        self.assertIn("docs/ai-workflow/review.md", result.detail)

    def test_superpowers_layout_rejects_history_directories_and_html(self) -> None:
        self.create_superpowers()
        self.write("docs/superpowers/reports/old.md", frontmatter())
        self.write("docs/superpowers/plans/Archive/old.md", frontmatter())
        self.write("docs/superpowers/specs/prototype.html", "<html></html>\n")

        result = docs_harness.check_superpowers_layout(self.repo_root)

        self.assertFalse(result.ok)
        self.assertIn("docs/superpowers/plans/Archive", result.detail)
        self.assertIn("docs/superpowers/reports", result.detail)
        self.assertIn("docs/superpowers/specs/prototype.html", result.detail)

    def test_superpowers_status_requires_active_or_draft(self) -> None:
        self.create_superpowers()
        self.write("docs/superpowers/explore/old.md", frontmatter("completed"))
        self.write("docs/superpowers/plans/missing-status.md", "# No status\n")
        readme = (self.repo_root / "docs/superpowers/README.md").read_text(
            encoding="utf-8"
        )
        self.write(
            "docs/superpowers/README.md",
            readme.replace("status: active", "status: completed", 1),
        )

        result = docs_harness.check_superpowers_statuses(self.repo_root)

        self.assertFalse(result.ok)
        self.assertIn("docs/superpowers/explore/old.md", result.detail)
        self.assertIn("docs/superpowers/plans/missing-status.md", result.detail)
        self.assertIn("docs/superpowers/README.md", result.detail)

    def test_superpowers_status_rejects_nested_duplicate_and_unclosed_values(
        self,
    ) -> None:
        self.create_superpowers()
        self.write(
            "docs/superpowers/plans/nested.md",
            "---\ntitle: Nested\nmetadata:\n  status: active\n---\n",
        )
        self.write(
            "docs/superpowers/plans/duplicate.md",
            "---\ntitle: Duplicate\nstatus: active\nstatus: draft\n---\n",
        )
        self.write(
            "docs/superpowers/plans/unclosed.md",
            "---\ntitle: Unclosed\nstatus: active\n# Missing closing fence\n",
        )

        result = docs_harness.check_superpowers_statuses(self.repo_root)

        self.assertFalse(result.ok)
        self.assertIn("docs/superpowers/plans/nested.md", result.detail)
        self.assertIn("docs/superpowers/plans/duplicate.md", result.detail)
        self.assertIn("docs/superpowers/plans/unclosed.md", result.detail)

    def test_superpowers_index_requires_readme(self) -> None:
        self.write("docs/superpowers/plans/current.md", frontmatter())

        result = docs_harness.check_superpowers_index(self.repo_root)

        self.assertFalse(result.ok)
        self.assertIn("docs/superpowers/README.md", result.detail)

    def test_superpowers_index_checks_both_directions(self) -> None:
        self.create_superpowers()
        self.write("docs/superpowers/explore/unlisted.md", frontmatter())
        readme = (self.repo_root / "docs/superpowers/README.md").read_text(
            encoding="utf-8"
        )
        self.write(
            "docs/superpowers/README.md",
            readme + "- [Missing plan](plans/missing.md): Missing plan.\n",
        )

        result = docs_harness.check_superpowers_index(self.repo_root)

        self.assertFalse(result.ok)
        self.assertIn("docs/superpowers/explore/unlisted.md", result.detail)
        self.assertIn("docs/superpowers/plans/missing.md", result.detail)

    def test_superpowers_index_ignores_paths_in_prose(self) -> None:
        self.create_superpowers()
        self.write("docs/superpowers/explore/unlisted.md", frontmatter())
        readme_path = self.repo_root / "docs/superpowers/README.md"
        readme = readme_path.read_text(encoding="utf-8")
        self.write(
            "docs/superpowers/README.md",
            readme + "\nDelete explore/unlisted.md after completion.\n",
        )

        result = docs_harness.check_superpowers_index(self.repo_root)

        self.assertFalse(result.ok)
        self.assertIn("docs/superpowers/explore/unlisted.md", result.detail)

    def test_superpowers_references_reject_deleted_local_document(self) -> None:
        self.create_superpowers()
        (self.repo_root / "docs/superpowers/specs/draft.md").unlink()
        readme_path = self.repo_root / "docs/superpowers/README.md"
        readme = readme_path.read_text(encoding="utf-8")
        self.write(
            "docs/superpowers/README.md",
            readme.replace(
                "- [Draft spec](specs/draft.md): Draft spec.\n",
                "",
            ),
        )

        result = docs_harness.check_superpowers_references(self.repo_root)

        self.assertFalse(result.ok)
        self.assertIn("docs/superpowers/specs/draft.md", result.detail)

    def test_superpowers_checks_accept_current_tree(self) -> None:
        self.create_superpowers()

        results = [
            docs_harness.check_superpowers_layout(self.repo_root),
            docs_harness.check_superpowers_statuses(self.repo_root),
            docs_harness.check_superpowers_index(self.repo_root),
            docs_harness.check_superpowers_references(self.repo_root),
        ]

        self.assertTrue(all(result.ok for result in results), results)


if __name__ == "__main__":
    unittest.main()
