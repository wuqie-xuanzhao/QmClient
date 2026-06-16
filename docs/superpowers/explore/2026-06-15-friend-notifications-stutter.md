---
type: question
date: 2026-06-15
updated: 2026-06-16
status: active
confidence: high
scope:
  - src/game/client/components/tclient/tclient.cpp
  - src/game/client/components/tclient/tclient.h
  - src/game/client/components/broadcast.cpp
  - src/game/client/gameclient.cpp
  - src/game/client/components/skins.cpp
  - src/game/client/components/skins7.cpp
  - src/engine/client/text.cpp
  - src/engine/client/serverbrowser.cpp
  - src/engine/client/serverbrowser_http.cpp
  - src/engine/client/friends.cpp
  - src/game/client/components/chat.cpp
  - src/engine/shared/config_variables_qmclient.h
  - src/engine/shared/config_variables_tclient.h
commit: ada827a1b
related:
  - file: 2026-06-02-设置页UI性能优化第一性原理分析.md
    relation: references
---

## Quick Answer

好友相关卡顿有两条不同路径，不能混在一起判断：

1. `qm_friend_online_notify` 是“好友上线提醒”，会周期性刷新/扫描 HTTP server list，可能卡在 server browser 刷新完成、整表解析、重建和排序。
2. 用户反馈的实际问题是“好友进入本服务器”，对应 `qm_friend_enter_broadcast` / `qm_friend_enter_auto_greet`。这条路径不扫全服服务器列表；检测本身很轻，最可疑的是“大字显示好友进服”触发广播文本首帧创建：`TextWidth()` + `CreateTextContainer()` 会同步生成中文 glyph、维护 text atlas、上传 GPU 文字纹理。TClient 的 `tc_custom_font` 会改变 TextRender 的主字体，如果自定义字体不含中文或中文 glyph 未预热，就会放大这类首次渲染尖峰。

适合 DDNet/QmClient 的优化方向不是先做全局异步文本系统，而是先做小范围预热和预算化：对好友进服广播、HUD 通知和常用中文提示预热固定中文字形；字体/语言切换后重新预热；必要时给 glyph upload 加帧预算。

## Key Evidence

| # | Conclusion | Evidence | Location |
|---|---|---|---|
| 1 | 好友相关功能分成“上线提醒”和“进本服提醒”两组配置 | `qm_friend_online_notify` / auto refresh / refresh seconds 是上线提醒；`qm_friend_enter_auto_greet` / `qm_friend_enter_broadcast` 是进本服提醒。 | `src/engine/shared/config_variables_qmclient.h:225`, `src/engine/shared/config_variables_qmclient.h:226`, `src/engine/shared/config_variables_qmclient.h:227`, `src/engine/shared/config_variables_qmclient.h:228`, `src/engine/shared/config_variables_qmclient.h:229` |
| 2 | “好友上线提醒”会周期性触发 HTTP server list 刷新，属于另一条卡顿来源 | `CheckFriendOnline()` 到达刷新时间且 server browser 不在拉列表时，调用 `Refresh(CurrentType, false)` 或 `RefreshHttpServerList()`。 | `src/game/client/components/tclient/tclient.cpp:2356`, `src/game/client/components/tclient/tclient.cpp:2357`, `src/game/client/components/tclient/tclient.cpp:2360`, `src/game/client/components/tclient/tclient.cpp:2361`, `src/game/client/components/tclient/tclient.cpp:2363` |
| 3 | HTTP 刷新完成阶段在主线程做整表解析、重建和排序 | `CServerBrowserHttp::Update()` 请求完成后 `ResultJson()` + `Parse()`；`CServerBrowser::Update()` 发现刷新结束后同步 `CleanUp()`、`UpdateFromHttp()`、`Sort()`。 | `src/engine/client/serverbrowser_http.cpp:365`, `src/engine/client/serverbrowser_http.cpp:397`, `src/engine/client/serverbrowser_http.cpp:399`, `src/engine/client/serverbrowser_http.cpp:450`, `src/engine/client/serverbrowser_http.cpp:460`, `src/engine/client/serverbrowser.cpp:1324`, `src/engine/client/serverbrowser.cpp:1326`, `src/engine/client/serverbrowser.cpp:1329`, `src/engine/client/serverbrowser.cpp:1330`, `src/engine/client/serverbrowser.cpp:1332` |
| 4 | “好友进入本服务器”检测本身只扫当前服务器玩家，并且 0.2 秒节流 | `OnUpdate()` 调 `CheckFriendEnterGreet()`；函数内 `Now < m_FriendEnterNextCheck` 直接返回，并设置下一次检查为 `Now + 0.2f`；循环只遍历 `ClientId < MAX_CLIENTS`。 | `src/game/client/components/tclient/tclient.cpp:1695`, `src/game/client/components/tclient/tclient.cpp:1769`, `src/game/client/components/tclient/tclient.cpp:2560`, `src/game/client/components/tclient/tclient.cpp:2562`, `src/game/client/components/tclient/tclient.cpp:2576` |
| 5 | 本服好友检测没有扫 server browser；它只看 active client 并做好友判断 | 读取 `GameClient()->m_aClients[ClientId]`，跳过 inactive/local client，然后 `Friends()->IsFriend()`。 | `src/game/client/components/tclient/tclient.cpp:2578`, `src/game/client/components/tclient/tclient.cpp:2579`, `src/game/client/components/tclient/tclient.cpp:2582`, `src/game/client/components/tclient/tclient.cpp:2584` |
| 6 | 进本服大字通知的重活不是 `DoBroadcast()` 当场完成，而是下一帧广播渲染首帧完成 | `CheckFriendEnterGreet()` 发现新好友后 `m_Broadcast.DoBroadcast()`；`DoBroadcast()` 只复制文本、重置 tick/offset、删除旧 text container。 | `src/game/client/components/tclient/tclient.cpp:2621`, `src/game/client/components/tclient/tclient.cpp:2623`, `src/game/client/components/tclient/tclient.cpp:2625`, `src/game/client/components/broadcast.cpp:89`, `src/game/client/components/broadcast.cpp:91`, `src/game/client/components/broadcast.cpp:94` |
| 7 | 广播首帧同步测量并创建文本容器，是中文首帧卡顿的直接候选 | `RenderServerBroadcast()` 在 offset 未初始化时调用 `TextWidth()`；text container 无效时调用 `CreateTextContainer()`。 | `src/game/client/components/broadcast.cpp:58`, `src/game/client/components/broadcast.cpp:59`, `src/game/client/components/broadcast.cpp:61`, `src/game/client/components/broadcast.cpp:67` |
| 8 | TextRender 首次遇到 glyph 时会同步栅格化并上传到 GPU atlas | `GetGlyph()` miss 后调用 `RenderGlyph()`；`RenderGlyph()` 内 `FT_Load_Glyph(... FT_LOAD_RENDER ...)`，随后 `UploadGlyph()` 更新 fill/outline 两张 text texture。 | `src/engine/client/text.cpp:753`, `src/engine/client/text.cpp:767`, `src/engine/client/text.cpp:779`, `src/engine/client/text.cpp:539`, `src/engine/client/text.cpp:541`, `src/engine/client/text.cpp:605`, `src/engine/client/text.cpp:606` |
| 9 | atlas 不够时会扩大并全量重传纹理，代码注释已指出这会造成首帧 GPU 上传尖峰 | `IncreaseGlyphMapSize()` 卸载纹理、扩容 CPU buffer、`UploadTextures()`；附近注释说明首帧填满触发扩容/全量上传会造成 GPU 上传尖峰。 | `src/engine/client/text.cpp:303`, `src/engine/client/text.cpp:389`, `src/engine/client/text.cpp:395`, `src/engine/client/text.cpp:396`, `src/engine/client/text.cpp:414` |
| 10 | TClient 自定义字体会影响全局 TextRender 的字体选择，因此可能放大中文首次 miss | `tc_custom_font` 配置存在；`CTClient::OnInit()` 调 `TextRender()->SetCustomFace(g_Config.m_TcCustomFont)`；glyph 查找顺序先查 selected/default/variant，再查 fallback。 | `src/engine/shared/config_variables_tclient.h:243`, `src/game/client/components/tclient/tclient.cpp:507`, `src/game/client/components/tclient/tclient.cpp:509`, `src/engine/client/text.cpp:441`, `src/engine/client/text.cpp:443`, `src/engine/client/text.cpp:456` |
| 11 | 自动打招呼不是即时渲染重活 | 自动打招呼先把名字追加到 pending，并设置 `m_FriendEnterPendingSendAt = Now + 3.0f`；到点后只组装 chat 并 `SendChat()`。 | `src/game/client/components/tclient/tclient.cpp:2532`, `src/game/client/components/tclient/tclient.cpp:2534`, `src/game/client/components/tclient/tclient.cpp:2546`, `src/game/client/components/tclient/tclient.cpp:2631`, `src/game/client/components/tclient/tclient.cpp:2635` |
| 12 | 好友加入同一快照还可能触发 skin/render info 刷新，与通知同帧叠加 | 新 snapshot 解析 `CLIENTINFO`/`PLAYERINFO` 后标记 client active；随后对所有 clients 调 `UpdateSkinInfo()`。新 client 首次 skin info 会 `CreateManagedTeeRenderInfo()`，创建时立即 `RefreshSkin()`。 | `src/game/client/gameclient.cpp:3353`, `src/game/client/gameclient.cpp:3368`, `src/game/client/gameclient.cpp:3387`, `src/game/client/gameclient.cpp:3679`, `src/game/client/gameclient.cpp:3681`, `src/game/client/gameclient.cpp:5210`, `src/game/client/gameclient.cpp:5213`, `src/game/client/gameclient.cpp:7366`, `src/game/client/gameclient.cpp:7369` |

## Details

### 两条好友路径

`CheckFriendOnline()` 是全服在线提醒。它会看 HTTP server list、自动刷新 server browser，并分帧扫描服务器玩家。这能解释“开了好友上线提醒后周期性卡顿”，但不是“好友进入本服务器”这个反馈的主路径。

`CheckFriendEnterGreet()` 是当前服务器检测。它只看当前快照中的 active clients，且 0.2 秒节流。单看这段检测，成本很小；真正值得怀疑的是新好友触发后同帧/下一帧发生的显示和资源刷新。

### 中文广播为什么会卡

默认大字文案含中文：`%s好友进入本服`。`DoBroadcast()` 本身只是设置文本，下一帧 `RenderServerBroadcast()` 才会首次测宽和创建 text container。对于没预热过的中文字符，TextRender 会在渲染路径里同步做：

- 查 selected/default/variant/fallback font face；
- FreeType glyph render；
- glyph outline 生成；
- fill/outline glyph 上传到 text atlas；
- 必要时扩大 atlas 并全量重传；
- 创建 text container 的 quad buffer。

这些操作不能简单全部搬到后台，因为 GPU texture upload 和 buffer object 创建依赖图形上下文。后台最多准备 CPU 侧 glyph/layout，最终合并仍要回主线程/渲染线程。

### TClient 自定义字体的影响

`tc_custom_font` 不是唯一根因，但会改变 glyph 命中行为。若自定义字体本身不含中文 glyph，TextRender 会先查 selected face 失败，再走 fallback；若 fallback glyph 之前也没进 atlas，就仍会在广播首帧栅格化/上传。若自定义字体包含中文但没有预热，也同样会在首次出现时生成这些中文字形。

### 玩家加入资源刷新

如果关闭 `qm_friend_enter_broadcast` 后仍卡，就要优先查新 client 加入快照后的 skin/render info 刷新。好友通知可能只是和 player join 同帧发生，而不是根因。

## Exploration Scope

- Focused directory: `src/game/client/components/tclient/`
- Files involved: `tclient.cpp`, `tclient.h`, `broadcast.cpp`, `gameclient.cpp`, `skins.cpp`, `skins7.cpp`, `text.cpp`, `serverbrowser.cpp`, `serverbrowser_http.cpp`, `friends.cpp`, `chat.cpp`, `config_variables_qmclient.h`, `config_variables_tclient.h`
- Skipped: 没有实机 perf log，因此没有量化 `broadcast TextWidth/CreateTextContainer`、glyph upload、atlas resize、skin refresh 各自耗时；没有修改代码，也没有做运行时复现。

## Confidence Notes

**confidence: high**

- 已明确区分“好友上线提醒”和“好友进入本服务器”两条路径。
- 已覆盖当前服务器检测、广播输出、自动打招呼、TextRender glyph/atlas 管线、TClient 自定义字体、玩家加入快照和 skin/render info 刷新。
- 根因排序仍需要实机 perf 采样确认：代码层最可疑是广播首帧中文 glyph/text container 创建；若关闭大字广播仍卡，再查 player skin/render info。

## Open Questions

- 只关闭 `qm_friend_enter_broadcast`、保留 `qm_friend_enter_auto_greet` 时是否还卡？
- 把 `qm_friend_enter_broadcast_text` 改成纯 ASCII（如 `%s joined`）后是否不卡？
- 使用默认 `DejaVu Sans` 与某个 TClient 自定义字体时，中文广播首帧耗时是否有明显差异？
- 卡顿帧是否对应 `broadcast TextWidth/CreateTextContainer`、TextRender glyph upload/atlas resize，还是新 client 的 skin/render info refresh？

## Related Documents

- `2026-06-02-设置页UI性能优化第一性原理分析.md` — 提供 p99/尖峰帧优先的性能判断背景。

## Next Steps

适合 DDNet/QmClient 的优化顺序：

1. 给好友进服广播、TextRender glyph miss/upload/atlas resize、`CClientData::UpdateSkinInfo()` 加 perf stage，先确认尖峰落点。
2. 增加小范围文本预热接口，例如 `TextRender()->PrewarmText(FontSize, Text)`，用于好友进服广播、HUD 通知、常用中文提示；在语言切换或 `tc_custom_font` 改变后重新预热。
3. 对 glyph upload/atlas resize 做帧预算或空闲帧合并，避免一帧塞入大量新中文字形。
4. 暂不优先做完整异步文本系统；若以后做，路线应是后台准备 glyph/layout CPU 数据，主线程按预算合并 GPU upload，UI 对未 ready 文本延迟显示或复用旧容器。
