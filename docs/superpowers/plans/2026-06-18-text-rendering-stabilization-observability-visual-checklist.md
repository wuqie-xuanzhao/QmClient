# Text Rendering Stabilization Visual Checklist

Use after implementing `docs/superpowers/plans/archive/2026-06-18-text-rendering-stabilization-observability.md`.

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
