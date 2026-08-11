---
title: 过图历史工作区与响应式卡片改造计划
date: 2026-07-11
status: active
scope: 收藏地图页面的信息架构与过图历史展示；不改历史数据格式、记录语义、协议、物理或预测
---

# 目标与行为边界

- 将“收藏地图 / 过图历史 / 本地存档”从三个默认展开且等分高度的面板改为互斥页签工作区，让当前内容使用完整可用高度。
- 过图历史从固定六列表格改为响应式卡片网格：常规宽度显示三列，中等宽度显示两列，窄宽度回退一列。
- 每张卡片以地图名和完成状态为主信息，保留死亡次数、游玩或通关用时、最近进入时间和单条删除。
- 筛选、清理已完成记录、清理全部记录与二次确认语义保持不变；窄宽度下控制区自动分为两行。
- 收藏地图、本地存档、历史记录模型和 `qm_auto_save_history_count` 配置语义保持兼容。
- 不新增持久化状态，不修改 `map_history.json` 格式，不改变排序、淘汰或自动记录逻辑。

# 成功标准

1. 历史页签占满工作区，不再因另外两个面板默认展开而只剩一行列表高度。
2. 卡片列数只由可用逻辑宽度决定，并稳定限制在 1 到 3 列；地图名和时间文本不会覆盖删除按钮或相邻字段。
3. 空状态、滚动、筛选、批量清理二次确认和单条删除在改造后仍可用。
4. 正常 UI 比例和至少一个非默认比例下完成实机视觉检查。

# 实施切片

1. 添加可单测的响应式布局 helper，先覆盖 1/2/3 列边界与窄屏控制区换行边界。
2. 将三块内容改为页签工作区，并保留各自现有空状态和列表行为。
3. 用多列 `CListBox` 渲染历史卡片，重新组织信息层级、截断和删除按钮命中区域。
4. 完成聚焦测试、全量 C++ 测试、客户端构建、quick gate、实机视觉检查与独立只读审查。

# 验证记录

```text
Command: cmake-build-release/testrunner.exe --gtest_filter='MapHistoryUi.*:QmNewUiMenuBranches.MapHistoryUsesFullHeightTabbedResponsiveCardGrid'
Result: pass，3/3 测试通过。
Scope: 覆盖 1/2/3 列边界、窄屏控制区换行边界和页面使用页签卡片工作区的结构约束。
Gaps: 过滤测试不代替全量测试。

Command: cmake-build-release/testrunner.exe
Result: fail，1697 项中 1693 项通过；旧的浏览器背景结构断言已随新工作区更新，另外 3 项失败来自并行的歌词默认值、歌词测试路径和 UI scale 文本审计改动。
Scope: 证明除列出的并行失败外，现有测试程序中的全量 C++ 用例（含本轮新增测试与既有 map history 数据测试）均通过。
Gaps: 标准 run_cxx_tests 入口受并行新增的 qm_hammer_hit_detection_test.cpp 错误 include 阻断；按用户要求不再做编译验证，未重编译更新后的背景结构断言。

Command: PowerShell source-layout assertions for RenderServerbrowserFavoriteMaps
Result: pass，页签、响应式 helper、固定卡片高度、卡片字段结构和新工作区透明度断言均存在，旧 SplitHistoryPanel / SplitHistoryColumns 均不存在。
Scope: 验证最终源码与更新后的结构测试意图一致。
Gaps: 源码断言不代替运行时视觉检查。

Command: python qmclient_scripts/gate/check_docs.py
Result: pass，治理文档入口、活动状态、索引和本地引用均通过。
Scope: 覆盖新增活动计划文档的治理一致性。
Gaps: 无。

Command: independent read-only review using docs/ai-workflow/review.md
Result: pass，总体结论“正确”，无 findings。
Scope: 复核布局几何、滚动条预留、数据修改时机、事件区域和测试覆盖。
Gaps: 按用户最新要求未继续构建、gate 或实机视觉验证。
```
