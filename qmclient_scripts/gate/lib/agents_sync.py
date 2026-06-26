from __future__ import annotations

import json
import tempfile
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
AGENTS_PATH = REPO_ROOT / "AGENTS.md"
CLAUDE_PATH = REPO_ROOT / "CLAUDE.md"
STATE_PATH = Path(tempfile.gettempdir()) / "qmclient-agents-claude-sync-state.json"


@dataclass
class SyncResult:
    ok: bool
    changed: bool
    title: str
    detail: str


def normalize_text(text: str) -> str:
    return text.replace("\r\n", "\n").replace("\r", "\n")


def detect_newline(text: str) -> str:
    if "\r\n" in text:
        return "\r\n"
    if "\r" in text:
        return "\r"
    return "\n"


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def write_text(path: Path, text: str, newline: str) -> None:
    normalized = normalize_text(text)
    rewritten = normalized.replace("\n", newline)
    with path.open("w", encoding="utf-8", newline="") as file:
        file.write(rewritten)


def read_state() -> dict:
    if not STATE_PATH.exists():
        return {}
    with STATE_PATH.open("r", encoding="utf-8") as file:
        try:
            return json.load(file)
        except json.JSONDecodeError:
            return {}


def write_state(agents_text: str, claude_text: str) -> None:
    STATE_PATH.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "agents_normalized": normalize_text(agents_text),
        "claude_normalized": normalize_text(claude_text),
    }
    with STATE_PATH.open("w", encoding="utf-8", newline="\n") as file:
        json.dump(payload, file, ensure_ascii=False, indent=2)
        file.write("\n")


def choose_source(
    agents_text: str,
    claude_text: str,
    state: dict,
    prefer: str,
) -> tuple[str | None, str]:
    agents_normalized = normalize_text(agents_text)
    claude_normalized = normalize_text(claude_text)
    if agents_normalized == claude_normalized:
        return None, "AGENTS.md 与 CLAUDE.md 已一致"

    last_agents = str(state.get("agents_normalized", ""))
    last_claude = str(state.get("claude_normalized", ""))
    has_state = bool(last_agents or last_claude)
    agents_changed = agents_normalized != last_agents
    claude_changed = claude_normalized != last_claude

    if prefer == "agents":
        return "agents", "按 AGENTS.md 作为源文件同步 CLAUDE.md"
    if prefer == "claude":
        return "claude", "按 CLAUDE.md 作为源文件同步 AGENTS.md"

    if not has_state:
        agents_mtime = AGENTS_PATH.stat().st_mtime_ns
        claude_mtime = CLAUDE_PATH.stat().st_mtime_ns
        if agents_mtime > claude_mtime:
            return "agents", "首次同步未找到历史状态，按更新较新的 AGENTS.md 作为源文件"
        if claude_mtime > agents_mtime:
            return "claude", "首次同步未找到历史状态，按更新较新的 CLAUDE.md 作为源文件"
        return (
            "agents",
            "首次同步未找到历史状态，时间戳相同，默认按 AGENTS.md 作为源文件",
        )

    if agents_changed and not claude_changed:
        return "agents", "检测到仅 AGENTS.md 有新改动，将同步到 CLAUDE.md"
    if claude_changed and not agents_changed:
        return "claude", "检测到仅 CLAUDE.md 有新改动，将同步到 AGENTS.md"
    if not agents_changed and not claude_changed:
        return "agents", "状态文件缺失或过旧，默认按 AGENTS.md 同步 CLAUDE.md"

    return None, "AGENTS.md 与 CLAUDE.md 都发生了未登记改动且内容不一致，已拒绝自动覆盖"


def sync_files(prefer: str) -> SyncResult:
    if not AGENTS_PATH.exists() or not CLAUDE_PATH.exists():
        missing = []
        if not AGENTS_PATH.exists():
            missing.append("AGENTS.md")
        if not CLAUDE_PATH.exists():
            missing.append("CLAUDE.md")
        return SyncResult(
            False, False, "AGENTS / CLAUDE 镜像同步", f"缺少文件: {', '.join(missing)}"
        )

    agents_text = read_text(AGENTS_PATH)
    claude_text = read_text(CLAUDE_PATH)
    state = read_state()
    source, reason = choose_source(agents_text, claude_text, state, prefer)

    if source is None:
        if normalize_text(agents_text) == normalize_text(claude_text):
            write_state(agents_text, claude_text)
            return SyncResult(True, False, "AGENTS / CLAUDE 镜像同步", reason)
        return SyncResult(False, False, "AGENTS / CLAUDE 镜像同步", reason)

    if source == "agents":
        target_newline = detect_newline(claude_text)
        write_text(CLAUDE_PATH, agents_text, target_newline)
        final_agents = agents_text
        final_claude = agents_text
    else:
        target_newline = detect_newline(agents_text)
        write_text(AGENTS_PATH, claude_text, target_newline)
        final_agents = claude_text
        final_claude = claude_text

    write_state(final_agents, final_claude)
    target_name = "CLAUDE.md" if source == "agents" else "AGENTS.md"
    return SyncResult(
        True, True, "AGENTS / CLAUDE 镜像同步", f"{reason}，已更新 {target_name}"
    )
