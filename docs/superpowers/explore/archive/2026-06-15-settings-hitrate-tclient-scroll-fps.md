---
type: question
date: 2026-06-15
status: active
confidence: medium
scope:
  - src/game/client/components/menus.cpp
  - src/game/client/components/menus_settings.cpp
  - src/game/client/components/tclient/menus_tclient.cpp
  - src/game/client/components/qmclient/menus_qmclient.cpp
  - src/game/client/ui_scrollregion.cpp
  - qmclient_scripts/perf/lib/stats.ts
commit: 60163edab
related:
  - file: 2026-06-10-设置页文本缓存覆盖现状.md
    relation: complements
---

# 设置页 hitrate、TClient 滚动抖动与 DDNet 性能差异调查

## 速答

这次实机反馈说明：60163edab 修的是 stable text cache 的一个明确 miss 来源，但它不能解释整页仍然卡顿。`hitRate` 只统计 stable text candidate 的命中比例，不覆盖 TClient/QmClient 页面的 section 布局、交互层、资源 jobs、QmUI runtime、背景/粒子/皮肤预览、GPU upload 和通用 `Ui()->DoLabel` 路径；即使 hit rate 很高，只要 `menu_ms_max` 或滚动期间每帧 layout/draw 仍重，体感依旧会卡。

TClient 向下滚动的抖动更像独立问题：QmClient 在 `CScrollRegion` 上加了 TClient modifier hack，TClient 页又在滚动开始帧记录旧 offset、结束帧才推进动画与 content height，且 TClient perf stage 仍不带真实 frame id，当前日志很难把抖动帧和具体 stage 精确对齐。官方 DDNet 流畅的核心差异不是单个 micro-optimization，而是它没有 QmClient 这 5.8 万行级别的菜单/组件扩展、没有 TClient/QmClient 重页、没有设置页文本/资源预热调度层，也没有这些额外运行时系统参与同一设置页帧。

## 关键证据

| # | 结论 | 证据 | 位置 |
|---|---|---|---|
| 1 | `hitRate` 只代表 stable text coverage，不代表整页 FPS | 分析器用 `hitCount / candidateTotal` 计算 hitRate，并把 `miss/stale/remaining/unplanned/keyMismatch/textNew` 作为验收阻断项；它没有纳入 layout、draw、资源 jobs 或 GPU upload | `qmclient_scripts/perf/lib/stats.ts:737`, `qmclient_scripts/perf/lib/stats.ts:749` |
| 2 | visible guard 下 miss/stale 会退回 immediate label，所以命中率高仍可能有可见帧成本 | 缺失或 stale 时会记录 `settings_text_miss/stale` 并返回 fallback element；fallback render 走 `Ui()->DoLabel` | `src/game/client/components/menus.cpp:4473`, `src/game/client/components/menus.cpp:4534` |
| 3 | `text_new` 仍是独立风险，不等同于 hit/miss | `DoMenuLabelStreamed()` 在 visible guard 下单独统计 `m_MenuTextStableTextNewThisFrame` / reused；新建 text container 仍会影响可见帧 | `src/game/client/components/menus.cpp:4566`, `src/game/client/components/menus.cpp:4588` |
| 4 | 设置页 FPS 真实口径在 `fps_summary`，应结合 `menu_ms_max` 看 | `fps_summary` 记录 `fps_avg/min/max`、`frame_ms_p95/p99/max` 和 `menu_ms_max`，这是解释 300 avg fps 的主指标，不是 hitRate | `src/game/client/components/menus.cpp:4186`, `src/game/client/components/menus.cpp:4202` |
| 5 | TClient 滚动路径有 QmClient 改动，可能制造向下滚动不连续感 | `CScrollRegion::End()` 在 modifier pressed 时清掉 scroll direction，并跳过平滑动画；这是带 TClient 注释的全局 hack | `src/game/client/ui_scrollregion.cpp:103`, `src/game/client/ui_scrollregion.cpp:146` |
| 6 | TClient 设置页滚动状态在 `Begin` 时读旧 offset，在 `End` 后才得到动画推进结果 | TClient 页在 `Begin()` 后立刻把 `ScrollOffset.y` 存到 runtime metadata 和 `m_SettingsScrollActive`，而真正平滑动画推进发生在 `s_ScrollRegion.End()` | `src/game/client/components/tclient/menus_tclient.cpp:1528`, `src/game/client/components/tclient/menus_tclient.cpp:3349` |
| 7 | TClient stage 日志无法逐帧对齐 FPS | `LogTClientPerfStage()` 调 `QmPerfLogStage(..., nullptr, ...)`，frame id 为 0；QmClient 对应 wrapper 已传 `Client()` | `src/game/client/components/tclient/menus_tclient.cpp:101`, `src/game/client/components/qmclient/menus_qmclient.cpp:92` |
| 8 | 官方 DDNet 与 QmClient 的设置页复杂度不是一个量级 | 对比 `ddnet/master..HEAD`，仅 `src/game/client/components/tclient`、`qmclient`、菜单和 gameclient/UI 相关范围就增加约 58807 行；官方没有 `SETTINGS_TCLIENT` / `SETTINGS_QMCLIENT` 页 | `git diff --stat ddnet/master..HEAD -- src/game/client/components/tclient src/game/client/components/qmclient src/game/client/components/menus.cpp src/game/client/components/menus_settings.cpp src/game/client/gameclient.cpp src/game/client/ui_scrollregion.cpp` |

## 细节

### 关于 hitRate

`hitRate` 高只能证明“已被登记为 stable text candidate 的文本，在可见窗口里找到了已构建容器”。它不能证明：

- 整页没有大量 non-stable 文本、动态 label、输入框或 tooltip。
- TClient/QmClient section layout 没有每帧重算。
- 资源加载、皮肤预览、GPU upload 没有抢帧。
- 背景粒子、QmUI runtime、TClient runtime 没有增加菜单帧成本。

所以如果实机卡顿仍明显，下一步应该优先看同一窗口里的 `fps_summary.menu_ms_max`、`frame_ms_p95/p99/max`，再把对应帧附近的 `perf/tclient`、`perf/qmclient`、`perf/settings-*`、`perf/ui_runtime` 关联起来。现在 TClient stage 缺 frame id，会阻断这一步。

### 关于 TClient 向下滚动抖动

当前 TClient 设置页滚动有两个可疑点：一个是全局 scroll region 的 modifier hack；另一个是滚动 offset 在页面渲染开头读取，但动画推进在页面渲染末尾完成。这意味着当前帧的内容位置使用上一轮 offset，下一帧才看到平滑推进结果；在页面内容高度/section culling 同时变化时，向下滚动可能出现“内容追着滚动条跳”的体感。

### 关于官方 DDNet

官方 DDNet 设置页的主要优势是“少做很多事”：页面数量少、模块少、状态少，设置页没有 TClient/QmClient 的复杂卡片、搜索、资源 jobs、文本计划收集、QmUI runtime、TClient 背景/粒子/皮肤预览等成本。QmClient 当前试图用缓存和预算调度把这些成本摊平，但这不等于可以达到官方 DDNet 的空设置页成本。

## 探索范围

- 已看：stable text 统计、prebuild/visible guard、fps summary、TClient/QmClient perf wrapper、TClient scroll region、上游 DDNet 基础对比。
- 未看：用户这次实机生成的原始 `qm_perf_*.log`，因为当前附件只有上一次最终汇报文本；未运行实机复现。
- 未看：GPU profile、RenderDoc、Windows ETW 或驱动层 frame pacing。

## 置信度说明

**confidence: medium**

代码路径证据足够说明“hitRate 不是整页流畅度指标”和“TClient 滚动存在独立嫌疑路径”。但缺少用户这次实机的原始 perf log，不能确认 300 avg fps 那个窗口的主耗时 stage，也不能判定滚动抖动是否来自 scroll offset、资源 jobs、GPU upload 或某个 TClient section。

## 未解决问题

- 用户这次实机日志中的 `targetSettings.stableTextCoverage.hitRate/reuseRate`、`fps_summary.menu_ms_max`、`frame_ms_p99` 分别是多少？
- `perf/tclient` 补真实 frame id 后，抖动帧对应的是 layout、interactive layer、text cache、还是资源/背景阶段？
- TClient 滚动 offset 是否应在 `End()` 后再写回 runtime metadata，或改成 begin/end 双样本记录？

## 2026-06-15 实现边界记录

本轮只统一设置页内部使用 `CScrollRegion` 的面板滚动生命周期：`BeginSettingsScrollRegion()` 负责 begin offset，`FinishSettingsScrollRegion()` 负责 `AddRect`、`End`、读取 End 后最终 offset、更新 `m_SettingsScrollActive`，以及可选写回 runtime scroll metadata。

保留三类滚动职责边界：

- `CScrollRegion`：设置页面板滚动，覆盖 TClient 主设置页、TClient chat binds、TClient configs、QmClient overview/config/function、Controls 和 Language。
- `CListBox`：Tee 皮肤、Assets、服务器列表等虚拟列表，不改造成 `CScrollRegion`。
- `DoSmoothScrollLogic`：聊天、console、文本框横向滚动等局部文本滚动，本轮不碰。

TClient 主设置页的 runtime scroll metadata 不再在 `Begin()` 后立即写回，而是在 `FinishSettingsScrollRegion()` 后用 `ContentScrollOffsetY()` 的最终 offset 写回。section loader 本帧仍使用 begin offset 渲染，下一帧调度和 metadata 使用 post-End offset，避免滚动状态落后一帧。

## 相关文档

- `2026-06-10-设置页文本缓存覆盖现状.md` — 说明 stable text cache 的覆盖边界。
- `docs/superpowers/reports/archive/2026-06-14-设置页性能优化最终收口报告.md` — 记录上一轮文本 coverage 验收目标。
- `docs/superpowers/reports/archive/2026-06-14-设置页性能优化二次根因收口报告.md` — 记录 visible guard 与资源 visible-ready 的上一轮收口。

## 下一步

先拿这次实机的原始 `qm_perf_*.log` 跑 analyzer；同时补 `perf/tclient` frame id 和滚动 begin/end offset 诊断，才能把“卡顿依旧”和“向下滚动抖动”落到具体帧、具体 stage。

## 2026-06-15 二次实机日志与收口实现记录

用户实机日志 `qm_perf_2026-06-15_19-46-13.log` 已通过 `qmclient_scripts/perf/analyze.ts` 分析，生成：

- `C:/Users/11054/AppData/Roaming/DDNet/dumps/QmClient_Perf/Perf_Report/qm_perf_2026-06-15_19-46-13_report.html`
- `C:/Users/11054/AppData/Roaming/DDNet/dumps/QmClient_Perf/Perf_Report/qm_perf_2026-06-15_19-46-13_summary.json`

关键结论：

- 本次 summary 判定 `FAIL`，全局最大帧约 `229ms`，`settings:tclient` 首次 tab switch 最大约 `236ms`，`settings:assets` 首次进入最大约 `55ms`。
- TClient tab 0 首次尖峰对应大量 stable text miss/key mismatch；日志中 `hitRate` 约 `6.64%`，`keyMismatchCount` 为 `9210`。
- Assets direct scroll 期间可见 GPU upload 没有稳定命中 jump-scroll 保护；raw log 中 `assets_preview_gpu_upload_batch` 多次记录为 `frame_context=idle`。

本轮实现边界：

- Assets 增加 direct-scroll upload cooldown：由 listbox active、animating、offset changed、large offset jump 触发，冷却窗口为 6 帧。
- 冷却期间 local preview GPU upload、workshop thumb GPU upload、workshop thumb decode finalize 预算为 0，并在 perf log 中以 `frame_context=scroll_cooldown` / `scroll_upload_cooldown` 暴露。
- TClient 只收敛 tab 0 已知 miss 来源：`tclient-prediction-margin` 改为带 subtab 的 scrollbar wrapper，`tclient-player-indicator-title` 改为 `DoSettingsMenuLabel` 以进入 plan collection。
- 未扩展到所有 TClient 子页；若修后仍有 50ms+ Assets spike，应单独调查 `assets_preview_draw_workshop_cards` 的 layout/thumb start 成本。
