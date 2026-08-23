# Upstream 206 Follow-up

## Status

- `e29d29ce8` 修复多地图异步保存绑定和 map-settings per-map 状态隔离。
- `0c66f660`、`8fb321d0` 不直接搬迁：两者针对已被上游回退的 C++ snapshot builder；当前 QmClient 使用 Rust `libtw2` builder/delta。
- 128 player 主体已由 `dae667a56` 及后续兼容修复落地；本轮进入行为审计与回归验证。
- `IHttpRequest` 解耦已由 `368d883a4` 落地，待独立网络生命周期审计。

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
