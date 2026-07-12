# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        return 1

    log_path = Path(sys.argv[1])
    prefixes = (
        "注意: 包含文件:".encode("mbcs"),
        "注意: 包含文件:".encode("utf-8"),
        "Note: including file:".encode("ascii"),
    )

    with log_path.open("rb") as log_file:
        for line in log_file:
            if any(line.lstrip().startswith(prefix) for prefix in prefixes):
                continue
            sys.stdout.buffer.write(line)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
