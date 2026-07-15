# 性能优化工作流

本文件用于性能优化、卡顿调查、长帧归因和页面降温任务。目标是用生产级性能量化系统驱动改动，而不是凭体感猜测。

## 入口原则

- 先采集基线，再写优化代码。
- 先归因，再决定技术路线。
- 先减少交互帧工作，再考虑缓存。
- FBO 不作为页面性能优化默认路线；不要新增、扩展或证明 FBO 收益。
- 任何性能结论都必须能对应到日志、报表、固定场景或 A/B 对照。

## 必备基线

性能优化前至少记录：

```text
qm_perf_debug 1
qm_perf_logfile 1
qm_perf_debug_threshold_ms 4
```

报告必须说明：

- 操作路径：例如打开 Settings、切到 Tee、滚动一屏、刷新 server browser。
- 环境：平台、renderer、窗口模式、刷新率、UI scale。
- 样本可信度：是否存在采样偏差、是否同一操作路径、是否有官方 DDNet baseline。
- 指标：p50、p95、p99、max、spike count、归因类别。

## 归因分类

长帧优先归到以下类别之一：

| 类别 | 典型字段 | 处理方向 |
| --- | --- | --- |
| Page switch | `event=page_switch` | 拆分切页同步工作，避免切页帧集中重建 |
| List frame | `event=list_frame` | 只处理可见行，缓存排序/筛选 plan |
| UI rebuild | `event=section`、`dirty`、`text_new` | 收紧 dirty，复用文本和布局 |
| Work drain | `event=work_drain`、`stop` | 分帧 drain，记录 stop reason |
| Resource | decode/upload/publish | jobs 化或预算化，不阻塞交互帧 |
| Device bound | `perf/device` | 判断 GPU/CPU/IO 是否真的打满 |

不能归因的长帧不是优化入口，先补 telemetry。

## 优化顺序

1. **不做**：跳过不可见 section、不可见列表行和无关后台结果。
2. **少做**：文本、布局、排序、筛选和 section plan 只在 dirty 时重建。
3. **分帧做**：merge、upload、publish、decode 结果消费按预算推进。
4. **异步做**：文件 I/O、图片 decode、列表 plan build 等不依赖 GPU context 的工作进入 jobs。
5. **证明后缓存**：缓存必须证明命中率、收益和内存/显存成本。

## 验收

性能优化完成时必须给出：

- 优化前后同一操作路径的 perf report 或等价日志摘要。
- 改动是否降低 p95/p99、spike count 或明确改善归因类别。
- 如果指标没有改善，说明保留改动的非性能理由；否则回退或改为后续调查。
- 对应测试或 gate 证据，常规代码改动至少跑 quick gate。
