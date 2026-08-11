---
title: 背景粒子、镜头 UI 与歌词链路收口计划
date: 2026-07-11
status: active
scope: QmClient 背景粒子投影/模型层、镜头与视野配置、HUD 避让、歌词时钟/HTTP/缓存；不改协议、物理、预测或 DDNet 图形接口
---

# 目标与行为边界

- 背景粒子保留现有生成、寿命、漂移、推动、旋转、粒子碰撞、拖尾、脉冲、闪烁和淡入淡出，只替换模型数据与投影/绘制层。
- 投影顺序固定为：局部顶点 -> 模型旋转 -> `m_Depth` -> 背景相机投影 -> 当前屏幕坐标。相机 zoom 只改变可见世界范围，不改变模型像素尺寸。
- 基础几何统一使用轻量 `Mesh`（vertices/edges/faces），不引入 3D 引擎、不修改 DDNet 图形核心；心形、星形、月牙等装饰模型保留独立生成器。
- 镜头与视野能力只作用于本地视觉；不改变输入、协议、物理、预测、碰撞或回放语义。新增 UI 大小配置必须可关闭/恢复默认值。
- 灵动岛不得覆盖 CheckPoint 时间信息；原始 HUD 风格和编辑器布局保持兼容。
- 汉化排查只修复能够由代码证据确认的本地化指针/加载路径问题；无法复现的问题记录为 gap。
- 歌词 seek 后必须立即以新的 SMTC 时间轴为锚；缓存命中先于网络；歌词缓存主文件名采用安全化的 `SongName+Singer.lrc`；网络失败必须保留明确 fallback。
- 歌词 HTTP 代理只对歌词请求生效，不改变 DDNet 其他 HTTP 请求；代理为空时保持当前 libcurl 行为。
- 搜索加速通过并发/减少串行等待实现，旧请求必须可取消且不得覆盖新歌曲状态。

# 实施切片

1. 为背景粒子建立可单测的 Mesh 生成与统一投影 helper，重写 Cube、Box、Pyramid、Octahedron、Sphere、Torus，并将现有形状映射到基础/装饰模型。
2. 对照 BestClient 的 Camera Drift / Dynamic FOV 配置与实现边界，收紧 QmClient 当前镜头状态恢复、平滑和设置页接线；加入 UI 大小配置。
3. 调整灵动岛 CheckPoint 布局优先级；验证本地化文本不跨语言切换缓存失效指针。
4. 收口歌词 seek 时钟；为 `CHttpRequest` 增加最小的逐请求代理 seam，并只在歌词数据源设置；优化搜索调度。
5. 将歌词缓存主文件命名改为安全化的 `SongName+Singer.lrc`，保留索引/元数据、安全路径校验和旧缓存 fallback。
6. 更新 QmClient 版本与必要翻译，完成聚焦测试、全量 C++ 测试、构建、quick/default gate、视觉检查及独立只读审查。

# 歌词代理故障修复设计

根因证据：

- `CLyricsSourceLrclib` 只在 `OnInit` 复制 `qm_lyrics_http_proxy` 与 `qm_lyrics_http_timeout_ms`，运行时控制台修改不会进入后续请求。
- 当前媒体 identity 把后补的 duration 当作换歌，导致同一首歌从未知时长变为已知时长时取消在途请求；curl 因主动取消返回 42。
- 2026-07-11 本机验证中，经 `http://127.0.0.1:7890` 请求 LRCLIB 搜索首字节约需 13 秒，超过当前 8 秒超时。
- `CHttpRequest` 把显式 `Abort()` 产生的 `CURLE_ABORTED_BY_CALLBACK` 作为错误打印，混淆预期 generation 取消和真实网络失败。

采用逐请求同步方案：LRCLIB 每次 dispatch 前读取当前代理与超时；代理非空时有效超时至少 15000 ms，用户配置的更高值保持优先。搜索进行中修改这两个配置时重启当前 generation；READY 状态不丢弃已加载歌词。媒体 identity 只比较歌曲元数据，不比较 duration。HTTP 层继续把非主动的 callback abort 视为错误，但不为显式 `Abort()` 打印错误。

## 聚焦执行清单

### Task 1：用失败测试锁定合同

**Files:**

- Test: `src/test/qm_lyrics_source_lrclib_test.cpp`
- Create: `src/game/client/components/qmclient/qm_lyrics/qm_lyrics_media_identity.h`

- [x] 添加测试：运行时请求选项替换旧代理，代理开启时 `8000 -> 15000`、`20000 -> 20000`。
- [x] 添加测试：同一媒体只补全 duration 时 identity 不变，title/artist 变化时 identity 改变。
- [x] 添加测试：`EHttpState::ABORTED + AbortRequested=true` 不记录 failure，非主动 callback abort 仍记录。
- [x] 运行 `cmake-build-release/testrunner.exe --gtest_filter=QmLyricsSourceLrclibProxy.*:QmLyricsMediaIdentity.*:HttpRequestLogging.*`，确认因缺少新合同而失败。

### Task 2：最小实现

**Files:**

- Modify: `src/game/client/components/qmclient/qm_lyrics/qm_lyrics_source_lrclib.{h,cpp}`
- Modify: `src/game/client/components/qmclient/qm_lyrics/qm_lyrics.cpp`
- Modify: `src/engine/shared/http.{h,cpp}`

- [x] 为 LRCLIB 增加逐请求 options 更新入口，并在创建 `CHttpRequest` 时应用有效超时与代理副本。
- [x] 将媒体 identity helper 移到聚焦 header，明确排除 duration。
- [x] 在 FETCHING 状态检测 HTTP options 变化并重新 dispatch；下一次 dispatch 使用实时配置。
- [x] 让 HTTP failure 判定排除显式主动取消，不改变其他 curl 错误和初始化失败的日志。
- [x] 重跑聚焦测试并确认 GREEN。

### Task 3：范围验证

- [x] 构建并运行全量 C++ 测试：`qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14`。
- [x] 构建客户端：`qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14`。
- [x] 运行 `python qmclient_scripts/gate/check_docs.py` 与 `python qmclient_scripts/gate/check_gate.py --mode quick`；环境允许时补 `--mode default`。
- [x] 派发新的只读子代理，按 `docs/ai-workflow/review.md` 审查 HTTP 隔离、generation 生命周期和测试覆盖。

# 验证记录

本次歌词代理故障修复证据：

```text
Command: qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
Result: RED；新增媒体 identity header 缺失，编译按预期失败。
Scope: 证明回归测试在实现前能捕获缺失合同。
Gaps: 仅 TDD RED，不证明实现正确。

Command: cmake-build-release/testrunner.exe --gtest_filter=QmLyricsSourceLrclib*:*QmLyricsMediaIdentity*:*HttpRequestLogging*
Result: PASS；21 tests passed。
Scope: 覆盖代理副本、运行时 options、实际请求 timeout、generation 取消顺序、duration-only identity 和主动取消日志 cause。
Gaps: 状态机顺序与日志接线测试包含源码结构约束，不是完整真机网络/logger 集成测试。

Command: qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
Result: PASS；DDNet.exe 链接成功。
Scope: 覆盖真实客户端中的歌词状态机、HTTP 与 LRCLIB 集成编译。
Gaps: 未启动客户端做 SMTC 真机交互。

Command: qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
Result: PASS；1747 tests passed。
Scope: 覆盖当前 build 目录全量 C++ 测试。
Gaps: 未运行 Rust 全量测试。

Command: python qmclient_scripts/gate/check_docs.py
Result: PASS；活动索引、状态、引用及 AGENTS/CLAUDE 镜像一致。
Scope: 覆盖治理文档一致性。
Gaps: 无。

Command: python qmclient_scripts/gate/check_gate.py --mode quick
Result: FAIL；代码格式干跑被工作区其他在途文件的既有 clang-format 违规阻断。
Scope: 其余 9 项 quick 检查通过；本轮独立检查的 http/header/test 文件格式通过。
Gaps: 未取得未跳过 style 的仓库级 quick gate 绿色结果。

Command: python qmclient_scripts/gate/check_gate.py --mode quick --skip-style-check
Result: PASS；9 checks passed。
Scope: 在已单独披露 style 阻断后覆盖其余 quick gate。
Gaps: 跳过仓库级 style 检查，未运行 default gate。

Review: 新的只读子代理初审提出 callback 前取消与 HTTP abort cause 两项 finding；修复后复审无 findings，总体结论“正确”。
```

# 歌词播放时间轴全链路修复设计

## 根因与采用方案

当前暂存实现用 `Anchor(..., ForceSnap)` 和 `SetPlaying(..., PreserveAnchorTick)` 两次调用表达一次播放器快照。这个接口无法区分“playback 状态已经变化”与“timeline 样本也已刷新”：恢复播放时若 SMTC 仍返回暂停前的 `Position/LastUpdatedTime`，生产调用会保留旧 sample tick，从而把整个暂停区间计入歌词进度。现有 clock 测试直接选择 `PreserveAnchorTick=false`，没有覆盖生产调用组合。

采用 BetterLyrics 的 clean-room 同步模型：播放器快照是权威校准源，歌词时钟在帧间使用单调时间推进；歌词请求只发布 `SLyricsTrack`，不创建、不重置也不恢复播放时钟。大跳变、切歌和 fresh seek 立即 hard snap；普通小漂移按真实经过时间收敛，不能按帧数或 `Now()` 调用次数收敛。

## 数据合同

`CSystemMediaControls::SState` 保留完整采样语义：

```cpp
int64_t m_PositionMs;
int64_t m_PositionUpdatedTick;
uint64_t m_TimelineGeneration;
bool m_Playing;
double m_PlaybackRate;
```

Windows worker 使用 `time_get_impl()` 取得 timeline 读取时刻；`m_TimelineGeneration` 仅在 `StartTime/EndTime/Position/LastUpdatedTime` 原始 timeline 变化时递增，不能因 `PlaybackInfo` 单独变化而递增。`PositionUpdatedTick` 仍由 `observed_steady - (observed_utc - LastUpdatedUtc)` 映射，非法或未来时间退回读取时刻。

歌词时钟只暴露原子快照入口，不再由调用方拼装布尔参数：

```cpp
struct SPlaybackSnapshot
{
	int64_t m_PositionMs;
	int64_t m_PositionUpdatedTick;
	uint64_t m_TimelineGeneration;
	double m_PlaybackRate;
	bool m_Playing;
	bool m_IdentityChanged;
};

void Update(const SPlaybackSnapshot &Snapshot, int64_t ObservedTick, int64_t TickFreq);
int64_t Now(int64_t NowTick, int64_t TickFreq) const;
```

`Update` 规则固定如下：

- 首个样本或曲目 identity 变化：用权威样本在 `ObservedTick` 的估算位置 hard snap。
- timeline generation 未变化但播放变为暂停：冻结当前本地预测位置。
- timeline generation 未变化但播放恢复：从冻结位置和 `ObservedTick` 重新起跑，不复用暂停前 sample tick。
- timeline generation 变化：比较新样本在当前时刻的位置与本地预测；seek/大漂移 hard snap，小漂移建立按毫秒衰减的 correction。
- 暂停样本不按 `LastUpdatedTime` 外推；播放样本按 playback rate 外推。
- offset 只在最终 `Now()` 结果上应用，同一个校准时间驱动行选择、逐字进度和滚动。

## 聚焦执行清单

### Task 4：用真实生产序列建立 RED

**Files:**

- Modify: `src/test/qm_lyrics_clock_test.cpp`

- [x] 增加 mid-song attach 后歌词延迟 5 秒才 READY 的序列，期望从 `65s` 对应行开始。
- [x] 增加 stale timeline 的 pause/resume 序列：`60s@10s -> pause@11s -> resume@20s`，恢复瞬间仍为 `61s`。
- [x] 增加 fresh pause、fresh resume、前后 seek、相同 position 但新 sample tick、rate change 和 identity change 序列。
- [x] 增加同 tick 多消费者与不同帧率测试，证明 correction 不依赖 `Now()` 调用次数。
- [x] 构建并运行 `cmake-build-release/testrunner.exe --gtest_filter=QmLyricsClock.*`，确认新增测试因旧的分裂 API/调用语义失败。

### Task 5：统一快照协调器

**Files:**

- Modify: `src/game/client/components/qmclient/qm_lyrics/qm_lyrics_clock.h`
- Modify: `src/game/client/components/qmclient/qm_lyrics/qm_lyrics_clock.cpp`
- Modify: `src/test/qm_lyrics_clock_test.cpp`

- [x] 引入 `SPlaybackSnapshot` 与单一 `Update()`，删除 `ShouldUpdateTimelineAnchor`、公开 `Anchor()` 和 `SetPlaying()`。
- [x] 实现 stale pause/resume、fresh timeline、identity、rate、hard snap 和按时间衰减 correction。
- [x] 保持 offset 方向与 `EffectivePlaybackOffsetMs()` 合同不变。
- [x] 重跑 `QmLyricsClock.*`，确认全部 GREEN，且每个新增行为测试都曾在实现前按预期失败。

### Task 6：修正 SMTC 采样新鲜度

**Files:**

- Modify: `src/game/client/components/system_media_controls.h`
- Modify: `src/game/client/components/system_media_controls.cpp`
- Modify: `src/test/qm_lyrics_clock_test.cpp`

- [x] worker 用 `time_get_impl()` 同时采样 steady clock，禁止后台线程调用缓存式 `time_get()`。
- [x] 基于原始 timeline 四元组维护 `m_TimelineGeneration`，并复制到主线程快照。
- [x] 保留 `Position - StartTime` 与 `EndTime - StartTime` 的归一化，测试 `Start=0`、非零 start、非法范围和相同 position/新 LastUpdated。
- [x] 保留 playback 与 timeline 的独立 generation 语义，不把 playback-only 变化标记为 fresh timeline。

### Task 7：将同步从渲染生命周期解耦

**Files:**

- Modify: `src/game/client/components/qmclient/qm_lyrics/qm_lyrics.h`
- Modify: `src/game/client/components/qmclient/qm_lyrics/qm_lyrics.cpp`

- [x] 在 `CQmLyrics::OnUpdate()` 调用 `TickStateMachine()`，`OnRender()` 只渲染。
- [x] 每帧从完整 `SState` 构造一次 `SPlaybackSnapshot` 并调用 `Clock.Update()`；歌词请求 callback 不访问 clock。
- [x] 候选发布后先应用新 track offset，再用同一播放器时刻定位活动行。
- [x] identity/generation 变化取消旧歌词请求；duration-only enrichment 继续不视为换歌。
- [x] 保证 HUD 在渲染前已经消费本帧 seek/pause/切歌，媒体岛和独立 HUD 使用同一 active line。

### Task 8：验证与真机证据

- [x] 聚焦运行 `QmLyricsClock.*`、`QmLyricsRender.*`、`QmLyricsMediaIdentity.*`。
- [x] 串行运行全量 C++ 测试、`game-client`、`check_docs.py` 和 `check_gate.py --mode quick`；环境允许时补 `--mode default`。
- [x] 派发新的只读子代理，按 `docs/ai-workflow/review.md` 审查 snapshot freshness、线程时钟、generation、offset 和 HUD 更新顺序。
- [ ] Windows 真机至少两个播放器验证 mid-song attach、延迟加载、pause/resume、前后 seek、切歌和播放器焦点切换；没有此证据时保留端到端 gap。

## 本轮时间轴修复验证记录

```text
TDD RED:
- 新快照 API 缺失时编译失败。
- stale resume 将暂停区间错误计入，期望 61000ms、实际 70000ms。
- correction 随 Now() 调用次数变化，同一时刻得到 640ms 与 733ms。
- fresh pause、暂停 seek、进程启动前 sample tick、新播放速率和禁用歌词生命周期测试均曾按预期失败。

Command: cmake-build-release/testrunner.exe --gtest_filter=QmLyrics*:*SystemMediaTimeline* --gtest_brief=1
Result: PASS；160 tests from 24 test suites passed。
Scope: 覆盖 mid-song attach、歌词延迟、stale/fresh pause-resume、前后 seek、timeline generation、rate、identity、offset 和生命周期。
Gaps: 纯数据与结构测试不替代 Windows 真播放器联调。

Command: qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
Result: PASS；SMTC、歌词时钟与歌词组件重编译，DDNet.exe 链接成功。
Scope: 覆盖 Windows WinRT adapter 与客户端真实集成编译。
Gaps: 未启动客户端执行播放器交互。

Command: qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
Result: PASS；1752 tests from 171 test suites passed。
Scope: 覆盖当前 release build 目录全量 C++ 测试。
Gaps: 无。

Command: python qmclient_scripts/gate/check_docs.py --sync-only --prefer agents && python qmclient_scripts/gate/check_docs.py
Result: PASS；15 个 workflow 文件、7 个活动文档通过。
Scope: 覆盖治理文档镜像、索引、状态和引用。
Gaps: 无。

Command: python qmclient_scripts/gate/check_gate.py --mode quick
Result: FAIL；9 项通过，仓库级 clang-format 干跑被其他在途改动阻断。
Scope: 本轮负责的完整文件和同文件新增行已单独通过 clang-format --dry-run --Werror，git diff --check 通过。
Gaps: 未取得不跳过 style 的仓库级绿色结果，也未修改用户并行改动。

Command: python qmclient_scripts/gate/check_gate.py --mode quick --skip-style-check
Result: PASS；9 checks passed。
Scope: 覆盖除仓库级 style 外的 quick gate。
Gaps: 跳过全局 style。

Command: python qmclient_scripts/gate/check_gate.py --mode default
Result: FAIL；C++ 1752 tests、Rust 24 doc-tests 均通过，唯一失败仍是仓库级 clang-format。
Scope: 覆盖 quick 层、C++ 全量测试和 Rust 全量测试。
Gaps: 未取得不跳过 style 的仓库级绿色结果。

Command: python qmclient_scripts/gate/check_gate.py --mode default --skip-style-check
Result: PASS；11 checks passed，C++ 1752 tests、Rust 24 doc-tests 通过。
Scope: 覆盖除仓库级 style 外的 default gate。
Gaps: 跳过全局 style。

Review: 独立只读复审最初发现负 sample tick、stale resume 新速率和禁用歌词时钟维护三项重要问题；补回归并修复后复审无 findings，总体结论“正确”。

End-to-end gap: 尚未用两个 Windows 播放器真机验证 mid-song attach、延迟加载、pause/resume、前后 seek、切歌和播放器焦点切换。
```
