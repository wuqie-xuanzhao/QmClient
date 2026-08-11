---
type: question
date: 2026-06-21
status: active
confidence: medium
scope:
  - src/game/client/components/tclient/tclient.cpp
  - src/game/client/components/tclient/tclient.h
  - src/game/client/components/tclient/fast_practice.cpp
  - src/game/client/gameclient.cpp
  - src/engine/shared/config_variables_qmclient.h
  - src/engine/shared/config_variables_tclient.h
commit: 6487bc60f
---

## Quick Answer

Gores 里“客户端卡”的首要嫌疑不是 `qm_gores` 本身的锤枪切换，而是 Gores 联动打开 `tc_fast_input` 后，客户端预测每帧会多模拟 1 个或更多 tick。默认 `tc_fast_input_amount = 20ms`，换算为 1 个额外 tick；如果玩家同时开启 `qm_gores_fast_input_others`，额外预测还会覆盖其他 tee，玩家多或实体多的 Gores 图里更容易表现成 CPU 长帧。

第二嫌疑是 Gores 地图进度和调试路线：普通进度会在启用后第一次构建整张地图距离场，是一次性重活；调试路线如果开启，会每帧重新构建路径点和分配临时 vector，不适合普通玩家常开。当前没有实机 perf log，所以结论是代码层根因排序，不是量化确认。

## Key Evidence

| # | Conclusion | Evidence | Location |
|---|---|---|---|
| 1 | Gores 自动启用会在收到新快照时根据游戏模式打开 `qm_gores` | `OnNewSnapshot()` 调 `ApplyGoresFastInputLink(true)`；该函数在 `AutoMapCheck && MapChanged` 时检查 `IsGoresGameMode()`，并在 `qm_gores_auto_enable` 打开时写入 `g_Config.m_QmGores`。 | `src/game/client/components/tclient/tclient.cpp:3003`, `src/game/client/components/tclient/tclient.cpp:3006`, `src/game/client/components/tclient/tclient.cpp:3777`, `src/game/client/components/tclient/tclient.cpp:3791`, `src/game/client/components/tclient/tclient.cpp:3793`, `src/game/client/components/tclient/tclient.cpp:3795` |
| 2 | Gores 联动会把 `tc_fast_input` / `tc_fast_input_others` 改成启用状态 | `ApplyGoresFastInputLink()` 用 `ApplyQmGoresLinkedConfig()` 根据 `g_Config.m_QmGores` 和 `qm_gores_fast_input*` 写 `g_Config.m_TcFastInput`、`g_Config.m_TcFastInputOthers`。 | `src/game/client/components/tclient/tclient.cpp:3808`, `src/game/client/components/tclient/tclient.cpp:3809`, `src/game/client/components/tclient/tclient.cpp:3810`, `src/game/client/components/tclient/tclient.cpp:3811`, `src/game/client/components/tclient/tclient.cpp:3814` |
| 3 | FastInput 的默认量正好是 1 个额外预测 tick | `tc_fast_input_amount` 默认是 `20`；`EffectiveFastInputOffsetTicks()` 返回 `amount / 20.0f`，`FastInputPredictionTicks()` 对它 `ceil()`。 | `src/engine/shared/config_variables_tclient.h:63`, `src/game/client/gameclient.cpp:191`, `src/game/client/gameclient.cpp:197`, `src/game/client/gameclient.cpp:200`, `src/game/client/gameclient.cpp:204` |
| 4 | 开启 FastInput 后，普通预测每帧循环终点变成 `FinalTickRegular + FastInputTicks` | `Predict()` 中 `FinalTickSelf = FinalTickRegular + FastInputTicks`，主循环从当前 tick 跑到 `FinalTickSelf`；额外 tick 内仍会 `ApplyPreInputs()`、`m_PredictedWorld.Tick()` 并处理角色/实体状态。 | `src/game/client/gameclient.cpp:4591`, `src/game/client/gameclient.cpp:4595`, `src/game/client/gameclient.cpp:4603`, `src/game/client/gameclient.cpp:4660`, `src/game/client/gameclient.cpp:4668`, `src/game/client/gameclient.cpp:4670` |
| 5 | 如果 `tc_fast_input_others` 开启，额外预测不只影响本地 tee | `FinalTickOthers` 默认等于 `FinalTickSelf`，只有 `!FastInputOthers` 才回退到常规终点；渲染平滑路径也会给其他玩家加 `FastInputTicks`。 | `src/game/client/gameclient.cpp:4592`, `src/game/client/gameclient.cpp:4596`, `src/game/client/gameclient.cpp:4597`, `src/game/client/gameclient.cpp:4598`, `src/game/client/gameclient.cpp:6525`, `src/game/client/gameclient.cpp:6526` |
| 6 | FastPractice 路径也会重复使用 FastInput 额外 tick 模拟 | `CFastPractice::ApplyVisualFastInputPrediction()` 用同一套 `amount / 20` 和 `ceil()` 计算额外 tick，并在 `FinalTickRegular + 1` 到 `FinalTickSelf` 循环里复制/推进 `VisualWorld`。 | `src/game/client/components/tclient/fast_practice.cpp:70`, `src/game/client/components/tclient/fast_practice.cpp:76`, `src/game/client/components/tclient/fast_practice.cpp:79`, `src/game/client/components/tclient/fast_practice.cpp:83`, `src/game/client/components/tclient/fast_practice.cpp:1685`, `src/game/client/components/tclient/fast_practice.cpp:1714`, `src/game/client/components/tclient/fast_practice.cpp:1789` |
| 7 | Gores 地图进度第一次启用会同步扫描整张地图并跑 Dijkstra | `UpdateGoresMapProgress()` 每 gameplay tick 调 `EnsureGoresDistanceField()`；首次构建会遍历 `MapSize`、扫描 map images/layers，并用 `priority_queue` 做距离场。 | `src/game/client/components/tclient/tclient.cpp:1803`, `src/game/client/components/tclient/tclient.cpp:1804`, `src/game/client/components/tclient/tclient.cpp:4081`, `src/game/client/components/tclient/tclient.cpp:3461`, `src/game/client/components/tclient/tclient.cpp:3462`, `src/game/client/components/tclient/tclient.cpp:3503`, `src/game/client/components/tclient/tclient.cpp:3575`, `src/game/client/components/tclient/tclient.cpp:3657` |
| 8 | Gores 调试路线如果开启，会每帧重建路径和临时数组 | `OnRender()` 每帧调用 `RenderGoresDebugRoute()`；该函数每次创建 `std::vector<vec2>`，`BuildGoresDebugRoute()` 又创建 `vVisited(MapCellCount)` 并最多沿 `MapCellCount + 64` 步找路线。 | `src/game/client/components/tclient/tclient.cpp:1813`, `src/game/client/components/tclient/tclient.cpp:1815`, `src/game/client/components/tclient/tclient.cpp:3985`, `src/game/client/components/tclient/tclient.cpp:3991`, `src/game/client/components/tclient/tclient.cpp:3992`, `src/game/client/components/tclient/tclient.cpp:3914`, `src/game/client/components/tclient/tclient.cpp:3917` |

## Details

### 为什么更像 FastInput

`qm_gores` 的锤枪自动切换本身很轻：`UpdateGoresWeaponCycle()` 只在 gameplay tick 里检查当前武器并写一次 wanted weapon。真正会持续扩大每帧 CPU 工作量的是 `tc_fast_input`：它把预测终点向后推，意味着客户端每帧多跑世界 tick、角色输入、pre-input 和实体 tick。

这个开销在 Gores 中更容易被玩家感知：Gores 图常见多人、freeze、tele、hook/枪相关预测和 dummy 玩法，预测世界里的角色和实体越多，多出来的 tick 成本越明显。如果玩家描述的是“画面还在但输入/画面一阵一阵顿”，这条路径比网络延迟更符合。

### 地图进度和调试路线

`qm_player_stats_map_progress` 默认关闭；普通用户只有开启地图进度后才会触发距离场构建。构建成功后每 tick 查询当前位置是轻量的，但首次构建在主线程做整图扫描和优先队列搜索，可能造成进图后一次明显尖峰。

`qm_player_stats_map_progress_dbg_route` 默认关闭，但如果被打开，它是更明显的帧级风险：每个 render frame 都重新分配 `vVisited(MapCellCount)` 并重建路线。这个选项应视为调试用途，不适合普通玩家常开。

## Exploration Scope

- Focused directory: `src/game/client/components/tclient/`
- Files involved: `tclient.cpp`, `tclient.h`, `fast_practice.cpp`, `gameclient.cpp`, `config_variables_qmclient.h`, `config_variables_tclient.h`
- Skipped: 没有实机复现和 `qm_perf_debug` 日志；没有量化比较官方 DDNet、关闭 FastInput、关闭地图进度、关闭调试路线四组场景。
- Tool note: 本会话没有暴露 `codegraph_*` MCP 工具；仓库存在 `.codegraph/`，但实际调查按 AGENTS.md fallback 使用 CLI。

## Confidence Notes

**confidence: medium**

- 代码证据能解释“Gores 模式中客户端卡”的持续成本来源，尤其是 FastInput 额外预测 tick。
- 还缺 perf log 证明卡顿帧实际落在 `Predict()` / `FastPractice` / `BuildGoresDistanceField()` / `BuildGoresDebugRoute()` 的哪一类。
- 当前工作区已有未提交修改，尤其 `fast_practice.cpp` 和 Gores dummy hammer 联动相关 diff；本报告没有判断这些未提交改动是否已经进入玩家反馈版本。

## Open Questions

- 反馈玩家是否开启了 `qm_gores_auto_enable`、`qm_gores_fast_input` 或 `qm_gores_fast_input_others`？
- 关闭 `qm_gores_fast_input` 后，同一 Gores 服务器是否还卡？
- 卡顿是进图/换图后的一次尖峰，还是游玩过程中持续掉帧？
- 反馈玩家是否开启了 `qm_player_stats_map_progress_dbg_route`？

## Next Steps

建议先做最小 A/B：同一 Gores 服务器分别关闭 `qm_gores_fast_input`、`qm_gores_fast_input_others`、`qm_player_stats_map_progress` 和 `qm_player_stats_map_progress_dbg_route`，同时采集 `qm_perf_debug 1`、`qm_perf_logfile 1`、`qm_perf_debug_threshold_ms 4` 的日志，用长帧归因决定是否修 FastInput 成本或地图进度构建。
