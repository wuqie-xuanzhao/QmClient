#!/usr/bin/env python3
"""Dilate 图像检查。"""

from __future__ import annotations

from pathlib import Path

from lib import runner
from lib.report import ResultCollector

REPO_ROOT = Path(__file__).resolve().parents[3]


def run(results: ResultCollector, included: list[str], dry_run: bool = False) -> None:
    if dry_run:
        results.add("INFO", "Dilate 图像检查", "DryRun，仅展示命令")
        return
    build_dir = REPO_ROOT / "cmake-build-dilate"
    build_dir.mkdir(exist_ok=True)
    cmake_script = REPO_ROOT / "qmclient_scripts" / "cmake-windows.cmd"
    if runner.resolve_cmake_command() == "cmd.exe" and cmake_script.exists():
        configure_cmd = [
            "cmd.exe",
            "/c",
            runner.to_windows_path(str(cmake_script)),
            "-G",
            "Ninja",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DCLIENT=OFF",
            "-DDOWNLOAD_GTEST=OFF",
            "-S",
            str(REPO_ROOT),
            "-B",
            str(build_dir),
        ]
        build_cmd = [
            "cmd.exe",
            "/c",
            runner.to_windows_path(str(cmake_script)),
            "--build",
            str(build_dir),
            "--config",
            "Release",
            "--target",
            "dilate",
            "-j",
            runner.resolve_parallel_jobs(),
        ]
    else:
        configure_cmd = [
            "cmake",
            "-G",
            "Ninja",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DCLIENT=OFF",
            "-DDOWNLOAD_GTEST=OFF",
            "-S",
            str(REPO_ROOT),
            "-B",
            str(build_dir),
        ]
        build_cmd = [
            "cmake",
            "--build",
            str(build_dir),
            "--config",
            "Release",
            "--target",
            "dilate",
            "-j",
            runner.resolve_parallel_jobs(),
        ]
    code, out = runner.run(
        configure_cmd,
        title="Build dilate tool (configure)",
        check=False,
    )
    if code != 0:
        results.add("FAIL", "Build dilate tool (configure)", out)
        return
    code, out = runner.run(
        build_cmd,
        title="Build dilate tool (build)",
        check=False,
    )
    if code != 0:
        results.add("FAIL", "Build dilate tool (build)", out)
        return
    results.add("PASS", "Build dilate tool", "通过")
    code, out = runner.run(
        [
            "python",
            str(REPO_ROOT / "qmclient_scripts" / "check_dilate.py"),
            str(build_dir),
            str(REPO_ROOT / "data"),
        ],
        title="Check dilated images",
        check=False,
    )
    if code != 0:
        results.add("FAIL", "Check dilated images", out)
        return
    results.add("PASS", "Check dilated images", "通过")
