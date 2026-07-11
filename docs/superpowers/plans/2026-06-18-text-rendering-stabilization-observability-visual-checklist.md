---
title: Text rendering stabilization visual checklist
date: 2026-06-18
last_reviewed: 2026-07-11
status: active
scope: 当前 UI frame pacing fresh runtime 计划的配套视觉验收
---

# Text Rendering Stabilization Visual Checklist

Use with the [UI frame pacing runtime plan](2026-06-18-ui-frame-pacing-full-performance-plan.md) and any later UI text-cache change.

本文与当前 runtime 计划同步收口。runtime gap 关闭后，若这些标准仍需作为长期合同，应在更新读取该路径的 source-contract 测试时一并迁入稳定 spec/workflow。

## Required Screens

- Settings: General tab, TClient tab, QmClient tab.
- Ingame Esc menu.
- Assets/resource page including small cards, right-side tags/buttons, map/video fallback cards.
- Server browser list.
- Chat translate button and popup labels.

## Required States

- UI scale 100%.
- One non-default UI scale.
- Normal DPI and HiDPI if available.
- English and Simplified Chinese language.
- First open after cache invalidation.
- Scroll active.
- Post-scroll settled.
- Hover/pressed/selected button state.

## Pass Criteria

- Button text is centered inside button rect.
- Placeholder text such as "TODO" is centered inside its card.
- Small-card right tags/buttons keep fixed priority; left ID/title text shrinks first.
- No visible blank text on first frame unless explicitly documented as deferred dynamic content.
- No visible stale text after language/font/UI-scale change.
- Perf report says target stable text coverage is OK only when render-ready hit coverage is complete.
