---
title: UI frame pacing fresh runtime 验收计划
date: 2026-06-18
last_reviewed: 2026-07-11
status: active
scope: 只验证已经落地的 UI attribution、Esc 预热和 Assets shell-first 行为；不继续扩展实现
---

# 当前状态

静态实现与自动化检查已经完成：perf parser/report/typecheck、六个 C++ source-contract、game-client 构建和 quick gate 均有历史 PASS 证据。唯一未闭环项是使用当前构建进行 fresh client runtime 采样。

旧日志只能作为背景，不能用于最终验收。若新报告仍失败，先记录具体 section，再为单一瓶颈创建新计划；不要在本文继续追加大范围优化任务。

# 验收阈值

## Ingame Esc

- frameMsP99 <= 16.7ms
- frameMsMax <= 33.4ms
- menuMsMax <= 12.0ms
- fpsOnePctLow >= 60

## Settings Assets

- cold first switch frameMsP99 <= 16.7ms
- repeated warm switch frameMsP99 <= 8.333ms
- menuMsMax <= 8.0ms

## TClient、QmClient、DDNet 设置页

- 目标窗口没有 ui_layout_or_render_total_ms > 16.7
- 任何失败都必须指向具体 UI section，不能只报告聚合 bucket

## 稳定文本

warmup 后目标窗口的 miss、stale、unplanned、fallback_immediate 和 build_queued 均为 0。

# 执行步骤

1. 串行构建当前 game-client。

   qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14

2. 启动 cmake-build-release/DDNet.exe，并启用：

   qm_perf_debug 1
   qm_perf_logfile 1
   qm_perf_debug_threshold_ms 4

3. 在同一新会话覆盖以下操作：

   - ingame Esc 连续打开两次；
   - ingame server info；
   - Settings Assets 首次冷切换和多次 warm 切换；
   - TClient、QmClient、DDNet 设置页切换；
   - Assets 页面滚动及滚动后稳定阶段。

4. 生成最新报告。

   cd qmclient_scripts/perf
   bun analyze.ts

5. 对照本文阈值记录报告路径、环境、operation signature 和结论。

# 收口规则

- 全部阈值通过：把准确证据写入提交说明，然后删除本文；Git 历史保存完成记录。
- 报告能定位具体 section 但阈值失败：本文保持 active，只记录失败 section，并为该单一瓶颈另写计划。
- 报告仍只有聚合归因：先修 perf report 可信度，不得凭体感优化。
- backend/GPU 只有在新报告证明占主导时才允许另立计划；本文不改 renderer、协议、物理、预测、demo 或文件格式。

# 当前 gap

尚未运行 fresh client runtime session 和最新日志分析。
