---
type: question
date: 2026-06-14
status: active
confidence: high
scope:
  - src/game/client/components/qmclient/hud_notifications/
  - src/test/qm_hud_notifications_test.cpp
commit: cecb13e74
related:
  - file: 2026-06-14-qmclient-i18n-src-status-audit.md
    relation: complements
  - file: 2026-06-14-qmclient-i18n-classification-and-audit-design.md
    relation: implements
---

## Quick Answer

通知栏的字符串不能按“语言”分，只能按“职责”分。

当前实现已经很清楚地分成三类：

1. **canonical 通知文案**：客户端自己定义的规范英文文案，属于 `must_i18n`
2. **服务器消息匹配字面量**：为了识别英文/中文/历史上游消息而保留的兼容串，属于 `business_data`
3. **无法归一的透传文本**：外部消息原样显示，不算客户端漏翻译，也不进入 active i18n 主链

换句话说，通知栏不是“所有字符串都应该被翻译”。真正应该进 i18n 的，是客户端自己产出的规范通知文案；真正不该误报的，是规则层里的兼容匹配串。

## Key Evidence

| # | Conclusion | Evidence | Location |
|---|-----------|----------|----------|
| 1 | 通知栏展示层只消费最终文本，不负责决定某条字符串是不是翻译项 | `BuildVisibleNotificationList` 只组装可见通知；`QueueEcho` 也是把已有文本入队；预览内容直接使用 `Localize("You are now in a solo part")` | `src/game/client/components/qmclient/hud_notifications/hud_notifications.cpp:29`, `src/game/client/components/qmclient/hud_notifications/hud_notifications.cpp:35`, `src/game/client/components/qmclient/hud_notifications/hud_notifications.cpp:235` |
| 2 | canonical 通知文案本体是英文规范文本，属于客户端自有文案 | `s_aMessageMetadata` / `s_aDynamicMessageMetadata` 中维护规范英文文案，如 `You will receive whispers`、`Team save already in progress` | `src/game/client/components/qmclient/hud_notifications/hud_notification_catalog.cpp:9-24` |
| 3 | 规则层先把外部消息归一，再把结果落到本地化文本 | `TryCopyStaticLocalizedNotification`、`AnalyzeTeamMessage`、`FormatDynamicLocalizedText` 和 `SetLocalizedAnalysis` 共同负责识别消息、生成 canonical 文案并标记为可本地化；`SetFallbackAnalysis` 则保留回退路径 | `src/game/client/components/qmclient/hud_notifications/hud_notification_rules.cpp:12`, `src/game/client/components/qmclient/hud_notifications/hud_notification_rules.cpp:23`, `src/game/client/components/qmclient/hud_notifications/hud_notification_rules.cpp:57`, `src/game/client/components/qmclient/hud_notifications/hud_notification_rules.cpp:142`, `src/game/client/components/qmclient/hud_notifications/hud_notification_rules.cpp:206` |
| 4 | 通知栏规则显式兼容中英文上游消息，这些字面量是匹配数据，不是翻译源 | 例如 `You are now in a solo part` 与 `你现在处于单人区域` 在规则层被并列匹配；它们的职责是识别消息语义，而不是成为运行时语言文件的 source key | `src/game/client/components/qmclient/hud_notifications/hud_notification_rules.cpp:971-973` |
| 5 | 测试已经把“canonical 文案”和“兼容匹配串”区别开了 | 测试同时断言英文/中文消息能被识别、被归一化到同一个结果；同时也断言部分 legacy compatibility literal 不应再被当作静态翻译项 | `src/test/qm_hud_notifications_test.cpp:74-77`, `src/test/qm_hud_notifications_test.cpp:165-213`, `src/test/qm_hud_notifications_test.cpp:306-326` |

## Classification

### 1. `must_i18n`

这些字符串属于客户端自有、直接给玩家看的规范通知文案：

- catalog 里的 canonical 英文文案
- 规则层归一化后的目标文本
- 预览通知文案

样例：

- `You will receive whispers`
- `Team save already in progress`
- `You are now in a solo part`
- `Timer isn't displayed.`

对应证据：

- `src/game/client/components/qmclient/hud_notifications/hud_notification_catalog.cpp:11`
- `src/game/client/components/qmclient/hud_notifications/hud_notification_catalog.cpp:18`
- `src/game/client/components/qmclient/hud_notifications/hud_notification_rules.cpp:1045`
- `src/game/client/components/qmclient/hud_notifications/hud_notification_rules.cpp:848`

### 2. `business_data`

这些字符串是规则层用来识别服务器消息的兼容字面量，不是翻译维护源：

- static rule 里的 alias/canonical 对
- `str_comp` / `str_startswith` / `ExtractWrappedValue` 等匹配串
- 中文兼容消息样本
- 英文历史消息样本

样例：

- `你正在发起投票，请等当前投票结束后再试`
- `未知救援模式参数`
- `你现在处于单人区域`
- `计时器显示在 `

对应证据：

- `src/game/client/components/qmclient/hud_notifications/hud_notification_static_rules.h:66`
- `src/game/client/components/qmclient/hud_notifications/hud_notification_static_rules.h:85`
- `src/game/client/components/qmclient/hud_notifications/hud_notification_rules.cpp:839-848`
- `src/game/client/components/qmclient/hud_notifications/hud_notification_rules.cpp:971-973`

### 3. 外部透传文本

这类文本来自服务器输入，本客户端不保证一定能归一；如果没命中规则，就走回退逻辑：

- 不算 active i18n 缺口
- 不进入 `translations/i18n/*.toml`
- 只有在后续确认它代表稳定通知语义时，才值得升格为 canonical 文案

对应证据：

- `src/game/client/components/qmclient/hud_notifications/hud_notification_rules.cpp:1086-1089`

## Open Questions

- `hud_notification_rules.cpp` 中部分状态/帮助前缀仍然是大段消息匹配字面量；目前应继续视为 `business_data`，但如果未来要进一步收敛，最好按“消息语义族”继续拆成更明确的规则表。
- 本文只覆盖通知栏，不覆盖聊天翻译、资源别名表、菜单预览文案等其他混合区域。
