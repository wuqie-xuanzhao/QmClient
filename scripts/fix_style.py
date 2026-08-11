#!/usr/bin/env python3

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
IGNORE_DIRS = ["src/game/generated", "src/rust-bridge/base"]


def filter_ignored(filenames):
	result = []
	for filename in filenames:
		real_filename = os.path.realpath(filename)
		if real_filename not in [os.path.realpath(ignore_file) for ignore_file in IGNORE_FILES] and not any(real_filename.startswith(os.path.realpath(subdir) + os.path.sep) for subdir in IGNORE_DIRS):
			result.append(filename)

	return result


def filter_cpp(filenames):
	return [filename for filename in filenames if any(filename.endswith(ext) for ext in ".c .cc .cpp .h .hpp".split())]


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
	p = argparse.ArgumentParser(description="Check and fix style of changed files")
	p.add_argument("-n", "--dry-run", action="store_true", help="Don't fix, only warn")
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
	if not args.dry_run:
		reformat(filenames)
	else:
		sys.exit(warn(filenames))


if __name__ == "__main__":
	main()
