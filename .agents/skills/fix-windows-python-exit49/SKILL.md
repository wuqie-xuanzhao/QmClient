---
name: fix-windows-python-exit49
description: 修复 Windows MSYS2/Git Bash 中 python 命令返回退出码 49 或 Scoop 更新后 py launcher 失效的问题
---

# 修复 Windows 上 OMP bash 中 Python 退出码 49

## 症状
在 OMP harness 的 MSYS2/Git Bash 中，`python`、`python3` 命令返回退出码 49，无输出。`py` launcher 报错 "Unable to create process using ... 3.14.4"。

## 根因
1. **WindowsApps python stub**：`C:\Users\<user>\AppData\Local\Microsoft\WindowsApps\python.exe` 是 0 字节的 Microsoft Store App Execution Alias stub。Store 未安装 python 时，执行它返回退出码 49。该 stub 在 PATH 中比 Scoop python 更靠前。
2. **py launcher 注册表指向旧版本**：Scoop 每次 update python 后，`HKCU\Software\Python\PythonCore\<ver>\InstallPath` 仍指向已删除的旧版本路径（如 3.14.4），导致 `py` 失效。

## 修复步骤

### 步骤 1：删除 WindowsApps stub
```bash
cmd.exe /c "del C:\Users\<user>\AppData\Local\Microsoft\WindowsApps\python.exe"
cmd.exe /c "del C:\Users\<user>\AppData\Local\Microsoft\WindowsApps\python3.exe"
```

### 步骤 2：创建 .cmd wrapper（防止 stub 被重建后再次遮蔽）
在 `C:\Users\<user>\.local\bin\`（PATH 中比 WindowsApps 更靠前）创建：

**python.cmd**:
```cmd
@echo off
"D:\scoop\apps\python\current\python.exe" %*
```

**python3.cmd**: 同上

### 步骤 3：修正 py launcher 注册表（指向 current 而非版本号）
```bash
cmd.exe /c "reg add HKCU\Software\Python\PythonCore\3.14\InstallPath /ve /d \"D:\scoop\apps\python\current\" /f"
cmd.exe /c "reg add HKCU\Software\Python\PythonCore\3.14\InstallPath /v ExecutablePath /d \"D:\scoop\apps\python\current\python.exe\" /f"
cmd.exe /c "reg add HKCU\Software\Python\PythonCore\3.14\InstallPath /v WindowedExecutablePath /d \"D:\scoop\apps\python\current\pythonw.exe\" /f"
cmd.exe /c "reg add HKCU\Software\Python\PythonCore\3.14\PythonPath /ve /d \"D:\scoop\apps\python\current\Lib\;D:\scoop\apps\python\current\DLLs\;\" /f"
```
关键：用 `current` symlink 路径而非具体版本号路径，这样 Scoop 更新后注册表自动跟随。

### 步骤 4：验证
```bash
python --version   # 应输出 Python 3.14.x
python3 --version  # 同上
py --version       # 同上
python -c "print(1+1)"  # 应输出 2
```

## 注意事项
- MSYS2/Git Bash 无法直接 exec 无 `.exe` 后缀的脚本文件，必须用 `.cmd`/`.bat` 或通过 `cmd.exe /c` 包装
- Windows Update 可能在系统更新后重建 WindowsApps stub，但 `.local\bin` 中的 `.cmd` wrapper 会在 PATH 中优先匹配
- 如果 python 仍不工作，检查 `echo $PATH | tr ':' '\n' | grep -i python` 确认路径顺序
- OMP harness bash 不读 `.bashrc`（非交互式），`/etc/bash.bashrc` 在非交互模式也 return，所以不能用 bash function 方案
