# HUD Notifications and Chat Interactions Design

> **文档已过时** — 本文档内容不再反映当前代码状态，仅供参考。

## Goal

Add a right-side HUD notification system for selected system information, then plan a separate second phase for chat and F1 console interaction improvements.

The first phase moves supported system messages, such as solo tile enter and leave prompts, out of the left chat area and into a configurable HUD toast. The original server output must remain visible in the F1 console log.

## Scope

Phase 1 includes:

- Right-side HUD notifications for solo enter and solo leave events.
- Optional Echo-to-HUD notification routing, disabled by default.
- HUD editor support for moving the notification module.
- Qimeng settings for notification sources, style, timing, animation, and compatibility behavior.
- Simplified Chinese localization for fixed system notification text.

Phase 2 is planned separately:

- Chat history scrolling and scrollbar.
- Chat message click or selection copy.
- F1 console scrollbar while preserving existing selection, link, and export behavior.
- Reference review of `docs/dyl/BestClient` chat and console behavior.

Phase 2 must not be implemented as part of Phase 1.

## Message Routing

Phase 1 uses message migration, not duplicate display. When a supported message is routed to the HUD notification queue, the left chat area must not also show that same message.

The F1 console log must keep the original server or client output for debugging and traceability.

Routing rules:

- Normal player chat stays in the left chat area.
- Solo enter and leave HUD notifications are enabled by a dedicated setting.
- Echo HUD notifications are controlled by a separate setting and are disabled by default.
- Echo text is shown as provided and is not localized.

## Solo Notification Triggering

Right-side solo notifications must not depend only on server text. Different servers may send different text for the same solo tile behavior.

The stable trigger is local character solo state transition:

- `false -> true`: queue `Localize("You are now in a solo part")`.
- `true -> false`: queue `Localize("You are now out of the solo part")`.

Left-chat suppression is more conservative:

- By default, suppress only known official or QmClient solo system messages.
- Known messages include the English DDNet text and the current QmClient Chinese server messages.
- Add an advanced compatibility option for custom server solo prompts.

When custom-server compatibility is enabled, a short suppression window opens after a local solo state transition. During that window, the next server system chat line can be treated as the corresponding solo prompt and skipped in the left chat area. This option is intentionally separate because broad text suppression can hide unrelated server output.

## HUD Notification Display

The notification module is a HUD element with a default position on the right side of the screen, slightly below center, matching the Cactus-style reference behavior.

Display behavior:

- Maximum visible notifications defaults to 3.
- New notifications enter from below.
- Older visible notifications move upward.
- If the queue exceeds the configured limit, the oldest visible notification is removed first.
- HUD editor preview shows a fixed sample notification without adding it to the real queue.

Base visual style:

- Rounded dark translucent background.
- Configurable text color.
- Configurable background color including alpha.
- Configurable text size.
- Text is width-limited and wraps cleanly.

Animation options:

- Fade and slide.
- Fade only.
- No animation.

The implementation should reuse existing HUD animation style where practical and keep the notification module as one queue with one position. Phase 1 does not include per-notification-type positions, custom font families, border editing, or notification click actions.

## HUD Editor

Add a notification element to the existing HUD editor layout system. The module must be draggable and persist through the current HUD editor layout configuration mechanism.

The editor preview should use the localized sample text `Localize("You are now in a solo part")`, but it must not enqueue a runtime notification.

## Settings

Add settings in the Qimeng settings UI, in the HUD or chat-related area:

- Enable solo HUD notifications.
- Enable Echo HUD notifications, default off.
- Background color with alpha.
- Text color.
- Text size.
- Notification hold duration.
- Animation type: fade and slide, fade only, none.
- Animation speed.
- Maximum visible notifications, default 3.
- Compatibility mode for custom server solo prompts, default off.

Keep the first version focused on these settings. Do not add a full notification theme editor.

## Localization

Only fixed system notification text is localized.

Add Simplified Chinese translations to `data/languages/simplified_chinese.txt` for:

- `You are now in a solo part`
- `You are now out of the solo part`

Echo messages and custom server text are user or server content and must be displayed as-is.

## Phase 2 Reference

Use `docs/dyl/BestClient` as a reference for chat and console interaction design, not as a direct source to copy.

Useful reference areas:

- Chat history scroll state like `m_BacklogCurLine`.
- Chat scrollbar dragging behavior.
- Chat text selection and clipboard copy behavior.
- F1 console scrollbar dragging.
- Preservation of console selection, link clicking, and export mode behavior.

QmClient already has its own chat translation UI, console export work, and HUD editor structure. Phase 2 must merge the behavior into the current QmClient architecture instead of replacing it with BestClient structure.

## Success Criteria

Phase 1 is successful when:

- Entering a solo tile queues a right-side localized HUD notification.
- Leaving a solo tile queues a right-side localized HUD notification.
- Supported migrated solo prompts do not duplicate in the left chat area.
- Original system output remains available in the F1 console log.
- Echo HUD notifications are disabled by default.
- When Echo HUD notifications are enabled, Echo text appears on the right HUD and not in the left chat area.
- The HUD editor can move the notification module.
- Qimeng settings change background color, text size, timing, and animation behavior.
- Simplified Chinese localization works for fixed solo notification text.
- Phase 2 remains only a plan/reference section and does not change chat or console interaction code.

## Non-Goals

- Do not route normal player chat to HUD notifications.
- Do not implement notification click-to-copy in Phase 1.
- Do not add per-source notification positions in Phase 1.
- Do not localize Echo or custom server content.
- Do not implement chat scrolling, chat copy, or F1 console scrollbar in Phase 1.
