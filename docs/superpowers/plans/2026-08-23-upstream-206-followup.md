# Upstream 206 Follow-up

## Status

- 本专项已收口：206 清单中登记的 3 个专项（128 player、IHttpRequest 生命周期、963f37bb 多地图 tabs）均已完成实现/审计并分别提交。
- 本记录不把真实混合版本会话、HTTP 网络请求、编辑器 UI 人工操作或 clang-format 历史问题误标为已验证；这些仍是明确的运行时/仓库卫生缺口。
- 下一步不再是 206 清单内的未完成专项，应另立新的上游批次或独立功能计划；当前工作树中的 Metal backend 改动不属于本次 206 收口。

- `e29d29ce8` 修复多地图异步保存绑定和 map-settings per-map 状态隔离。
- `0c66f660`、`8fb321d0` 不直接搬迁：两者针对已被上游回退的 C++ snapshot builder；当前 QmClient 使用 Rust `libtw2` builder/delta。
- 128 player 主体已由 `dae667a56` 及后续兼容修复落地；本轮进入行为审计与回归验证。
- `IHttpRequest` 解耦已由 `368d883a4` 落地，待独立网络生命周期审计。

## IHttpRequest Lifecycle Review Result

- 发现确定性阻断：客户端解耦后只注册 `IEngineHttp`，遗漏原有 `m_Http.Init()` / `m_Http.Shutdown()`；`CHttpCurl::Run()` 在 HTTP 状态为 `UNINITIALIZED` 时会等待初始化条件，首次 HTTP 请求因此永久等待。
- 在 `CClient::Run()` 的网络初始化成功后调用 `m_pHttp->Init(std::chrono::seconds{1})`；初始化失败记录错误、显示提示并终止客户端启动。
- 在客户端正常关闭、`Engine()->ShutdownJobs()` 前调用 `m_pHttp->Shutdown()`，保持 HTTP worker 生命周期早于 jobs 关闭。
- `game-client` 构建通过；C++ 全量 `2783/2783`、Rust 全量测试通过（updater 15 个、Rust doc-tests 34 个）。

### Remaining Gaps

- 尚未启动工作区内开发客户端执行真实 HTTP 请求和关闭流程；当前证据覆盖编译、测试与静态生命周期接线，未覆盖真实网络运行时。
- `check_gate.py --mode default` 仍可能被仓库既有 clang-format 违规阻断；若阻断，不把它归因于本专项改动。

## 963f37bb Multi-Map Tabs

### Scope

- 保持当前 `CEditorMap` 多实例、tab 切换、Ctrl+Tab/Ctrl+Shift+Tab、新建/打开/保存/关闭流程。
- 补齐 tab 的中键关闭、右键上下文菜单（关闭、复制名称、复制路径、打开所在目录）以及保存中/未保存状态提示。
- 不扩展到跨地图复制粘贴、批量关闭/保存、tab 重排或协议/地图格式改动。

### Status

- 核心多地图生命周期已在当前分支存在；本轮补齐上游 963f37bb 的缺失 tab 交互。
- 已完成：`game-client` 编译、C++ 全量 `2783/2783`、Rust 全量测试、专项 review。
- `check_gate.py --mode default` 的测试层通过；quick 层仍被仓库既有 11 个 clang-format 违规阻断，未发现本专项新增格式失败。

### Review Result

- 未发现严重或重要级正确性问题。关闭请求始终使用 tab index，异步保存完成仍按 `CEditorMap *` 匹配，避免同路径 tab 误关闭。
- 右键菜单在地图实例已被移除时主动关闭；保存中的地图禁用关闭并显示 spinner；未保存地图显示危险状态圆点，保留确认关闭流程。

### Remaining Gaps

- 尚未启动工作区内开发客户端做真实 UI 操作验证（中键关闭、右键菜单、Ctrl+Tab、异步保存期间关闭）；当前证据为源码审查、编译、全量测试和 gate。
- 跨地图复制粘贴、批量关闭/保存、tab 重排仍明确不属于本专项。

## 128 Player Review Result

- 修复 `CPlayerMapping::InitPlayer` 对未映射同 IP 玩家误设 reserved 的边界条件：`m_pReverseMap[i] == -1` 不再进入保留槽路径，避免后续更新永久跳过该玩家。
- 收紧 timeout protection 接管状态读取：在 `DelClientCallback` 前快照 DDNet 版本、flags 和 client brand，再写入接管槽，避免回调清理旧槽后丢失协议状态。
- `game-server` 构建通过；C++ 全量 `2783/2783`、Rust 全量测试通过。
- `check_gate.py --mode default` 的测试层通过，但 quick 层代码格式检查被仓库既有 clang-format 违规阻断；本轮未修改这些格式问题。

### Remaining Gaps

- 尚无混合 0.6/0.7/128 人运行时场景覆盖映射、see-others 分页和 timeout reconnect；后续需独立集成测试或人工会话验证。
- `CPlayerMapping` 尚无可隔离的单元测试 seam，本轮以静态边界审计和全量回归为主。

## 128 Player Audit

### Scope

- legacy 64-slot client ID map and reverse map
- 0.7/sixup client-info and chat translation
- timeout reconnect map stability
- see-others paging and team-state projection
- demo, vote, whisper, projectile and hook client-id translation

### Evidence To Collect

1. Static call-path review of `CPlayerMapping`, `IServer::Translate`, and `ReverseTranslate`.
2. Existing C++ full tests and protocol boundary tests.
3. Targeted regression tests for mapping bounds and unsupported client versions where a deterministic seam exists.
4. Manual/runtime validation remains required for mixed 0.6/0.7/128-player sessions and timeout reconnects.

### Stop Conditions

- Do not change protocol fields or snapshot layout in this audit.
- Do not claim 128-player behavior complete without mixed-version runtime evidence.
- Any finding involving prediction, demo semantics, or team protocol requires a separate focused patch and review.
