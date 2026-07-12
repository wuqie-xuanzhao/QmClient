> 请抬头享受阳光｜日子很好 我很我---------致咩子
# QmClient Perf — 性能日志分析工具

## 快速开始

```bash
cd qmclient_scripts/perf
bun install
bun analyze.ts          # 自动读取最新日志
bun analyze.ts path/to/qm_perf_xxx.log  # 指定日志文件
```

## 前置条件

游戏运行时需开启：

```
qm_perf_debug 1
qm_perf_logfile 1
```

日志输出到 `%APPDATA%/DDNet/dumps/QmClient_Perf/qm_perf_*.log`。

## 输出

生成与日志同名的 `_report.html` 文件，浏览器打开即可查看交互式报表。

## 生产级使用约定

- 默认采集阈值使用 `qm_perf_debug_threshold_ms 4`；4.17ms 只表示 240Hz 帧预算线。
- HTML 报表用于人工分析，`*_summary.json` 用于按日志归档，固定名 `perf_summary.json` 用于 debug bundle 自动拾取。
- 自动选择的上一份日志只作为趋势提示；只有 page/system operation signature 一致时，才可以作为严格对比依据。
- 空日志、缺字段、畸形行和采样偏差必须看作质量警告，不能解读为性能优秀。

## 固定场景

性能优化 PR 的前后对比应使用相同的页面、操作步骤和系统环境，避免把不同操作路径的日志当成严格回归数据。

## 报表内容

- KPI 卡片: p50 / p95 / p99 / Max / 240Hz / 120Hz / 60Hz 合规率
- Session 自动对比（自动使用当前日志的上一份日志）
- 采样偏差提示（使用 p5 估计采样阈值）
- 帧时间趋势图、直方图 + KDE、QQ 图、百分位图
- 页面级耗时分解与尖峰帧详情
- 页面性能归因: page switch / list frame / UI rebuild / work drain
- Section Top-10（基于当前 `perf/section` 样本，仍受 C++ 覆盖面限制）
- 交互窗口、Tee 收敛、设备资源表格

## 开发

```bash
bun run analyze    # 等同于 bun analyze.ts
bun run test       # 使用 test/sample.log 和内联边界样本验证解析/报表
```
