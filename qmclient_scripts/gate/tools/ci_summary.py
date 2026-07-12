# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""Write GitHub Actions step summaries for QmClient CI gates."""

import argparse
import json
import re
from collections import Counter
from pathlib import Path


REPO_MARKER = "QmClient/QmClient/"
FIRST_PARTY_EXCLUDE_RE = re.compile(
    r"[/\\](engine[/\\]external|rust-bridge[/\\]base[/\\]cxx\.h|generated)([/\\:]|$)"
)
CLANG_TIDY_WARNING_RE = re.compile(r"warning: .*\[[^]]+\]")
ERROR_RE = re.compile(r"(^|[ \t])error:")
SANITIZER_RE = re.compile(
    r"(ERROR: AddressSanitizer|ERROR: LeakSanitizer|runtime error:|UndefinedBehaviorSanitizer)"
)
SANITIZER_ERROR_RE = re.compile(
    r"(^FAILED:|^CMake Error|^ninja: build stopped:|^[^:\s][^:]*:\d+(?::\d+)?: error:| error C\d+:)"
)


def read_text(path):
    if path is None or not path.exists():
        return ""
    return path.read_text(encoding="utf-8", errors="replace")


def tail_lines(text, limit):
    lines = [line.rstrip() for line in text.splitlines() if line.strip()]
    return lines[-limit:]


def append_or_print(path, text):
    if path:
        with path.open("a", encoding="utf-8", newline="\n") as out:
            out.write(text)
    else:
        print(text, end="")


def normalize_line(line):
    line = line.replace("\\", "/").strip()
    if REPO_MARKER in line:
        line = line.split(REPO_MARKER, 1)[1]
    line = re.sub(r"^.*?/QmClient/", "", line)
    return line


def is_first_party_line(line):
    normalized = line.replace("\\", "/")
    if FIRST_PARTY_EXCLUDE_RE.search(normalized):
        return False
    return (
        "/src/" in normalized
        or normalized.startswith("src/")
        or "QmClient/QmClient/src/" in normalized
    )


def status_label(status):
    if not status:
        return "未知"
    return {
        "success": "成功",
        "failure": "失败",
        "cancelled": "已取消",
        "in_progress": "进行中",
        "queued": "排队中",
    }.get(status, status)


def report_status_label(status):
    return {
        "present": "已生成",
        "missing": "缺失",
        "invalid": "损坏",
    }.get(status, status)


def render_table(rows):
    if not rows:
        return ""
    lines = ["| 项目 | 数量 |", "| --- | ---: |"]
    lines.extend(f"| {name} | {count} |" for name, count in rows)
    return "\n".join(lines) + "\n"


def summarize_style(args):
    report = {}
    report_status = "missing"
    if args.report and args.report.exists():
        try:
            report = json.loads(read_text(args.report))
            report_status = "present"
        except json.JSONDecodeError:
            report_status = "invalid"
    log_text = read_text(args.log)
    summary = report.get("Summary", {})
    items = report.get("Items", [])
    scoped = report.get("ScopedFiles", [])
    failures = [item for item in items if item.get("Level") == "FAIL"]
    warnings = [item for item in items if item.get("Level") == "WARN"]

    lines = [
        "## Check style 摘要",
        "",
        f"- 状态: {status_label(args.status)}",
        f"- 报告: {report_status_label(report_status)}",
        f"- 模式: {report.get('Mode', '未知')}",
        f"- 范围文件数: {len(scoped)}",
        f"- 通过 / 警告 / 失败: {summary.get('Pass', 0)} / {summary.get('Warn', 0)} / {summary.get('Fail', 0)}",
    ]
    if failures:
        lines.extend(["", "### 失败项"])
        for item in failures[:10]:
            lines.append(
                f"- {item.get('Title', 'unknown')}: {item.get('Detail', '').splitlines()[0]}"
            )
    if warnings:
        lines.extend(["", "### 警告项"])
        for item in warnings[:10]:
            lines.append(
                f"- {item.get('Title', 'unknown')}: {item.get('Detail', '').splitlines()[0]}"
            )
    if scoped:
        lines.extend(["", "### 范围文件"])
        lines.extend(f"- {path}" for path in scoped[:30])
        if len(scoped) > 30:
            lines.append(f"- ... 以及另外 {len(scoped) - 30} 个文件")
    if report_status != "present" and log_text:
        lines.extend(["", "### 日志尾部"])
        lines.extend(f"- {line}" for line in tail_lines(log_text, 20))
    lines.append("")
    return "\n".join(lines)


def summarize_clang_tidy(args):
    text = read_text(args.log)
    log_present = args.log is not None and args.log.exists()
    lines_in = text.splitlines()
    warning_lines = [
        line
        for line in lines_in
        if CLANG_TIDY_WARNING_RE.search(line) and is_first_party_line(line)
    ]
    error_lines = [
        line for line in lines_in if ERROR_RE.search(line) and is_first_party_line(line)
    ]
    unique_warnings = sorted(
        {
            normalize_line(
                re.sub(r"^(.+?:\d+:\d+: warning: .*\[[^]]+\]).*$", r"\1", line)
            )
            for line in warning_lines
        }
    )
    checks = Counter()
    files = Counter()
    for line in unique_warnings:
        check_match = re.search(r"\[([^]]+)\]$", line)
        if check_match:
            checks[check_match.group(1)] += 1
        file_match = re.match(r"([^:]+):\d+:\d+:", line)
        if file_match:
            files[file_match.group(1)] += 1

    other_warnings = []
    for line in lines_in:
        if "warning:" not in line and "CMake Warning" not in line:
            continue
        if CLANG_TIDY_WARNING_RE.search(line):
            continue
        other_warnings.append(normalize_line(line))

    lines = [
        "## clang-tidy 摘要",
        "",
        f"- 状态: {status_label(args.status)}",
        f"- 日志: {'已生成' if log_present else '缺失'}",
        f"- 首方错误数: {len(error_lines)}",
        f"- 首方原始警告数: {len(warning_lines)}",
        f"- 首方去重警告数: {len(unique_warnings)}",
    ]
    if checks:
        lines.extend(
            ["", "### 高频检查项", render_table(checks.most_common(10)).rstrip()]
        )
    if files:
        lines.extend(["", "### 高频文件", render_table(files.most_common(10)).rstrip()])
    if error_lines:
        lines.extend(["", "### 错误样例"])
        lines.extend(f"- {normalize_line(line)}" for line in error_lines[:10])
    if unique_warnings:
        lines.extend(["", "### 警告样例"])
        lines.extend(f"- {line}" for line in unique_warnings[:15])
    if other_warnings:
        lines.extend(["", "### 其他警告样例"])
        for line in other_warnings[:15]:
            lines.append(f"- {line}")
    lines.append("")
    return "\n".join(lines)


def summarize_sanitizer(args):
    present_logs = [path for path in args.logs if path.exists()]
    log_text = "\n".join(read_text(path) for path in args.logs if path.exists())
    lines_in = log_text.splitlines()
    sanitizer_lines = [
        normalize_line(line) for line in lines_in if SANITIZER_RE.search(line)
    ]
    warning_lines = [
        normalize_line(line)
        for line in lines_in
        if "warning:" in line or "CMake Warning" in line
    ]
    error_lines = [
        normalize_line(line) for line in lines_in if SANITIZER_ERROR_RE.search(line)
    ]
    warning_kinds = Counter()
    for line in warning_lines:
        if "CMake Warning" in line:
            warning_kinds["CMake 警告"] += 1
        elif "#warning" in line:
            warning_kinds["预处理警告"] += 1
        else:
            warning_kinds["编译器警告"] += 1

    lines = [
        "## ASan & UBSan 摘要",
        "",
        f"- 状态: {status_label(args.status)}",
        f"- 日志: 已生成 {len(present_logs)} / {len(args.logs)}",
        f"- Sanitizer 命中数: {len(sanitizer_lines)}",
        f"- 错误行数: {len(error_lines)}",
        f"- 警告行数: {len(warning_lines)}",
    ]
    if warning_kinds:
        lines.extend(
            ["", "### 警告类型", render_table(warning_kinds.most_common()).rstrip()]
        )
    if sanitizer_lines:
        lines.extend(["", "### Sanitizer 样例"])
        lines.extend(f"- {line}" for line in sanitizer_lines[:20])
    if error_lines:
        lines.extend(["", "### 错误样例"])
        lines.extend(f"- {line}" for line in error_lines[:15])
    if warning_lines:
        lines.extend(["", "### 警告样例"])
        lines.extend(f"- {line}" for line in warning_lines[:20])
    lines.append("")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Write GitHub Actions step summaries for QmClient CI gates."
    )
    parser.add_argument(
        "--kind", choices=("style", "clang-tidy", "sanitizer"), required=True
    )
    parser.add_argument("--status", default="")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--log", type=Path)
    parser.add_argument("--logs", nargs="*", type=Path, default=[])
    args = parser.parse_args()

    if args.kind == "style":
        text = summarize_style(args)
    elif args.kind == "clang-tidy":
        text = summarize_clang_tidy(args)
    else:
        text = summarize_sanitizer(args)
    append_or_print(args.output, text)


if __name__ == "__main__":
    main()
