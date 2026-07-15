#!/usr/bin/env python3
"""Migrate CJK MACRO Desc strings in config headers to English keys.

Usage:
  py -3 qmclient_scripts/languages_qmclient/migrate_cjk_config_help.py --generate-map
  py -3 qmclient_scripts/languages_qmclient/migrate_cjk_config_help.py --apply
  # Main DDNet config only (separate map file):
  py -3 ... --map translations/_migrations/cjk_config_help_map_ddnet.json --generate-map --apply
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parents[1]
sys.path.insert(0, str(SCRIPT_DIR))

import i18n_store  # noqa: E402
import source_keys  # noqa: E402
from local_http_client import ChatMessage, LocalHttpClient  # noqa: E402
from translate_with_local_http import (  # noqa: E402
    DEFAULT_BASE_URL,
    DEFAULT_MODEL,
    resolve_api_key,
)

HEADER_RELS = tuple(
    PROJECT_ROOT / header for header in source_keys.CONFIG_MACRO_HELP_HEADERS
)
DEFAULT_MAP_PATH = (
    SCRIPT_DIR / "translations" / "_migrations" / "cjk_config_help_map.json"
)
MAP_PATH = DEFAULT_MAP_PATH

SYSTEM_PROMPT = (
    "You translate DDNet/QmClient MACRO_CONFIG help descriptions from Chinese "
    "to concise English UI source keys.\n"
    "Rules:\n"
    "- Keep product/tech names: QmClient, TClient, Tee, DDNet, HUD, IME, CTF, "
    "Ping, OKLAB, Presentation State, Windows, Apple Music, DeepSeek, OpenAI, "
    "LibreTranslate, ZhipuAI, LLM, CFG, etc.\n"
    "- Preserve numbers, units, and parenthetical option lists (0=Off, 1=...).\n"
    "- Prefer concise English like existing DDNet config help strings.\n"
    "- Do not wrap the whole translation in quotes.\n"
    "- Reply with ONLY a JSON object mapping string indices to English strings."
)


def cpp_escape(text: str) -> str:
    return (
        text.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )


def humanize_script_name(script_name: str) -> str:
    parts = [part for part in re.split(r"[_\s]+", script_name.strip()) if part]
    if not parts:
        return "Config option"
    if parts[0].lower() in {"qm", "tc", "dbg"} and len(parts) > 1:
        parts = parts[1:]
    words: list[str] = []
    for index, part in enumerate(parts):
        if part.isupper() and len(part) <= 4:
            words.append(part)
            continue
        word = part.lower()
        if index == 0:
            words.append(word[:1].upper() + word[1:])
        else:
            words.append(word)
    return " ".join(words) or "Config option"


def parse_macro_entries(path: Path) -> list[dict[str, object]]:
    raw = path.read_text(encoding="utf-8")
    content = source_keys.strip_cpp_comments(raw)
    entries: list[dict[str, object]] = []
    for match in source_keys.CONFIG_MACRO_CALL_RE.finditer(content):
        open_paren = content.find("(", match.start())
        close_paren = source_keys._find_matching_paren(content, open_paren)
        if close_paren == -1:
            continue
        args = source_keys._split_top_level_args(content[open_paren + 1 : close_paren])
        if not args:
            continue
        description = source_keys._decode_string_argument(args[-1])
        if not description:
            continue
        script_name = args[1].strip() if len(args) > 1 else ""
        # bare identifier or string
        decoded_script = source_keys._decode_string_argument(args[1]) if len(args) > 1 else None
        if decoded_script is not None:
            script_name = decoded_script
        else:
            script_name = args[1].strip() if len(args) > 1 else ""
        entries.append(
            {
                "header": path.relative_to(PROJECT_ROOT).as_posix(),
                "script_name": script_name,
                "old": description,
                "start": match.start(),
                "open_paren": open_paren,
                "close_paren": close_paren,
                "args": args,
                "has_cjk": source_keys.has_cjk(description),
            }
        )
    return entries


def collect_cjk_unique(entries: list[dict[str, object]]) -> list[dict[str, str]]:
    """One representative entry per unique CJK Desc."""
    unique: dict[str, dict[str, str]] = {}
    for entry in entries:
        if not entry["has_cjk"]:
            continue
        old = str(entry["old"])
        if old in unique:
            continue
        unique[old] = {
            "header": str(entry["header"]),
            "script_name": str(entry["script_name"]),
            "old": old,
        }
    return list(unique.values())


def _strip_code_fence(text: str) -> str:
    stripped = text.strip()
    if stripped.startswith("```"):
        lines = stripped.splitlines()
        if lines and lines[0].startswith("```"):
            lines = lines[1:]
        if lines and lines[-1].strip() == "```":
            lines = lines[:-1]
        stripped = "\n".join(lines).strip()
    return stripped


def parse_index_map(response: str, expected: int) -> dict[int, str]:
    text = _strip_code_fence(response)
    # tolerate trailing commentary by locating outermost JSON object
    start = text.find("{")
    end = text.rfind("}")
    if start == -1 or end == -1 or end <= start:
        raise ValueError("response is not a JSON object")
    payload = json.loads(text[start : end + 1])
    if not isinstance(payload, dict):
        raise ValueError("JSON root must be an object")
    result: dict[int, str] = {}
    for key, value in payload.items():
        try:
            index = int(key)
        except (TypeError, ValueError) as exc:
            raise ValueError(f"invalid index key {key!r}") from exc
        if not isinstance(value, str) or not value.strip():
            raise ValueError(f"empty translation for index {index}")
        result[index] = value.strip()
    missing = [i for i in range(1, expected + 1) if i not in result]
    if missing:
        raise ValueError(f"missing indices: {missing[:10]}")
    return result


def translate_batch_deepseek(
    client: LocalHttpClient, items: list[dict[str, str]], batch_number: int
) -> dict[str, str]:
    lines = [f"{index}. {item['old']}" for index, item in enumerate(items, start=1)]
    user = (
        "Translate these config help strings to English. "
        "Return JSON object with keys as the numeric indices as strings.\n\n"
        + "\n".join(lines)
    )
    response = client.chat_completion(
        [ChatMessage("system", SYSTEM_PROMPT), ChatMessage("user", user)],
        temperature=0.2,
        max_tokens=max(800, 80 * len(items)),
    )
    index_map = parse_index_map(response, len(items))
    out: dict[str, str] = {}
    for index, item in enumerate(items, start=1):
        out[item["old"]] = index_map[index]
    return out


def translate_all_deepseek(
    items: list[dict[str, str]],
    *,
    batch_size: int = 20,
    parallel: int = 4,
) -> tuple[dict[str, str], list[str]]:
    api_key = resolve_api_key("", DEFAULT_BASE_URL)
    if not api_key:
        return {}, ["DEEPSEEK_API_KEY missing"]

    client = LocalHttpClient(
        base_url=DEFAULT_BASE_URL,
        model=DEFAULT_MODEL,
        api_key=api_key,
        timeout_seconds=120.0,
        max_retries=3,
        retry_backoff_seconds=1.5,
        max_tokens=4096,
    )
    batches: list[tuple[int, list[dict[str, str]]]] = []
    for start in range(0, len(items), batch_size):
        batches.append((start // batch_size + 1, items[start : start + batch_size]))

    translations: dict[str, str] = {}
    failures: list[str] = []

    def run(batch_number: int, batch: list[dict[str, str]]) -> tuple[dict[str, str], str | None]:
        try:
            return translate_batch_deepseek(client, batch, batch_number), None
        except Exception as exc:  # noqa: BLE001 - collect and continue
            # retry once in smaller chunks
            if len(batch) > 1:
                half = max(1, len(batch) // 2)
                merged: dict[str, str] = {}
                err: str | None = None
                for offset in range(0, len(batch), half):
                    sub = batch[offset : offset + half]
                    try:
                        merged.update(
                            translate_batch_deepseek(client, sub, batch_number)
                        )
                    except Exception as sub_exc:  # noqa: BLE001
                        err = f"batch {batch_number}: {sub_exc}"
                if err is None:
                    return merged, None
                return merged, err
            return {}, f"batch {batch_number}: {exc}"

    with ThreadPoolExecutor(max_workers=max(1, parallel)) as pool:
        futures = {
            pool.submit(run, batch_number, batch): batch_number
            for batch_number, batch in batches
        }
        for future in as_completed(futures):
            batch_number = futures[future]
            result, error = future.result()
            translations.update(result)
            if error:
                failures.append(error)
            print(
                f"DeepSeek batch {batch_number}/{len(batches)}: "
                f"+{len(result)} (total {len(translations)})"
                + (f" WARN {error}" if error else ""),
                flush=True,
            )
    return translations, failures


def dedupe_new_keys(
    items: list[dict[str, str]], proposed: dict[str, str]
) -> list[dict[str, str]]:
    used: dict[str, str] = {}
    result: list[dict[str, str]] = []
    for item in items:
        old = item["old"]
        script_name = item["script_name"]
        method = "deepseek"
        new = proposed.get(old, "").strip()
        if not new or source_keys.has_cjk(new):
            new = humanize_script_name(script_name)
            method = "script_name_fallback"
        # collision: same new for different old
        if new in used and used[new] != old:
            new = f"{new} ({script_name})"
            method = (
                "deepseek_deduped"
                if method == "deepseek"
                else "script_name_fallback_deduped"
            )
            # still collide?
            suffix = 2
            base = new
            while new in used and used[new] != old:
                new = f"{base} #{suffix}"
                suffix += 1
        used[new] = old
        result.append(
            {
                "header": item["header"],
                "script_name": script_name,
                "old": old,
                "new": new,
                "method": method,
            }
        )
    return result


def generate_map(*, batch_size: int = 20, parallel: int = 4) -> list[dict[str, str]]:
    all_items: list[dict[str, str]] = []
    for path in HEADER_RELS:
        entries = parse_macro_entries(path)
        all_items.extend(collect_cjk_unique(entries))

    # prefer qmclient representative when same old appears in both (unlikely)
    by_old: dict[str, dict[str, str]] = {}
    for item in all_items:
        existing = by_old.get(item["old"])
        if existing is None:
            by_old[item["old"]] = item
            continue
        if existing["header"].endswith("tclient.h") and item["header"].endswith(
            "qmclient.h"
        ):
            by_old[item["old"]] = item
    items = list(by_old.values())
    print(f"Unique CJK descs: {len(items)}", flush=True)

    proposed, failures = translate_all_deepseek(
        items, batch_size=batch_size, parallel=parallel
    )
    if failures:
        print(f"DeepSeek failures: {len(failures)}", flush=True)
        for failure in failures[:20]:
            print(f"  {failure}", flush=True)

    mapped = dedupe_new_keys(items, proposed)
    fallbacks = [entry for entry in mapped if "fallback" in entry["method"]]
    print(
        f"Map ready: {len(mapped)} entries, fallbacks={len(fallbacks)}, "
        f"deepseek_hits={sum(1 for e in mapped if e['method'].startswith('deepseek'))}",
        flush=True,
    )
    MAP_PATH.parent.mkdir(parents=True, exist_ok=True)
    MAP_PATH.write_text(
        json.dumps(mapped, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    print(f"Wrote {MAP_PATH.relative_to(PROJECT_ROOT).as_posix()}", flush=True)
    return mapped


def load_map() -> list[dict[str, str]]:
    data = json.loads(MAP_PATH.read_text(encoding="utf-8"))
    if not isinstance(data, list):
        raise ValueError(f"map must be a list: {MAP_PATH}")
    result: list[dict[str, str]] = []
    for entry in data:
        if not isinstance(entry, dict):
            continue
        old = entry.get("old")
        new = entry.get("new")
        if not isinstance(old, str) or not isinstance(new, str) or not old or not new:
            continue
        result.append(
            {
                "header": str(entry.get("header", "")),
                "script_name": str(entry.get("script_name", "")),
                "old": old,
                "new": new,
                "method": str(entry.get("method", "")),
            }
        )
    return result


def rewrite_header(path: Path, old_to_new: dict[str, str]) -> int:
    raw = path.read_text(encoding="utf-8")
    # Work on raw text with MACRO scan on comment-stripped only for positions —
    # comments are rare inside macros; scan raw for MACRO calls.
    replacements = 0
    pieces: list[str] = []
    cursor = 0
    for match in source_keys.CONFIG_MACRO_CALL_RE.finditer(raw):
        open_paren = raw.find("(", match.start())
        close_paren = source_keys._find_matching_paren(raw, open_paren)
        if close_paren == -1:
            continue
        args = source_keys._split_top_level_args(raw[open_paren + 1 : close_paren])
        if not args:
            continue
        last = args[-1].strip()
        description = source_keys._decode_string_argument(last)
        if not description or description not in old_to_new:
            continue
        new_desc = old_to_new[description]
        if new_desc == description:
            continue
        # locate last arg span inside the call
        # rebuild args with replaced last string
        new_last = f'"{cpp_escape(new_desc)}"'
        # find exact last-arg region by walking from open_paren
        arg_src = raw[open_paren + 1 : close_paren]
        # re-split to get offsets of last arg
        depth = 0
        in_string = False
        escape = False
        arg_start = 0
        last_arg_start = 0
        last_arg_end = len(arg_src)
        for index, ch in enumerate(arg_src):
            if in_string:
                if escape:
                    escape = False
                elif ch == "\\":
                    escape = True
                elif ch == '"':
                    in_string = False
                continue
            if ch == '"':
                in_string = True
                continue
            if ch in "([{":
                depth += 1
            elif ch in ")]}":
                depth -= 1
            elif ch == "," and depth == 0:
                last_arg_start = index + 1
                arg_start = index + 1
        last_arg_end = len(arg_src)
        # preserve whitespace around last arg
        last_raw = arg_src[last_arg_start:last_arg_end]
        leading = last_raw[: len(last_raw) - len(last_raw.lstrip())]
        trailing = last_raw[len(last_raw.rstrip()) :]
        new_arg_src = (
            arg_src[:last_arg_start] + leading + new_last + trailing
        )
        call_start = match.start()
        pieces.append(raw[cursor:open_paren + 1])
        pieces.append(new_arg_src)
        pieces.append(raw[close_paren : close_paren + 1])
        cursor = close_paren + 1
        replacements += 1
        # Actually we duplicated call name... fix approach:
        # pieces currently has up to open_paren+1 from cursor which is correct
        # only if we start pieces from cursor to open_paren+1 inclusive.
    if not replacements:
        return 0

    # Rebuild carefully in a second pass to avoid the bug above
    pieces = []
    cursor = 0
    replacements = 0
    for match in source_keys.CONFIG_MACRO_CALL_RE.finditer(raw):
        open_paren = raw.find("(", match.start())
        close_paren = source_keys._find_matching_paren(raw, open_paren)
        if close_paren == -1:
            continue
        args = source_keys._split_top_level_args(raw[open_paren + 1 : close_paren])
        if not args:
            continue
        description = source_keys._decode_string_argument(args[-1])
        if not description or description not in old_to_new:
            continue
        new_desc = old_to_new[description]
        if new_desc == description:
            continue
        arg_src = raw[open_paren + 1 : close_paren]
        depth = 0
        in_string = False
        escape = False
        last_arg_start = 0
        for index, ch in enumerate(arg_src):
            if in_string:
                if escape:
                    escape = False
                elif ch == "\\":
                    escape = True
                elif ch == '"':
                    in_string = False
                continue
            if ch == '"':
                in_string = True
                continue
            if ch in "([{":
                depth += 1
            elif ch in ")]}":
                depth -= 1
            elif ch == "," and depth == 0:
                last_arg_start = index + 1
        last_raw = arg_src[last_arg_start:]
        leading = last_raw[: len(last_raw) - len(last_raw.lstrip())]
        trailing = last_raw[len(last_raw.rstrip()) :]
        new_last = f'"{cpp_escape(new_desc)}"'
        new_arg_src = arg_src[:last_arg_start] + leading + new_last + trailing
        pieces.append(raw[cursor : open_paren + 1])
        pieces.append(new_arg_src)
        pieces.append(raw[close_paren : close_paren + 1])
        cursor = close_paren + 1
        replacements += 1
    pieces.append(raw[cursor:])
    path.write_text("".join(pieces), encoding="utf-8", newline="\n")
    return replacements


def preferred_module_for_header(header: str) -> str:
    if header.endswith("config_variables_tclient.h"):
        return "tclient"
    if header.endswith("config_variables_qmclient.h"):
        return "qmclient"
    if header.endswith("config_variables.h"):
        return "menus"
    return "menus"


def remap_store(map_entries: list[dict[str, str]]) -> dict[str, int]:
    store = i18n_store.load_language_store()
    old_to_new = {entry["old"]: entry["new"] for entry in map_entries}
    old_to_module: dict[str, str] = {}
    for entry in map_entries:
        old_to_module[entry["old"]] = preferred_module_for_header(entry["header"])

    stats = {"renamed": 0, "created": 0, "sc_filled": 0, "removed_old": 0}

    # Collect existing translations for old keys across modules
    existing_by_old: dict[str, dict[str, str]] = {}
    locations: dict[str, list[tuple[str, str]]] = {}  # old -> [(module, context)]
    for module_name, entries in store.items():
        for (key, context), translations in list(entries.items()):
            if key not in old_to_new:
                continue
            locations.setdefault(key, []).append((module_name, context))
            merged = existing_by_old.setdefault(key, {})
            for language, value in translations.items():
                if value and language not in merged:
                    merged[language] = value

    # Remove old identities
    for old, spots in locations.items():
        for module_name, context in spots:
            if (old, context) in store.get(module_name, {}):
                del store[module_name][(old, context)]
                stats["removed_old"] += 1

    # Insert/merge new identities under preferred modules
    for old, new in old_to_new.items():
        preferred = old_to_module.get(old, "qmclient")
        contexts = {context for _module, context in locations.get(old, [("", "")])}
        if not contexts:
            contexts = {""}
        translations = dict(existing_by_old.get(old, {}))
        if not translations.get("simplified_chinese"):
            translations["simplified_chinese"] = old
            stats["sc_filled"] += 1
        module_entries = store.setdefault(preferred, {})
        for context in contexts:
            identity = (new, context)
            if identity in module_entries:
                merged = dict(module_entries[identity])
                for language, value in translations.items():
                    if value and not merged.get(language):
                        merged[language] = value
                module_entries[identity] = merged
            else:
                module_entries[identity] = dict(sorted(translations.items()))
                stats["created"] += 1
            stats["renamed"] += 1

    i18n_store.write_language_store(store)
    return stats


def apply_map() -> None:
    map_entries = load_map()
    if not map_entries:
        raise SystemExit(f"empty map: {MAP_PATH}")
    old_to_new = {entry["old"]: entry["new"] for entry in map_entries}
    print(f"Loaded map entries: {len(map_entries)}", flush=True)

    for path in HEADER_RELS:
        count = rewrite_header(path, old_to_new)
        print(f"Rewrote {path.relative_to(PROJECT_ROOT).as_posix()}: {count} macros", flush=True)

    stats = remap_store(map_entries)
    print(f"Store remap: {stats}", flush=True)

    # residual CJK check
    for path in HEADER_RELS:
        remaining = [
            entry["old"]
            for entry in parse_macro_entries(path)
            if entry["has_cjk"]
        ]
        print(
            f"Remaining CJK in {path.name}: {len(remaining)}",
            flush=True,
        )
        if remaining:
            for sample in remaining[:5]:
                print(f"  {sample}", flush=True)


def main() -> None:
    global MAP_PATH
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--generate-map", action="store_true")
    parser.add_argument("--apply", action="store_true")
    parser.add_argument(
        "--map",
        type=Path,
        default=None,
        help="map JSON path (default: translations/_migrations/cjk_config_help_map.json)",
    )
    parser.add_argument(
        "--headers-only",
        default="",
        help="comma-separated header basenames to include, e.g. config_variables.h",
    )
    parser.add_argument("--batch-size", type=int, default=20)
    parser.add_argument("--parallel", type=int, default=4)
    args = parser.parse_args()
    if not args.generate_map and not args.apply:
        parser.error("specify --generate-map and/or --apply")
    if args.map is not None:
        MAP_PATH = args.map if args.map.is_absolute() else (SCRIPT_DIR / args.map)
    # optional filter of HEADER_RELS for generate/apply
    global HEADER_RELS
    if args.headers_only.strip():
        wanted = {name.strip() for name in args.headers_only.split(",") if name.strip()}
        HEADER_RELS = tuple(path for path in HEADER_RELS if path.name in wanted)
        if not HEADER_RELS:
            raise SystemExit(f"no headers matched --headers-only={args.headers_only!r}")
    if args.generate_map:
        generate_map(batch_size=args.batch_size, parallel=args.parallel)
    if args.apply:
        apply_map()


if __name__ == "__main__":
    main()
