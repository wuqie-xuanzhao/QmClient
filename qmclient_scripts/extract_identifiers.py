# 请抬头享受阳光｜日子很好 我很我---------致咩子
import argparse
import csv
import json
import os
import shlex
import site
import sys


def ensure_clang_python_available():
	try:
		import clang.cindex as clang_cindex  # pylint: disable=import-error

		return clang_cindex
	except ImportError as exc:
		last_error = exc
		candidate_paths = []
		for root in site.getsitepackages() + [site.getusersitepackages()]:
			if not root:
				continue
			candidate_paths.append(root)
			candidate_paths.append(os.path.join(root, "Lib", "site-packages"))

		for candidate in candidate_paths:
			if not candidate or not os.path.isdir(candidate) or candidate in sys.path:
				continue
			sys.path.append(candidate)
			try:
				import clang.cindex as clang_cindex  # pylint: disable=import-error

				return clang_cindex
			except ImportError as inner_exc:
				last_error = inner_exc

		raise RuntimeError("缺少 Python clang 模块。请先执行 `python -m pip install clang`，如果你使用 Scoop Python，也可以执行 `py -3 -m pip install clang`。") from last_error


clang = ensure_clang_python_available()


def configure_libclang():
	candidate_files = []
	if "LIBCLANG_PATH" in os.environ:
		candidate_files.append(os.environ["LIBCLANG_PATH"])

	candidate_files.extend([
		r"D:\Scoop\apps\llvm\current\bin\libclang.dll",
		r"D:\Scoop\apps\llvm\22.1.5\bin\libclang.dll",
		r"D:\Scoop\apps\llvm\22.1.4\bin\libclang.dll",
		r"D:\Scoop\apps\llvm\22.1.3\bin\libclang.dll",
	])

	for candidate in candidate_files:
		if candidate and os.path.isfile(candidate):
			clang.Config.set_library_file(candidate)
			return


configure_libclang()

from clang.cindex import CursorKind, Diagnostic, LinkageKind, StorageClass, TypeKind  # noqa: E402 # pylint: disable=import-error

try:
	from tqdm import tqdm
except ImportError:

	def tqdm(it, *_args, **_kwargs):
		return it


def traverse_namespaced(root, filter_files=None, skip_namespaces=1, namespace=()):
	if root.location.file is not None and root.location.file.name not in filter_files:
		return
	yield namespace, root
	if root.displayname != "":
		if skip_namespaces > 0:
			skip_namespaces -= 1
		else:
			namespace += (root.spelling,)
	for node in root.get_children():
		yield from traverse_namespaced(node, filter_files, skip_namespaces, namespace)


INTERESTING_NODE_KINDS = {
	CursorKind.CLASS_DECL: "class",
	CursorKind.CLASS_TEMPLATE: "class",
	CursorKind.ENUM_DECL: "enum",
	CursorKind.ENUM_CONSTANT_DECL: "enum_constant",
	CursorKind.FIELD_DECL: "variable",
	CursorKind.PARM_DECL: "variable",
	CursorKind.STRUCT_DECL: "struct",
	CursorKind.UNION_DECL: "union",
	CursorKind.VAR_DECL: "variable",
	CursorKind.FUNCTION_DECL: "function",
}


def is_array_type(typ):
	return typ.kind in (
		TypeKind.CONSTANTARRAY,
		TypeKind.DEPENDENTSIZEDARRAY,
		TypeKind.INCOMPLETEARRAY,
	)


def get_complex_type(typ, seen=None):
	if seen is None:
		seen = set()
	key = (typ.kind.value, typ.spelling)
	if key in seen:
		return ""
	seen.add(key)
	if typ.spelling in ("IOHANDLE", "LOCK"):
		return ""
	if typ.kind == TypeKind.AUTO:
		return get_complex_type(typ.get_canonical(), seen)
	if typ.kind == TypeKind.LVALUEREFERENCE:
		return get_complex_type(typ.get_pointee(), seen)
	if typ.kind == TypeKind.POINTER:
		return "p" + get_complex_type(typ.get_pointee(), seen)
	if is_array_type(typ):
		return "a" + get_complex_type(typ.element_type, seen)
	if typ.kind == TypeKind.FUNCTIONPROTO:
		return "fn"
	if typ.kind == TypeKind.TYPEDEF:
		return get_complex_type(typ.get_declaration().underlying_typedef_type, seen)
	if typ.kind == TypeKind.ELABORATED:
		return get_complex_type(typ.get_named_type(), seen)
	if typ.kind in (TypeKind.UNEXPOSED, TypeKind.RECORD):
		if typ.get_declaration().spelling in "shared_ptr unique_ptr".split():
			return "p" + get_complex_type(typ.get_template_argument_type(0), seen)
		if typ.get_declaration().spelling in "array sorted_array".split():
			return "a" + get_complex_type(typ.get_template_argument_type(0), seen)
		if typ.get_declaration().spelling == "vector":
			return "v"
	return ""


def is_static_member_definition_hack(node):
	last_colons = False
	for t in node.get_tokens():
		t = t.spelling
		if t == "::":
			last_colons = True
		elif last_colons:
			if t.startswith("ms_"):
				return True
			last_colons = False
		if t == "=":
			return False
	return False


def is_const(typ):
	if typ.is_const_qualified():
		return True
	if is_array_type(typ):
		return is_const(typ.element_type)
	return False


class ParseError(RuntimeError):
	pass


def normalize_include_arg(value):
	if not value:
		return None
	value = value.strip()
	if not value:
		return None
	if value.startswith('"') and value.endswith('"') and len(value) >= 2:
		value = value[1:-1]
	return value


def collect_compile_command_args(file):
	compile_commands = os.path.join("cmake-build-debug", "compile_commands.json")
	if not os.path.isfile(compile_commands):
		return []

	target_file = os.path.normcase(os.path.abspath(file))
	with open(compile_commands, encoding="utf-8") as compile_commands_file:
		entries = json.load(compile_commands_file)

	for entry in entries:
		entry_file = entry.get("file")
		if not entry_file:
			continue
		if os.path.normcase(os.path.abspath(entry_file)) != target_file:
			continue

		command_args = entry.get("arguments")
		if command_args:
			tokens = command_args[1:]
		else:
			command = entry.get("command", "")
			tokens = shlex.split(command, posix=False)

		candidate_args = []
		for token in tokens:
			if token.startswith(("-I", "/I", "-external:I", "/external:I", "-D", "/D", "-std:", "/std:")):
				candidate_args.append(token)

		normalized_args = []
		for arg in candidate_args:
			if arg.startswith(("-external:I", "/external:I")):
				include_path = normalize_include_arg(arg.split(":", 1)[1])
				if include_path and include_path.startswith("I"):
					include_path = include_path[1:]
				if include_path:
					normalized_args.append("-I" + include_path)
				continue
			if arg.startswith(("-D", "/D")):
				macro = normalize_include_arg(arg[2:])
				if macro:
					normalized_args.append("-D" + macro)
				continue
			if arg.startswith(("-std:", "/std:")):
				normalized_args.append("-std=" + arg.split(":", 1)[1])
				continue
			prefix = "-I" if arg.startswith("-I") else "/I"
			include_path = normalize_include_arg(arg[len(prefix) :])
			if include_path:
				normalized_args.append("-I" + include_path)
		return normalized_args

	return []


def process_source_file(out, file, extra_args, break_on):
	args = extra_args + collect_compile_command_args(file)
	if any(arg in ("-D_WIN32", "-DWIN32") or arg.startswith("-D_WIN32_WINNT=") for arg in args) and "-D__PRFCHWINTRIN_H" not in args:
		args.append("-D__PRFCHWINTRIN_H")
	if not any(arg == "-Isrc" or arg.endswith("\\src") or arg.endswith("/src") for arg in args):
		args.append("-Isrc")
	if file.endswith(".c"):
		header = f"{file[:-2]}.h"
	elif file.endswith(".cc"):
		header = f"{file[:-3]}.h"
	elif file.endswith(".cpp"):
		header = f"{file[:-4]}.h"
	else:
		raise ValueError(f"unrecognized source file: {file}")

	index = clang.Index.create()
	unit = index.parse(file, args=args)
	diagnostics = list(unit.diagnostics)
	errors = [diagnostic for diagnostic in diagnostics if diagnostic.severity >= Diagnostic.Error]
	if errors:
		for error in errors:
			print(f"{file}: {error.format()}", file=sys.stderr)
		print(args, file=sys.stderr)
		raise ParseError(f"failed parsing {file}")

	filter_files = frozenset([file, header])

	for namespace, node in traverse_namespaced(unit.cursor, filter_files=filter_files):
		cur_file = None
		if node.location.file is not None:
			cur_file = node.location.file.name
		if cur_file is None or cur_file not in (file, header):
			continue
		if node.kind in INTERESTING_NODE_KINDS and node.spelling:
			typ = get_complex_type(node.type)
			qualifiers = ""
			if INTERESTING_NODE_KINDS[node.kind] in {"variable", "function"}:
				is_member = node.semantic_parent.kind in {
					CursorKind.CLASS_DECL,
					CursorKind.CLASS_TEMPLATE,
					CursorKind.STRUCT_DECL,
					CursorKind.UNION_DECL,
				}
				is_static = node.storage_class == StorageClass.STATIC or is_static_member_definition_hack(node)
				if is_static:
					qualifiers = "s" + qualifiers
				if is_member:
					qualifiers = "m" + qualifiers
				if is_static and not is_member and is_const(node.type):
					qualifiers = "c" + qualifiers
				if node.linkage == LinkageKind.EXTERNAL and not is_member:
					qualifiers = "g" + qualifiers
			out.writerow({
				"file": cur_file,
				"line": node.location.line,
				"column": node.location.column,
				"kind": INTERESTING_NODE_KINDS[node.kind],
				"path": "::".join(namespace),
				"qualifiers": qualifiers,
				"type": typ,
				"name": node.spelling,
			})
			if node.spelling == break_on:
				breakpoint()  # pylint: disable=forgotten-debug-statement


def main():
	p = argparse.ArgumentParser(description="Extracts identifier data from a Teeworlds source file and its header, outputting the data as CSV to stdout")
	p.add_argument("file", metavar="FILE", nargs="+", help="Source file to analyze")
	p.add_argument(
		"--break-on",
		help="Break on a specific variable name, useful to debug issues with the script",
	)
	args = p.parse_args()

	extra_args = []
	if "CXXFLAGS" in os.environ:
		extra_args = os.environ["CXXFLAGS"].split()

	out = csv.DictWriter(sys.stdout, "file line column kind path qualifiers type name".split())
	out.writeheader()
	files = args.file
	if len(files) > 1:
		files = tqdm(files, leave=False)
	error = False
	for file in files:
		try:
			process_source_file(out, file, extra_args, args.break_on)
		except ParseError:
			error = True
	return int(error)


if __name__ == "__main__":
	sys.exit(main())
