# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""
QmClient 代码格式化工具

基于上游 scripts/fix_style.py 适配，添加了 QmClient 特有的忽略规则。
与上游版本的区别：
  - 添加了 ChaiScript 脚本目录到忽略列表
  - 添加了 QmClient 数据目录到忽略列表
  - 支持 --qm-only 参数仅格式化 QmClient 特有文件

用法：
  python qmclient_scripts/fix_style.py              # 格式化所有文件
  python qmclient_scripts/fix_style.py --dry-run    # 仅检查，不修改
  python qmclient_scripts/fix_style.py --qm-only    # 仅格式化 QmClient 特有文件
"""

import os
import subprocess
import sys
import argparse
import re

os.chdir(os.path.dirname(__file__) + "/..")


def recursive_file_list(path):
    result = []
    for dirpath, _, filenames in os.walk(path):
        result += [os.path.join(dirpath, filename) for filename in filenames]
    return result


IGNORE_FILES = []

IGNORE_DIRS = [
    "src/game/generated",
    "src/rust-bridge",
]

QMCLIENT_DIRS = [
    "src/game/client/components/qmclient",
    "src/game/client/components/tclient",
    "src/game/client/QmUi",
    "src/engine/shared/config_variables_tclient.h",
    "src/engine/shared/config_variables_qmclient.h",
    "src/engine/shared/config_variables_qimeng.h",
]


def filter_ignored(filenames):
    result = []
    for filename in filenames:
        real_filename = os.path.realpath(filename)
        if real_filename not in [
            os.path.realpath(ignore_file) for ignore_file in IGNORE_FILES
        ] and not any(
            real_filename.startswith(os.path.realpath(subdir) + os.path.sep)
            for subdir in IGNORE_DIRS
        ):
            result.append(filename)
    return result


def filter_qmclient(filenames):
    result = []
    for filename in filenames:
        real_filename = os.path.realpath(filename)
        if any(
            real_filename.startswith(os.path.realpath(d) + os.path.sep)
            for d in QMCLIENT_DIRS[:3]
        ):
            result.append(filename)
        elif any(real_filename == os.path.realpath(d) for d in QMCLIENT_DIRS[3:]):
            result.append(filename)
    return result


def filter_cpp(filenames):
    return [
        filename
        for filename in filenames
        if any(filename.endswith(ext) for ext in ".c .cc .cpp .h .hpp".split())
    ]


def normalize_input_filenames(filenames):
    return [os.path.normpath(filename) for filename in filenames]


def chunked_for_command(base_args, filenames, max_chars=28000):
    current = []
    current_len = sum(len(arg) + 3 for arg in base_args)
    for filename in filenames:
        filename_len = len(filename) + 3
        if current and current_len + filename_len > max_chars:
            yield current
            current = []
            current_len = sum(len(arg) + 3 for arg in base_args)
        current.append(filename)
        current_len += filename_len
    if current:
        yield current


def find_clang_format(version):
    versioned_binaries = [f"clang-format-{major}" for major in range(version, 31)]
    for binary in (
        "clang-format",
        f"clang-format-{version}",
        f"/opt/clang-format-static/clang-format-{version}",
        *versioned_binaries,
    ):
        try:
            out = subprocess.check_output([binary, "--version"])
        except FileNotFoundError:
            continue
        version_text = out.decode("utf-8")
        match = re.search(r"clang-format version (\d+)\.", version_text)
        if not match:
            continue
        major = int(match.group(1))
        if major >= version:
            if major != version:
                print(
                    f"Using clang-format {major} for required minimum {version}",
                    file=sys.stderr,
                )
            return binary
    print(f"Found no clang-format >= {version}")
    sys.exit(-1)


clang_format_bin = find_clang_format(20)


def reformat(filenames):
    for filename in filenames:
        with open(filename, "r+b") as f:
            try:
                f.seek(-1, os.SEEK_END)
                if f.read(1) != b"\n":
                    f.write(b"\n")
            except OSError:
                f.seek(0)
    base_args = [clang_format_bin, "-i"]
    for batch in chunked_for_command(base_args, filenames):
        subprocess.check_call(base_args + batch)


def warn(filenames):
    clang = 0
    base_args = [clang_format_bin, "-Werror", "--dry-run"]
    for batch in chunked_for_command(base_args, filenames):
        clang = subprocess.call(base_args + batch) or clang
    newline = 0
    for filename in filenames:
        with open(filename, "rb") as f:
            try:
                f.seek(-1, os.SEEK_END)
                if f.read(1) != b"\n":
                    print(filename + ": error: missing newline at EOF", file=sys.stderr)
                    newline = 1
            except OSError:
                f.seek(0)
    return clang or newline


def main():
    p = argparse.ArgumentParser(
        description="Check and fix style of QmClient source files"
    )
    p.add_argument("-n", "--dry-run", action="store_true", help="Don't fix, only warn")
    p.add_argument(
        "--qm-only", action="store_true", help="Only check QmClient-specific files"
    )
    p.add_argument(
        "files",
        nargs="*",
        help="Optional explicit file list to check instead of scanning src/",
    )
    args = p.parse_args()
    if args.files:
        filenames = normalize_input_filenames(args.files)
    else:
        filenames = recursive_file_list("src")
    filenames = filter_ignored(filter_cpp(filenames))
    if args.qm_only:
        filenames = filter_qmclient(filenames)
    if not args.dry_run:
        reformat(filenames)
    else:
        sys.exit(warn(filenames))


if __name__ == "__main__":
    main()
