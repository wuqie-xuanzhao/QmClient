# 回归防护工作流

本文件用于固定场景、A/B 对照、release checklist、自动化测试和长期质量闭环。

## 核心问题

每修一个 bug 或性能问题，都问：

```text
以后怎么自动知道它又坏了？
```

如果不能自动知道，至少要进入固定手工场景或 release checklist。

## 固定场景

当前仓库没有独立的根目录 `test_maps/`、`test_demos/` 或 `test_configs/`，不要把不存在的目录写成既有资产，也不要为满足文档创建空目录。固定场景按任务选择真实可维护的载体：

- 自动化测试中的 fixture 或源码合同。
- 当前活动 plan/spec 中可重复执行的操作步骤、输入和失败信号。
- 性能任务使用的同环境、同操作、同指标 DDNet A/B 记录。
- 无法自动化的视觉检查清单、截图或录屏证据。

需要新增地图、demo 或配置样本时，先确认现有测试入口和模块归属，再把非空样本放到对应测试可实际消费的位置。固定场景必须记录操作步骤、期望结果或指标和失败信号。

## 自动化优先级

优先自动化：

1. config parser / migration。
2. server browser parser。
3. demo playback smoke。
4. map loading smoke。
5. settings/page telemetry contract。
6. perf report parser/statistics/report generation。

视觉和手感类问题如果无法完全自动化，也要保留截图、录屏或 checklist。

## Release 前检查

发布或大范围合并前至少确认：

- quick/default/full gate 选择合理。
- C++ 和 Rust 测试按风险覆盖。
- 性能改动有前后对比。
- UI 改动做过视觉检查。
- 已知 review findings 收口。
- 未完成事项写成明确 gap，而不是隐含为 done。

## Changelog 维度

质量类 changelog 优先按维度写：

- Diagnostics
- Rendering
- Networking
- Input
- Prediction
- Performance
- Regression prevention

这比“修了一些问题”更利于长期追踪质量改进。
