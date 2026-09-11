#!/usr/bin/env python3
"""QmClient 真实进程端到端场景。

每个场景使用独立的 ProcessEnvironment，验证玩家路径产生的日志和落盘产物。
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
import time
from collections.abc import Callable

try:
	from qmclient_scripts.integration.process_harness import ProcessEnvironment
except ModuleNotFoundError:
	from process_harness import ProcessEnvironment  # type: ignore[no-redef]


def _wait_for_file(path: Path, description: str, timeout: float = 10.0) -> Path:
	deadline = time.monotonic() + timeout
	while time.monotonic() < deadline:
		if path.is_file():
			return path
		time.sleep(0.1)
	raise TimeoutError(f"timed out waiting for {description}: {path}")


def _wait_for_perf_log(env: ProcessEnvironment, timeout: float = 15.0) -> Path:
	deadline = time.monotonic() + timeout
	pattern = "dumps/QmClient_Perf/qm_perf_*.log"
	while time.monotonic() < deadline:
		for path in env.temp_dir.glob(pattern):
			if path.is_file() and path.stat().st_size > 0 and '"stage":' in path.read_text(encoding="utf-8", errors="replace"):
				return path
		time.sleep(0.1)
	raise TimeoutError(f"timed out waiting for performance log with a stage field: {env.path(pattern)}")


def _quit_client(env: ProcessEnvironment) -> None:
	env.client.command("quit")
	code = env.client.wait_for_exit(15)
	if code != 0:
		raise RuntimeError(f"client exited with {code}")


def scenario_demo_recording(env: ProcessEnvironment) -> None:
	"""连接、切图、录制、加 marker、停止并校验 demo 文件。"""
	env.start_server()
	env.connect_client(["cl_auto_demo_record 1"])
	# Tutorial 是当前构建的默认地图，先切到另一张内置地图，确保后续
	# `sv_map Tutorial` 产生可观测的真实切图事件，而不是复用初始状态。
	env.server.command("sv_map ctf1")
	# 地图加载提示使用 ADDINFO 级别，默认 stdout logger 不会输出；自动录像
	# 的文件名包含当前地图，是同一真实加载路径的稳定外部观测点。
	env.client.wait_for(lambda line: "Recording to 'demos/auto/ctf1_" in line, "ctf1 map load", 15)
	env.server.command("sv_map Tutorial")
	env.client.wait_for(lambda line: "Recording to 'demos/auto/Tutorial_" in line, "Tutorial map load", 15)

	env.client.command("record e2e_demo")
	env.client.wait_for(lambda line: "Recording to 'demos/e2e_demo.demo'" in line, "demo recording start", 10)
	# Marker 至少需要一个已写入的 demo tick；等待一小段真实游戏时间再发命令。
	time.sleep(1.2)
	env.client.command("add_demomarker")
	env.client.wait_for(lambda line: line.endswith(": Added timeline marker"), "demo timeline marker", 10)
	env.client.command("stoprecord")
	env.client.wait_for(lambda line: "Stopped recording to 'demos/e2e_demo.demo'" in line, "demo recording stop", 10)

	demo_path = _wait_for_file(env.path("demos", "e2e_demo.demo"), "demo output")
	if demo_path.stat().st_size <= 0:
		raise RuntimeError(f"demo output is empty: {demo_path}")
	_quit_client(env)


def scenario_qm_lifecycle_persistence(env: ProcessEnvironment) -> None:
	"""验证 Qm 生命周期 marker、client id 和 statistics JSON 的真实落盘。"""
	env.start_server()
	env.connect_client([])
	# OnInit 会先创建 client id/statistics，OnUpdate 会写入生命周期 marker。
	marker_path = _wait_for_file(env.path("qmclient", "lifecycle_pending.marker"), "lifecycle marker", 15)
	client_id_path = _wait_for_file(env.path("qmclient", "playtime_client_id.txt"), "playtime client id", 15)
	statistics_path = _wait_for_file(env.path("qmclient", "statistics.json"), "statistics JSON", 15)

	_quit_client(env)

	marker = marker_path.read_text(encoding="utf-8")
	for key in ("session=", "started_at=", "last_seen_at=", "client_id="):
		if key not in marker:
			raise AssertionError(f"lifecycle marker is missing {key}: {marker!r}")

	client_id = client_id_path.read_text(encoding="utf-8").strip()
	if len(client_id) != 34 or not client_id.startswith("qm") or any(char not in "0123456789abcdef" for char in client_id[2:]):
		raise AssertionError(f"invalid playtime client id: {client_id!r}")

	try:
		json.loads(statistics_path.read_text(encoding="utf-8"))
	except json.JSONDecodeError as exc:
		raise AssertionError(f"statistics JSON is invalid: {statistics_path}") from exc


def scenario_invalid_statistics_preserved(env: ProcessEnvironment) -> None:
	"""验证损坏的 statistics 文件会被拒绝覆盖并保留原始字节。"""
	invalid_bytes = b'{"local": [broken\n'
	statistics_path = env.path("qmclient", "statistics.json")
	statistics_path.parent.mkdir(parents=True, exist_ok=True)
	statistics_path.write_bytes(invalid_bytes)

	env.start_client([], connect=False)
	env.client.wait_for(
		lambda line: "statistics file is invalid and will be preserved" in line,
		"invalid statistics warning",
		15,
	)
	_quit_client(env)
	if statistics_path.read_bytes() != invalid_bytes:
		raise AssertionError("invalid statistics file was modified")


def scenario_perf_log_persistence(env: ProcessEnvironment) -> None:
	"""验证开启性能调试后真实进程写出带 stage 字段的日志。"""
	env.start_client(["qm_perf_debug 1", "qm_perf_logfile 1"], connect=False)
	env.client.wait_for(lambda line: "writing performance log to" in line, "performance log setup", 15)
	perf_path = _wait_for_perf_log(env)
	_quit_client(env)
	if perf_path.stat().st_size <= 0:
		raise RuntimeError(f"performance log is empty: {perf_path}")


def scenario_connection_failure_recovery(env: ProcessEnvironment) -> None:
	"""验证服务端断开后客户端报告离线并可正常退出。"""
	port = env.start_server()
	env.start_client([], connect=True, connect_address=f"localhost:{port}")
	env.server.wait_for(lambda line: line.startswith("server: player has entered the game"), "client connection", 15)
	env.server.command("shutdown")
	env.client.wait_for(lambda line: "offline error='" in line, "connection failure fallback", 15)
	_quit_client(env)


def scenario_recording_without_connection(env: ProcessEnvironment) -> None:
	"""验证未连接时录制命令走错误回退而不触发崩溃。"""
	env.start_client([], connect=False)
	env.client.command("record e2e_unloaded")
	env.client.wait_for(lambda line: line.endswith(": Client is not online."), "recording error fallback", 10)
	_quit_client(env)


E2E_TESTS: dict[str, Callable[[ProcessEnvironment], None]] = {
	"connection_failure_recovery": scenario_connection_failure_recovery,
	"demo_recording": scenario_demo_recording,
	"invalid_statistics_preserved": scenario_invalid_statistics_preserved,
	"perf_log_persistence": scenario_perf_log_persistence,
	"qm_lifecycle_persistence": scenario_qm_lifecycle_persistence,
	"recording_without_connection": scenario_recording_without_connection,
}


def main() -> int:
	parser = argparse.ArgumentParser(description="Run QmClient process end-to-end tests")
	parser.add_argument("build_dir", type=Path)
	parser.add_argument("test", choices=sorted(E2E_TESTS), nargs="?")
	args = parser.parse_args()

	tests = {args.test: E2E_TESTS[args.test]} if args.test else E2E_TESTS
	failed = 0
	for name, test in tests.items():
		env = ProcessEnvironment(args.build_dir, temp_prefix="qmclient_e2e_")
		keep_temp = False
		try:
			test(env)
		except Exception as exc:  # pylint: disable=broad-exception-caught
			failed += 1
			keep_temp = True
			print(f"{name}: FAILED\n{exc}\nartifacts: {env.temp_dir}", file=sys.stderr)
		else:
			print(f"{name}: passed")
		finally:
			env.close(keep_temp=keep_temp)
	return 1 if failed else 0


if __name__ == "__main__":
	raise SystemExit(main())
