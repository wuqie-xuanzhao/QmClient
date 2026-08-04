---
title: Superpowers 活动文档入口
date: 2026-08-03
status: active
---

# 目录职责

- plans/：正在执行或仍有明确验收 gap 的计划。
- specs/：当前稳定行为规格和待办索引。
- explore/：只允许当前调查临时存在；结论进入 plan/spec 后立即删除。
- reports/：只保留已归档证据，活动入口不依赖报告目录。
- reviews/：只保留已归档审查记录，不作为当前实现依据。

活动文档只允许 `status: active` 或 `status: draft`。完成、过时、已取代的文档移入对应 `archive/`，并更新活动入口；提交与 Git 历史承担完成记录。

# 当前计划

- [UI 文本视觉验收清单](plans/2026-06-18-text-rendering-stabilization-observability-visual-checklist.md)：当前 runtime 计划的配套检查。
- [UI fresh runtime 性能验收](plans/2026-06-18-ui-frame-pacing-full-performance-plan.md)：只剩 fresh runtime 证据。
- [QmLive Phase 2-4 剩余工作](plans/2026-06-28-qmlive-match-live-plan.md)：后续客户端与服务端阶段。
- [背景粒子、镜头和歌词收口](plans/2026-07-11-background-camera-lyrics-module-refactor.md)：当前并行实现。
- [过图历史工作区与响应式卡片改造](plans/2026-07-11-map-history-workspace-redesign.md)：当前并行实现。

# 当前规格

- [macOS Metal 原生渲染后端规格](specs/2026-08-04-QmClient-macOS-Metal原生渲染后端规格.md)：Metal 后端的架构、平台约束、实施阶段与验证合同。
- [设置页 UI 统一与滚动体系规格](specs/2026-07-10-QmClient-设置页UI统一与滚动体系规格.md)：设置页公共组件与滚动体系的权威规格。
- [设置页 UI/UX 现状审查与防回归设计](specs/2026-07-21-QmClient-设置页UIUX现状审查与防回归设计.md)：最新用户决定与防回归约束。
- [当前 master 未完成需求索引](specs/2026-06-20-待办整合规格.html)：未完成需求的唯一总索引。
- [长期性能优化路线图 v3](specs/2026-06-20-长期性能优化路线图-v3.html)：性能优化长期路线与阶段边界。

# 生命周期

1. 新任务先核对是否已有匹配活动文档。
2. 探索只记录当前未知问题；一旦形成决策，结论迁入单一 plan/spec。
3. 实现完成且验证闭环后，将已消费的 plan/explore/report/review 移入对应 `archive/`，并同步活动入口。
4. 稳定用户行为仍需长期维护时保留精简 spec；过程、提交号和会话日志不写入 spec。
5. 入口列表必须与活动目录同步，不能列出不存在、已归档或已完成的文件；活动文档之间使用 Markdown 链接。
