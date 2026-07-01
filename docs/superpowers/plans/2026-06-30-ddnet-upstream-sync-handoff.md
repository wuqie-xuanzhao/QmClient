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

截至当前 `HEAD`：

- 官方 `ddnet/master` 相对基线总提交数：`1316`
- 排除 merge commit 后的官方普通提交数：`786`
- 当前分支相对基线本地提交数：`401`
- 已记录的官方 cherry-pick trailer：`285`
- 去重后的官方 cherry-pick 提交数：`285`
- 仅按已 cherry-pick 计，剩余官方普通提交上界：`786 - 285 = 501`
- 已人工判定 covered / empty / skip 的提交会降低实际剩余量，但不会增加 cherry-pick trailer 数；当前仍应按约 `500+` 个待处理项规划后续批次。

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

## 2026-07-01 批量同步进展

本轮从 editor/Rust 工具链后续候选附近继续，按 20-50 个候选批量处理，当前停在官方 `654573ea634fff7c2ecac53bb37d6ab95c8499d1` covered/empty 之后，下一候选为 `3f8a31b44cac7f0864c5db325bd8ed39fbd03c22`。

已合入官方提交：

- `7ffb2bd5f17250fddb3a5b839434de21bee0f363` → `70d199b244 Reuse editor envelope point edit action for tangent reset action`
- `4a33f66df4aeda80b55af5bc741f9e9877d1e1a1` → `ccf1f89dfe Replace SHA256_ZEROED uses with std::optional<SHA256_DIGEST>s`
- `31f8efe0f308b54b7289a3f599de61caa878cf2f` → `8e2d0c664e Revert "Fix macOS client always changing current working directory"`
- `aa92a4389e95e7caf6cbd73b6aa41a30a83e25cc` → `7fa7aa42b4 Alternative fix for macOS CI: Detect whether running inside of DDNet.app`
- `7bcd3f11a0fca121046699032270c2cc872a25b0` → `2193162b41 Change sitting lifefrozen tee emote to blink`
- `b691998d0e53172830a3fd84fda056c878f24f1f` → `b4acf660b3 shutdown http without delay if there are no requests remaining`
- `5a859feeb5e03a7ad49ea690f82b5d3fa0fa7639` → `a3ecfa417f fix width height value selector visual glitch`
- `33849f53fe7d4951aefda0dd627ab72e95965346` → `13baae1fe5 fix map best time beeing assumed not finished instead of unset on zero legacy message`
- `ad2eeac70380eafb03486754a354a44142956e03` → `cb34fc0f20 Ensure strict-weak ordering for CMapNameItem::CompareFilenameAscending`
- `33f5db387609d0a6579d15b4ed4cc136b40abae3` → `2293250f05 Rename ddrace controller to ddnet controller`
- `bf24fad555ad60732e29bc7a40e890800dd3d702` → `d7eda7f4c0 Add c parameter for colors commands, cleanup argument parsing`
- `c8100f22b5c2293b51c4a86aba3898036b7aac35` → `14958347fd Make sure we always have a game type when we ask for it`
- `d8262dad4d2ec95920531a4fb22702fa282ee94a` → `461f4a5309 switch map best time interpretation dynamically on send time infos`

本轮 covered / empty 并跳过：

- `5477d5fa4301681d6b268c1481ed3f60268db010`：client-side weapon switch guard 拆分已由当前代码覆盖，cherry-pick 为空。
- `0390cb430658989e47b6bb3f7606904d7509f43e`：`cl_auto_demo_on_connect` 无效调用删除和描述语义已由当前代码覆盖，cherry-pick 为空。
- `9203a5d1017b383731603b182ff77745385878c8`：dummy 连接时 auto demo 重启修复已由当前代码覆盖，cherry-pick 为空。
- `654573ea634fff7c2ecac53bb37d6ab95c8499d1`：tile render 极端 zoom 下 `XR` 下溢保护已由当前代码覆盖，cherry-pick 为空。

本轮明确 skip / deferred：

- `29408b5acbe38027c7096144373c0f19ff6be85b`：P3 macOS codesign option。
- `a3f1f5b732c4110fd2dc512449475f96b7999383`：P3 macOS codesign CMake CI。
- `24dc7b389e69f749de70cc8ffd7773d309b98dc1`：P3 DMG background packaging。
- `a860b364d823a0f77a3073d2aed053a6c9d357ec`：P3 revert DMG packaging。
- `a2e6368ed89f5decbcaf0c5aa8372cb9a81a6357`：P3 translations。
- `f9e69715fca05595c96b26613cad954d4da202f5`：P3 upstream dev version bump。
- `0f3dcddf2ac6d9051e62ab7a2f36cc7cacde7e8d`：P3 DMG tooling。

本轮冲突处理要点：

- `7ffb2bd5f1`：`CEditorActionResetEnvelopePointTangent` 改为复用 `CEditorActionEditEnvelopePointValue`，保留 QmClient `pEditor->Map()` 结构和中文显示文本。
- `4a33f66df4`：保留 QmClient `std::optional<CMapDetails>` 结构，补齐 optional SHA256 语义；本地剩余 `SHA256_ZEROED` 等价改为 `SHA256_DIGEST{}` / `{}`。
- `5a859feeb5`：保留 editor 属性中文名，应用宽高 value selector 下限 `2` 和上游条件语义。
- `bf24fad555`：采用上游 `?c` color 参数和新 `ParseArgs` 结构，保留 QmClient `ExecuteLine` client id 适配。
- `d8262dad4d`：scoreboard title 竞速分数显示改用 `m_MapBestTimeSeconds/Millis` 与上游 race/time-score 判定，保留 QmClient 标题布局。
- `654573ea63`：保留 QmClient clip-aware tile 可见矩形路径，确认上游 `XR` 下溢保护已覆盖。

本轮验证：

- `cmd /c qmclient_scripts\cmake-windows.cmd --build cmake-build-release --target game-client -j 14`：通过。首次链接因 stale `menus.cpp.obj` 仍引用已删除的 `SHA256_ZEROED` 失败；源码无残留后删除该单个 stale object 并重跑，同一目标通过。
- `python qmclient_scripts/gate/check_gate.py --mode quick --base-ref d87500ace94a3d6ab43b2bbbbb49828952eaa9fb`：失败，7 项通过、2 项失败。失败项为既有格式债：`src/test/net_test.cpp`、`src/test/translate_llm_provider_test.cpp` 等 clang-format；ruff 仍为 `datasrc/network.py`、`scripts/generate_rust_bridge.py`、`scripts/generate_unicode_confusables_data.py`。本轮引入/暴露的 `src/game/server/gamemodes/ddnet.cpp` 格式问题已单独修复，第二次 quick gate 不再报该文件。

本轮量化：

- 本轮候选处理日志：`PICK` 16 次，其中实质合入 13 个官方提交；`covered/empty` 4 个；明确 P3 `skip/deferred` 7 个。
- 当前基线 `d87500ace94a3d6ab43b2bbbbb49828952eaa9fb` 之后本地提交数：355。
- 当前按 cherry-pick trailer 统计的唯一官方提交数：245。
- 按约 786 个官方普通提交估算，剩余上界：541；该数字未扣除历史和本轮 covered / skip，因此是保守上界。

## 2026-07-01 后续批次进展

本轮继续从 Rust 工具链后续候选推进，并把冲突直接解掉到批末。

已合入官方提交：

- `7d6365ab845e7a4635036665114b4f946a2c6bd8` → `836af7f5f8 cleanup DoPropertiesWithState`
- `ec8ce8dc85c324631f60857840961fbc52a96598` → `8cd7d31afc Reduce conditional compilation of debug dummies`
- `85f17b77ef8606f8170f18aba9ed3757a4ca03d2` → `6c4d4a8a6d Remove conditional compilation for dbg_stress`
- `f109640c2d0433d5b18172f2487dae99be787ce3` → `e38185dd60 Exclude debug only settings from html documentation`
- `a0828b6291c399aca15434df6ef39a017e637cbe` → `045c22b961 Include Android package name in log system/tag string`
- `ea670dcc104a724954a9c2399756b81e0039e053` → `9502942c68 demo menu mouse seek improvements`
- `5fda31e735e44c05f01be6db59d5f506384026f0` → `c6d5c79844 Show hundreths or thousands in scoreboard, scoreboard title and hud`

已判定 covered / empty 并跳过：

- `2c6c75f1cc32f2e8c534fd2559dbc0f7f5e62c7d`：`AntiPingPlayers` 已不再依赖 collision/hooking gating，且保留 QmClient fast-practice 强制预测分支，cherry-pick 为空。
- `b354144d9a66670a908abb1c834cfbef6a928878`：`Fix prediction when player has no weapon` 对应行为已被 QmClient 当前预测代码覆盖，cherry-pick 为空。

本轮冲突处理要点：

- `editor_props.cpp`：保留 QmClient 中文 tooltip / 按钮文本，合入上游 `DoPropertiesWithState` 的去重清理。
- `config_variables.h` / `export_settings_commands_table.py`：把 `CFGFLAG_DEBUG_SERVER` / `CFGFLAG_DEBUG_CLIENT` 语义接入导出脚本，保持 QmClient 中文配置文案。
- `scripts/android/cmake_android.sh` / `src/base/log.cpp` / `src/engine/server/main.cpp`：保留 QmClient Android 构建参数分层，新增 dotted package name 用于 Android log tag，JNI 仍用下划线形式。
- `menus_demo.cpp`：合入上游 demo seek 改进，同时保留 QmClient timeline marker 吸附逻辑。
- `scoreboard.cpp` / `ui.cpp` / `gameclient.cpp`：合入上游千分/百分精度时间显示，保留 QmClient scoreboard 布局、品牌列和 Points 列。

本轮验证：

- `cmd /c qmclient_scripts\cmake-windows.cmd --build cmake-build-release --target game-client -j 14`
  - 结果：通过。首次链接因 stale 对象文件还抓到旧符号，删除 `background.cpp.obj` 后重编通过。
- `python qmclient_scripts/gate/check_gate.py --mode quick --base-ref d87500ace94a3d6ab43b2bbbbb49828952eaa9fb`
  - 结果：失败 2 项，均为仓库既有格式债：
    - `src/test/net_test.cpp`、`src/test/translate_llm_provider_test.cpp` 等 clang-format 旧问题
    - `datasrc/network.py`、`scripts/generate_rust_bridge.py`、`scripts/generate_unicode_confusables_data.py` 的 ruff format

本轮量化：

- 本轮新增官方 cherry-pick trailer：`7`
- 当前基线后本地提交数：`372`
- 当前按 cherry-pick trailer 统计的唯一官方提交数：`261`
- 按约 `786` 个官方普通提交估算，剩余上界：`525`

剩余 gap：

- Rust / vendor / CMake 后续：`a4dd3b943daf50437405a2a3ce5f433398264809`、`03c77ae831e0aff149dd440d416553efcb427bcd`、`f1357b2ce86177a20e8a6e00077c1d431b1acda0`、`7bcb5a738e7bf1a14038f1327dba3ad925320e7c`
- `CScrollRegion` / editor map-object / editor object 相关 P2 批次
- demo / teehistorian / 0.7 兼容的剩余批次
- quick gate 的既有格式债未收口，当前不能当作仓库整体已通过

## 当前重要决策

## 2026-07-01 本轮批量推进记录

本轮从 Rust 工具链后续和后续客户端/基础设施候选继续推进，保持只 cherry-pick 官方单提交，不整体 merge / rebase。

已合入官方提交：

- `7bcb5a738e7bf1a14038f1327dba3ad925320e7c` → `3fb51a7be5 Upgrade cxx to 1.0.194`
- `73bbb3435a` → `7158b9f7b7 Minor refactoring of fullscreen popup rendering`
- `8cf6af652c` → `992fcd23ff Improve user experience when joining Tutorial server`
- `af4a274f82` → `c630f7a830 Assert that client slot not empty when sending reconnect/redirect`
- `6cc1dd44ef` → `d2389d3625 Fix clang-analyzer-security.ArrayBound by adding assertions`
- `f28d978cab` → `4372925924 Add some comments to client time functions`
- `3c6d7c9887` → `1fa9f6a5f7 Remove unused includes and definitions in system.cpp for macOS`
- `02f0df0dcd` → `f5b03a982d Remove incorrect doc about client time`
- `af7be5f58e` → `a2b25b4a0e Improve Emscripten log output in HTML wrapper`
- `e485cf2a0e` → `b7e9093170 Parse ANSI colors of log lines in Emscripten wrapper`
- `737d8e1d0c` → `9b8ef4765b Improve Emscripten error handling and quitting behavior`
- `936d30a4e3` → `b225bb431c Line split clangd array to help forks with git conflicts`

本轮 covered / empty 并跳过：

- `03c77ae831e0aff149dd440d416553efcb427bcd`：Rust 版本检查本地已覆盖，QmClient 已有 `1.85.0` MSRV 检查。
- `f1357b2ce86177a20e8a6e00077c1d431b1acda0`：Rust MSRV bump 本地已覆盖；保留 QmClient 自有 `build.yml`，不引入官方旧 CI 结构。
- `bde78f3202`：server auto demo 记录 tuning 的语义已由当前 Rust snapshot builder / `OnSnap(-1, ..., true)` 路径覆盖，解冲突后为空。
- `69dbf395dc`：racefinish 停止 demo/ghost 记录已由当前代码覆盖，cherry-pick 为空。
- `b55657a02c`：particle texture OOB 防护已由当前代码覆盖，cherry-pick 为空。
- `3a90f3e98d`：`std::optional<CMapDetails>` 已由当前代码覆盖，QmClient 保留更严格的 `m_Size` 校验字段。
- `3041cc262c`：`NETMSG_MAP_DETAILS` / `NETMSG_MAP_CHANGE` map size 校验已由当前代码覆盖，cherry-pick 为空。

本轮明确 skip / deferred：

- `a4dd3b943daf50437405a2a3ce5f433398264809`：merge commit，不按普通单提交 cherry-pick。
- `e152c80d52`：纯线程函数搬迁重构，冲突扩到 `CMakeLists.txt`、`system.cpp`、`jobs.cpp`，本轮普通同步线 deferred。
- `2302875af1`：font icons 搬到 `engine/font_icons.h`，冲突覆盖 HUD、菜单、编辑器多个 QmClient 高改区，作为整组重构 deferred。
- `9fcd88046e`：依赖 `2302875af1` 的 `font_icons.h` 字面量清理，随前置提交 deferred。

本轮冲突处理要点：

- `.github/workflows/build.yml`：保留 QmClient 自有 release/build workflow，只吸收 Rust MSRV 已覆盖事实；不回退到官方旧 CI。
- `scripts/generate_rust_bridge.py`：保持 QmClient 当前 Python 风格与 `cxxbridge 1.0.194`。
- `src/game/client/components/menus_start.cpp` / `menus.h`：教程按钮改用本地已有 `CMenus::JoinTutorial()` 状态机，保留 QmClient V2 start menu 布局。
- `src/engine/server/server.cpp`：保留 QmClient Rust snapshot builder / live observer 结构，不回退到旧 `m_SnapshotBuilder`。
- `src/game/client/components/menus_settings_assets.cpp`：保留 QmClient 10-tab assets 结构，只保留非法 tab 断言语义。
- `src/base/system.cpp`：保留 `_NSGetExecutablePath` 需要的 `<mach-o/dyld.h>`，移除未使用的 macOS 旧 include。
- `src/engine/client/client.cpp` / `.h`：保留已存在的 `std::optional<CMapDetails>`，同时保留 QmClient 的 map size 校验字段。
- `src/game/client/components/background.cpp`：补齐 `EIoSeekOrigin Origin` 变量名，收口官方 enum-class seek 改动后的 Windows 编译错误。
- `src/game/client/components/menus_settings_assets.cpp`：把非法 assets tab 断言接到 `switch` 的 `default` 分支，保留 QmClient 10-tab 刷新结构。
- `src/game/client/ui_scrollregion.cpp` / `.h`：当前代码已半吸收横向滚动参数，补齐 `ScrollRelativeDirect(vec2)`，保留旧 `float` 重载以兼容本地纵向调用。
- `src/game/client/components/voting.cpp`：恢复 QmClient 未完成地图投票链实现，修复头文件声明与实现丢失导致的链接错误。

本轮量化：

- 当前基线 `d87500ace94a3d6ab43b2bbbbb49828952eaa9fb` 之后本地提交数：`401`
- 当前按 cherry-pick trailer 统计的唯一官方提交数：`285`
- 按约 `786` 个官方普通提交估算，剩余上界：`501`
- 本轮处理候选约 `24` 个：实质合入 `12` 个，covered / empty `7` 个，明确 deferred / skip `4` 个，merge commit `1` 个。

本轮验证：

- `cmd /c qmclient_scripts\cmake-windows.cmd --build cmake-build-release --target game-client -j 14`
  - 结果：通过。期间修复 `background.cpp` 变量名、`menus_settings_assets.cpp` 悬空 `else`、`CScrollRegion` 双轴接口半合状态、`CVoting` 丢失实现；另清理了受 `CHeap::Allocate(size_t, size_t)` 签名变化影响的 stale MSVC 对象后重链通过。
- `python qmclient_scripts/gate/check_gate.py --mode quick --base-ref d87500ace94a3d6ab43b2bbbbb49828952eaa9fb`
  - 结果：失败 2 项，7 项通过。失败项已收敛为既有格式债：`src/game/client/components/countryflags.cpp` 的 clang-format；`datasrc/network.py`、`scripts/generate_unicode_confusables_data.py` 的 ruff format。本轮新增 `voting.cpp` 格式问题已修复。

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

1. 用户点名的 chunk unpacker 边界收口：
   - QmClient 当前 `CPacketChunkUnpacker::UnpackNextChunk` 已有 `pEnd` 边界判断，但仍需专项确认 skip loop 是否覆盖所有路径。
2. P2 必做区域：
   - `CScrollRegion` 横向滚动整组。
   - `CEditorMap` / `CEditorObject` 编辑器整组。
   - demo / teehistorian 格式提交，例如 `80aec863ef`、`5238ab4717`、`368ab203b3`、`9226cb6a69`。
3. 继续 P0 / P1 客户端修复：
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
