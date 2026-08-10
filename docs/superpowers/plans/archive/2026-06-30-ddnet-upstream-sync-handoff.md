> **已归档，禁止作为实现依据。** 本文保留为历史记录；当前实现请以活动 spec/plan/skill 为准。

# DDNet 官方上游同步交接记录

status: archived

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

## 2026-07-01 独立修复小批次进展

本轮在 Rust/vendor/CMake 验证收口后，继续尝试 IMap / client / network / server 附近候选。IMap 入口提交冲突面涉及 client map 所有权、背景地图、菜单背景和 server map 接口，已作为 P2 架构组 deferred；随后切换到独立小修复批次。

已合入官方提交：

- `c0c9934db2418756c91af7a0b919ae66c5c8e47f` → `325f492dcd Make descriptions of refresh rate settings more clear`
- `be9e4ef823b1627b67449eac03957e6bdde3316a` → `661423fc9e fixup! Make descriptions of refresh rate settings more clear`
- `72319957ce916c3e4ed17d9c4147c480b3ea349b` → `027b13c25d Fix malformed network chunk sending for maplist`
- `36ea91508ed47c84797cbbe0e0ca9063aa6c4e4c` → `2cb0c010b8 Add assertion to check chunk size in CNetChunkHeader::Pack`
- `dbacbe161f13d0796c5ed27205634bbb1d763ce3` → `a7a33c8e4d Add FindPlayerByName() helper`

本轮 covered / empty 并跳过：

- `797a3c38cf8fa2ea5debe182b544949d884f2756`：`TEAM_SPECTATORS` 替换已由本地代码覆盖，cherry-pick 为空。
- `ced6ea148da4b858ecb8056d73bb2ce23bf6108e`：多行粘贴替换空格的 chat 行为已由本地代码覆盖，cherry-pick 为空。
- `7ec722151b0c8251c2e5b39c92941a6ad7ab1ff3`：Hook Line Tip alpha 预览行为已由 QmClient 设置页/预览代码覆盖，cherry-pick 为空。
- `4aa818fd8b83bba3bcb31aac157c3bdf5166c7fb`：SDL3 headless `SetWindowGrab()` 崩溃修复已由当前 backend_sdl 覆盖，cherry-pick 为空。

本轮明确 skip / deferred：

- `14adb4cf0ccd16f7d9c6aca4aad90ea77fe99279`：IMap 非 kernel 接口合并入口，冲突覆盖 `client.cpp`、`background.cpp/h`、`menu_background.cpp/h`、`gameclient.cpp/h` 和 server map 接口；作为 IMap P2 架构组 deferred。
- `8cb5f698c97078cf560421ea0aa7f7ae57146e0c`、`911b4f93261557616a5392c50a6bbf0b8fb51780`、`b07f47a2098fda43dc8c89ef3543ced007758728`、`863bad84aa11fa5078c60f45ac9df72f611b1055`：依赖 IMap 入口的后续组，随前置 deferred。
- `f1de2b9214235ef4c7e8f66c701449545eefe991`：标准头检查脚本 Python 重写冲突 QmClient 自有 governance workflow 和已存在脚本，本轮 deferred。
- `3a15659d56e7485fb8cba9b8aa290ac9ef352f3b`：server rcon auth 纯结构搬迁，冲突在高改 server 区，本轮 deferred。
- `35951acb9f09df7308c33a8561cb279a29415b65`：server client version 方法搬迁，冲突在高改 server 区，本轮 deferred。

本轮冲突处理要点：

- `config_variables.h` / `menus_settings.cpp`：保留 QmClient 中文配置文案和设置页 tracking wrapper，合入上游对 game update rate 与 rendering refresh rate 的命名区分。
- `network.h` / `server.cpp`：合入 maplist chunk 发送边界修复，保持 QmClient server 结构。
- `network.cpp`：合入 `CNetChunkHeader::Pack` chunk size 断言。
- `gamecontext.cpp/h`、`ddracechat.cpp`、`player.cpp`：合入 `FindPlayerByName()` helper，保留本地 server 行为。

本轮验证：

- `python qmclient_scripts/gate/check_gate.py --mode quick --base-ref d87500ace94a3d6ab43b2bbbbb49828952eaa9fb`
  - 结果：失败 2 项，7 项通过。失败项仍为既有格式债：`src/game/client/components/countryflags.cpp` 的 clang-format；`datasrc/network.py`、`scripts/generate_unicode_confusables_data.py` 的 ruff format。
- `cmd /c qmclient_scripts\cmake-windows.cmd --build cmake-build-release --target game-client -j 14`
  - 结果：通过；仅有 `client.cpp` 若干 `[[nodiscard]]` warning。

本轮量化：

- 当前基线 `d87500ace94a3d6ab43b2bbbbb49828952eaa9fb` 之后本地提交数：`407`
- 当前按 cherry-pick trailer 统计的唯一官方提交数：`290`
- 按原交接约 `786` 个官方普通提交估算，剩余上界：`496`
- 当前本地 `ddnet/master` 相对基线普通提交数为 `787`，按该动态口径剩余上界：`497`

## 2026-07-01 UI / editor / graphics 修复批次进展

本轮从 `dbacbe161f` 后续继续挑可独立合入的 UI、editor、graphics 和 client 行为修复，跳过 base 文件搬迁、server 结构搬迁和 i18n/CI。

已合入官方提交：

- `2eab41a6f00d79fb9d7b13bd4dc09bda1ddad6ec` → `d9b6a53eb0 add real HSL values to the Hsla scrollbars`
- `fb49e396c999345a2f27a125b34f9bc86b91b58f` → `ea62c77edf Fix redo of envelope edit point time action`
- `8863fe6101cfeb0047b9c5ade4219f9849c2026a` → `c6aaddeb34 Fix redo of envelope edit action`
- `a6275b59f3a1be6c9b73be5ea1fcc079f53f6404` → `85d75a8a69 Fix: Editor can't find tele out (#11854)`
- `9fc61447255d56b3a4c0573a84a6599b041354d2` → `bb29a59a60 Fix editor map not being saved if editor is quit before job is done`
- `53bddfdebdebf3f980d285a13f7f8010d78d81e7` → `643d6499c0 Improve OpenGL backend logging`
- `3e89dca02c62821ce8a82ead2522abb18d310656` → `34ae4772be Keep selected player highlighted while scoreboard popup is open`

本轮本地 follow-up：

- `7056e907e1 fix(sync): 补齐编辑器保存任务日志依赖`：为 `9fc6144725` 引入的 `log_error` / `log_trace` 调用补充 `base/log.h` include，修复 Windows `game-client` 构建。

本轮 covered / empty 并跳过：

- `703b4a046750d4e06172b6d116726e61593e1d1a`：death tests stuck 修复已由本地测试入口覆盖，cherry-pick 为空。
- `d2165262cb28a007657f886f70932fe98e4fe1bb`：scrollbar 多 flag 行为已由当前 UI 代码覆盖，cherry-pick 为空。
- `f71afae23a55c1f7b07503ea64de27b50e0a7806`：vanilla pickup sound 预测修复已覆盖，cherry-pick 为空。
- `e782b2b0ed938521dfc3fe2432b1391124e3db0a`：0.6 rcon command completion 修复已覆盖，cherry-pick 为空。
- `32fef046e079cb3448caa94ff8df574d9c792ef5`：redo envelope point crash 修复已覆盖，cherry-pick 为空。
- `4299da8fb8b726b3ed22e329abbe8c268b8bdc39`：Vulkan heap binary search UB 修复已覆盖，cherry-pick 为空。
- `e34cc23628ef8ba8cc6ed8e0a2e49a49cf04788e`：fatal graphics error popup 改进已基本覆盖；冲突解后无净 diff，保留 QmClient `dumps/QmClient_Crash` 路径。
- `5d05431b81f334c416d283e37a08fffac6149722`：dummy control input storage 修复已覆盖，cherry-pick 为空。

本轮明确 skip / deferred：

- `35951acb9f09df7308c33a8561cb279a29415b65`：server client version 方法搬迁，前一轮已判定高改 server 区 deferred。
- `7e13c27055ccb1ccb74c8c9feb4d84d656c47900`、`57ab0e6560c038ecc57f917e02fdf43d2dd5ee0d`、`c4b0813f0df002ecabf2c52035eb70275493597e`、`8353085e21aa9a88b099e9754684923422abaabe`、`6e321611f767a642aed05842a283c6e04899ceb4`、`e63b31d2662dcf564359f3dfe3aca13a1a0057e8` 等：base/process/bytes/os/net 文件搬迁或文档重构，普通同步线 deferred。
- `3552f7c9eb68d2bf0271f329aa21d7a1765ddaf0`、`58b19570fbefcb708fdcf236af4f79b8f20e9a3d`、`aba214e895aef9c9e80871a40297d9d0c76827bd`、`27eb413e4ac71c413ec7f011e5604534923fcc20`、`7de1b10535ff01b2d0870adcde4f82a549268c49` 等：gameplay / prediction 行为决策项，需按兼容性边界单独审。

本轮冲突处理要点：

- `editor.cpp` / `mapitems/map_io.cpp`：采用上游把最终 rename 放进 writer job 的修复，保留 QmClient 协作快照保存失败状态和成功后 `UploadCollabSnapshot()`。
- `backend_opengl.cpp` / `backend_opengl3.cpp`：采用上游 `log_warn` / `log_debug` 日志 API，同时保留 QmClient 更详细的 texture resize slot/size 信息。
- `scoreboard.cpp`：保留 QmClient `ClientData` 按钮 ID 结构，只补 popup 打开时继续高亮选中玩家的条件。

本轮验证：

- `python qmclient_scripts/gate/check_gate.py --mode quick --base-ref d87500ace94a3d6ab43b2bbbbb49828952eaa9fb`
  - 结果：失败 2 项，7 项通过。失败项仍为既有格式债：`src/game/client/components/countryflags.cpp` 的 clang-format；`datasrc/network.py`、`scripts/generate_unicode_confusables_data.py` 的 ruff format。
- `cmd /c qmclient_scripts\cmake-windows.cmd --build cmake-build-release --target game-client -j 14`
  - 结果：通过。

本轮量化：

- 当前基线 `d87500ace94a3d6ab43b2bbbbb49828952eaa9fb` 之后本地提交数：`415`
- 当前按 cherry-pick trailer 统计的唯一官方提交数：`297`
- 按原交接约 `786` 个官方普通提交估算，剩余上界：`489`
- 当前本地 `ddnet/master` 相对基线普通提交数为 `787`，按该动态口径剩余上界：`490`
## 当前重要决策

## 2026-07-01 editor / UI / server 小批次推进记录

本轮从 `be3dd82a5f` 后续候选继续，优先处理 editor、scoreboard/serverbrowser、server 投票与 text 渲染附近的小修复；遇到大范围 base 搬迁、协议对象扩展、editor map/object 重构时按 P2 deferred，不强行塞入普通同步线。

已合入官方提交：

- `cbd4d12091700ab73c8e30dd83f51a502afb6f6f` → `1e248c73ef Fix editor value selectors returning property even when unchanged`

本轮 covered / empty 并跳过：

- `be3dd82a5f3b3762cf99d7555c73d9fc66dbeadb`：QmClient 已有独立 `CQuadPopupContext` / `CPointPopupContext`，并且 editor 选择状态已迁移到 `Map()`；保留本地 MapView/editor 架构。
- `785fb4fdfcb83ea61c8b687cd7bb7b02c7d8a7ef`：`std::begin` / `std::end` tracker 写法已覆盖，冲突解后无净 diff。
- `f48532d205b1be6df41ba26ac9f33abb60308c81`：`CEditorActionEditQuadPoint::Apply()` 已存在，保留 QmClient 额外 `CEditorActionEditQuadColor`。
- `72a5a8643263d3d740894b911d615f12ff33e4c5`：设置所有 quad points 颜色的 action / tracker / popup 路径已由 QmClient 覆盖，保留本地中文 UI 与 `Map()` 结构。
- `4c806941f8`：yes forced vote 已按当前 console 签名走与普通 passed vote 等价的 `ExecuteLine(m_aVoteCommand, IConsole::CLIENT_ID_UNSPECIFIED)` 路径。
- `a2cb0702e1`：server-only `logfile` 标记已覆盖，cherry-pick 为空。
- `1a584373af`：scoreboard 玩家行与 spectator 行点击 popup 已由 QmClient 当前布局覆盖，保留本地品牌列、SMTC 面板和 streamer identity 处理。
- `70225fe48e`：friend highlighting 已由 QmClient 朋友列表颜色/分类逻辑覆盖，冲突解后为空。
- `671d2edcb9`、`71c561d394`、`d5fecf9550`、`db530cfafe`：当前 server / motd 代码已覆盖对应小清理或限制修复，cherry-pick 为空。
- `8837a59425`：`CTextCursor::m_MaxLines` 与显式换行的边界处理已覆盖，保留本地 selection / calculated line-end 检查。

本轮明确 skip / deferred：

- `557b59bbbc`、`c9bb2a195a`：Windows-specific system 函数搬迁，归入 base/windows 文件搬迁 P2 组。
- `aed6660c52`：`CEnvelopeState` 重构会覆盖 QmClient 预测 tick / demo playback / spectator 分支，归入客户端 envelope 行为审查 P2 组。
- `3c5fcab898`：扩展 player info 与 millisecond 排序，涉及 `datasrc/network.py`、协议头、HUD、scoreboard、gameclient、server player，归入协议兼容性 P2 组。
- `d8234eb894`：editor 改用 `log_*` 的重构与 editor map/object 迁移耦合，归入 editor logging / object P2 组。
- `f4473c3ed3`：上游提交会重新引入旧 `DoQuadKnife` 裸字段实现；QmClient 已使用 `QuadKnife()` 组件封装，归入 editor quad knife P2 复核组。

本轮冲突处理要点：

- `editor_props.cpp`：保留 QmClient 中文 tooltip / 按钮文本，合入上游 value selector 只有在值实际变化或正在编辑时才返回 property 的逻辑；angle scroll 的加减按钮改为先汇总 `NewValue` 再统一判断是否变化。
- editor popup / tracker / action 冲突：均优先保留 QmClient `Map()`、`QuadKnife()` 和本地 action 扩展；已覆盖项不制造空提交。
- scoreboard / browser 冲突：保留 QmClient 新布局、朋友分类、备注、自动跟随、SMTC 和 streamer identity 处理；已覆盖项 skip。

本轮验证：

- `cmd /c qmclient_scripts\cmake-windows.cmd --build cmake-build-release --target game-client -j 14`
  - 结果：通过；仅有 `src/engine/client/client.cpp` 既有 `[[nodiscard]]` warning。
- `python qmclient_scripts/gate/check_gate.py --mode quick --base-ref d87500ace94a3d6ab43b2bbbbb49828952eaa9fb`
  - 结果：失败 2 项，7 项通过。失败项仍为既有格式债：`src/game/client/components/countryflags.cpp` 的 clang-format；`datasrc/network.py`、`scripts/generate_unicode_confusables_data.py` 的 ruff format。

本轮量化：

- 当前基线 `d87500ace94a3d6ab43b2bbbbb49828952eaa9fb` 之后本地提交数：`426`
- 当前按 cherry-pick trailer 统计的唯一官方提交数：`303`
- 当前本地 `ddnet/master` 相对基线普通提交数：`787`
- 按动态口径估算，剩余官方普通提交上界：`787 - 303 = 484`
- 本轮处理候选约 `20+` 个：实质合入 `1` 个，covered / empty `12+` 个，明确 deferred `7` 个。

## 2026-07-01 客户端 / editor / snapshot 小批次进展

本轮继续按 20-50 个候选窗口推进，从 `5d05431b81` 后续候选中选择可独立合入的客户端、editor、基础时间和 snapshot 语义修复；P3 文档 / i18n / CI / 版本号项不进入普通同步线。

已合入官方提交：

- `366a3591460ae2024b4b0b9add65b3e27c7ed3d2` → `c3283c6a48 demo: add tooltips to demo browser buttons`
- `e1dfa30f8b1b7fea47d09cb14ba2c95b7b4f159d` → `d362c18f0e editor: preserve quad art group for undo/redo`
- `49c07c63ec42415e5c1fdc10328eed78f2156862` → `21a8598bc0 Remove unnecessary calls of IGraphics::BlendNormal function`
- `0f2aef81bb375a06461288d03c781e6dcbbae5be` → `53b022d221 implement precise milliseconds_from_float function and refactor millisecond compute code`
- `e7aecb02cd77d93d7f2a08dd26bc0b56aad3dad8` → `f7af9f74e9 fix hook collision line of unpredicted players having wrong tunings`
- `00b3be431bb8f62e67aff410d36ecb39e1b91cda` → `dd3c739b66 Make sure to not use the internal item type of snaps`

本轮本地 follow-up：

- `2b55188175 style(sync): 收口上游同步触发的 lineinput 格式`：`49c07c63ec` 触发 `src/game/client/lineinput.cpp` include 顺序格式检查，按项目 clang-format 结果收口，不处理既有 `countryflags.cpp` 格式债。

本轮 covered / empty 并跳过：

- `aafe1c7ac1560f0860e10249067185e52a2a39be`：bit flag enum 的 `1U<<` 修复已由 `datasrc/compile.py` 当前代码覆盖，冲突解后为空。
- `825a2fc374...`：unbuffered backend debug tile clip 修复已覆盖，cherry-pick 为空。
- `8b6a42ef45...`：unbuffered backend quad clipping 修复已覆盖，cherry-pick 为空。
- `a8a5a42264...`：`CMenus::CompareFilenameAscending` strict-weak ordering 已覆盖，cherry-pick 为空。
- `ec20049369...`：`CDemoItem::operator<` strict-weak ordering 已覆盖，cherry-pick 为空。
- `cb252fde7c...`：OpenGL 1 default blend mode 修复已覆盖，cherry-pick 为空。
- `6ea7f4ae1a354a1da5cba1402fc3f56c7f3e2d60`：`find_next_power_of_two_minus_one` 的 `>> 8` 修复已在 QmClient 当前 `src/base/system.cpp` 中存在；官方修改的 `src/base/secure.cpp` 在本地已迁移 / 删除。
- `e33e31f793...`：GLSL smooth qualifier 修复已覆盖，cherry-pick 为空。
- `de326e250d32a50ec9b8335ad1ed53b4be812617`：warmup timer 使用固定 `"0.0"` 宽度避免抖动的修复已由 QmClient HUD editor 包装后的 `RenderWarmupTimer()` 覆盖，冲突解后为空。

本轮明确 skip / deferred：

- `4dd20cc2f4`、`3421ee2907` 等 i18n / translations：QmClient 有独立翻译体系，普通同步线 skip。
- `5a0a059de7`、`657434a07c`、`b8f059ef1d`、`ae69fe4749`、`f3ac2c06f1`、`ccd3f932d0`、`5204da1cc8`、`fc793d9036` 等 Doxyfile / README / 文档重组：P3 文档项，本轮 skip。
- `3e1288c57b`、`fa18b36bad`：官方版本号 / nightly bump，QmClient 使用自有版本流程，skip。
- `3400febf94`、`2063fa5152`、`281620af8c` 及 integration-test / CI 运行环境提交：P3 CI / 测试基础设施项，本轮 skip。
- `9a2211788f`、`246ae771a1`、`2e83185f1e` 等 server gameplay / practice 语义项：进入 P2 兼容性决策队列，未在普通客户端批次处理。

本轮冲突处理要点：

- `menus_demo.cpp`：保留 QmClient demo 浏览器的多选删除、截图浏览和 pending render source 状态，只把官方 play / rename / delete / render tooltip 接入本地按钮流程。
- `editor_actions.cpp/h`、`quadart.cpp`：采用上游保存已创建 `CLayerGroup` 指针的 quad art undo/redo 语义，保留 QmClient 中文 action 文本和本地 include 组织。
- `render_layer.cpp`：删除多余 `BlendNormal()`，但保留 QmClient tele / switch 实体层 alpha 分层、checkpoint 可选 visual 和 overlay 配置。
- `time.cpp/h`、`str_test.cpp`、`time_test.cpp`、`ddnet.cpp`：引入 `time_milliseconds_from_seconds()`，但不迁移到官方新版 `ETimeFormat`，保持 QmClient 当前 `TIME_*` API；新增/适配大秒数精度测试。
- `players.cpp`：未预测玩家 hook collision line 的 fallback tuning 改为本地 predicted core，保留 QmClient 注释掉的 TClient cursor-distance 分支不启用。
- `client.cpp`、`snapshot.cpp/h`、`sixup_translate_snapshot.cpp`、`demo_extract_chat.cpp`：接入 `CSnapshotItem::InternalType()` 命名和 external `ItemType` 使用约束；保留 QmClient Rust `CSnapshotBuilder` / `CSnapshotBuffer`，不回退到官方旧 C++ delta / builder 实现。

本轮验证：

- `cmd /c qmclient_scripts\cmake-windows.cmd --build cmake-build-release --target game-client -j 14`
  - 结果：通过；仅有 `client.cpp` 既有 `[[nodiscard]]` warning。
- `python qmclient_scripts/gate/check_gate.py --mode quick --base-ref d87500ace94a3d6ab43b2bbbbb49828952eaa9fb`
  - 首次结果：失败 2 项，其中新增触发 `src/game/client/lineinput.cpp` clang-format；已用 `2b55188175` 收口。
  - 第二次结果：失败 2 项，7 项通过。失败项已回到既有格式债：`src/game/client/components/countryflags.cpp` clang-format；`datasrc/network.py`、`scripts/generate_unicode_confusables_data.py` ruff format。

本轮量化：

- 当前基线 `d87500ace94a3d6ab43b2bbbbb49828952eaa9fb` 之后本地提交数：`424`
- 当前按 cherry-pick trailer 统计的唯一官方提交数：`302`
- 当前本地 `ddnet/master` 相对基线普通提交数：`787`
- 按动态口径估算，剩余官方普通提交上界：`787 - 302 = 485`
- 本轮处理候选约 `30+` 个：实质合入官方提交 `6` 个，covered / empty `9` 个，P3 / P2 skip-deferred 约 `15+` 个。

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
