#!/usr/bin/env python3
"""QmClient 仓库级门禁总入口（Python 版）。

纯编排层：只负责模式定义、参数解析、范围收集和检查调度。
具体检查逻辑下沉到 checks/ 各模块中。
"""

from __future__ import annotations

import argparse
import sys
import traceback
from pathlib import Path

# 确保 gate/lib 和 gate/checks 在路径上
SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

from checks import (  # noqa: E402
    clang_format,
    clang_tidy_warn,
    ci_build,
    config_vars,
    dilate,
    docs,
    env,
    headers,
    identifiers,
    python,
    shell,
    strict_build,
    style,
    tests,
)
from lib import runner, scope  # noqa: E402
from lib.report import ResultCollector  # noqa: E402

REPO_ROOT = SCRIPT_DIR.parents[1]

# ------------------------------------------------------------------
# 模式定义
# ------------------------------------------------------------------

_MODE_SPECS: dict[str, dict] = {
    "quick": {
        "target": "开发期快速自查",
        "expectation": "通常应在数分钟内完成，只扫源码卫生层。",
        "blocking_rule": "只阻断明显的脚本/规范问题，不做真实构建与测试。",
        "checks": [
            "env",
            "config_vars",
            "docs",
            "headers",
            "style",
            "python",
            "shell",
        ],
        "tests": {"cxx": False, "rust": False, "all": False},
        "extras": {
            "identifiers": False,
            "clang_format": False,
            "clang_tidy_warn": False,
            "strict_build": False,
            "dilate": False,
        },
    },
    "default": {
        "target": "日常提交前严格门",
        "expectation": "运行 quick 层源码卫生检查，并覆盖 C++ 全量测试和 Rust 全量测试。",
        "blocking_rule": "quick 层检查或全量测试任一失败都应阻断。",
        "checks": [
            "env",
            "config_vars",
            "docs",
            "headers",
            "style",
            "python",
            "shell",
        ],
        "tests": {"cxx": True, "rust": True, "all": False},
        "extras": {
            "identifiers": False,
            "clang_format": False,
            "clang_tidy_warn": False,
        },
    },
    "full": {
        "target": "集中收口 / 准发布门",
        "expectation": "在 default 基础上增加更重的附加检查。",
        "blocking_rule": "默认阻断 default 层和 full 的硬失败项；高噪音附加检查先以 WARN 方式试跑。",
        "checks": [
            "env",
            "config_vars",
            "docs",
            "headers",
            "style",
            "python",
            "shell",
            "strict_build",
            "dilate",
        ],
        "tests": {"cxx": True, "rust": True, "all": False},
        "extras": {
            "identifiers": True,
            "clang_format": False,
            "clang_tidy_warn": True,
        },
    },
    "build": {
        "target": "CI 等价构建验证",
        "expectation": "在本地复现 CI 的 clang-tidy 和 sanitizer 流程。耗时较长，仅在需要验证 CI 行为时执行。",
        "blocking_rule": "任一构建失败即阻断。",
        "checks": [
            "env",
            "config_vars",
            "headers",
            "style",
            "python",
            "shell",
            "ci_build",
        ],
        "tests": {"cxx": False, "rust": False, "all": False},
        "extras": {},
    },
}

_CHECK_MAP = {
    "env": env,
    "config_vars": config_vars,
    "docs": docs,
    "headers": headers,
    "style": style,
    "python": python,
    "shell": shell,
    "strict_build": strict_build,
    "dilate": dilate,
    "identifiers": identifiers,
    "clang_format": clang_format,
    "clang_tidy_warn": clang_tidy_warn,
    "ci_build": ci_build,
}


# ------------------------------------------------------------------
# 参数解析
# ------------------------------------------------------------------


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="QmClient 仓库级门禁总入口")
    parser.add_argument("--build-dir", default="cmake-build-release")
    parser.add_argument(
        "--base-ref",
        default=None,
        help="差异基线；默认使用 origin/HEAD，缺失时依次回退 origin/master、origin/main、master、main",
    )
    parser.add_argument(
        "--mode", choices=["quick", "default", "full", "build"], default="default"
    )
    parser.add_argument("--skip-ci-build", action="store_true")
    parser.add_argument("--skip-preflight", action="store_true")
    parser.add_argument("--skip-config-checks", action="store_true")
    parser.add_argument("--skip-workflow-docs", action="store_true")
    parser.add_argument("--skip-header-checks", action="store_true")
    parser.add_argument("--skip-style-check", action="store_true")
    parser.add_argument("--skip-strict-debug", action="store_true")
    parser.add_argument("--skip-cxx-tests", action="store_true")
    parser.add_argument("--run-all-tests", action="store_true")
    parser.add_argument("--include-identifier-check", action="store_true")
    parser.add_argument("--enable-clang-format-check", action="store_true")
    parser.add_argument("--enable-full-clang-tidy-warn", action="store_true")
    parser.add_argument("--skip-ruff-check", action="store_true")
    parser.add_argument("--skip-shell-check", action="store_true")
    parser.add_argument("--skip-dilate-check", action="store_true")
    parser.add_argument(
        "--ci-mode",
        action="store_true",
        help="CI 模式：跳过本地-only 检查（.ai 文档同步等）",
    )
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--explain-scope", action="store_true")
    parser.add_argument("--branch-scope-only", action="store_true")
    parser.add_argument("--report-json-path")
    parser.add_argument("--scope-report-path")
    return parser.parse_args()


# ------------------------------------------------------------------
# 调度逻辑
# ------------------------------------------------------------------


def _should_run_check(name: str, mode_spec: dict, args: argparse.Namespace) -> bool:
    """根据模式定义和命令行开关，判断某个检查是否应该执行。"""
    # 显式跳过参数
    skip_map = {
        "env": args.skip_preflight,
        "config_vars": args.skip_config_checks,
        "docs": args.skip_workflow_docs or args.ci_mode,
        "headers": args.skip_header_checks,
        "style": args.skip_style_check,
        "python": args.skip_ruff_check,
        "shell": args.skip_shell_check,
        "strict_build": args.skip_strict_debug,
        "dilate": args.skip_dilate_check,
        "ci_build": args.skip_ci_build,
    }
    if skip_map.get(name, False):
        return False

    # 显式启用参数（用于 extras）
    enable_map = {
        "identifiers": args.include_identifier_check,
        "clang_format": args.enable_clang_format_check,
        "clang_tidy_warn": args.enable_full_clang_tidy_warn,
    }

    if name in mode_spec.get("checks", []):
        return True
    if name in mode_spec.get("extras", {}):
        return mode_spec["extras"][name] or enable_map.get(name, False)
    return False


def _finalize_run(
    args: argparse.Namespace,
    mode_spec: dict,
    results: ResultCollector,
    scoped_files: list[str],
) -> None:
    report_path = Path(args.report_json_path) if args.report_json_path else None
    results.write_json(
        report_path,
        mode=args.mode,
        mode_spec={
            "Name": args.mode,
            "Target": mode_spec["target"],
            "Expectation": mode_spec["expectation"],
            "BlockingRule": mode_spec["blocking_rule"],
        },
        scoped_files=scoped_files,
    )
    results.write_summary(
        mode=args.mode,
        mode_target=mode_spec["target"],
        mode_expectation=mode_spec["expectation"],
        mode_blocking_rule=mode_spec["blocking_rule"],
    )


def main() -> int:
    args = _parse_args()
    if args.base_ref is None:
        args.base_ref = scope.default_base_ref()

    if args.mode not in _MODE_SPECS:
        print(f"未知 mode: {args.mode}", file=sys.stderr)
        return 2
    mode_spec = _MODE_SPECS[args.mode]

    allowlist_path = (
        REPO_ROOT / "qmclient_scripts" / "gate" / "baseline_debt_allowlist.json"
    )
    results = ResultCollector(allowlist_path if allowlist_path.exists() else None)
    scoped_files: list[str] = []

    try:
        # 范围收集
        sc = scope.collect_scope(args.base_ref, args.branch_scope_only)
        scoped_files = sc.included
        results.add(
            "INFO",
            "差异范围统计",
            f"branch={len(sc.branch)}, unstaged={len(sc.unstaged)}, staged={len(sc.staged)}, "
            f"untracked={len(sc.untracked)}, included={len(sc.included)}, excluded={len(sc.excluded)}",
        )
        if not sc.base_ref_available:
            msg = f"差异基线不可用: {args.base_ref}"
            if sc.base_ref_failure_reason:
                msg += f" ({sc.base_ref_failure_reason})"
            results.add("WARN", "差异基线检查", msg)

        if args.explain_scope:
            runner.print_section("差异范围说明")
            print(f"BaseRef: {args.base_ref}")
            print(f"BaseRef 可用: {sc.base_ref_available}")
            if sc.base_ref_failure_reason:
                print(f"BaseRef 失败原因: {sc.base_ref_failure_reason}")
            print(f"纳入首方范围文件数: {len(sc.included)}")
            print(f"排除文件数: {len(sc.excluded)}")

        scope_report_path = (
            Path(args.scope_report_path) if args.scope_report_path else None
        )
        results.write_scope_json(
            scope_report_path,
            args.base_ref,
            sc.base_ref_available,
            sc.base_ref_failure_reason,
            sc.included,
            sc.excluded,
        )

        # 检查调度
        for name in _CHECK_MAP:
            if not _should_run_check(name, mode_spec, args):
                continue
            check_module = _CHECK_MAP[name]
            if name == "strict_build":
                check_module.run(
                    results, sc.included, args.dry_run, base_ref=args.base_ref
                )
            else:
                check_module.run(results, sc.included, args.dry_run)

        # 测试调度
        test_spec = mode_spec.get("tests", {})
        run_cxx = (
            test_spec.get("cxx", False)
            and not args.skip_cxx_tests
            and not args.run_all_tests
        )
        run_rust = test_spec.get("rust", False) and not args.run_all_tests
        run_all = args.run_all_tests
        if run_cxx or run_rust or run_all:
            tests.run(
                results,
                sc.included,
                dry_run=args.dry_run,
                build_dir=args.build_dir,
                run_all=run_all,
                run_cxx=run_cxx,
                run_rust=run_rust,
            )
    except KeyboardInterrupt:
        results.add("FAIL", "Gate 入口中断", "收到 KeyboardInterrupt，已提前终止。")
    except Exception:
        results.add("FAIL", "Gate 入口异常", traceback.format_exc())
    finally:
        try:
            _finalize_run(args, mode_spec, results, scoped_files)
        except Exception:
            print("\n[FAIL] Gate 收尾异常", file=sys.stderr)
            print(traceback.format_exc(), file=sys.stderr)

    print("\n仓库级检查完成。")
    return 1 if results.has_failures() else 0


if __name__ == "__main__":
    sys.exit(main())
