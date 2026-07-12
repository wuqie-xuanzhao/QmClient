#!/usr/bin/env python3
"""
Batch-add signature line to all QmClient original source files.
"""
import os
import fnmatch

SIGNATURE = "请抬头享受阳光｜日子很好 我很我---------致咩子"

LINE_PREFIX = {
    ".cpp": "// ", ".cc": "// ", ".cxx": "// ", ".c": "// ",
    ".h": "// ", ".hpp": "// ", ".hh": "// ",
    ".rs": "// ",
    ".ts": "// ", ".tsx": "// ", ".js": "// ", ".jsx": "// ",
    ".inc": "// ",
    ".py": "# ",
    ".toml": "# ",
    ".cfg": "# ",
    ".cmake": "# ",
    ".txt": "# ",
    ".cmd": ":: ",
    ".md": "> ",
    ".sh": "# ",
}

def add_signature(filepath):
    ext = os.path.splitext(filepath)[1].lower()
    prefix = LINE_PREFIX.get(ext)
    if prefix is None:
        return False
    if prefix == ":: " and ext == ".cmd":
        line = f"{prefix}{SIGNATURE}\n"
    else:
        line = f"{prefix}{SIGNATURE}\n"

    with open(filepath, "r", encoding="utf-8", errors="replace") as f:
        content = f.read()

    if SIGNATURE in content:
        return False
    if not content.strip():
        return False

    with open(filepath, "w", encoding="utf-8") as f:
        f.write(line + content)
    return True

def main():
    project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    supported_exts = tuple(LINE_PREFIX.keys())

    # --- Directories that are 100% QmClient original ---
    safe_dirs = [
        "src/game/client/QmUi",
        "src/game/client/components/qmclient",
        "src/game/client/live",
        "src/test",
        "qmclient_scripts",
    ]

    # --- QmClient-original file patterns in mixed directories ---
    mixed_patterns = [
        # src/game/client/ (root level)
        "src/game/client/qm_*",
        "src/game/client/ui_scrollregion.*",
        # src/game/client/components/
        "src/game/client/components/binds_deepfly_mode.*",
        "src/game/client/components/chat_completion.*",
        "src/game/client/components/hud_media_island_logic.*",
        "src/game/client/components/message_gradient.*",
        "src/game/client/components/pie_menu.*",
        "src/game/client/components/player_points.*",
        "src/game/client/components/settings_warmup.*",
        "src/game/client/components/system_media_controls.*",
        "src/game/client/components/theme_scan.*",
        "src/game/client/components/ui_effects.*",
        "src/game/client/components/assets_*",
        # src/engine/shared/
        "src/engine/shared/config_variables_qmclient.*",
        "src/engine/shared/qm_*",
    ]

    count = 0
    # Phase 1: safe directories (walk all files)
    for sdir in safe_dirs:
        full_dir = os.path.join(project_root, sdir)
        if not os.path.isdir(full_dir):
            print(f"[SKIP] Dir not found: {full_dir}")
            continue
        for root, dirs, files in os.walk(full_dir):
            for fname in files:
                if fname.lower().endswith(supported_exts):
                    fpath = os.path.join(root, fname)
                    if add_signature(fpath):
                        print(f"[OK] {os.path.relpath(fpath, project_root)}")
                        count += 1

    # Phase 2: mixed directories (file patterns only)
    for pattern in mixed_patterns:
        full_pattern = os.path.join(project_root, pattern)
        base_dir = os.path.dirname(full_pattern)
        fname_pattern = os.path.basename(full_pattern)

        if not os.path.isdir(base_dir):
            continue

        for fname in os.listdir(base_dir):
            if fnmatch.fnmatch(fname, fname_pattern):
                if fname.lower().endswith(supported_exts):
                    fpath = os.path.join(base_dir, fname)
                    if add_signature(fpath):
                        print(f"[OK] {os.path.relpath(fpath, project_root)}")
                        count += 1

    print(f"\nDone. {count} files updated.")

if __name__ == "__main__":
    main()
