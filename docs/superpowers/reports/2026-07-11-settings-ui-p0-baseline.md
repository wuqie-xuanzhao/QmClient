# Settings UI P0 合并前验证基线

- 基线提交：`d0e693e956aeb1a33fc8cbf29a5328c93c271f29`
- 目标范围：`4f76dcb4e7b5bc86bba9a271b563a8b6a34d53f3^..8ee4fa22ba0172a66605b2c5033f0736d66ced34`
- 对象数：12（11 个普通提交、1 个 merge 路由提交）。
- 构建依赖：当前 checkout 已执行 `git submodule update --init --recursive`；`ddnet-libs` 与 `docs/QmClient_docs` 均已初始化。
- 用户并行改动：6 个 `data/qmclient/icons/qm_icons_*` 文件保持未暂存、未纳入本报告和后续 merge 提交。

| Check | Command | 合并前证据 |
|---|---|---|
| game-client | `cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14` | PASS，退出码 0，85.35 秒；完成 `DDNet.exe` 链接。 |
| testrunner | `cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14` | PASS，退出码 0，155.40 秒；完成 `testrunner.exe` 链接。 |
| run_cxx_tests | `cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14` | PASS，退出码 0，130.95 秒；169 个 suite、2,030 个测试全部通过。 |
| check_docs | `python qmclient_scripts/gate/check_docs.py` | PASS，退出码 0，0.11 秒；AGENTS / CLAUDE 镜像一致。 |
| quick gate | `python qmclient_scripts/gate/check_gate.py --mode quick` | PASS，退出码 0，37.51 秒；10 项通过、0 警告、0 失败。 |

## 记录说明

最初按计划文本中的裸 `cmd /c qmclient_scripts/cmake-windows.cmd ...` 启动前三个 Windows 命令时，PowerShell 没有调用批处理宿主，均在约 0.05 秒内以退出码 1 结束且无构建输出；这不是编译或测试失败。随后以仓库 Windows 规范的 `cmd /c call ...` 串行重跑，以上表中的结果才是有效基线。

本报告只描述 merge 前 `dyl_dev` 状态，不能用来证明远端范围或后续修复无回归；merge 后必须重新完成目标构建、全量 C++/Rust、docs、default gate 和 `git diff --check`。
