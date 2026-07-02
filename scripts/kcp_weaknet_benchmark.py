#!/usr/bin/env python3
import argparse
import ctypes
import os
import queue
import re
import subprocess
import sys
import tempfile
import threading
import time

EXE_SUFFIX = ".exe" if os.name == "nt" else ""

CASE_CONFIGS = {
	"legacy": ["sv_kcp 0"],
	"kcp": ["sv_kcp 1", "sv_kcp_required 0"],
}

WEAKNET_CONFIGS = [
	"sv_net_fake_loss {loss}",
	"sv_net_fake_jitter {jitter}",
	"sv_net_fake_rtt {rtt}",
	"sv_net_fake_reorder {reorder}",
]

CSV_COLUMNS = [
	"case",
	"transport",
	"connect_ms",
	"input_latency_proxy_ms",
	"teleport_frequency",
	"snapshot_continuity_percent",
	"snapshot_delay_ms",
	"timeout_rate",
	"cpu_percent",
	"cpu_seconds",
	"rtt_ms",
	"loss_percent",
	"resend",
	"jitter_ms",
	"send_queue_depth",
	"recv_queue_depth",
	"bandwidth_down_bytes",
	"bandwidth_up_bytes",
]


def executable_path(builddir, name):
	return os.path.join(builddir, f"{name}{EXE_SUFFIX}")


def process_cpu_seconds(process):
	if process is None:
		return 0.0
	if os.name == "nt":
		kernel32 = ctypes.windll.kernel32
		creation = ctypes.c_ulonglong()
		exit_time = ctypes.c_ulonglong()
		kernel = ctypes.c_ulonglong()
		user = ctypes.c_ulonglong()
		if not kernel32.GetProcessTimes(
			int(process._handle),
			ctypes.byref(creation),
			ctypes.byref(exit_time),
			ctypes.byref(kernel),
			ctypes.byref(user),
		):  # pylint: disable=protected-access
			return 0.0
		return (kernel.value + user.value) / 10000000.0

	try:
		with open(f"/proc/{process.pid}/stat", "r", encoding="utf-8") as stat:
			fields = stat.read().split()
		ticks_per_second = os.sysconf(os.sysconf_names["SC_CLK_TCK"])
		return (int(fields[13]) + int(fields[14])) / ticks_per_second
	except (FileNotFoundError, KeyError, OSError, ValueError):
		return 0.0


class ProcessLog:
	def __init__(self, process):
		self.process = process
		self.lines = []
		self.queue = queue.Queue()
		self.thread = threading.Thread(target=self._read, daemon=True)
		self.thread.start()

	def _read(self):
		for line in self.process.stdout:
			self.lines.append(line)
			self.queue.put(line)

	def wait_for_line(self, pattern, timeout):
		deadline = time.monotonic() + timeout
		compiled = re.compile(pattern)
		while time.monotonic() < deadline:
			remaining = max(0.0, deadline - time.monotonic())
			try:
				line = self.queue.get(timeout=remaining)
			except queue.Empty:
				break
			if compiled.search(line):
				return line
			if self.process.poll() is not None and self.queue.empty():
				break
		raise TimeoutError(f"timed out waiting for {pattern!r}. Captured output:\n{''.join(self.lines[-40:])}")

	def drain_for(self, seconds):
		deadline = time.monotonic() + seconds
		start = len(self.lines)
		while time.monotonic() < deadline:
			try:
				self.queue.get(timeout=max(0.0, deadline - time.monotonic()))
			except queue.Empty:
				break
		return self.lines[start:]

	def discard_pending(self):
		while True:
			try:
				self.queue.get_nowait()
			except queue.Empty:
				break

	def wait_for_new_line(self, pattern, timeout):
		deadline = time.monotonic() + timeout
		compiled = re.compile(pattern)
		start = len(self.lines)
		while time.monotonic() < deadline:
			for line in self.lines[start:]:
				if compiled.search(line):
					return line
			start = len(self.lines)
			remaining = max(0.0, deadline - time.monotonic())
			try:
				line = self.queue.get(timeout=remaining)
			except queue.Empty:
				break
			if compiled.search(line):
				return line
		return ""


def wait_for_line(log, pattern, timeout):
	return log.wait_for_line(pattern, timeout)


def write_fifo(path, line):
	if os.name == "nt":
		path = rf"\\.\pipe\{path}"
	with open(path, "w", encoding="utf-8", buffering=1) as fifo:
		fifo.write(line + "\n")


class FifoWriter:
	def __init__(self, path, timeout=10.0):
		if os.name == "nt":
			path = rf"\\.\pipe\{path}"
		deadline = time.monotonic() + timeout
		while True:
			try:
				self.file = open(path, "w", encoding="utf-8", buffering=1)
				break
			except FileNotFoundError:
				if time.monotonic() >= deadline:
					raise
				time.sleep(0.05)

	def write(self, line):
		self.file.write(line + "\n")

	def close(self):
		self.file.close()


def input_fifo_name(tmpdir, case_name, kind):
	if os.name == "nt":
		return f"kcp_bench_{os.getpid()}_{case_name}_{kind}"
	fifo = os.path.join(tmpdir, f"{case_name}_{kind}.fifo")
	os.mkfifo(fifo)
	return fifo


def launch_server(args, tmpdir, case_name):
	server = executable_path(args.builddir, "DDNet-Server")
	if not os.path.exists(server):
		raise RuntimeError(f"server binary not found: {server}")

	fifo = input_fifo_name(tmpdir, case_name, "server")
	cmd = (
		[
			server,
			f"sv_input_fifo {fifo}",
			"sv_register 0",
			"sv_map coverage",
			"sv_kcp_debug 1",
			"sv_kcp_stats 1",
		]
		+ CASE_CONFIGS[case_name]
		+ [config.format(loss=args.loss, jitter=args.jitter, rtt=args.rtt, reorder=args.reorder) for config in WEAKNET_CONFIGS]
	)
	process = subprocess.Popen(
		cmd,
		cwd=tmpdir,
		stdout=subprocess.PIPE,
		stderr=subprocess.STDOUT,
		text=True,
		encoding="utf-8",
		errors="replace",
	)
	log = ProcessLog(process)
	line = wait_for_line(log, r"server: using port (\d+)", 15)
	port = int(re.search(r"server: using port (\d+)", line).group(1))
	return process, log, fifo, port


def launch_client(args, tmpdir, case_name, port):
	client = executable_path(args.builddir, "DDNet")
	if not os.path.exists(client):
		raise RuntimeError(f"client binary not found: {client}")

	fifo = input_fifo_name(tmpdir, case_name, "client")
	cmd = [
		client,
		f"cl_input_fifo {fifo}",
		"gfx_fullscreen 0",
		"gfx_vsync 0",
		"player_name kcp_bench",
		f"connect localhost:{port}",
	]
	process = subprocess.Popen(
		cmd,
		cwd=tmpdir,
		stdout=subprocess.PIPE,
		stderr=subprocess.STDOUT,
		text=True,
		encoding="utf-8",
		errors="replace",
	)
	log = ProcessLog(process)
	return process, log, fifo


def collect_status(log, fifo, seconds, interval):
	deadline = time.monotonic() + seconds
	lines = []
	while time.monotonic() < deadline:
		log.discard_pending()
		fifo.write("kcp_status")
		line = log.wait_for_new_line(r"transport=", min(interval, max(0.0, deadline - time.monotonic())))
		if line:
			lines.append(line)
	return lines


def parse_metrics(lines, client_lines):
	status_lines = [line for line in lines if "id=" in line and "transport=" in line]
	last = status_lines[-1] if status_lines else ""

	def int_metric(name, default=-1):
		match = re.search(rf"{name}=(-?\d+)", last)
		return int(match.group(1)) if match else default

	def float_metric(name, default=-1.0):
		match = re.search(rf"{name}=(-?\d+(?:\.\d+)?)", last)
		return float(match.group(1)) if match else default

	samples = max(1, len(status_lines))
	healthy_samples = 0
	for line in status_lines:
		delay_match = re.search(r"snap_delay=(-?\d+)", line)
		send_q_match = re.search(r"send_q=(-?\d+)", line)
		if delay_match and send_q_match and int(delay_match.group(1)) < 500 and int(send_q_match.group(1)) < 64:
			healthy_samples += 1
	drop_count = sum(1 for line in lines if "client dropped." in line or "has left the game" in line)
	teleport_proxy_count = sum(1 for line in client_lines if "prediction time reset!" in line or "prediction error" in line)

	return {
		"transport": re.search(r"transport=(\w+)", last).group(1) if last and re.search(r"transport=(\w+)", last) else "unknown",
		"rtt_ms": int_metric("rtt"),
		"loss_percent": float_metric("loss"),
		"resend": int_metric("resend"),
		"jitter_ms": int_metric("jitter"),
		"snapshot_delay_ms": int_metric("snap_delay"),
		"snapshot_continuity_percent": round(healthy_samples * 100.0 / samples, 1),
		"timeout_rate": drop_count,
		"send_queue_depth": int_metric("send_q"),
		"recv_queue_depth": int_metric("recv_q"),
		"bandwidth_down_bytes": int_metric("bw_down"),
		"bandwidth_up_bytes": int_metric("bw_up"),
		"teleport_frequency": teleport_proxy_count,
	}


def stop_process(process, fifo, command):
	if process.poll() is None:
		if fifo is not None:
			try:
				fifo.write(command)
			except OSError:
				process.terminate()
		else:
			process.terminate()
	try:
		process.wait(timeout=10)
	except subprocess.TimeoutExpired:
		process.kill()
		process.wait(timeout=5)


def run_case(args, tmpdir, case_name):
	server = None
	client = None
	server_log = None
	server_fifo = None
	client_fifo = None
	server_writer = None
	client_writer = None
	lines = []
	try:
		server, server_log, server_fifo, port = launch_server(args, tmpdir, case_name)
		server_writer = FifoWriter(server_fifo)
		client, client_log, client_fifo = launch_client(args, tmpdir, case_name, port)
		client_writer = FifoWriter(client_fifo)
		connect_start = time.monotonic()
		wait_for_line(server_log, r"server: player has entered the game", 20)
		connect_ms = int((time.monotonic() - connect_start) * 1000)
		cpu_start = process_cpu_seconds(server) + process_cpu_seconds(client)
		sample_start = time.monotonic()
		lines.extend(server_log.lines)
		lines.extend(collect_status(server_log, server_writer, args.duration, args.interval))
		elapsed = max(time.monotonic() - sample_start, 0.001)
		cpu_seconds = max(0.0, process_cpu_seconds(server) + process_cpu_seconds(client) - cpu_start)
		metrics = parse_metrics(lines, client_log.lines)
		metrics["connect_ms"] = connect_ms
		metrics["input_latency_proxy_ms"] = metrics["snapshot_delay_ms"]
		metrics["cpu_seconds"] = round(cpu_seconds, 3)
		metrics["cpu_percent"] = round(cpu_seconds * 100.0 / elapsed, 1)
		return metrics
	finally:
		if client is not None:
			stop_process(client, client_writer, "quit")
		if server is not None:
			stop_process(server, server_writer, "shutdown")
		if client_writer is not None:
			client_writer.close()
		if server_writer is not None:
			server_writer.close()


def main():
	parser = argparse.ArgumentParser(description="Run a repeatable weak-network Legacy UDP vs KCP transport comparison.")
	parser.add_argument("builddir", help="CMake build directory containing DDNet and DDNet-Server")
	parser.add_argument("--duration", type=float, default=30.0, help="seconds to sample each case")
	parser.add_argument("--interval", type=float, default=2.0, help="seconds between kcp_status samples")
	parser.add_argument("--loss", type=int, default=10, help="server-side fake packet loss percentage")
	parser.add_argument("--jitter", type=int, default=120, help="server-side fake jitter in ms")
	parser.add_argument("--rtt", type=int, default=220, help="server-side fake RTT in ms")
	parser.add_argument("--reorder", type=int, default=10, help="server-side fake reorder percentage")
	args = parser.parse_args()
	args.builddir = os.path.abspath(args.builddir)

	with tempfile.TemporaryDirectory(prefix="kcp_weaknet_") as tmpdir:
		data_dir = os.path.abspath(os.path.join(args.builddir, "data"))
		with open(os.path.join(tmpdir, "storage.cfg"), "w", encoding="utf-8") as storage:
			storage.write(f"add_path .\nadd_path {data_dir}\n")
		results = {}
		for case_name in ("legacy", "kcp"):
			results[case_name] = run_case(args, tmpdir, case_name)

	print(",".join(CSV_COLUMNS))
	for case_name in ("legacy", "kcp"):
		metrics = results[case_name]
		print(
			",".join(
				str(value)
				for value in [
					case_name,
					*(metrics[column] for column in CSV_COLUMNS[1:]),
				]
			)
		)
	return 0


if __name__ == "__main__":
	sys.exit(main())
