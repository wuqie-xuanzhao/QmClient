from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import subprocess
import unittest


REPO_ROOT = Path(__file__).resolve().parents[2]
THIS_FILE = Path(__file__).resolve()


@dataclass(frozen=True)
class TrackedEntry:
    mode: str
    path: Path


def tracked_entries() -> list[TrackedEntry]:
    result = subprocess.run(
        ["git", "ls-files", "--stage", "-z"],
        cwd=REPO_ROOT,
        check=True,
        capture_output=True,
    )
    entries: list[TrackedEntry] = []
    for record in result.stdout.decode("utf-8", errors="surrogateescape").split("\0"):
        if not record:
            continue
        metadata, path = record.split("\t", 1)
        mode = metadata.split(" ", 1)[0]
        entries.append(TrackedEntry(mode=mode, path=REPO_ROOT / path))
    return entries


def tracked_text_files() -> list[Path]:
    return [entry.path for entry in tracked_entries() if entry.mode != "160000"]


class RemovedQmLiveAndSuperpowersTest(unittest.TestCase):
    def test_agent_workflow_stays_removed(self) -> None:
        self.assertFalse((REPO_ROOT / ".agents").exists())

        tracked_agent_paths = [
            entry.path.relative_to(REPO_ROOT).as_posix()
            for entry in tracked_entries()
            if entry.path.relative_to(REPO_ROOT).as_posix().startswith(".agents/")
        ]
        self.assertEqual(tracked_agent_paths, [], ".agents files must remain untracked")

        matches: list[str] = []
        for path in tracked_text_files():
            if path.resolve() in {THIS_FILE, REPO_ROOT / ".gitignore"} or not path.is_file():
                continue
            try:
                content = path.read_text(encoding="utf-8").lower()
            except (UnicodeDecodeError, OSError):
                continue
            if ".agents/" in content or ".agents\\" in content:
                matches.append(path.relative_to(REPO_ROOT).as_posix())

        self.assertEqual(matches, [], ".agents workflow references must remain removed")

    def test_qmlive_sources_and_references_stay_removed(self) -> None:
        markers = ("qm_live", "qmlive", "conf_qm_live", "netmsg_qm_live")
        matches: list[str] = []

        for path in tracked_text_files():
            if path.resolve() == THIS_FILE or not path.is_file():
                continue
            try:
                content = path.read_text(encoding="utf-8").lower()
            except (UnicodeDecodeError, OSError):
                continue
            if any(marker in content for marker in markers):
                matches.append(path.relative_to(REPO_ROOT).as_posix())

        self.assertEqual(matches, [], "QmLive references must remain removed")

    def test_superpowers_workflow_stays_removed(self) -> None:
        self.assertFalse((REPO_ROOT / "docs" / "superpowers").exists())
        documentation_gitlinks = [
            entry.path.relative_to(REPO_ROOT).as_posix()
            for entry in tracked_entries()
            if entry.mode == "160000"
            and entry.path.relative_to(REPO_ROOT).as_posix().startswith("docs/")
        ]
        self.assertEqual(
            documentation_gitlinks,
            [],
            "documentation gitlinks must not hide removed superpowers references",
        )

        matches: list[str] = []
        for path in tracked_text_files():
            if path.resolve() == THIS_FILE or not path.is_file():
                continue
            try:
                content = path.read_text(encoding="utf-8").lower()
            except (UnicodeDecodeError, OSError):
                continue
            if "docs/superpowers" in content or "docs\\superpowers" in content:
                matches.append(path.relative_to(REPO_ROOT).as_posix())

        self.assertEqual(matches, [], "docs/superpowers workflow references must remain removed")


if __name__ == "__main__":
    unittest.main()
