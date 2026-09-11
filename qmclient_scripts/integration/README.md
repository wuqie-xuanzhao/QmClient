# QmClient 进程级测试

这里放 QmClient 专属的真实进程冒烟和端到端测试。根目录 `scripts/` 是 DDNet 上游同步区，不在其中增加 QmClient 场景。

运行最小冒烟集：

```text
python qmclient_scripts/integration/qmclient_smoke.py <build-dir>
```

运行单个场景：

```text
python qmclient_scripts/integration/qmclient_smoke.py <build-dir> gores_configuration
```

运行端到端场景：

```text
python qmclient_scripts/integration/e2e_qmclient.py <build-dir>
python qmclient_scripts/integration/e2e_qmclient.py <build-dir> demo_recording
```

可用的 E2E 场景包括 `demo_recording`、`qm_lifecycle_persistence`、`invalid_statistics_preserved`、`perf_log_persistence`、`connection_failure_recovery` 和 `recording_without_connection`。每个场景使用独立临时目录，并验证真实进程产生的日志、退出状态和文件产物。gate 中使用 `--run-qm-smoke` 时会先运行 4 个 smoke，再运行 6 个 E2E。

进程测试必须从外部可观察结果断言启动、连接、日志、退出和失败回退。客户端或服务端崩溃必须失败，不得通过放宽超时或忽略退出码掩盖。
