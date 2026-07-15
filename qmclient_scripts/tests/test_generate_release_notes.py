#!/usr/bin/env python3
"""generate_release_notes 通道与渲染头的轻量单测（不依赖完整 git 历史）。"""

from __future__ import annotations

import unittest

from qmclient_scripts.generate_release_notes import (
    detect_channel,
    render_header,
    render_markdown,
)


class DetectChannelTests(unittest.TestCase):
    def test_stable_tags(self) -> None:
        self.assertEqual(detect_channel("v2.74.9"), "stable")
        self.assertEqual(detect_channel("2.74.9"), "stable")

    def test_pre_release_tags(self) -> None:
        self.assertEqual(detect_channel("nightly"), "pre-release")
        self.assertEqual(detect_channel("v2.0.0-rc1"), "pre-release")
        self.assertEqual(detect_channel("19.9-rc1"), "pre-release")
        self.assertEqual(detect_channel("v3.0.0-beta.1"), "pre-release")


class RenderHeaderTests(unittest.TestCase):
    def test_stable_header_mentions_channel(self) -> None:
        lines = render_header(
            channel="stable",
            version="v2.74.9",
            current_tag="v2.74.9",
            previous_tag="v2.74.8",
            commit=None,
            built_at=None,
            branch=None,
        )
        text = "\n".join(lines)
        self.assertIn("正式版", text)
        self.assertIn("Stable", text)
        self.assertIn("v2.74.8..v2.74.9", text)

    def test_pre_release_header_warns(self) -> None:
        lines = render_header(
            channel="pre-release",
            version="nightly",
            current_tag="nightly",
            previous_tag="v2.74.9",
            commit="abc123",
            built_at="2026-07-15 00:00:00 UTC",
            branch="main",
        )
        text = "\n".join(lines)
        self.assertIn("预发布", text)
        self.assertIn("不建议当主力", text)
        self.assertIn("abc123", text)

    def test_empty_notes_markdown(self) -> None:
        md = render_markdown(
            version="v2.74.9",
            current_tag="v2.74.9",
            previous_tag="v2.74.8",
            notes=[],
            channel="stable",
        )
        self.assertIn("正式版", md)
        self.assertIn("完整变更", md)


if __name__ == "__main__":
    unittest.main()
