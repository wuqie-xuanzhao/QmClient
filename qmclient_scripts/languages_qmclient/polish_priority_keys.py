#!/usr/bin/env python3
"""Rewrite selected keys for non-SC languages via DeepSeek, optional write-back.

Safety:
  - Default is **draft-only** (writes translations_draft/). Do not pass --write-back
    from multiple parallel agents: write_language_store rewrites the whole TOML store
    and concurrent processes will clobber each other.
  - Parallelize by language with drafts only; a single process (or Main) should
    merge write-back serially after review.
  - Every listed key is rewritten for the target language (no skip-if-good).

Usage:
  py -3 polish_priority_keys.py \\
    --languages japanese,korean \\
    --keys-file ../../tmp/i18n_polish_priority_keys.json
  # after reviewing drafts:
  py -3 polish_priority_keys.py --languages japanese --keys-file ... --write-back
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

import i18n_store
import translate_with_local_http as tr


def load_keys(path: Path) -> set[str]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if isinstance(data, list):
        return {str(x) for x in data}
    return {str(x) for x in data.get("keys", [])}


def collect_priority_tasks(
    language: str,
    keys: set[str],
    store: dict[str, dict[tuple[str, str], dict[str, str]]],
) -> list[tr.TranslationTask]:
    tasks: list[tr.TranslationTask] = []
    for module, entries in sorted(store.items()):
        for (key, context), translations in entries.items():
            if key not in keys:
                continue
            if language == "simplified_chinese":
                continue
            existing = (translations.get(language) or "").strip()
            tasks.append(
                tr.TranslationTask(
                    module=module,
                    identity=(key, context),
                    source_text=key,
                    existing_translation=existing,
                )
            )
    return tasks


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--languages", required=True)
    parser.add_argument("--keys-file", type=Path, required=True)
    parser.add_argument("--write-back", action="store_true")
    parser.add_argument("--batch-size", type=int, default=10)
    parser.add_argument("--parallel-requests", type=int, default=2)
    parser.add_argument("--limit", type=int, default=None)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--model", default="")
    parser.add_argument("--base-url", default="")
    parser.add_argument("--api-key", default="")
    parser.add_argument("--timeout", type=float, default=180.0)
    parser.add_argument("--max-tokens", type=int, default=4096)
    parser.add_argument("--reasoning-effort", default="")
    parser.add_argument("--chat-extra-json", default="")
    args = parser.parse_args()

    keys = load_keys(args.keys_file)
    languages = tr.parse_csv_values(args.languages)
    store = i18n_store.load_language_store()
    prompt_assets = tr.load_prompt_assets()

    for k, v in tr.load_dotenv().items():
        os.environ.setdefault(k, v)

    base_url = args.base_url or tr.DEFAULT_BASE_URL
    model = args.model or tr.DEFAULT_MODEL
    api_key = tr.resolve_api_key(args.api_key, base_url)
    if not api_key and not args.dry_run:
        print("ERROR: no API key (set DEEPSEEK_API_KEY)", file=sys.stderr)
        return 2

    client = None
    if not args.dry_run:
        client = tr.LocalHttpClient(
            base_url=base_url,
            model=model,
            api_key=api_key,
            timeout_seconds=args.timeout,
            max_tokens=args.max_tokens,
            chat_extra_body=tr.build_chat_extra_body(
                args.chat_extra_json, args.reasoning_effort
            ),
        )

    total_applied = 0
    for language in languages:
        tasks = collect_priority_tasks(language, keys, store)
        if args.limit is not None:
            tasks = tasks[: args.limit]
        print(f"[{language}] tasks={len(tasks)} write_back={args.write_back}")
        if not tasks:
            continue
        if args.dry_run:
            for task in tasks[:8]:
                print(f"  dry {task.module}: {task.source_text[:80]}")
            continue

        by_module: dict[str, list[tr.TranslationTask]] = {}
        for task in tasks:
            by_module.setdefault(task.module, []).append(task)

        translations_by_module, failures_by_module = tr.translate_task_batches(
            client=client,
            language=language,
            tasks_by_module=by_module,
            batch_size=args.batch_size,
            prompt_assets=prompt_assets,
            store=store,
            parallel_requests=args.parallel_requests,
        )

        applied = 0
        for module, module_map in translations_by_module.items():
            if not module_map:
                continue
            module_tasks = by_module.get(module, [])
            if args.write_back:
                tr.write_back_module(
                    language, module, module_tasks, module_map, store
                )
            else:
                tr.write_draft_module(
                    language,
                    module,
                    module_tasks,
                    module_map,
                    merge_existing=True,
                )
            applied += len(module_map)
            print(f"  {module}: {len(module_map)} ok")
        for module, fails in failures_by_module.items():
            if fails:
                print(f"  FAIL {module}: {len(fails)}")
                for line in fails[:5]:
                    print(f"    - {line}")
        total_applied += applied
        print(f"  [{language}] applied={applied}")

    if args.write_back and not args.dry_run:
        i18n_store.write_language_store(store)
        print(f"WROTE language store (entries applied across langs ≈ {total_applied})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
