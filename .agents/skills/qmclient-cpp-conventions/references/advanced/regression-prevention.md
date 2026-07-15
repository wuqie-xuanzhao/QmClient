# 回归防护工作流

本文件用于固定场景、A/B 对照、release checklist、自动化测试和长期质量闭环。

## 核心问题

每修一个 bug 或性能问题，都问：

```text
以后怎么自动知道它又坏了？
```

如果不能自动知道，至少要进入固定手工场景或 release checklist。

## 固定场景

维护以下资产：

- `test_maps/`：地图加载、渲染、prediction、资源边界。
- `test_demos/`：demo playback、demo browser、回放兼容性。
- `test_configs/`：UI scale、renderer、语言、输入、资源配置。
- 官方 DDNet A/B 表：同一环境、同一操作、同一指标。

固定场景必须记录操作步骤、期望指标和失败信号。

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
