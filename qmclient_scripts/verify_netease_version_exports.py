#!/usr/bin/env python3
"""Verify that the Netease version proxy forwards every system export."""

from __future__ import annotations

import argparse
import ctypes
import os
import struct
import sys
from dataclasses import dataclass
from pathlib import Path


EXPECTED_EXPORTS = frozenset(
	{
		"GetFileVersionInfoA",
		"GetFileVersionInfoByHandle",
		"GetFileVersionInfoExA",
		"GetFileVersionInfoExW",
		"GetFileVersionInfoSizeA",
		"GetFileVersionInfoSizeExA",
		"GetFileVersionInfoSizeExW",
		"GetFileVersionInfoSizeW",
		"GetFileVersionInfoW",
		"VerFindFileA",
		"VerFindFileW",
		"VerInstallFileA",
		"VerInstallFileW",
		"VerLanguageNameA",
		"VerLanguageNameW",
		"VerQueryValueA",
		"VerQueryValueW",
	}
)


class PeFormatError(ValueError):
	pass


@dataclass(frozen=True)
class ExportTable:
	names: frozenset[str]
	ordinal_only_count: int


def _unpack(data: bytes, fmt: str, offset: int) -> tuple[int, ...]:
	size = struct.calcsize(fmt)
	if offset < 0 or offset + size > len(data):
		raise PeFormatError(f"field outside file at offset {offset}")
	return struct.unpack_from(fmt, data, offset)


def _read_c_string(data: bytes, offset: int) -> str:
	if offset < 0 or offset >= len(data):
		raise PeFormatError("export name points outside file")
	end = data.find(b"\0", offset, min(len(data), offset + 4096))
	if end < 0:
		raise PeFormatError("unterminated export name")
	try:
		return data[offset:end].decode("ascii")
	except UnicodeDecodeError as error:
		raise PeFormatError("non-ASCII export name") from error


def read_exports(path: Path) -> ExportTable:
	data = path.read_bytes()
	if len(data) < 64 or data[:2] != b"MZ":
		raise PeFormatError(f"{path}: missing DOS header")
	(pe_offset,) = _unpack(data, "<I", 0x3C)
	if data[pe_offset : pe_offset + 4] != b"PE\0\0":
		raise PeFormatError(f"{path}: missing PE signature")

	file_header = pe_offset + 4
	(number_of_sections,) = _unpack(data, "<H", file_header + 2)
	(optional_size,) = _unpack(data, "<H", file_header + 16)
	optional = file_header + 20
	(magic,) = _unpack(data, "<H", optional)
	if magic == 0x10B:
		data_directories = optional + 96
	elif magic == 0x20B:
		data_directories = optional + 112
	else:
		raise PeFormatError(f"{path}: unsupported optional-header magic 0x{magic:x}")
	if data_directories + 8 > optional + optional_size:
		raise PeFormatError(f"{path}: export data directory is missing")
	(export_rva, export_size) = _unpack(data, "<II", data_directories)
	if export_rva == 0 or export_size == 0:
		return ExportTable(frozenset(), 0)
	(size_of_headers,) = _unpack(data, "<I", optional + 60)

	sections: list[tuple[int, int, int, int]] = []
	section_table = optional + optional_size
	for index in range(number_of_sections):
		section = section_table + index * 40
		virtual_size, virtual_address, raw_size, raw_offset = _unpack(data, "<IIII", section + 8)
		sections.append((virtual_address, max(virtual_size, raw_size), raw_offset, raw_size))

	def rva_to_offset(rva: int) -> int:
		if rva < size_of_headers:
			if rva >= len(data):
				raise PeFormatError("header RVA points outside file")
			return rva
		for virtual_address, mapped_size, raw_offset, raw_size in sections:
			if virtual_address <= rva < virtual_address + mapped_size:
				delta = rva - virtual_address
				if delta >= raw_size or raw_offset + delta >= len(data):
					raise PeFormatError("RVA points outside section data")
				return raw_offset + delta
		raise PeFormatError(f"unmapped RVA 0x{rva:x}")

	export_offset = rva_to_offset(export_rva)
	(number_of_functions,) = _unpack(data, "<I", export_offset + 20)
	(number_of_names,) = _unpack(data, "<I", export_offset + 24)
	(address_of_names,) = _unpack(data, "<I", export_offset + 32)
	if number_of_names > 65536 or number_of_functions < number_of_names:
		raise PeFormatError(f"{path}: invalid export counts")

	names_offset = rva_to_offset(address_of_names)
	names: set[str] = set()
	for index in range(number_of_names):
		(name_rva,) = _unpack(data, "<I", names_offset + index * 4)
		names.add(_read_c_string(data, rva_to_offset(name_rva)))
	return ExportTable(frozenset(names), number_of_functions - number_of_names)


def system_version_path() -> Path:
	if os.name != "nt":
		raise RuntimeError("--system is required outside Windows")
	kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
	kernel32.GetSystemDirectoryW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint]
	kernel32.GetSystemDirectoryW.restype = ctypes.c_uint
	buffer = ctypes.create_unicode_buffer(32768)
	length = kernel32.GetSystemDirectoryW(buffer, len(buffer))
	if length == 0 or length >= len(buffer):
		raise OSError(ctypes.get_last_error(), "GetSystemDirectoryW failed")
	return Path(buffer.value) / "version.dll"


def _describe_difference(expected: frozenset[str], actual: frozenset[str]) -> str:
	missing = sorted(expected - actual)
	extra = sorted(actual - expected)
	parts: list[str] = []
	if missing:
		parts.append("missing=" + ",".join(missing))
	if extra:
		parts.append("extra=" + ",".join(extra))
	return "; ".join(parts) or "no named-export difference"


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--proxy", required=True, type=Path)
	parser.add_argument("--system", type=Path)
	args = parser.parse_args()

	try:
		system_path = args.system if args.system is not None else system_version_path()
		proxy = read_exports(args.proxy)
		system = read_exports(system_path)
	except (OSError, PeFormatError, RuntimeError) as error:
		print(f"version export verification failed: {error}", file=sys.stderr)
		return 1

	if system.names != EXPECTED_EXPORTS or system.ordinal_only_count != 0:
		print(
			"system version.dll export contract changed: "
			+ _describe_difference(EXPECTED_EXPORTS, system.names)
			+ f"; ordinal-only={system.ordinal_only_count}",
			file=sys.stderr,
		)
		return 1
	if proxy != system:
		print(
			"proxy export mismatch: "
			+ _describe_difference(system.names, proxy.names)
			+ f"; proxy ordinal-only={proxy.ordinal_only_count}, system ordinal-only={system.ordinal_only_count}",
			file=sys.stderr,
		)
		return 1

	print(f"verified {len(proxy.names)} version.dll exports: {args.proxy}")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
