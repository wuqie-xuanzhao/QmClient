# DDNet 官方上游同步交接记录

status: active

## 当前工作区

- 主工作区：`E:/Coding/DDNet/QmClient`，本轮未在主工作区改动。
- 隔离 worktree：`C:/Users/11054/.config/superpowers/worktrees/QmClient/codex-upstream-sync-ddnet-2026-06-29`
- 分支：`codex/upstream-sync-ddnet-2026-06-29`
- 基线 commit：`d87500ace94a3d6ab43b2bbbbb49828952eaa9fb`
- 官方 remote：`ddnet/master`
- 同步策略：只 cherry-pick 官方 `ddnet/ddnet` 单提交；不以 QmClient `origin/master` 作为同步基线；不整体 merge / rebase 官方分支。

## 量化进度

截至 `7739668a63 Rename generated protocolglue.cpp to protocolglue_generated.cpp`：

- 官方 `ddnet/master` 相对基线总提交数：`1316`
- 排除 merge commit 后的官方普通提交数：`786`
- 当前分支相对基线本地提交数：`198`
- 已记录的官方 cherry-pick trailer：`123`
- 去重后的官方 cherry-pick 提交数：`116`
- 仅按已 cherry-pick 计，剩余官方普通提交上界：`786 - 116 = 670`
- 已人工判定 covered / empty / skip 的提交会降低实际剩余量，但不会增加 cherry-pick trailer 数；当前仍应按约 `650+` 个待处理项规划后续批次。

## 最近完成的验证点

在提交 `b93a6d254f chore(sync): 修复 libtw2 vendor 构建适配` 后，已验证：

- `python qmclient_scripts/gate/check_gate.py --mode quick --base-ref d87500ace94a3d6ab43b2bbbbb49828952eaa9fb`
  - 结果：通过 `10` 项，`0` 失败。
- `cmd /c qmclient_scripts\cmake-windows.cmd --build cmake-build-release --target game-client -j 14`
  - 结果：通过。

注意：`git submodule update --init --recursive` 对 `docs/QmClient_docs` 失败，原因是远端不包含 gitlink `89a27cdefc839065ff33738d06cf4b259f2db2b6`。本轮已用 `git submodule deinit -f docs/QmClient_docs` 清理失败初始化状态；构建和 quick gate 不依赖该文档子模块。

## 最近合入或处理的官方提交

已实际 cherry-pick：

- `2d670ac5536741b0704d99346251cb42074e7099` → `26da63b253 Vendor libtw2`
- `77d544806305d12d0ba55bcbf2ebe62831526d47` → `22306a59c0 Fix server-side 0.7 snapshots by having two snapshot delta objects`
- `1cf25f5ea628bfe1db1c0023e3fe57ce9c5fb170` → `7739668a63 Rename generated protocolglue.cpp to protocolglue_generated.cpp`

已判定 covered / empty 并跳过：

- `35efb390b1dd4199e6e9122e4c65327cd940cfda`：官方新增 `generate_protocol` target；QmClient 当前 `CMakeLists.txt` 已有同等 target 和 `engine-shared` / `game-shared` 依赖。冲突解决后 cherry-pick 为空，已 `git cherry-pick --skip`。
- `0c174c9ca1e39b61a5075963c687693b4eeab088`：官方封装旧 `CNetRecvUnpacker` 成员并删除未用 `m_aBuffer`；QmClient 当前已经使用 `CPacketChunkUnpacker`，成员已私有化且没有旧 `CNetRecvUnpacker::m_aBuffer`。本轮已跳过，后续应单独处理用户要求的 chunk unpacker 边界检查，而不是回退为官方旧类形态。

## 当前重要决策

- snapshot/Rust 线保留 QmClient 现有 Rust bridge，不接受官方旧 C++ builder / delta API 回退。
- server snapshot 继续保留：
  - `rust::Box<CSnapshotDelta> m_pSnapshotDelta`
  - `rust::Box<CSnapshotDelta> m_pSnapshotDeltaSixup`
  - `rust::Box<CSnapshotBuilder> m_pSnapshotBuilder`
- `src/engine/shared/sixup_translate_snapshot.cpp` 是官方旧路径；QmClient 使用 `src/game/client/sixup_translate_snapshot.cpp`。
- Windows Vulkan 默认行为暂保留 QmClient 当前偏好，不跟官方禁用默认，除非另有明确决策。
- `Cargo.lock` 不要盲目接受官方旧 lock；当前 `game-client` 构建已验证 vendor 替换链路可用。
- `vendor/**/*.rs` 已在 `.gitattributes` 固定 LF，避免 Windows checkout 改写 Cargo vendored source checksum。

## 当前剩余重点批次

优先继续处理：

1. snapshot / Rust 工具链后续：
   - `a4dd3b943d`、`03c77ae831`、`f1357b2ce8`：Rust 版本检测和最低版本。
   - `7bcb5a738e`：升级 `cxx` 到 `1.0.194`，需要构建验证。
2. 用户点名的 chunk unpacker 边界收口：
   - QmClient 当前 `CPacketChunkUnpacker::UnpackNextChunk` 已有 `pEnd` 边界判断，但仍需专项确认 skip loop 是否覆盖所有路径。
3. P2 必做区域：
   - `CScrollRegion` 横向滚动整组。
   - `CEditorMap` / `CEditorObject` 编辑器整组。
   - demo / teehistorian 格式提交，例如 `80aec863ef`、`5238ab4717`、`368ab203b3`、`9226cb6a69`。
4. 继续 P0 / P1 客户端修复：
   - 网络、浏览器、音频、渲染、0.7 兼容、demo / ghost 健壮性。

## 下一会话启动建议

1. 进入 worktree：

   ```powershell
   cd C:/Users/11054/.config/superpowers/worktrees/QmClient/codex-upstream-sync-ddnet-2026-06-29
   git status --short --branch
   ```

2. 确认当前 tip：

   ```powershell
   git log --oneline -5
   ```

3. 从 Rust 工具链提交继续：

   ```powershell
   git cherry-pick -x a4dd3b943daf50437405a2a3ce5f433398264809
   ```

4. 每个小批次结束至少跑：

   ```powershell
   python qmclient_scripts/gate/check_gate.py --mode quick --base-ref d87500ace94a3d6ab43b2bbbbb49828952eaa9fb
   ```

5. 涉及 Rust / vendor / CMake 后补：

   ```powershell
   cmd /c qmclient_scripts\cmake-windows.cmd --build cmake-build-release --target game-client -j 14
   ```

## 汇报要求

每次阶段汇报至少写清：

- worktree 路径
- 基线 commit
- 已合入的官方提交
- covered / empty / skip 的官方提交
- 冲突点和解决方式
- 验证命令和结果
- 剩余未做项和量化剩余量
