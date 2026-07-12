# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""CMake 测试目标执行。"""

from __future__ import annotations

from pathlib import Path

from lib import runner
from lib.report import ResultCollector

REPO_ROOT = Path(__file__).resolve().parents[3]
CXX_TEST_SKIPPED_MARKER = "[  SKIPPED ]"


def _add_cxx_skip_warning(results: ResultCollector, out: str) -> None:
    if CXX_TEST_SKIPPED_MARKER in out:
        results.add("WARN", "C++ tests skipped", out)


def _run_cmake_target(
    target: str,
    build_dir: str,
    title: str,
) -> tuple[int, str]:
    cmake_script = REPO_ROOT / "qmclient_scripts" / "cmake-windows.cmd"
    if runner.resolve_cmake_command() == "cmd.exe" and cmake_script.exists():
        cmd = [
            "cmd.exe",
            "/c",
            runner.to_windows_path(str(cmake_script)),
            "--build",
            build_dir,
            "--target",
            target,
            "-j",
            runner.resolve_parallel_jobs(),
        ]
    else:
        cmd = [
            "cmake",
            "--build",
            build_dir,
            "--target",
            target,
            "-j",
            runner.resolve_parallel_jobs(),
        ]
    return runner.run(cmd, title=title, check=False)


def run(
    results: ResultCollector,
    included: list[str],
    dry_run: bool = False,
    build_dir: str = "cmake-build-release",
    run_all: bool = False,
    run_cxx: bool = False,
    run_rust: bool = False,
) -> None:
    if dry_run:
        results.add("INFO", "CMake 测试目标", "DryRun，仅展示命令")
        return
    if run_all:
        code, out = _run_cmake_target("run_tests", build_dir, "CMake run_tests")
        if code != 0:
            results.add("FAIL", "CMake run_tests", out)
        else:
            results.add("PASS", "CMake run_tests", "通过")
            _add_cxx_skip_warning(results, out)
        return
    if run_cxx:
        code, out = _run_cmake_target("run_cxx_tests", build_dir, "CMake run_cxx_tests")
        if code != 0:
            results.add("FAIL", "CMake run_cxx_tests", out)
        else:
            results.add("PASS", "CMake run_cxx_tests", "通过")
            _add_cxx_skip_warning(results, out)
    if run_rust:
        code, out = _run_cmake_target(
            "run_rust_tests", build_dir, "CMake run_rust_tests"
        )
        if code != 0:
            results.add("FAIL", "CMake run_rust_tests", out)
        else:
            results.add("PASS", "CMake run_rust_tests", "通过")
