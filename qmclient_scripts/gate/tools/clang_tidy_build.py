# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""QmClient clang-tidy 独立工具。"""

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
CLANG_VERSION = os.environ.get("CLANG_VERSION", "22")
BUILD_DIR = REPO_ROOT / os.environ.get("CLANG_TIDY_BUILD_DIR", "build-clang-tidy")
BUILD_TARGET = os.environ.get("CLANG_TIDY_TARGET", "game-client")
COMPILER_MODE = os.environ.get(
    "CLANG_TIDY_COMPILER", "clang-cl" if os.name == "nt" else "clang"
)

SOURCE_EXCLUDES = (
    "src/rust-bridge/base/cxx.h",
    "src/engine/external/",
    "src/generated/",
)


def resolve_tool(tool, env_var=None, versioned=False):
    candidate = os.environ.get(env_var) if env_var else None
    if candidate:
        resolved = shutil.which(candidate)
        if resolved:
            return resolved
        raise SystemExit(f"{env_var} points to an unavailable command: {candidate}")

    candidates = []
    if versioned:
        candidates.extend((f"{tool}-{CLANG_VERSION}", f"{tool}-{CLANG_VERSION}.exe"))
    candidates.extend((tool, f"{tool}.exe"))

    for name in candidates:
        resolved = shutil.which(name)
        if resolved:
            return resolved

    raise SystemExit(f"Required command not found: {', '.join(candidates)}")


def require_llvm_major(command, display_name):
    result = subprocess.run(
        [command, "--version"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    output = result.stdout
    if not re.search(rf"(version|LLVM version)\s+{re.escape(CLANG_VERSION)}\.", output):
        raise SystemExit(
            f"{display_name} must be LLVM/Clang {CLANG_VERSION}.x, got:\n{output}"
        )
    print(output.splitlines()[0])


def normalize_path(path):
    return path.replace("\\", "/")


def load_header_exclude_regex():
    clang_tidy = REPO_ROOT / ".clang-tidy"
    for line in clang_tidy.read_text(encoding="utf-8").splitlines():
        if line.startswith("ExcludeHeaderFilterRegex:"):
            value = line.split(":", 1)[1].strip()
            if len(value) >= 2 and value[0] == value[-1] == "'":
                value = value[1:-1]
            return value.replace("/", r"[\\/]")
    return None


def write_filter_script(filter_script, clang_tidy_bin):
    header_exclude = load_header_exclude_regex()
    filter_script.write_text(
        f"""#!/usr/bin/env python3
import os
import subprocess
import sys

SOURCE_EXCLUDES = {SOURCE_EXCLUDES!r}
CLANG_TIDY = {str(clang_tidy_bin)!r}
HEADER_EXCLUDE = {header_exclude!r}


def normalize(path):
\treturn path.replace('\\\\', '/')


def find_source(argv):
\tfor arg in argv:
\t\tif arg == '--':
\t\t\tbreak
\t\tif arg.startswith('--source='):
\t\t\treturn arg[len('--source='):]
\t\tif not arg.startswith('-'):
\t\t\treturn arg
\treturn ''


source = normalize(find_source(sys.argv[1:]))
for excluded in SOURCE_EXCLUDES:
\tif source.endswith(excluded) or f'/{{excluded}}' in source:
\t\tsys.exit(0)

command = [CLANG_TIDY, '--allow-no-checks']
if os.name == 'nt':
\tcommand.append('--extra-arg-before=/clang:-Wno-unused-command-line-argument')
\tcommand.append('--extra-arg-before=/EHsc')
if HEADER_EXCLUDE:
\tcommand.append(f'--exclude-header-filter={{HEADER_EXCLUDE}}')
command.extend(sys.argv[1:])
sys.exit(subprocess.call(command))
""",
        encoding="utf-8",
        newline="\n",
    )


def run_command(command, cwd=None, log_path=None):
    if log_path:
        with log_path.open("w", encoding="utf-8", newline="\n") as log_file:
            process = subprocess.Popen(
                command,
                cwd=cwd,
                text=True,
                encoding="utf-8",
                errors="replace",
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )
            assert process.stdout is not None
            for line in process.stdout:
                print(line, end="")
                log_file.write(line)
            return process.wait()
    subprocess.run(command, cwd=cwd, check=True)
    return 0


def main():
    if COMPILER_MODE == "clang-cl":
        clang_bin = resolve_tool("clang-cl", "CLANG_BIN", versioned=True)
        clangxx_bin = clang_bin
    else:
        clang_bin = resolve_tool("clang", "CLANG_BIN", versioned=True)
        clangxx_bin = resolve_tool("clang++", "CLANGXX_BIN", versioned=True)

    clang_tidy_bin = resolve_tool("clang-tidy", "CLANG_TIDY_BIN", versioned=True)
    cmake_bin = resolve_tool("cmake", "CMAKE_BIN")
    ninja_bin = resolve_tool("ninja", "NINJA_BIN")

    require_llvm_major(clang_bin, "clang")
    require_llvm_major(clangxx_bin, "clang++")
    require_llvm_major(clang_tidy_bin, "clang-tidy")

    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    filter_script = BUILD_DIR / "clang-tidy-filter.py"
    write_filter_script(filter_script, clang_tidy_bin)

    clang_tidy_command = f"{sys.executable};{filter_script}"
    configure_command = [
        cmake_bin,
        "-G",
        "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={ninja_bin}",
        f"-DCMAKE_CXX_COMPILER={clangxx_bin}",
        f"-DCMAKE_C_COMPILER={clang_bin}",
        f"-DCMAKE_CXX_CLANG_TIDY={clang_tidy_command}",
        f"-DCMAKE_C_CLANG_TIDY={clang_tidy_command}",
        "-DQM_DISABLE_PARALLEL_COMPILE=ON",
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DHEADLESS_CLIENT=ON",
        "-DVULKAN=OFF",
        "-DDOWNLOAD_GTEST=ON",
        "..",
    ]
    run_command(configure_command, cwd=BUILD_DIR)

    build_command = [
        cmake_bin,
        "--build",
        ".",
        "--config",
        "Debug",
        "--target",
        BUILD_TARGET,
        "--",
        "-k",
        "0",
    ]
    return run_command(
        build_command, cwd=BUILD_DIR, log_path=BUILD_DIR / "clang-tidy.log"
    )


if __name__ == "__main__":
    sys.exit(main())
