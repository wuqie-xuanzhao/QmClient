---
title: QmClient 游戏内歌词 HUD 叠加层（CQmLyrics）规格
date: 2026-06-21
status: active
source: BetterLyrics（https://github.com/jayfunc/BetterLyrics, GPL-3.0）架构调研 + QmClient HUD/编辑器/i18n 现状映射
scope: 仅 Windows，仅游戏内 HUD 叠加层（不创建独立窗口），允许只读 SMTC 接入；不动协议、物理、预测、demo 格式
license: 实现为 clean-room 复刻，仅复用 BetterLyrics 的行为/算法描述，不复制其源码或资源
strategy: 替换重写 + 保留 QQ/网易能力。删 CLyrics 主体/SMTC 接入/双模式时间轴/LRC 解析/渲染/配置/设置页；保留 QQ HTTP+QRC 解密、网易 HTTP+EAPI 签名、QRC/YRC 解析、MergeLineTextByTimestamp 等纯函数
---

## 背景

参考实现是开源项目 BetterLyrics（仓库 `jayfunc/BetterLyrics`，GPL-3.0），用 WinUI 3 + Win2D + .NET 提供桌面歌词与全屏歌词体验。本规格在 QmClient 中规划一个**纯游戏内 HUD 叠加层**形态的歌词组件 `CQmLyrics`，让玩家可以一边玩 DDNet 一边看到当前播放歌曲的逐字滚动歌词。

GPL-3.0 是 strong copyleft——直接合并 BetterLyrics 源码会污染整个 QmClient 仓库。因此本组件**必须以 clean-room 方式实现**：只参考其架构决策、数据流和动画思路，不复制任何 C# 源码、不引用其编译产物、不复制 XAML/Shader 资源；所有 C++ 代码自写，注释中可以记叙「与 BetterLyrics 思路一致」但不得粘贴函数体。

本规格写明：BetterLyrics 的关键架构调研结论（让实现者无需再翻原仓库），QmClient 侧的接入点（组件、配置、i18n、HUD 编辑器、渲染管线），以及验收 / 风险 / 任务拆分。

---

## 实现进度（每次会话开头读这里、结尾更新这里）

> 这是单一权威进度状态。会话中断后，下次重新读 spec 就能续上。
> 已完成项前置 ✅，正在做置 🚧，未开始置 ⏳。

**整体策略**：替换重写 - 完全推倒重来，不保留 QQ/网易能力（2026-06-22 决策变更）。

**策略变更记录**：
- 原计划保留 QQ/网易 HTTP + QRC/YRC 解密 + EAPI 签名等纯函数代码（T0c 抽取后续复用）。
- 实际进展中 T0c 重构（把 CLyrics state 依赖剥离为纯函数 + 重新设计接口）评估为 1-2 小时工程，且会让 commit 之间耦合复杂。
- 用户最终决策：放弃 QQ/网易能力，整个 lyrics_component.{h,cpp} + lyrics/lyric_*.{h,cpp} + 旧测试一刮子全删。新组件 T1+ 阶段从零写 LRC/eslrc/TTML 解析器，数据源首版只 LRCLIB；QQ/网易日后愿意时另起项目重做。

**保留代码清单**（不许动）：

- ~~已废弃：原本保留 QQ/网易 + QRC/YRC 等纯函数（见上方策略变更记录）~~。当前**无保留代码**——整个 lyrics_component.{h,cpp}、lyrics/lyric_parser.{h,cpp}、lyrics/lyric_model.h、src/test/qm_lyrics_parser_test.cpp 全删。

**删除代码清单**（按 H 节"删除清单" + 策略变更后的扩展）：

- `lyrics_component.h/cpp` 整个文件（含 CLyrics class、SMTC 接入、QQ/网易 HTTP、双模式时间轴）
- `lyrics/lyric_parser.h/cpp` 整个文件（含 QRC/YRC 解析、AES/TripleDES 解密、EAPI 签名）
- `lyrics/lyric_model.h` 整个文件
- `src/test/qm_lyrics_parser_test.cpp` 整个文件
- 20 个 `qm_lyrics_*` / `qm_smtc_lyrics_*` 配置
- `hud.cpp` 中 5 处 `m_Lyrics` 调用 + 整个 `RenderLyricsHud`（已完成 T0b2/T0b3）
- 设置页歌词卡片（已完成 T0b1）
- `gameclient.h:245` 成员 + `gameclient.cpp:295` Add

**进度看板**：

| 阶段 | 内容 | 状态 |
|------|------|------|
| Spec 改终版 | 加进度看板 + 保留/删除清单 + #10 合进 T0 + 调整 T1 解析器为「只写 LRC/eslrc/TTML 不动 QRC/YRC」 | ✅ |
| T0a | #10 共用贴边 Helpers：`QmHudEditor::SEdgeMargin` / `ApplyEdgeMargin` / `EHorizontalFlow` / `ResolveHorizontalFlow` 加到 `hud_editor.h`；通知栏 `InsetAnchoredRect` 迁移；通知栏测试全绿 | ✅ commit `e54bb209a` |
| T0b1 | 删 menus_qmclient 设置页歌词卡 + 模块表项 + 7 处枚举 case + HUD tab 新功能红点；`QmModuleCount` 36→35 | ✅ commit `16f7e226b` |
| T0b2 拆除主入口 | 删 `hud.h` RenderLyricHud 声明 + `hud.cpp` RenderLyricHud 函数体（130 行）+ 调用点；不动 RenderMediaIsland 内嵌歌词块（T0b3） | ✅ commit `09dcee910` |
| T0b3 拆除媒体岛嵌入 | 删 `hud.cpp` RenderMediaIsland 内 9 处 DockedLyric*/m_LyricHudDocked* 探测+布局+绘制；删 RenderLegacyMediaInfoAt 内嵌歌词块；删 hud.h 3 个 m_LyricHud* 成员；OnRender 主循环复位 | ✅ commit `265e0a277` |
| T0b4 一刮子全删 | 删 `lyrics_component.{h,cpp}` + `lyrics/lyric_parser.{h,cpp}` + `lyrics/lyric_model.h` + `src/test/qm_lyrics_parser_test.cpp` + 20 个配置 + `gameclient.h:245` 成员 + `gameclient.cpp:295` Add + CMakeLists.txt 引用 + i18n 提取规则（含原 T0c+T0d+T0e 内容，因策略变更全部并掉） | ✅ commit `089610c82` |
| ~~T0c 抽取 QQ/网易~~ | ~~已废弃，策略变更后不再保留~~ | 🚫 |
| ~~T0d 删 CLyrics + LRC 解析~~ | ~~并入 T0b4~~ | 🚫 |
| ~~T0e 测试拆分~~ | ~~不需要拆分，QQ/YRC 测试全删~~ | 🚫 |
| T0c 抽取 | `lyrics_component.cpp` 中 QQ/网易 4 段代码抽到新模块 `lyrics/qq_music_api.{h,cpp}` 与 `lyrics/netease_music_api.{h,cpp}`；纯函数接口；不依赖 CLyrics state | ⏳ |
| T0d 删 CLyrics | 删 `lyrics_component.h/cpp` 主类；`lyric_parser.h/cpp` 删 `ParseLrcLyrics` / `BuildVisibleLineText` 实现 | ⏳ |
| T0e 测试拆分 | 删旧测试文件中 LRC + BuildVisibleLineText 那两条；保留的 QRC/YRC/Merge 拆到新文件 | ⏳ |
| T0f gate | 跑 `python qmclient_scripts/gate/check_gate.py --mode quick`，build/test 全绿后 commit T0 | ⏳ |
| T1 | 新写 `qm_lyrics_model.h`（`SLyricsTrack/SLyricsLine/SLyricsWord` + EFormat），`qm_lyrics_parser_lrc.{h,cpp}`（标准 + Enhanced + ESLRC），`qm_lyrics_parser_ttml.{h,cpp}`（TTML），`qm_lyrics_parser_lrc_test.cpp`（10 用例），`qm_lyrics_parser_ttml_test.cpp`（11 用例）。CMake 加 5 个源文件 + 2 个测试 + 2 个 TESTS_EXTRA。**目录改为 snake_case `qm_lyrics/`** 与项目惯例（hud_notifications/、monitoring/）一致。 | ✅ |
| T2 | 匹配器 + 缓存 | ✅ |
| T3 | LRCLIB 数据源（用 `std::make_shared<CHttpRequest>` + `Http()->Run`）+ IQmLyricsSource 抽象 + 12 个测试 | ✅ |
| T4 | SMTC 适配层 — **复用现有 `CSystemMediaControls`**：该组件已暴露 `SState`（含 m_aSourceAppId/Title/Artist/Album/PositionMs/DurationMs/Playing），不需要再从零写 WinRT。T6 组件骨架里直接 `GameClient()->m_SystemMediaControls.GetStateSnapshot(...)` 转 `SSourceQuery`。 | ✅（复用） |
| T5 | 时钟插值器 + 7 个单元测试 | ✅ |
| T6 | CQmLyrics 组件骨架 + 30 个 `qm_lyrics_*` 配置 + 注册到 gameclient | ✅ |
| T7 | 设置页 UI：恢复 EQmModuleId::Lyrics + QmModuleCount 35→36 + 模块表/搜索关键词/标题表项；新增设置页歌词卡（启用/自动拉取/逐字/暂停隐藏/翻译显示 5 开关 + 17 滑块 + 3 颜色选择器）；4 个预留配置（source/cache_enable/show_transliteration/hide_no_lyrics）在 OnInit touch 通过配置使用检查。i18n 流水线已重新跑通：`extract_strings.py` / `generate_all.py` / `validate.py` / `review_duplicate_entries.py` 全通过。 | ✅ |
| T8 | 静态布局 + HUD 编辑器接入 — RenderHud 已接入 `BeginTransform` + `EHudEditorElement::Lyrics` + 透视淡出；模块表项映射随 T7 恢复。 | ✅ |
| T9 | 行级动画 — 行切换 easeOutCubic 滚动、滑入/滑出；本轮补强为按实际活动行/非活动行/翻译块高度计算滚动距离，并避免中途打开/大跳转触发错误长距离动画。 | ✅ |
| T10 | 逐字软边 + 长音脉冲 — RenderHud 按 `m_vWords` 做逐字颜色推进，保留 `qm_lyrics_karaoke` 与长音脉冲配置；本轮补齐渲染辅助测试覆盖。 | ✅ |
| T11 | 端到端联调 — 代码侧已覆盖 SMTC 时间轴锚点、开关恢复、offset、滚动和翻译生成；真机 Windows + Spotify/Apple Music for Windows/Netease 视觉联调仍需人工播放器环境确认。 | ⚠️ 代码侧完成，真机联调待验 |
| T12 | 代码审查 + gate — 已派只读子代理按 docs/ai-workflow/review.md 审；复审发现的 toggle 状态保留与行间空档显示问题已修复并补测试；已跑 `game-client`、目标歌词测试、i18n 流水线、`check_gate.py --mode quick`、`check_gate.py --mode default`。 | ✅ |

**当前会话目标**：收口歌词重写后的 BetterLyrics 行为差距：SMTC 时间轴锚点、mid-song 显示、滚动恢复、开关后重新搜索、offset 语义和翻译/i18n 生效。

**下次会话续接指引**：
1. 读本节进度看板；
2. 找最早 ⏳ 的阶段开始；
3. 完成后改成 ✅ 并 commit。

**中断点记录**（每次中断时写一条，记到这里）：
- 2026-06-22 收在 T0b1 commit `16f7e226b`。next: T0b2 拆 hud.cpp 主入口（5479-5611 + 5620+ + 5634/5640/5661），行号会因后续 grep 重新定位。**注意环境问题**：本地 default gate 因 vswhere PATH 和 Git Bash find 拦截无法跑，T0a/T0b1 quick gate 全绿但未走真实编译。续接前建议在原生 cmd 手动跑 `qmclient_scripts\cmake-windows.cmd --build cmake-build-release --target game-client -j 12` 验证 T0a+T0b1 真编译通过，再动 T0b2。
- 2026-06-22 第二轮一口气干完：T0b2/T0b3/T0b4/T1/T2/T3/T4(复用)/T5/T6 全部就位（commits e54bb209a..本轮最新）。约 14 个 commit，~4400 行净增量，~2700 行删除。49 测试（21 解析 + 18 匹配 + 9 缓存 + 12 LRCLIB + 8 时钟 = 68 测试，T6 没加测试）。**未完成**：T7（设置页 UI + i18n 流水线）、T9（行切换 easeOutCubic 滚动）、T10（逐字软边 + 长音脉冲）、T11（真机联调，需要 Windows + Spotify/Netease）、T12（review + default gate）。default gate 仍未在本地跑通（vswhere/Git Bash find 环境问题）。续接前先跑构建验证整个 T0a-T6 真编译。
- 2026-06-22 第三轮针对歌词错轴/滚动/开关/i18n 继续收口：按 BetterLyrics 的 `position_at_last_update + steady_now_delta * playback_rate` 思路，把 SMTC `LastUpdatedTime` 和 `PlaybackRate` 暴露给歌词时钟；关闭歌词 HUD 时保留已加载轨道，只有关闭期间取消未完成搜索时才清空 identity 以便重开后重新发起搜索；禁用 SMTC/无媒体时重置播放身份；`[offset:]` 语义改为从播放时间中扣除轨道 offset；行切换滚动距离改为按实际文字块高度计算，大跳转/中途打开不做错误动画；行间空档显示保持上一句而不是跳到最后一句。验证：`game-client` 通过，歌词目标测试 49/49 通过，`run_cxx_tests` 1565/1565 通过，i18n 生成/校验通过，default gate 通过（ruff/shellcheck 缺失仅警告）。真机播放器视觉联调仍需外部环境确认。
- 2026-06-26 歌词模块优化收口：请求取消时递增搜索 generation，防止旧源回调污染新状态；候选选择改为按匹配分排序并逐个尝试解析，最高分坏歌词不再挡住同批次可用候选；同分候选保持源/API 原始顺序；缓存 payload 文件名增加白名单校验，索引损坏/过期时同步清理；匹配归一化会继续扫描后续括号修饰；逐字渲染在首词前和末词后快速返回，减少长行重复测宽。环境已补齐：VS BuildTools 2022、CMake、Ninja、LLVM/clang-format、Git、Rust MSVC 标准库、ruff、shellcheck。验证：`git diff --check -- src/game/client/components/qmclient/qm_lyrics src/test/qm_lyrics_cache_test.cpp src/test/qm_lyrics_match_test.cpp docs/superpowers/specs/qm_lyrics_hud.md` 通过；`cmake --build cmake-build-release --target testrunner -j 14` 通过；`cmake-build-release/testrunner.exe --gtest_filter=QmLyrics*:QmLyrics*.*` 通过（128/128）；`python qmclient_scripts/gate/check_gate.py --mode quick` 通过（10/10，0 warnings）；`python qmclient_scripts/gate/check_gate.py --mode default` 通过（12/12，0 warnings，C++ 1577/1577，Rust doctest 24/24）。
- 2026-06-26 HUD/设置继续收口：歌词新增 `qm_lyrics_in_media_island`，可作为灵动岛独立行显示，并在该模式下隐藏独立歌词 HUD 以避免重复；灵动岛新增 `qm_hud_island_edge_margin`，贴边拖拽和渲染共用安全边距，同时按真实屏幕接触决定圆角裁剪，修复贴边后视觉被错误裁角；HUD 编辑器拖拽时恢复横/纵对齐提示线；新增 `qm_respawn_default_weapon` 和设置页下拉框，重生/首次拿到本地角色时自动请求默认武器。只读审查指出的长歌词撑爆灵动岛宽度、旁观/切 dummy 误触发默认武器已修复；灵动岛歌词读取最多晚于歌词组件状态机一帧，评估为低风险残留，未在本轮扩大组件调用边界。i18n 已同步维护源和运行时语言文件。验证：i18n 提取/生成/校验/重复审查通过；`testrunner.exe --gtest_filter=QmLyrics*:QmHudEditorGeometry.*:QmHudNotificationsGeometry.*:QmMonitoringHelpers.*` 通过（405/405）；`game-client` 构建通过；`git diff --check` 与 `check_docs.py` 通过；`python qmclient_scripts/gate/check_gate.py --mode default` 通过（12/12，0 warnings，C++ 1579/1579，Rust doctest 24/24）。

---

## 目标

1. 在 QmClient 游戏内提供一个可拖拽、可贴边的歌词 HUD 元素，展示当前 Windows 上正在播放歌曲的歌词。
2. 数据源使用 Windows 自带的 `GlobalSystemMediaTransportControlsSessionManager`（SMTC）——不绑定特定播放器，凡是接入 SMTC 的播放器（Spotify、Apple Music for Windows、Netease 云音乐、QQ 音乐、本机播放器、浏览器媒体会话等）都能识别。
3. 歌词数据从远程歌词服务获取（LRCLIB 默认；其余源以「数据源 provider」形式预留扩展位），按 BetterLyrics 的匹配策略选择最相近的歌词文档。
4. 支持 `.lrc`（标准 + 增强）、`.eslrc`、`.ttml` 三种格式；运行时统一为「行 → 词」的双层时间轴模型。
5. 渲染走 QmClient 现有 HUD 管线（`BeginTransform` / HUD 编辑器 / `qm_lyrics_*` 配置），逐字高亮 + 当前行放大 + 远离行虚化/缩小，时长由 `QmUiMotionLevel` 全局动效等级缩放。
6. 配置项前缀 `qm_lyrics_`，所有用户可见文案走 QmClient 现有 i18n 流水线（`extract_strings.py` → `generate_all.py` → `validate.py`）。
7. 全部代码为 clean-room 自写。不要拷贝 BetterLyrics 任何源代码、资源、shader。

## 非目标

1. **不**创建独立窗口、桌面歌词、任务栏歌词、全屏歌词模式。BetterLyrics 的 AppBar / Wallpaper / Always-On-Top 等模式**全部排除**——QmClient 是游戏，不抢用户屏幕焦点。
2. **不**实现 BetterLyrics 的背景特效（Fluid / Blur / Fog / 雪花粒子 / 频谱可视化 / Cover 背景）。HUD 叠加层不需要这些。
3. **不**实现播放控制（上一首/下一首/播放暂停）。SMTC 提供 `Play/Pause/Skip` API 但本规格只**读**，不写。
4. **不**实现 Last.fm / Discord RPC / 媒体库 / WebDAV / FTP / Wallpaper Engine 等周边功能。
5. **不**实现 BetterLyrics 的本地 LLM、本地 NMT、Romaji 插件。翻译/音译留作扩展位（接口预留），首版不带任何在线翻译。
6. **不**移植 BetterLyrics 的 MSIX 打包、Crowdin 翻译流程。
7. **不**支持非 Windows 平台。Linux/macOS/Android 上 `CQmLyrics` 完全编译条件屏蔽（`#if defined(CONF_FAMILY_WINDOWS)`），HUD 编辑器不显示该元素，配置项依然写入文件但不生效。
8. 首版**不**实现 BetterLyrics 的多源「持久化源记忆」（记住某首歌从哪个源拿到歌词）——只做一次命中即可。
9. **不**首版实现插件 DLL 热加载。「插件架构」首版只是 C++ 内部接口分层（`IQmLyricsSource` 等），第三方扩展留到二期。

---

## BetterLyrics 架构调研结论（clean-room 复述）

> 以下结论来自对 `jayfunc/BetterLyrics` 公开仓库结构 + README + 公开目录 listing 的阅读，不含任何源码片段。实现 `CQmLyrics` 时按本节描述的**行为**和**数据流**自行写 C++。

### A. 仓库布局（用于定位职责，不是抄目录）

参考实现分为三层：

- `BetterLyrics.Core/`：领域模型、Domain 模型（`LyricsSearchResult` 等）、插件抽象基类（`PluginBase`、`PluginConfigBase`）、插件契约接口（`IPlugin`、`IConfigurator`、`ILocalizer`、`ILyricsSource`、`ILyricsTranslator`、`ILyricsTransliterator`、`IAIService`）、设置 schema、序列化、Helpers（`LangGenerator`、`SettingBuilder`、`SettingsIO`）。
- `BetterLyrics.WinUI3/`：UI/渲染/服务。Services 子目录给出最权威的服务清单（每个是一个文件夹）：
  - `SMTCService`、`GSMTCService`（**两个 SMTC 服务并存**——典型实现是 `GSMTCService` 是 Global session manager（订阅当前会话切换、读取媒体元数据/时间线），`SMTCService` 是本进程作为媒体控制器/被控者）。本规格只需要 Global 那一边。
  - `LyricsSearchService`、`LyricsCacheService`、`SongSearchMapService`（搜索 / 缓存 / 搜索结果到歌曲映射）。
  - `AlbumArtSearchService`、`PlayHistoryService`、`FileSystemService`、`FileWatchService`、`AppLifecycleService`、`AppUpdateService`、`NavigationService`、`SettingsService`、`LocalizationService`、`PluginService`、`TranslationService`、`TransliterationService`、`DiscordService`、`LastFMService`。后面这些与 HUD 叠加层无关，本规格不涉及。
- `BetterLyrics.WinUI3/Renderer/`：
  - 背景特效集合（`CoverBackgroundRenderer`、`FluidBackgroundRenderer`、`FogRenderer`、`SnowRenderer`、`RaindropRenderer`、`PureColorBackgroundRenderer`、`SpectrumRenderer`、`EdgeFadeMaskRenderer`、`CompositionRenderer`、`EffectRendererBase`）—— **本规格全部不复刻**。
  - 歌词渲染子树 `Renderer/LyricsRenderer/`：`LyricsRenderer.cs`、`LyricsLineRendererBase.cs`、`HorizontalLyricsLineRenderer.cs`、`VerticalLyricsLineRenderer.cs`。这一层就是「歌词文字本身的渲染算法」，是 `CQmLyrics` 真正要复刻**行为**的部分。
- `Plugins/`：`BetterLyrics.Plugins.Source.LRCLIB`、`BetterLyrics.Plugins.Translation.LocalAI`、`BetterLyrics.Plugins.Transliteration.Romaji`、`BetterLyrics.Plugins.AI.Local.LLM`、`BetterLyrics.Plugins.AI.Local.NMT`。LRCLIB 在参考实现里也是插件——说明 Core 不直接耦合 LRCLIB，而是通过 `ILyricsSource` 契约。
- 顶层 `index.json` 是插件清单。
- 顶层 `LICENSE` 是 GPL v3。README 明确 GPL-3.0。文件级 SPDX header 未在公开 listing 上验证，但 GPL 整库覆盖。
- README 列出的依赖：**LRCLIB**（开放歌词 API）、**Lyricify-Lyrics-Helper**（QQ / 网易云 / 酷狗的抓取+解密+解析）、**Manzana-Apple-Music-Lyrics**（Apple Music 逐字歌词，需要 Python）、**Isolation**（流体背景参考实现）、**SpectrumVisualization**（频谱参考）。本规格只关心前三者。

### B. 当前播放歌曲检测

确认：参考实现使用 Windows SMTC，对应类名是 `Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager`。WinRT 投影；C++ 侧用 C++/WinRT 头 `winrt/Windows.Media.Control.h` 即可访问。

关键 API（C++/WinRT 等价）：

- `GlobalSystemMediaTransportControlsSessionManager::RequestAsync()` —— 拿到 Manager。
- `Manager.GetCurrentSession()` —— 当前活跃会话；可能为 `nullptr`（无播放器）。
- `Manager.CurrentSessionChanged(token)` —— 会话切换事件。
- `Session.TryGetMediaPropertiesAsync()` → `GlobalSystemMediaTransportControlsSessionMediaProperties`：`Title`、`Artist`、`AlbumArtist`、`AlbumTitle`、`TrackNumber`、`Thumbnail`。
- `Session.GetTimelineProperties()` → `Position`、`StartTime`、`EndTime`、`MinSeekTime`、`MaxSeekTime`、`LastUpdatedTime`。
- `Session.GetPlaybackInfo()` → `PlaybackStatus`（`Playing` / `Paused` / `Stopped` / `Closed` 等）、`PlaybackRate`、`Controls`（不需要写）。
- 事件：`Session.MediaPropertiesChanged`、`Session.PlaybackInfoChanged`、`Session.TimelinePropertiesChanged`。

时间线插值要点（参考实现这样做，复刻时也照做）：

- 每次 `TimelinePropertiesChanged` 拿到 `Position` 和 `LastUpdatedTime`。
- 帧渲染时不再调用 SMTC，按本地高分辨率时钟 + `PlaybackRate` 推算当前播放秒数：`now = position_at_last_update + (steady_now - last_updated) * playback_rate`，并夹到 `[StartTime, EndTime]`。
- 暂停时不推进；状态从暂停回到播放时刷新基准。
- 每 N 秒强制同步一次以纠正漂移（参考值 2-5 秒）。

### C. 歌词数据源管线

参考实现的关键抽象是 `ILyricsSource`（在 `Core/Interfaces/Features/`）。每个 source 是一个插件 DLL，注册到 `LyricsSearchService`。

工作流（行为复述，不是源码）：

1. 当 SMTC 报告新歌曲（title/artist 变化），`LyricsSearchService` 构造查询：`{ title, artist, album, duration_ms }`。
2. 命中 `LyricsCacheService` 缓存——参考实现把每首歌找到的歌词缓存到本地文件系统；缓存 key 是 `(title|artist|album|duration)` 经规范化（小写、去标点、去括号注释（feat. / cover / live / OST 等）、ASCII 折叠）后的字符串哈希。
3. 缓存未命中则并行/顺序查询所有启用的 source。「持久化源记忆」机制（`SongSearchMapService`）记录某个 `(title, artist)` 上次成功的 source，下次优先该 source。
4. 每个 source 返回 0..N 个候选 `LyricsSearchResult`，每个候选含：歌词文本（可能是 LRC/eslrc/TTML 字符串）、原始 title/artist/album/duration、source 自评的置信度。
5. 上层做**模糊匹配评分**：
   - title 用 Levenshtein 或 token-set ratio，规范化后比对。
   - artist 同样。
   - duration 用 `1 - |dur_query - dur_candidate| / tolerance`，tolerance 通常 5 秒。
   - 综合分 = w1·title + w2·artist + w3·duration，参考实现把阈值暴露给用户（"Smart Matching threshold"）。
6. 评分超过阈值的最高分候选胜出；同分时按 source 优先级。
7. 命中后写缓存（含原始文本和解析后的双层时间轴）。

参考实现的 source 列表：

- **LRCLIB**：GET `https://lrclib.net/api/get?artist_name=...&track_name=...&album_name=...&duration=...`。返回 JSON 含 `syncedLyrics`（增强 LRC，带 `[mm:ss.xx]` 行戳和可能的 `<mm:ss.xx>` 词戳）和 `plainLyrics`（无时间戳）。HTTP 200 + 非空 syncedLyrics 即视为命中。还有 `/api/search?q=...` 用于模糊搜索。
- **Lyricify-Lyrics-Helper**：覆盖 QQ 音乐、网易云、酷狗。这些源的歌词通常加密 + 私有时间戳格式，参考实现把 Lyricify-Lyrics-Helper 作为 NuGet/DLL 引入做抓取、解密、解析。注意：这是第三方独立项目，许可证需各自核对。
- **Manzana-Apple-Music-Lyrics**：用 Python 脚本接 Apple Music 私有 API 拿逐字 TTML。**需要 Python 运行时**——对 QmClient 是负担，**首版不集成**。

### D. 歌词格式与解析

参考实现支持 `.lrc`（Standard / Enhanced）、`.eslrc`、`.ttml`。统一为：

```
struct LyricsLine {
  int64_t start_ms;
  int64_t end_ms;           // 下一行 start 或最后一个词 end
  std::string raw_text;
  std::vector<LyricsWord> words;   // 可能为空（行级时间轴）
  std::optional<std::string> translation;
  std::optional<std::string> transliteration;
}
struct LyricsWord { int64_t start_ms, end_ms; std::string text; }
```

格式细节：

- **Standard LRC**：每行 `[mm:ss.xx]文本`，多个时间戳前缀表示该行在多个时间点出现。把每条 (时间戳, 文本) 摊开为一行；`end_ms = 下一行 start_ms`；`words` 留空。元数据 tag：`[ar:][ti:][al:][by:][offset:]`，`offset` 是毫秒级整体偏移。
- **Enhanced LRC**：行首 `[mm:ss.xx]`，行中 `<mm:ss.xx>词` 给逐词时间。词的 `start_ms = 当前内联时间戳`，`end_ms = 下一个内联时间戳` 或 `行 end_ms`。
- **`.eslrc`**：和 Enhanced LRC 同构（社区扩展，存储更工整）。同一套词级解析。
- **`.ttml`**：XML，根 `<tt>`，体 `<body>` 含 `<div>` → `<p begin="..." end="...">`，p 内有 `<span begin="..." end="...">词</span>` 或纯文本。时间属性可以是 `0:01:23.456`、`83.456s`、`83456ms` 等格式，要写一个时间解析器。`<p>` 上的 `agent` / `ttm:agent` 标识对唱角色。`<span ttm:role="x-translation">` 是翻译，`x-transliteration` 是音译。可选 namespace `xmlns:itunes="http://music.apple.com/lyric-ttml-internal"` 携带 Apple Music 扩展。

解析后存到内存的 vector，按 `start_ms` 已排序；活动行/词查找用二分。

### E. 渲染与动画

参考实现栈：WinUI 3 应用 + Win2D（`CanvasAnimatedControl`）。每帧回调画整面歌词。

QmClient 不能用 Win2D（是 D2D 投影），但渲染思路完全可以用 DDNet 已有的 `IGraphics` + `CTextRender` 复刻。算法层面的复述（自己实现这套，不抄代码）：

1. **帧时间轴**：每帧 `OnRender` 拿到 `current_ms`（D 节插值得到的当前播放毫秒）。
2. **活动行查找**：在已排序的 `lines` 上二分 `start_ms <= current_ms < end_ms`。记 `active_index`。
3. **逐行视觉状态**：对每个 line `i`，记 `distance = i - active_index`：
   - `alpha = clamp(1 - |distance| * fade_per_line, alpha_min, 1)`，参考值 `fade_per_line = 0.18`，`alpha_min = 0.10`。
   - `scale = active ? scale_active : 1 - |distance| * scale_falloff`，参考值 `scale_active = 1.06`，`scale_falloff = 0.04`。
   - `blur_radius = |distance| * blur_per_line`，参考值 `blur_per_line = 0.6`（用近似：QmClient 没有 Win2D 高斯，可以用文字描边外发光的 alpha 降级，或者多遍透明绘制叠加。**首版可以彻底跳过 blur**——视觉差距可接受）。
   - `vertical_offset = 行间距累加 + transition_offset(active_index_changed_at)`，过渡用 `easeOutCubic`（`1 - (1-t)^3`），参考实现也是 cubic 缓动（README 措辞 "fluid animations" + Material 风格曲线）。
4. **活动行滚动到屏幕中心**：当 `active_index` 切换时，启动一次 `vertical_offset` 的滚动动画，时长按 `QmUiMotionLevel` 缩放：
   - level 0（关闭）：瞬切。
   - level 1（降低）：200ms easeOutCubic。
   - level 2（完整）：350ms easeOutCubic。
5. **逐字高亮**：活动行 line `i` 内，对每个词 `w` 计算 `wp = clamp((current_ms - w.start) / max(1, w.end - w.start), 0, 1)`：
   - 整个词的颜色从「未播放色」过渡到「已播放色」。
   - 字符级渐进：把词宽按字符等分（汉字按一字一格，英文按字宽），过渡边界沿 x 推进。可以做硬切（`x < wp * word_width` 用 played 色，否则 unplayed 色），也可以做软边（在边界附近线性混合 6-12 px 宽度，参考实现的 "per-syllable highlighting" 走软边）。
   - **长音持续辉光**：如果 `w.end - w.start > long_word_threshold`（参考 1500 ms），叠加一个亮度脉冲 `1 + 0.15 * sin(2π * (current_ms - w.start) / pulse_period)`，`pulse_period` 约 600 ms。
6. **进入/离开动画**：行从屏幕边缘进入/离开时，`alpha` 从 0 升起、`y_offset` 从 +20px 滑入；缓动 easeOutCubic。
7. **空白行 / 间奏指示**：检测 `lines[i].end_ms - lines[i+1].start_ms > 5000 ms` 时，参考实现画 3 个小圆点逐个亮起，进度按间隔切片。可选。
8. **抗时钟漂移**：每帧的 `current_ms` 由本地时钟推算（B 节），每次 SMTC 给出新 `Position` 时，平滑过渡（不要瞬切防视觉抖动）—— 用 `current_ms = lerp(current_ms_local, current_ms_smtc, 0.2)` 每帧靠拢，差值 > 1000 ms 才硬切。

视觉特色（参考实现的而 QmClient HUD 叠加层值得复刻的子集）：

- 「perspective fade」：远离活动行的行越来越暗、越来越小。已含在 3 中。
- 「per-syllable highlight」：逐字色推进。已含在 5 中。
- 「long-note glow」：长音辉光。已含在 5 中。

参考实现的而 QmClient HUD 叠加层**不**复刻的：流体背景、雪花/雾粒子、频谱、专辑封面背景、卡片主题（Vinyl / CD / Polaroid / Cyberpunk）。HUD 叠加层只画文字本体。

### F. 配置面

参考实现可配置项极多。本规格只挑对 HUD 叠加层有意义的子集，详见后续「QmClient 配置」节。

### G. 插件契约

参考实现的核心契约（在 `Core/Interfaces/`）：

- `IPlugin`：所有插件基类标识。
- `IConfigurator`：插件向宿主描述自己的 UI 配置 schema。
- `ILocalizer`：插件提供自己的本地化字符串。
- `ILyricsSource`（`Interfaces/Features/`）：歌词数据源。
- `ILyricsTranslator`：翻译。
- `ILyricsTransliterator`：音译（如 Romaji）。
- `IAIService`（`Interfaces/Services/`）：本地 LLM/NMT 推理服务（给 Translator/Transliterator 用）。

参考实现的插件通过 `index.json` 清单 + 子目录约定 + `PluginService` 动态加载 DLL。

QmClient 首版**不**做 DLL 热加载，但**类层级**复刻这些契约（参见「QmClient 插件分层」节）。二期可考虑用 `IStorage` + `IConsole` 的 lua/JS 嵌入做扩展，但**绝不**做 GPL 代码引入。

### H. 平台依赖与跨平台障碍

- WinUI 3 + Win2D + WinRT：UI 渲染完全 Windows。**QmClient 不依赖它们**，渲染走 DDNet 自有 `IGraphics`/`CTextRender`。
- SMTC：Windows-only。Linux/macOS 上是 MPRIS / MediaRemote；本规格直接屏蔽。
- `Windows.Media.Control` C++/WinRT：包含在 Windows SDK；MinGW 工具链需要 `c++/winrt` 支持。CI 上 QmClient 用 MSVC 即可。
- Manzana（Apple Music TTML）需要 Python——本规格不集成。
- HTTP：复用 DDNet `IHttp` / `CHttp` 即可，不需要新依赖。
- JSON：复用 `external/json-parser` 或现有 nlohmann 包装（按 QmClient 现状选择）。

### I. 许可证

仓库根 `LICENSE` 是 GPL v3，README 明确声明 GPL-3.0。文件级 SPDX header 未在公开 listing 上完整确认（只看到 README 在仓库根强调一次），但即使个别源文件没有头部标注，GPL v3 整库覆盖仍生效。

**结论**：禁止把任何 BetterLyrics 源码、资源、shader、PNG、XAML、Lyricify-Lyrics-Helper DLL 引入 QmClient 仓库。本规格只用其**架构思路**——架构思想不受版权保护，clean-room 重写完全合规。所有 C++ 实现自写，注释 / commit message 可以说明「思路参考 BetterLyrics（GPL-3.0）」。

---

## QmClient 组件设计

### 组件位置

- 新增源码：`src/game/client/components/qmclient/qm_lyrics/`（snake_case 与项目其它子目录如 `hud_notifications/`、`monitoring/`、`scripting/` 一致）
  - `qm_lyrics.h` / `qm_lyrics.cpp`：组件主类 `CQmLyrics`，继承 `CComponent`，注册到 `gameclient.cpp`（T6 阶段创建）。
  - `qm_lyrics_smtc.h` / `qm_lyrics_smtc.cpp`：SMTC 适配层（Windows 专属，`#if defined(CONF_FAMILY_WINDOWS)` 包裹整个 .cpp）。
  - `qm_lyrics_model.h`：`SLyricsLine`、`SLyricsWord`、`SLyricsTrack` POD 结构（**与现有 `lyrics/lyric_model.h` 的 `CLyricLine` / `CSyllable` 是不同类型**——后者为保留代码服务，新模型为新数据源/渲染服务）。
  - `qm_lyrics_parser_lrc.h` / `qm_lyrics_parser_lrc.cpp`：LRC + Enhanced LRC + ESLRC 解析（**新写**，不复用旧 `ParseLrcLyrics`）。
  - `qm_lyrics_parser_ttml.h` / `qm_lyrics_parser_ttml.cpp`：TTML 解析（**全新**）。
  - `qm_lyrics_match.h` / `qm_lyrics_match.cpp`：标题/艺术家归一化、模糊匹配评分。
  - `qm_lyrics_cache.h` / `qm_lyrics_cache.cpp`：本地缓存读写。
  - `qm_lyrics_source.h`：`IQmLyricsSource` 抽象接口。
  - `qm_lyrics_source_lrclib.h` / `qm_lyrics_source_lrclib.cpp`：LRCLIB HTTP 实现。
  - `qm_lyrics_source_qq.h` / `qm_lyrics_source_qq.cpp`：**复用** `lyrics/qq_music_api`（T0c 抽取出来的纯函数）作 `IQmLyricsSource` 适配。
  - `qm_lyrics_source_netease.h` / `qm_lyrics_source_netease.cpp`：**复用** `lyrics/netease_music_api` 作 `IQmLyricsSource` 适配。
  - `qm_lyrics_render.h` / `qm_lyrics_render.cpp`：HUD 帧渲染（行/词布局、动画、文字绘制）。
- 保留并被复用的源码：
  - `src/game/client/components/qmclient/lyrics/lyric_model.h`（保留 `CSyllable` / `CLyricLine`）
  - `src/game/client/components/qmclient/lyrics/lyric_parser.h` / `.cpp`（保留 QRC/YRC/解密/EAPI/合并 等所有非 LRC 函数）
  - 新增 `src/game/client/components/qmclient/lyrics/qq_music_api.h` / `.cpp`（T0c 从旧 `lyrics_component.cpp` 抽取）
  - 新增 `src/game/client/components/qmclient/lyrics/netease_music_api.h` / `.cpp`（T0c 从旧 `lyrics_component.cpp` 抽取）
- 删除源码：
  - `src/game/client/components/qmclient/lyrics_component.h` / `.cpp`（旧 CLyrics 类整个）
- 数据流：新源（LRCLIB）输出 `SLyricsTrack`；旧源（QQ/网易）输出 `CLyricLine` 然后通过 `qm_lyrics_model_adapter` 转换为 `SLyricsTrack`，统一交给渲染层。
- 测试：`src/test/qm_lyrics_parser_lrc_test.cpp`、`qm_lyrics_parser_ttml_test.cpp`、`qm_lyrics_match_test.cpp`、`qm_lyrics_clock_test.cpp`、`qm_lyrics_qq_test.cpp`（保留的 QRC）、`qm_lyrics_netease_test.cpp`（保留的 YRC）、`qm_lyrics_model_test.cpp`（保留的 Merge）。SMTC 适配层不直接跑单元测试（依赖 WinRT），用 fake clock + fake metadata 注入测试 `CQmLyrics` 状态机。

### 生命周期

`CQmLyrics` 实现 `CComponent` 的：

- `OnInit`：在 Windows 上初始化 SMTC manager（异步 `RequestAsync`），订阅 `CurrentSessionChanged`；建一个 HTTP 客户端句柄；加载本地缓存索引。非 Windows 上 `OnInit` 直接 return。
- `OnReset`：清除当前轨道、关闭未完成的 HTTP 请求。
- `OnRender`：每帧根据当前 `current_ms` 计算可见行/词并绘制（见 E 节）。
- `OnShutdown`：解绑 SMTC 事件，刷写缓存索引到磁盘。
- `OnConfigChange`（如有）：重新读 `qm_lyrics_*`，必要时刷新缓存策略。

### HUD 编辑器接入

复用 #10 共用贴边 Helpers（见 `2026-06-20-backlog-consolidation-spec.html` 的 #10 节）：

- 在 `EHudEditorElement` 加 `Lyrics`。
- `BeginTransform(EHudEditorElement::Lyrics, BaseRect)` 让歌词框可拖拽/缩放/贴边。
- 暴露 `qm_lyrics_edge_margin` 走通用 `ApplyEdgeMargin`。
- 编辑器选中时显示半透明矩形 + 一行模拟歌词，方便用户调位置。

### 数据流

```
SMTC manager
  ↓ CurrentSessionChanged / MediaPropertiesChanged
SMTC Adapter (qm_lyrics_smtc)
  ↓ struct SNowPlaying { title, artist, album, duration_ms, position_ms, last_updated, rate, status }
CQmLyrics::OnNowPlayingChanged
  ↓
LyricsCache::Lookup(SNowPlaying) → hit? render; miss? ↓
LyricsSource::FetchAsync(SNowPlaying) → IQmLyricsSource::QueryAsync (LRCLIB)
  ↓ SLyricsCandidate (raw text + format hint + score)
LyricsMatcher::Score(query, candidate) → best candidate
  ↓
Parser::ParseLrc / ParseTtml → SLyricsTrack { vector<SLyricsLine> }
  ↓
LyricsCache::Store
  ↓
CQmLyrics::SetActiveTrack
  ↓ (每帧 OnRender)
Renderer 取 current_ms = ClockInterpolator::Now() → 二分活动行/词 → 绘制
```

### 时钟插值器

`SClockInterpolator`：

- 输入：每次 SMTC `TimelinePropertiesChanged` 推进 `m_AnchorMs`（SMTC 报告的 position）和 `m_AnchorWall`（本地 `time_get()`）。
- 输出 `Now()`：`m_AnchorMs + (time_get() - m_AnchorWall) * rate * 1000 / time_freq()`。
- 暂停时 freeze（`rate=0`）。
- 平滑漂移：保留一个「目标」和「实际」，每帧 `actual = lerp(actual, target, 0.2)`；差 > 1000 ms 硬切。

---

## H. 现状代码：保留 / 删除 / 抽取 清单

> 这是 T0a-T0f 的精确依据。任何模糊都按本节决断。

### 保留（不动，不删，不替换）

**`src/game/client/components/qmclient/lyrics/lyric_parser.cpp` 内的纯函数**：

| 行号 | 函数 | 类别 |
|------|------|------|
| 30-105 | `SetError`、`TrimString`、`ParseInt64`、`ParseTimestampMs` | 内部工具 |
| 107-150 | `ParseBracketMsDuration`、`ParseParenMsDuration`、`JoinSyllables` | 内部工具 |
| 152-251 | `ParseSyllableLine`、`ParseYrcSyllableLine` | QRC/YRC 行解析 |
| 253-330 | `ExtractJsonCreditsText`、`ExtractJsonCreditTime`、`ParseBase64` | YRC 辅助 |
| 332-391 | `LooksLikeSyncedLyricText`、`ParseHex`、`InflateBytes` | 共用工具 |
| 393-417 | `TripleDesEcbDecrypt` | **QQ 解密**（关键） |
| 419-476 | `Aes128EcbEncrypt`、`BytesToHexUpper`、`ToEapiPath` | **网易 EAPI**（关键） |
| 480-492 | `SortAndFillDurations` | 共用 |
| 599-625 | `ParseQrcLyrics` | **QQ 解析**（关键） |
| 627-667 | `ParseYrcLyrics` | **网易解析**（关键） |
| 669-688 | `MergeLineTextByTimestamp` | **翻译合并**（关键） |
| 690-740 | `DecryptQqQrcPayload` | **QQ 完整解密**（关键） |
| 742-763 | `BuildNeteaseEapiBody` | **网易 EAPI 完整签名**（关键） |

**`lyric_parser.h` 内对应声明**：除 `ParseLrcLyrics` 和 `BuildVisibleLineText` 外，全部保留。

**`lyric_model.h`**：`CSyllable` / `CLyricLine` 整个结构保留（不动字段）。新引入 `qm_lyrics_model.h` 的 `SLyricsLine` / `SLyricsWord` / `SLyricsTrack` 是**额外**的类型，不替换。两套类型并存，由适配层桥接。

### 删除

**`lyric_parser.cpp`**：

- 494-544 `BuildVisibleLineText`
- 546-597 `ParseLrcLyrics`

**`lyric_parser.h`**：

- `ParseLrcLyrics` 声明
- `BuildVisibleLineText` 声明

**`src/game/client/components/qmclient/lyrics_component.h` / `.cpp`**：**整个文件删除**（约 1426 行）。其中要被抽取出去的 4 段在 T0c 处理。

**`src/game/client/gameclient.h:245`**：删 `CLyrics m_Lyrics;` 成员。

**`src/game/client/gameclient.cpp:295`**：删 `&m_Lyrics,` 行。

**`src/game/client/components/hud.cpp`**：

- 3298 处 `GameClient()->m_Lyrics.GetCurrentLineState()` 调用
- 5479-5480 处 `GetCurrentLineState`/`GetNextLineState` 调用
- 5603-5604 处 `GetCurrentLineState`/`GetNextLineState` 调用
- 5620-5700+ 整个 `RenderLyricsHud` 函数
- 5634 和 5661 处 `BeginTransform(EHudEditorElement::Lyrics, ...)` 调用

**`src/game/client/components/qmclient/menus_qmclient.cpp`**：

- 2806-2807 模块表项 `case EQmModuleId::Lyrics: return {13, ...}`
- 7210-7299 整个歌词卡片渲染块

**`src/engine/shared/config_variables_qmclient.h:402-421`**：删全部 20 项 `qm_smtc_lyrics_*` / `qm_lyrics_*` 配置。

**`src/test/qm_lyrics_parser_test.cpp`**：

- `LrcParsesMultipleTimestampsAndSkipsEmptyLines`（覆盖删除的 LRC 解析）
- `LastLineHasNoNextLine`（依赖删除的辅助函数）
- `BuildsVisibleLineTextFromTimedSyllables`（覆盖删除的 BuildVisibleLineText）
- 整个文件留下空骨架后，被 T0e 拆成 3 个新测试文件

**保留 token**：`EHudEditorElement::Lyrics` 不删，新 `CQmLyrics` 会用同一 token 复用 HUD 编辑器接入。

### 抽取（T0c：从旧 `lyrics_component.cpp` 抽到新模块）

抽到 `src/game/client/components/qmclient/lyrics/qq_music_api.{h,cpp}`：

- `lyrics_component.cpp:479-492` QQ 搜索请求构造（包装为 `QmLyrics::QqMusic::BuildSearchRequest(const char *pTitle, const char *pArtist, std::string *pOutUrl, std::string *pOutBody)` 纯函数）
- `lyrics_component.cpp:494-531` QQ 歌词请求构造（`BuildLyricRequest`）
- `lyrics_component.cpp:819-891` QQ 搜索响应解析（`ParseSearchResponse`）
- `lyrics_component.cpp:893-957` QQ 歌词响应解析（`ParseLyricResponse`）

抽到 `src/game/client/components/qmclient/lyrics/netease_music_api.{h,cpp}`：

- `lyrics_component.cpp:180-216` Netease Header/Cookie 构造（`BuildHeader`/`BuildCookie`）
- `lyrics_component.cpp:533-561` Netease 搜索请求构造
- `lyrics_component.cpp:574-598` Netease 歌词请求构造
- `lyrics_component.cpp:959-1022` Netease 搜索响应解析
- `lyrics_component.cpp:1024-1054` Netease 歌词响应解析

抽出后，**所有抽出函数都不持有 `CLyrics` state**，只接受参数 + 返回结果。

### T0c 抽取的接口签名（设计）

```cpp
// lyrics/qq_music_api.h
namespace QmLyrics::QqMusic {
struct SSearchResult { std::string m_SongId, m_SongMid; bool m_Valid = false; };
struct SLyricResult { std::vector<CLyricLine> m_vLines; bool m_HasSync = false; };

void BuildSearchRequest(const char *pTitle, const char *pArtist,
                        std::string *pOutUrl, std::string *pOutBody);

void BuildLyricRequest(const char *pSongId, const char *pSongMid,
                       bool UseNewEndpoint,
                       std::string *pOutUrl, std::string *pOutBody);

// 输入：搜索响应 body；输出：song id/mid + 候选评分
bool ParseSearchResponse(const char *pBody, size_t BodyLen,
                         const char *pWantTitle, const char *pWantArtist,
                         const char *pWantAlbum, int WantDurationSec,
                         SSearchResult *pOut,
                         char *pErr, size_t ErrSize);

// 输入：歌词响应 body；输出：行列表
bool ParseLyricResponse(const char *pBody, size_t BodyLen,
                        bool OldEndpoint,
                        SLyricResult *pOut,
                        char *pErr, size_t ErrSize);
}

// lyrics/netease_music_api.h
namespace QmLyrics::NeteaseMusic {
struct SSearchResult { std::string m_SongId; bool m_Valid = false; };
struct SLyricResult { std::vector<CLyricLine> m_vLines; bool m_HasSync = false; };

void BuildSearchRequest(const char *pTitle, const char *pArtist,
                        std::string *pOutUrl, std::string *pOutBody,
                        std::string *pOutCookie);

void BuildLyricRequest(const char *pSongId,
                       std::string *pOutUrl, std::string *pOutBody,
                       std::string *pOutCookie);

bool ParseSearchResponse(const char *pBody, size_t BodyLen,
                         const char *pWantTitle, const char *pWantArtist,
                         const char *pWantAlbum, int WantDurationSec,
                         SSearchResult *pOut,
                         char *pErr, size_t ErrSize);

bool ParseLyricResponse(const char *pBody, size_t BodyLen,
                        SLyricResult *pOut,
                        char *pErr, size_t ErrSize);
}
```

新数据源适配（T0 之后）：

- `qm_lyrics_source_qq.cpp` 调用上述 `QqMusic::*`，实现 `IQmLyricsSource`
- `qm_lyrics_source_netease.cpp` 调用上述 `NeteaseMusic::*`
- 把返回的 `std::vector<CLyricLine>` 通过 `qm_lyrics_model_adapter::FromLegacy(...)` 转换为新 `SLyricsTrack`

---

## QmClient 配置（`qm_lyrics_*`）

放 `src/engine/shared/config_variables_qmclient.h`。前缀 `Qm`/`qm_lyrics_`。所有数值范围保守，避免极端值卡渲染。

| 配置变量 | 描述 | 范围 | 默认 |
|---|---|---|---|
| `QmLyrics` / `qm_lyrics` | 总开关 | 0/1 | 0 |
| `QmLyricsSource` / `qm_lyrics_source` | 数据源（0=LRCLIB，1..=预留） | 0..3 | 0 |
| `QmLyricsAutoFetch` / `qm_lyrics_auto_fetch` | 检测到新歌时自动联网拉取 | 0/1 | 1 |
| `QmLyricsCacheEnable` / `qm_lyrics_cache_enable` | 启用本地缓存 | 0/1 | 1 |
| `QmLyricsCacheTtlDays` / `qm_lyrics_cache_ttl_days` | 缓存过期天数（0=永久） | 0..3650 | 30 |
| `QmLyricsMatchThreshold` / `qm_lyrics_match_threshold` | 模糊匹配阈值（0-100） | 0..100 | 60 |
| `QmLyricsLinesAbove` / `qm_lyrics_lines_above` | 活动行之上显示行数 | 0..6 | 2 |
| `QmLyricsLinesBelow` / `qm_lyrics_lines_below` | 活动行之下显示行数 | 0..6 | 3 |
| `QmLyricsFontSize` / `qm_lyrics_font_size` | 活动行字号（px） | 8..48 | 18 |
| `QmLyricsFontSizeOther` / `qm_lyrics_font_size_other` | 非活动行字号（px） | 6..40 | 14 |
| `QmLyricsLineSpacing` / `qm_lyrics_line_spacing` | 行间距（px） | 0..40 | 6 |
| `QmLyricsOpacity` / `qm_lyrics_opacity` | 整体不透明度 | 0..100 | 90 |
| `QmLyricsInactiveOpacity` / `qm_lyrics_inactive_opacity` | 非活动行最小不透明度 | 0..100 | 35 |
| `QmLyricsColorPlayed` / `qm_lyrics_color_played` | 已播放词颜色 RGB | hex | 白 |
| `QmLyricsColorUnplayed` / `qm_lyrics_color_unplayed` | 未播放词颜色 RGB | hex | 浅灰 |
| `QmLyricsColorTranslation` / `qm_lyrics_color_translation` | 翻译/音译颜色 | hex | 中灰 |
| `QmLyricsShowTranslation` / `qm_lyrics_show_translation` | 显示翻译（若数据源给） | 0/1 | 1 |
| `QmLyricsShowTransliteration` / `qm_lyrics_show_transliteration` | 显示音译（若数据源给） | 0/1 | 0 |
| `QmLyricsKaraoke` / `qm_lyrics_karaoke` | 启用逐字高亮 | 0/1 | 1 |
| `QmLyricsHighlightEdgeSoft` / `qm_lyrics_highlight_edge_soft` | 逐字边缘软过渡宽度 px | 0..32 | 8 |
| `QmLyricsScaleActive` / `qm_lyrics_scale_active` | 活动行缩放（百分比） | 100..200 | 106 |
| `QmLyricsScaleFalloff` / `qm_lyrics_scale_falloff` | 远离行每行缩小（百分比） | 0..20 | 4 |
| `QmLyricsFadePerLine` / `qm_lyrics_fade_per_line` | 远离行每行衰减（百分比） | 0..40 | 18 |
| `QmLyricsScrollMs` / `qm_lyrics_scroll_ms` | 行切换滚动时长 ms（最终值还要乘 MotionLevel） | 0..1000 | 350 |
| `QmLyricsDriftCorrectMs` / `qm_lyrics_drift_correct_ms` | 时钟硬切阈值 | 100..5000 | 1000 |
| `QmLyricsEdgeMargin` / `qm_lyrics_edge_margin` | 贴边外边距 | 0..64 | 8 |
| `QmLyricsHttpTimeoutMs` / `qm_lyrics_http_timeout_ms` | LRCLIB 请求超时 | 500..30000 | 8000 |
| `QmLyricsOffsetMs` / `qm_lyrics_offset_ms` | 手动时间轴偏移（可负） | -5000..5000 | 0 |
| `QmLyricsHideWhenPaused` / `qm_lyrics_hide_when_paused` | 暂停时隐藏 | 0/1 | 1 |
| `QmLyricsHideNoLyrics` / `qm_lyrics_hide_no_lyrics` | 未找到歌词时隐藏（而非显示「未找到」） | 0/1 | 0 |

设置页位置：栖梦 → 视觉 → 新增「歌词 HUD」子区。各项分组：开关、数据源/匹配、布局、视觉、动画、缓存、调试（手动偏移、显示当前匹配评分）。

---

## i18n

所有字符串通过 QmClient 现有 i18n 流水线：

- 字符串提取：`qmclient_scripts/languages_qmclient/extract_strings.py` 自动扫描 `Localize()` / `Localizable()` 调用。
- 生成：`generate_all.py` 输出 `data/languages/qmclient/*.json`。
- 校验：`validate.py` 必须全绿。

待翻译字符串清单（首版）：

- 设置页：歌词 HUD、启用歌词、数据源、自动联网获取、本地缓存、缓存过期天数、模糊匹配阈值、活动行上方行数、活动行下方行数、活动行字号、其他行字号、行间距、整体不透明度、非活动行最小不透明度、已播放词颜色、未播放词颜色、翻译颜色、显示翻译、显示音译、逐字高亮、边缘软过渡、活动行放大比例、远离行缩小、远离行透明度衰减、行切换时长、时钟漂移阈值、贴边外边距、HTTP 超时、手动偏移、暂停时隐藏、无歌词时隐藏。
- 状态文案（HUD 内）：「未连接到任何播放器」、「正在搜索歌词…」、「未找到歌词」、「网络错误」、「该歌曲无可同步歌词，仅有纯文本」。
- 编辑器：「歌词 HUD」（元素名）。

---

## 缓存

- 位置：DDNet 用户数据目录下 `qmclient/lyrics/`（通过 `IStorage::GetPath(IStorage::TYPE_SAVE, "qmclient/lyrics/...", ...)` 解析；Windows 上典型展开为 `%APPDATA%/DDNet/qmclient/lyrics/`，便携模式下为 `<exe-dir>/qmclient/lyrics/`）。
- 文件结构：`<sha256(normalized_key).first16>.json`，内含原始格式文本 + 解析后的紧凑 JSON。
- 索引：`index.json` 维护 `key → file, last_used_at, source, score`。启动时整文件读入；关闭时刷写。
- LRU 上限：1000 条；超出时按 `last_used_at` 淘汰。
- TTL：`qm_lyrics_cache_ttl_days` 控制。
- 缓存键规范化：lowercase、删除 `(feat. xxx)` / `[live]` / `- remaster*` 等修饰、删除标点、保留中日韩汉字、ASCII 折叠（去重音）。

---

## QmClient 插件分层（首版仅 C++ 内部抽象）

```
struct SLyricsQuery { std::string title, artist, album; int64_t duration_ms; };
struct SLyricsCandidate { std::string raw_text; EQmLyricsFormat format; float source_score; std::string source_id; };

class IQmLyricsSource {
public:
  virtual ~IQmLyricsSource() = default;
  virtual const char *Id() const = 0;             // "lrclib"
  virtual const char *DisplayName() const = 0;    // i18n
  virtual void QueryAsync(const SLyricsQuery &Query,
                          std::function<void(std::vector<SLyricsCandidate>)> OnDone,
                          std::function<void(const char *Error)> OnError) = 0;
};

class IQmLyricsTranslator {  // 预留，首版不实现
public:
  virtual ~IQmLyricsTranslator() = default;
  virtual void TranslateAsync(...) = 0;
};

class IQmLyricsTransliterator { /* 同上 */ };
```

`CQmLyrics` 持有 `std::vector<std::unique_ptr<IQmLyricsSource>>`，按 `qm_lyrics_source` 选当前源。新增源只需实现接口 + 在 `CQmLyrics::OnInit` 注册。**首版只注册 LRCLIB**。

---

## 风险与约束

1. **GPL 污染**：任何形式（源码片段、shader、字体、PNG、XAML、Lyricify DLL）的 BetterLyrics 资产进入仓库都会污染。code review 必须明确这一点。
2. **WinRT 接入**：`Windows.Media.Control` 通过 C++/WinRT 接入；要在 CMake 里 `target_link_libraries` 加 `RuntimeObject.lib` 并启用 `/await`。MSVC 项目已开 C++20，无额外构建系统改动。`mingw` 构建 QmClient 的场景需评估（可选：mingw 下整个组件 `#if 0`）。
3. **SMTC 异步**：所有 `*Async` 调用必须在主线程取结果或经线程安全派发。建议把 SMTC 适配层放独立工作线程，通过 thread-safe queue 把 `SNowPlaying` 投递给 `CQmLyrics`；`OnRender` 在主线程 pop。
4. **网络请求**：用 DDNet `IHttp`/`CHttp` 异步接口，不阻塞渲染。超时由 `qm_lyrics_http_timeout_ms` 控制。
5. **歌词版权**：LRCLIB 是社区贡献，可能不全；某些热门曲目缺词。UI 上要友好提示「未找到」。
6. **隐私**：SMTC 元数据（曲名/艺人）会被发送给 LRCLIB。设置页要说明清楚，可关 `qm_lyrics_auto_fetch`。
7. **性能**：每帧二分查找 + 词级 alpha 计算 ~O(可见行数 × 词数)，可见行通常 < 10，词数通常 < 30，总计 < 300 项，对 DDNet 60 fps 无压力。
8. **HUD 编辑器一致性**：必须与 #10 共用贴边 Helpers 完成后再做 `BeginTransform` 接入，否则会复制和通知栏一样的「编辑器贴边了实际渲染贴不到」bug。
9. **时钟漂移**：SMTC 报告的 `Position` 在快进/拖动后会跳变，必须有 `qm_lyrics_drift_correct_ms` 硬切兜底，否则进度条卡死。
10. **TTML 时间格式**：实测 Apple Music TTML 用 `MM:SS.fff`，但 LRCLIB 偶尔返回 TTML 用 `Ss`、`Sms`，解析器必须容错三种。
11. **i18n 文案要走脚本生成**，不要手写 JSON。新增字符串后必须重跑 `extract_strings.py` → `generate_all.py` → `validate.py`。
12. **平台屏蔽**：非 Windows 平台 `CQmLyrics::OnInit` 立刻返回，HUD 编辑器不列出该元素，设置页隐藏对应子区。

---

## 验收标准

1. **功能**
   1.1 在 Windows 上启动 Spotify / Apple Music for Windows / Netease 云音乐任一播放器并播放，HUD 出现歌词，标题/艺人识别正确。
   1.2 切歌时 1-3 秒内更新到新歌词。
   1.3 暂停时停止滚动，恢复播放后继续。
   1.4 拖动播放进度时，1 秒内追上新位置。
   1.5 找不到歌词时显示「未找到歌词」（或按设置隐藏）。
   1.6 关闭 `qm_lyrics` 后组件不画任何东西、不发任何网络请求、不读 SMTC。
2. **渲染**
   2.1 活动行居中、放大；远离行变小、变暗。
   2.2 逐字高亮逐字推进，软边过渡可见。
   2.3 `QmUiMotionLevel=0` 时所有动画瞬切。
   2.4 行切换滚动平滑（无瞬跳）。
   2.5 HUD 编辑器可拖拽、可贴边四角，预览与实际位置一致。
3. **数据源**
   3.1 LRCLIB 返回 200 + syncedLyrics 时正确解析并显示。
   3.2 LRCLIB 返回纯文本 plainLyrics 时按行展示，无逐字效果。
   3.3 LRCLIB 网络超时不阻塞渲染、不崩溃。
   3.4 命中缓存时无网络请求（用 wireshark 或 HTTP 日志验证）。
4. **格式**
   4.1 标准 `.lrc` 解析正确（含 `[offset:]`、多时间戳行）。
   4.2 增强 `.lrc` 含 `<mm:ss.xx>` 词戳正确解析、逐字高亮正确。
   4.3 `.eslrc` 解析与增强 LRC 等价。
   4.4 `.ttml` 解析含 `<p>`/`<span>`、三种时间格式、`x-translation` / `x-transliteration` role。
5. **匹配**
   5.1 同名不同曲（如多版本）按 duration 区分能选对。
   5.2 `(feat. xxx)` / `[Live]` 注释不影响匹配。
   5.3 阈值低于配置时拒绝匹配并显示「未找到歌词」。
6. **i18n**
   6.1 所有设置项中英文均有翻译。
   6.2 `extract_strings.py` 提取无遗漏。
   6.3 `validate.py` 全绿。
7. **测试**
   7.1 解析单元测试覆盖三种格式各 3+ 样本。
   7.2 匹配评分单元测试覆盖完全匹配、部分匹配、duration 误差、噪声注释。
   7.3 时钟插值单元测试覆盖暂停/恢复/拖动/漂移硬切。
8. **平台**
   8.1 Linux/macOS 构建通过，组件存在但 stub。
   8.2 Windows MSVC 构建通过，WinRT 引用正确。
9. **gate**
   9.1 `python qmclient_scripts/gate/check_docs.py` 全绿（本规格新增文档应被识别）。
   9.2 `python qmclient_scripts/gate/check_gate.py --mode quick` 全绿。
   9.3 代码审查（独立只读子代理按 `docs/ai-workflow/review.md`）的 findings 全部收口。
10. **许可证**
    10.1 仓库无任何 BetterLyrics 源码、资源、shader、字体的拷贝。
    10.2 commit message 可说明思路参考来源 + 许可证。

---

## 任务拆分（建议实现顺序）

> 一次一个功能。每一步独立 commit + 跑 gate。

0. **T0 拆除与抽取（含 #10 贴边 Helpers）**——本组合代替原 spec 仅"新建"的设定，分子任务：
   - **T0a #10 共用贴边 Helpers**：`hud_editor.h` 加 `QmHudEditor::SEdgeMargin` / `ApplyEdgeMargin` / `EHorizontalFlow` / `ResolveHorizontalFlow`；`ClampStateToScreen` / `ComputeTransformPlacement` 接收 `SEdgeMargin`；通知栏私有 `InsetAnchoredRect` / `ResolveHorizontalFlow` 迁移到通用层；`qm_hud_notifications_test` + `QmHudEditorGeometry` snap 测试全绿。
   - **T0b 拆除**：删 `hud.cpp` 中 5 处 `m_Lyrics` 调用 + `RenderLyricsHud`，删设置页歌词卡 + 模块表项，删 20 个 `qm_lyrics_*` 配置，删 `gameclient.h:245` 成员 + `gameclient.cpp:295` Add；保留 `EHudEditorElement::Lyrics` token。
   - **T0c 抽取**：从 `lyrics_component.cpp` 抽 QQ 4 段和 Netease 5 段到 `lyrics/qq_music_api.{h,cpp}` 和 `lyrics/netease_music_api.{h,cpp}`，纯函数接口（H 节"抽取"子节签名）。
   - **T0d 删 CLyrics**：删 `lyrics_component.h/cpp` 整个；删 `lyric_parser.cpp` 中 `ParseLrcLyrics` / `BuildVisibleLineText`，删对应 `lyric_parser.h` 声明。
   - **T0e 测试拆分**：删旧 `qm_lyrics_parser_test.cpp` 中 LRC + BuildVisibleLineText 两条；保留 QRC/YRC/Merge 拆到 `qm_lyrics_qq_test.cpp` / `qm_lyrics_netease_test.cpp` / `qm_lyrics_model_test.cpp`。
   - **T0f gate**：`python qmclient_scripts/gate/check_gate.py --mode quick` 全绿后 commit。
1. **T1 新解析器**：建 `qm_lyrics_model.h`（`SLyricsTrack/SLyricsLine/SLyricsWord`），`qm_lyrics_parser_lrc.{h,cpp}`（标准 + Enhanced + ESLRC）、`qm_lyrics_parser_ttml.{h,cpp}`，`qm_lyrics_model_adapter.{h,cpp}`（旧 `CLyricLine` → 新 `SLyricsTrack`）+ 单元测试。无组件、无配置。
2. **T2 匹配器 + 缓存**：`qm_lyrics_match.cpp`、`qm_lyrics_cache.cpp` + 单元测试。
3. **T3 LRCLIB 数据源**：`qm_lyrics_source.h` 抽象 + `qm_lyrics_source_lrclib.cpp` 实现（用 `std::make_shared<CHttpRequest>` + `Http()->Run`）；同期把 T0c 抽取的 QQ/Netease 接入 `qm_lyrics_source_qq.cpp` / `qm_lyrics_source_netease.cpp`。集成测试用录制的固定 HTTP 响应。
4. **T4 SMTC 适配层（Windows-only）**：`qm_lyrics_smtc.cpp` 通过 C++/WinRT 订阅 manager 事件、生产 `SNowPlaying`。fake clock 单元测试。
5. **T5 时钟插值器**：`SClockInterpolator` + 单元测试。
6. **T6 组件骨架 + 配置**：`CQmLyrics` 注册到 `gameclient`，加全部 `qm_lyrics_*` 配置变量（不画渲染，先验证组件生命周期 + 配置热更）。
7. **T7 设置页 UI**：栖梦 → 视觉 → 「歌词 HUD」子区，串通所有配置。i18n 走脚本。
8. **T8 渲染：静态布局**：先画固定行 + 当前行高亮，无动画。HUD 编辑器接入，复用 T0a 贴边 Helpers。
9. **T9 渲染：动画**：行切换滚动、行级 alpha/scale 过渡。MotionLevel 接入。
10. **T10 渲染：逐字高亮**：词级颜色过渡 + 软边 + 长音脉冲。
11. **T11 端到端联调**：真机播 Spotify / Apple Music / Netease 各测一遍，调阈值默认值。
12. **T12 代码审查 + gate**：派只读子代理按 `docs/ai-workflow/review.md` 审查；跑 `check_gate.py --mode default`；按 findings 收口。

---

## 后续扩展（不在本规格首版范围）

- E1 翻译/音译：实现 `IQmLyricsTranslator` / `IQmLyricsTransliterator`，可选接入本地 ONNX 模型（如 sherpa-onnx）。无在线翻译以避免隐私 / 配额问题。
- E2 多数据源 + 持久化源记忆：补 Lyricify-Lyrics-Helper 等的等价 C++ 重写（必须自写或选 MIT/BSD 许可的等价库），加 `SongSearchMapService` 等价。
- E3 编辑歌词偏移 UI：用户给特定歌曲存自定义 offset。
- E4 歌词导出到 demo：把当前帧歌词信息附带到 demo 录制（需上游 demo 格式扩展，**需明确批准**）。
- E5 房间分享：把当前歌词通过 QmClient 中心服务广播给同房间队友（需中心服务协议扩展，**需远程仓库主实现**）。
- E6 插件 DLL 热加载：评估安全性后做，沙箱化必须先到位。

---

## 附：BetterLyrics 调研引用清单

仅作为本规格的事实来源，不是引用的源码：

- README（仓库根）：列出依赖 LRCLIB / Lyricify-Lyrics-Helper / Manzana-Apple-Music-Lyrics / Isolation / SpectrumVisualization，特性列表含 "Native handling of `.lrc` (Standard/Enhanced), `.eslrc`, and `.ttml` formats"，"per-syllable highlighting"，"long-note duration glows"，"perspective-based fading"，"Smart Matching with customizable thresholds, manual metadata mapping, and persistent source memory"。许可证标注 GPL-3.0。
- `BetterLyrics.Core/Interfaces/`：`IPlugin.cs`、`IConfigurator.cs`、`ILocalizer.cs`、`Features/ILyricsSource.cs`、`Features/ILyricsTranslator.cs`、`Features/ILyricsTransliterator.cs`、`Services/IAIService.cs`。
- `BetterLyrics.Core/Abstractions/`：`PluginBase.cs`、`PluginConfigBase.cs`。
- `BetterLyrics.Core/Models/Domain/`：`LyricsSearchResult.cs`。
- `BetterLyrics.Core/Helpers/`：`LangGenerator.cs`、`SettingBuilder.cs`、`SettingsIO.cs`。
- `BetterLyrics.WinUI3/Services/`：19 个服务子目录，对本规格关键的是 `SMTCService`、`GSMTCService`、`LyricsSearchService`、`LyricsCacheService`、`SongSearchMapService`、`PluginService`、`SettingsService`、`LocalizationService`、`TranslationService`、`TransliterationService`、`FileSystemService`。
- `BetterLyrics.WinUI3/Renderer/`：背景特效系列 + `LyricsRenderer/`（`LyricsRenderer.cs`、`LyricsLineRendererBase.cs`、`HorizontalLyricsLineRenderer.cs`、`VerticalLyricsLineRenderer.cs`）。
- `BetterLyrics.WinUI3/Providers/`：`AppleMusic.cs`。
- `Plugins/`：`BetterLyrics.Plugins.Source.LRCLIB`、`BetterLyrics.Plugins.Translation.LocalAI`、`BetterLyrics.Plugins.Transliteration.Romaji`、`BetterLyrics.Plugins.AI.Local.LLM`、`BetterLyrics.Plugins.AI.Local.NMT`，加 `index.json`。

> 上述均为 BetterLyrics 的**路径与文件名**事实，不是其源码内容。实现 `CQmLyrics` 时所有 C++ 自写。
