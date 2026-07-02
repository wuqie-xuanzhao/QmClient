#!/usr/bin/env python3
from pathlib import Path
import re
import sys


# C 标准头集合（不含 .h 后缀）
C_HEADER_SET = {
	"assert",
	"complex",
	"ctype",
	"errno",
	"fenv",
	"float",
	"inttypes",
	"iso646",
	"limits",
	"locale",
	"math",
	"setjmp",
	"signal",
	"stdarg",
	"stdbool",
	"stddef",
	"stdint",
	"stdio",
	"stdlib",
	"string",
	"tgmath",
	"time",
	"wchar",
	"wctype",
}
C_HEADER_INCLUDE_PATTERN = re.compile(rf"#include\s+<({'|'.join(C_HEADER_SET)})\.h>")


def get_cpp_header(c_header: str):
	if c_header == "complex":
		return "complex"
	return f"c{c_header}"


def check_standard_headers_file(filename: Path):
	errors = False
	with open(filename, encoding="utf-8") as f:
		content = f.read()
	# 先做一次快速命中判断，避免在未使用 C 头时重复扫描。
	if C_HEADER_INCLUDE_PATTERN.search(content):
		# 逐个输出替换建议，便于直接修复。
		for c_header in C_HEADER_SET:
			if re.search(rf"#include\s+<{c_header}\.h>", content):
				print(f"错误：'{filename}' 使用了 C 头文件 '{c_header}.h'，请改为 C++ 头文件 '{get_cpp_header(c_header)}'。")
				errors += 1
	return errors


def check_standard_headers_directory(path: Path):
	errors = 0
	for file in Path.iterdir(path):
		if file.is_dir():
			if file.name in ["external", "masterping", "mastersrv"]:
				continue
			errors += check_standard_headers_directory(file)
		elif file.name.endswith((".cpp", ".h")):
			errors += check_standard_headers_file(file)
	return errors


def main():
	if check_standard_headers_directory(Path("src")) != 0:
		return 1
	print("通过：未发现标准 C 头文件误用。")
	return 0


if __name__ == "__main__":
	sys.exit(main())
