---
type: question
date: 2026-06-15
status: active
confidence: high
scope:
  - src/game/client/components/qmclient/hud_notifications/
  - src/game/client/components/qmclient/menus_qmclient.cpp
  - src/game/client/components/chat.cpp
  - src/engine/shared/config_variables_qmclient.h
  - src/test/qm_hud_notifications_test.cpp
commit: ada827a1b
related:
  - file: 2026-06-14-qmclient-hud-notification-string-classification.md
    relation: complements
---

## Quick Answer

当前通知栏没有用户可编辑的“黑名单/白名单”。所谓黑名单是 `ShouldExcludeSystemNotification` 里的硬编码排除规则：空消息、帮助/用法/示例、版本/官网/规则、进出服广播，以及 catalog metadata 标记为排除的语义消息；所谓白名单是规则分析后能归类为 `Solo` 或 `System Prompt` 的消息才会入通知栏。

i18n 后续方向应该保持现有职责边界：通知栏显示的 canonical 文案进 i18n；服务器原文匹配串继续是业务匹配数据；未知服务器消息如果走 fallback 透传，不算漏翻译。UI 简化方向是保留“系统提示通知”和“Echo 通知”两个主要入口，把兼容 Solo、颜色、动画、数量、边距这类细项收进高级区域或 HUD 编辑器，不再把“blacklist”这种实现词暴露给用户。

## Key Evidence

| # | Conclusion | Evidence | Location |
|---|-----------|----------|----------|
| 1 | “黑名单”是规则函数，不是配置项 | `ShouldExcludeSystemNotification` 对空消息、帮助/用法/示例、版本/官网、规则、进出服广播和部分长说明直接 `return true` | `src/game/client/components/qmclient/hud_notifications/hud_notification_rules.cpp:986` |
| 2 | “白名单”是路由结果，不是列表 | `AnalyzeServerMessage` 先识别 Solo、semantic/static/dynamic 消息，命中后设置 `EServerMessageRoute::Solo/System`；没有显式 whitelist 数据结构 | `src/game/client/components/qmclient/hud_notifications/hud_notification_rules.cpp:1036` |
| 3 | 入队由路由结果和设置共同决定 | `DecideServerMessageEntry` 只有在 `RouteSystemMessages` 开启且 route 为 Solo/System 时才 `m_QueueNotification = true` | `src/game/client/components/qmclient/hud_notifications/hud_notification_rules.cpp:1093` |
| 4 | 聊天入口会把被通知栏接管的服务器消息从聊天主显示中拦截，但仍打印到 console | server chat 分支调用 `HandleServerChat(..., g_Config.m_QmHudNotificationsSystem != 0, ...)`，成功后 `PrintSuppressedServerMessage()` 并返回 | `src/game/client/components/chat.cpp:1448` |
| 5 | Echo 通知是独立入口，受专注模式和通知栏 Echo 开关共同影响 | `CChat::Echo` 先检查 `QmFocusModeHideEcho`，再调用 `QueueEcho`；`QueueEcho` 内部再检查 `QmHudNotificationsEcho` | `src/game/client/components/chat.cpp:955`, `src/game/client/components/qmclient/hud_notifications/hud_notifications.cpp:235` |
| 6 | 目前 UI 暴露的通知栏选项偏多，且有实现词 | 菜单直接展示系统提示、Echo、兼容 Solo、背景色、系统提示文字色、Echo 继承色、Echo 覆盖色、文字大小、停留时间、动画、动画时长、最大数量、边距 | `src/game/client/components/qmclient/menus_qmclient.cpp:6031` |
| 7 | 默认配置已经偏向“系统提示接管开、Echo 接管关、兼容 Solo 关” | `qm_hud_notifications_system=1`，`qm_hud_notifications_echo=0`，`qm_hud_notifications_compat_solo=0` | `src/engine/shared/config_variables_qmclient.h:155` |
| 8 | 测试覆盖了黑名单、白名单、fallback 和 canonical/兼容串边界 | 测试断言版本/官网/进出服不会进通知，短系统反馈不会被黑名单排除，中文兼容串可归一为英文 canonical 文案 | `src/test/qm_hud_notifications_test.cpp:102` |

## Details

### 当前黑名单

硬编码排除项主要有四组：

- 空消息和空指针。
- 帮助/用法/示例类前缀：`Usage:`、`用法：`、`Example:`、`示例：`、`Available practice commands:`、`可用练习命令：` 等。
- 基础信息和噪声：DDNet 版本、Git hash、官网、更多命令、规则、入服、离服。
- 已能识别成 semantic message 但 metadata 标记为排除的消息。

这些内容更像“不要弹 toast 的服务器信息过滤规则”，不是用户语言意义上的黑名单。

### 当前白名单

没有显式 whitelist 表。能进入通知栏的路径是：

- Solo 提示：`You are now in a solo part` / `你现在处于单人区域` 等。
- 语义 catalog：`EMessageKey` / `EDynamicMessageKey` 对应的 canonical 文案。
- 静态兼容规则：team、swap/rescue、vote moderation、status 等上游消息映射。
- 动态解析：队伍加入、swap 请求、投票管理、状态提示等。
- fallback：未知服务器消息在系统提示接管开启时也可以进通知栏，并用原文透传。

因此后续如果要“白名单化”，要先决定是否取消 fallback。取消 fallback 会明显改变现有行为：部分未知服务器提示不再弹通知。

### i18n 边界

后续适配 i18n 时建议保持三类文本：

- canonical 通知文案：客户端最终展示的规范英文文本，必须进 i18n。
- 匹配字面量：英文/中文服务器原文，只用于识别消息，继续是业务数据。
- fallback 透传文本：服务器原文，客户端不承诺翻译，不进主 i18n 链。

这个边界与 `2026-06-14-qmclient-hud-notification-string-classification.md` 一致。

### UI 简化方向

建议把通知栏 UI 分成“常用”和“高级”：

- 常用保留：`系统提示通知`、`Echo 通知`、`停留时间`、`文字大小`。
- 高级收起：兼容其他服务器 Solo 提示、系统提示文字色、Echo 颜色策略、动画类型、动画时长、最大显示数量、边距。
- 颜色和位置类设置尽量迁移到 HUD 编辑器语义里理解：位置靠 HUD 编辑器拖拽，边距不应作为普通用户第一屏配置。
- 文案避免出现 `blacklist`，改成用户能理解的“忽略入场、版本、帮助等基础信息”。

## Exploration Scope

- Focused directory: `src/game/client/components/qmclient/hud_notifications/`
- Files involved: `hud_notification_rules.cpp`, `hud_notification_catalog.cpp`, `hud_notifications.cpp`, `hud_notifications.h`, `menus_qmclient.cpp`, `chat.cpp`, `config_variables_qmclient.h`, `qm_hud_notifications_test.cpp`
- Skipped: 视觉原型和当前未提交菜单改动的设计意图没有展开，只读取了现有代码事实。

## Confidence Notes

**confidence: high**

- 已覆盖规则分析、聊天入口、通知入队、菜单配置、默认配置和现有测试。
- 没有修改运行时代码；本文只记录当前行为和后续方向。

## Open Questions

- 是否保留 fallback 透传进入通知栏？保留更兼容，取消更可控。
- UI 高级区域使用折叠块、二级页面，还是复用当前模块布局里的“高级设置”模式，需要结合当前菜单重构状态决定。
- 是否要把 `ShouldExcludeSystemNotification` 的硬编码排除项拆成命名规则表，便于后续审计和 i18n 分类脚本识别？

## Related Documents

- `2026-06-14-qmclient-hud-notification-string-classification.md` — 已归档通知栏字符串职责分类。

## Next Steps

后续实现可以按两步走：先只改菜单文案和布局，不动通知路由；再单独做规则表命名化和 i18n 审计增强。
