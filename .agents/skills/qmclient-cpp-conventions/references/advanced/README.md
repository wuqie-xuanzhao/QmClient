# Advanced AI 工作流

`references/advanced/` 存放跨会话稳定的专项工程规则。这里不是历史记录，也不是某个功能计划；只有当任务触发对应风险时才读取对应文件。

## 阅读路由

| 任务类型 | 先读 |
| --- | --- |
| 性能优化、长帧归因、页面降温 | `performance-workflow.md` |
| 性能量化系统本身的迭代 | `perf-system-workflow.md` |
| 行为保持型代码重构 | `refactor-workflow.md` |
| 新特性或新配置引入 | `feature-introduction.md` |
| 下载、文件、日志、用户反馈包、外部输入 | `safety-security.md` |
| 缓存、纹理、指针、UI element、生命周期 | `memory-lifetime.md` |
| jobs、后台任务、主线程发布、GPU context | `threading-jobs.md` |
| debug 面板、debug bundle、复现信息 | `observability-debugging.md` |
| 固定场景、A/B、release 前回归防护 | `regression-prevention.md` |

## 使用原则

- 只读和当前任务直接相关的文件。
- 如果专项规则和 `../ddnet-development.md` 冲突，优先保留 DDNet 兼容性和当前代码模式。
- 如果专项规则需要变成脚本约束，优先补 gate 或测试，不要只扩写文档。
- 性能量化系统按生产级诊断系统对待，不再按 MVP 口径规划。
