# 线程与 Jobs 工作流

本文件用于后台任务、资源 jobs、队列、预算、取消、主线程发布和 GPU context 边界。

## 线程边界

- GPU 上传、UI state 修改、渲染对象发布必须在主线程。
- 文件 I/O、图片 decode、列表 plan build 可以进入后台。
- 后台 job 输出必须是可验证的数据，不是直接修改 UI 的副作用。
- 共享状态需要 owner 或同步策略，不要临时加锁掩盖设计不清。

## Jobs 生命周期

每类 job 必须定义：

- 请求 key 和去重策略。
- 优先级：visible、prefetch、background。
- 取消条件：页面离开、筛选变化、资源版本变化。
- 发布条件：目标仍有效、版本仍匹配、预算允许。
- fallback：结果缺失时 UI 仍能显示 source/live 路径。

## 主线程预算

交互帧中 drain job 结果必须有预算：

- 限制 count、bytes 或 duration。
- 记录 `work_drain` 的 `kind`、`count`、`bytes`、`dur_ms`、`stop`。
- `stop` 不能只写 success；预算耗尽、队列空、目标失效都要区分。
- 大量结果不能在页面切换帧集中发布。

## 验收

线程/jobs 改动至少验证：

- 取消后不发布旧结果。
- 同 key 重复请求不会产生错误覆盖。
- 页面离开再回来仍能恢复。
- perf 日志能解释 drain 是否被预算截断。
