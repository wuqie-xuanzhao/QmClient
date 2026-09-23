#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成网易云注入自检 cmd 脚本（GBK 编码，避免 cmd 中文乱码）。

用法：
    py -3 qmclient_scripts/support/check_netease_inject.py [输出路径]

默认输出到仓库 `tmp/check_netease_inject.bat`。脚本内容与
`docs/support/netease_dll_inject_check.md` 中列出的命令一致；
改文档里的检查项时同步改这里的 BAT_TEXT。
"""

from __future__ import annotations

import sys
from pathlib import Path

BAT_TEXT = r"""@echo off
setlocal
echo ============================================================
echo  QmClient 网易云注入自检
echo ============================================================
echo.
echo [1/4] 找主进程：cloudmusic.exe 且命令行不含 --type=
tasklist /fi "imagename eq cloudmusic.exe" /nh
echo      ^> 记下命令行里没有 --type= 的那个 PID，下面用得到
echo.
echo [2/4] 关键一项：主进程模块表里有没有 qm-nmt-hook64.dll
tasklist /fi "imagename eq cloudmusic.exe" /m qm-nmt-hook64.dll
echo      ^> 列出 PID + qm-nmt-hook64.dll = 注入成功
echo      ^> 提示 No tasks are running = 没注入成功 或 已退出
echo.
echo [3/4] 注入器 helper 是否在跑
tasklist /fi "imagename eq qm-nmt-helper.exe" /nh
echo      ^> 找不到这一行 = helper 没运行，需要重开客户端
echo.
echo [4/4] 主进程完整命令行：看是否带 --remote-debugging-port
powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='cloudmusic.exe'\" | Where-Object { $_.CommandLine -notmatch '--type=' } | ForEach-Object { 'PID ' + $_.ProcessId.ToString() + ' : ' + $_.CommandLine }"
echo      ^> 带 --remote-debugging-port= 说明 version.dll 引导也生效
echo.
echo 详细判读与常见失败原因见 docs/support/netease_dll_inject_check.md
echo.
pause
"""


def main() -> int:
    if len(sys.argv) > 1:
        target = Path(sys.argv[1])
    else:
        target = Path(__file__).resolve().parents[2] / "tmp" / "check_netease_inject.bat"
    target.parent.mkdir(parents=True, exist_ok=True)
    # cmd.exe 按当前代码页逐字节解析批处理，UTF-8 中文会被拆断成非法命令，
    # 因此固定写 GBK（中文 Windows 的 cmd 默认代码页 936）。
    target.write_text(BAT_TEXT.replace("\r\n", "\n").replace("\n", "\r\n"), encoding="gbk")
    print(f"已生成：{target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
