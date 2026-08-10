# HUD Notification Rule Reorg Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reorganize HUD notification message matching so upstream DDNet English server messages remain the baseline, fork-specific Chinese messages are handled as aliases, and final HUD presentation is driven by stable semantic keys instead of whichever server literal happens to be in the current repo.

**Architecture:** Introduce a semantic message-key layer between message matching and final HUD rendering. Static upstream literals and local alias literals both map into the same key catalog, while dynamic parsers emit the same keys plus extracted parameters. Classification, blacklist decisions, and final localized HUD text all read from the semantic catalog instead of reading raw server literals directly.

**Tech Stack:** C++17, DDNet/QmClient HUD notification pipeline, existing `QmHudNotifications` tests in `gtest`, Windows `qmclient_scripts/cmake-windows.cmd`, existing `hud_notification_rules.*` and `hud_notification_static_rules.h`.

---

## File Structure

- Create: `src/game/client/components/qmclient/hud_notification_catalog.h`
  - Declares semantic message key enum, static metadata, and catalog lookup helpers.
- Create: `src/game/client/components/qmclient/hud_notification_catalog.cpp`
  - Defines the canonical metadata table and key-to-text lookup functions.
- Create: `src/game/client/components/qmclient/hud_notification_static_upstream_rules.h`
  - Holds upstream DDNet English static literal to semantic key mappings only.
- Create: `src/game/client/components/qmclient/hud_notification_static_alias_rules.h`
  - Holds fork/local Chinese alias literal to semantic key mappings only.
- Modify: `src/game/client/components/qmclient/hud_notification_rules.h`
  - Adds semantic-key-bearing analysis structures and helper declarations.
- Modify: `src/game/client/components/qmclient/hud_notification_rules.cpp`
  - Refactors static lookup, dynamic parsing, blacklist routing, and localized text generation to use semantic keys.
- Delete or reduce: `src/game/client/components/qmclient/hud_notification_static_rules.h`
  - Either remove it after migration or shrink it to compatibility shims if required by staged rollout.
- Modify: `src/test/qm_hud_notifications_test.cpp`
  - Adds semantic-key assertions, upstream-vs-alias equivalence tests, and regression coverage for static/dynamic notifications.
- Modify: `CMakeLists.txt`
  - Adds any new catalog source file to the client and test build targets.
- Modify: `docs/superpowers/plans/2026-06-07-hud-notification-rule-reorg.md`
  - Append verification evidence after execution.

## Behavior Boundary

- Public behavior preserved:
  - Original upstream DDNet English server messages still classify into the same notification route/class/domain as today.
  - Existing fork Chinese server messages still classify and render into the same final HUD notifications as today.
  - `QmHudNotifications::AnalyzeServerMessage`, `TryFormatLocalizedNotificationMessage`, `ServerMessageRoute`, `ServerMessageClass`, and fallback behavior remain externally consistent.
  - Existing HUD tests for score/race/notification behavior stay green unless intentionally updated to assert the new internal semantic layer.
- Explicitly out of scope:
  - No protocol changes.
  - No service-side i18n framework.
  - No changes to server send paths or command registration.
  - No fuzzy text matching or localization by client locale on the server.
  - No UI redesign of HUD notification presentation.

## Refactor Plan

| Step | Smell/Finding | Change | Verification |
|------|---------------|--------|--------------|
| 1 | Static rules conflate source literal and final display text | Add semantic message key catalog and metadata table | `testrunner.exe --gtest_filter="*QmHudNotifications*Catalog*"` |
| 2 | Upstream baseline and local aliases are mixed in one macro table | Split static rules into upstream and alias tables that both map to keys | `testrunner.exe --gtest_filter="*QmHudNotifications*Static*"` |
| 3 | Dynamic rules emit localized text directly instead of semantic meaning | Make dynamic analyzers produce key + params, then render via catalog | `testrunner.exe --gtest_filter="*QmHudNotificationRules*Dynamic*"` |
| 4 | Blacklist/classification depend on ad hoc text checks | Route static and dynamic classification through semantic metadata | `testrunner.exe --gtest_filter="*QmHudNotifications*Blacklist*:*QmHudNotifications*Route*"` |
| 5 | Tests prove text equality but not upstream/alias equivalence | Add equivalence tests for English upstream literals and local Chinese aliases | `testrunner.exe --gtest_filter="*QmHudNotifications*:*QmHudNotificationRules*"` |
| 6 | Refactor risk spans client build wiring | Add new catalog sources to build and run full affected verification | `cmake-windows.cmd --build cmake-build-release --target testrunner -j 14`, `testrunner.exe --gtest_filter="*Score*:*QmHudNotifications*:*RaceHelper*"`, `cmake-windows.cmd --build cmake-build-release --target game-client -j 14` |

## Task 1: Introduce Semantic Message Catalog

**Files:**
- Create: `src/game/client/components/qmclient/hud_notification_catalog.h`
- Create: `src/game/client/components/qmclient/hud_notification_catalog.cpp`
- Modify: `src/game/client/components/qmclient/hud_notification_rules.h`
- Modify: `src/test/qm_hud_notifications_test.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing catalog test**

Add a new test block near the top of `src/test/qm_hud_notifications_test.cpp`:

```cpp
TEST(QmHudNotifications, CatalogProvidesCanonicalTextAndMetadata)
{
	using namespace QmHudNotifications;

	const auto *pMeta = FindMessageMetadata(EMessageKey::WhispersOn);
	ASSERT_NE(pMeta, nullptr);
	EXPECT_EQ(pMeta->m_Domain, EServerMessageDomain::Status);
	EXPECT_EQ(pMeta->m_Class, EServerMessageClass::Prompt);
	EXPECT_STREQ(CanonicalMessageText(EMessageKey::WhispersOn), "你现在会收到私聊消息");
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
E:\Coding\DDNet\QmClient\cmake-build-release\testrunner.exe --gtest_filter="*QmHudNotifications*Catalog*"
```

Expected: compile failure because `EMessageKey`, `FindMessageMetadata`, or `CanonicalMessageText` do not exist yet.

- [ ] **Step 3: Add the semantic key and metadata declarations**

Create `src/game/client/components/qmclient/hud_notification_catalog.h`:

```cpp
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_HUD_NOTIFICATION_CATALOG_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_HUD_NOTIFICATION_CATALOG_H

#include "hud_notification_rules.h"

namespace QmHudNotifications
{
	enum class EMessageKey
	{
		None,
		WhispersOn,
		WhispersOff,
		ShowAllOn,
		ShowAllOff,
		RescueDisabled,
		UnknownEmote,
		TimeoutCodeSet,
		TeamSaveInProgress,
	};

	struct SMessageMetadata
	{
		EServerMessageRoute m_Route;
		EServerMessageClass m_Class;
		EServerMessageDomain m_Domain;
		bool m_ExcludeFromNotifications;
		const char *m_pCanonicalText;
	};

	const SMessageMetadata *FindMessageMetadata(EMessageKey Key);
	const char *CanonicalMessageText(EMessageKey Key);
} // namespace QmHudNotifications

#endif
```

- [ ] **Step 4: Define the metadata table**

Create `src/game/client/components/qmclient/hud_notification_catalog.cpp`:

```cpp
#include "hud_notification_catalog.h"

namespace QmHudNotifications
{
	namespace
	{
		const SMessageMetadata s_aMessageMetadata[] = {
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Unknown, false, ""},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Status, false, "你现在会收到私聊消息"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Status, false, "你将不再收到私聊消息"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Status, false, "你现在可以看到本服所有 tee，不受距离限制"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Status, false, "你将不再看到本服所有 tee"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::SwapRescue, false, "本服务器未开启救援功能，而你所在的队伍也没有开启 /practice。注意：练习模式下无法获得排名。"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Status, false, "未知表情。输入 /emote 查看帮助"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Status, false, "你的超时保护码已设置"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Team, false, "队伍存档已在进行中"},
		};
	}

	const SMessageMetadata *FindMessageMetadata(EMessageKey Key)
	{
		const int Index = static_cast<int>(Key);
		if(Index < 0 || Index >= std::size(s_aMessageMetadata))
			return nullptr;
		return &s_aMessageMetadata[Index];
	}

	const char *CanonicalMessageText(EMessageKey Key)
	{
		const auto *pMeta = FindMessageMetadata(Key);
		return pMeta == nullptr ? "" : pMeta->m_pCanonicalText;
	}
} // namespace QmHudNotifications
```

- [ ] **Step 5: Expose the catalog in existing headers**

Modify `src/game/client/components/qmclient/hud_notification_rules.h` to include:

```cpp
namespace QmHudNotifications
{
	enum class EMessageKey;
}
```

and extend `SServerMessageAnalysis` with:

```cpp
EMessageKey m_MessageKey = EMessageKey::None;
```

Do not remove any existing fields in this step.

- [ ] **Step 6: Add the new source to builds**

Modify `CMakeLists.txt` to add:

```cmake
src/game/client/components/qmclient/hud_notification_catalog.cpp
```

to both the client build list and the test extra sources list if needed by the current test target organization.

- [ ] **Step 7: Run the catalog test to verify it passes**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
E:\Coding\DDNet\QmClient\cmake-build-release\testrunner.exe --gtest_filter="*QmHudNotifications*Catalog*"
```

Expected: the catalog test passes.

- [ ] **Step 8: Commit**

```powershell
git add CMakeLists.txt src/game/client/components/qmclient/hud_notification_catalog.h src/game/client/components/qmclient/hud_notification_catalog.cpp src/game/client/components/qmclient/hud_notification_rules.h src/test/qm_hud_notifications_test.cpp
git commit -m "refactor(hud): add notification semantic catalog"
```

## Task 2: Split Static Upstream Rules from Local Alias Rules

**Files:**
- Create: `src/game/client/components/qmclient/hud_notification_static_upstream_rules.h`
- Create: `src/game/client/components/qmclient/hud_notification_static_alias_rules.h`
- Modify: `src/game/client/components/qmclient/hud_notification_rules.cpp`
- Modify: `src/test/qm_hud_notifications_test.cpp`

- [ ] **Step 1: Write an equivalence test for upstream and alias literals**

Add to `src/test/qm_hud_notifications_test.cpp`:

```cpp
TEST(QmHudNotifications, StaticEnglishAndChineseMessagesShareTheSameSemanticKey)
{
	const auto English = QmHudNotifications::AnalyzeServerMessage("You will receive whispers", QmHudNotifications::ESoloPrompt::None);
	const auto Chinese = QmHudNotifications::AnalyzeServerMessage("你现在会收到私聊消息", QmHudNotifications::ESoloPrompt::None);

	EXPECT_EQ(English.m_MessageKey, Chinese.m_MessageKey);
	EXPECT_STREQ(English.m_aLocalizedText, Chinese.m_aLocalizedText);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
E:\Coding\DDNet\QmClient\cmake-build-release\testrunner.exe --gtest_filter="*QmHudNotifications*StaticEnglishAndChineseMessagesShareTheSameSemanticKey*"
```

Expected: fail because `m_MessageKey` is not set meaningfully yet.

- [ ] **Step 3: Create the upstream static mapping table**

Create `src/game/client/components/qmclient/hud_notification_static_upstream_rules.h`:

```cpp
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_HUD_NOTIFICATION_STATIC_UPSTREAM_RULES_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_HUD_NOTIFICATION_STATIC_UPSTREAM_RULES_H

#define QM_HUD_NOTIFICATION_STATIC_UPSTREAM_RULES(X) \
	X("You will receive whispers", WhispersOn) \
	X("You will not receive any further whispers", WhispersOff) \
	X("You will now see all tees on this server, no matter the distance", ShowAllOn) \
	X("You will no longer see all tees on this server", ShowAllOff) \
	X("Rescue is not enabled on this server and you're not in a team with /practice turned on. Note that you can't earn a rank with practice enabled.", RescueDisabled) \
	X("Unknown emote... Say /emote", UnknownEmote) \
	X("Your timeout code has been set. 0.7 clients can not reclaim their tees on timeout; however, a 0.6 client can claim your tee ", TimeoutCodeSet) \
	X("Team save already in progress", TeamSaveInProgress)

#endif
```

- [ ] **Step 4: Create the local alias mapping table**

Create `src/game/client/components/qmclient/hud_notification_static_alias_rules.h`:

```cpp
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_HUD_NOTIFICATION_STATIC_ALIAS_RULES_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_HUD_NOTIFICATION_STATIC_ALIAS_RULES_H

#define QM_HUD_NOTIFICATION_STATIC_ALIAS_RULES(X) \
	X("你现在会收到私聊消息", WhispersOn) \
	X("你将不再收到私聊消息", WhispersOff) \
	X("你现在可以看到本服所有 tee，不受距离限制", ShowAllOn) \
	X("你将不再看到本服所有 tee", ShowAllOff) \
	X("本服务器未开启救援功能，而你所在的队伍也没有开启 /practice。注意：练习模式下无法获得排名。", RescueDisabled) \
	X("未知表情。输入 /emote 查看帮助", UnknownEmote) \
	X("你的超时保护码已设置", TimeoutCodeSet) \
	X("队伍存档已在进行中", TeamSaveInProgress)

#endif
```

- [ ] **Step 5: Replace the old static macro lookup with key lookup**

In `src/game/client/components/qmclient/hud_notification_rules.cpp`, replace the current static lookup macro block with logic shaped like:

```cpp
	bool TryMatchStaticMessageKey(const char *pMessage, EMessageKey &Key, EServerMessageDomain &Domain, ESoloPrompt &SoloPrompt)
	{
		SoloPrompt = QmHudNotifications::MatchKnownSoloPrompt(pMessage);
		if(SoloPrompt == ESoloPrompt::Enter || SoloPrompt == ESoloPrompt::Leave)
		{
			Key = EMessageKey::None;
			Domain = EServerMessageDomain::Solo;
			return false;
		}

#define QM_TRY_UPSTREAM_LITERAL(pLiteral, pKey) \
		if(str_comp(pMessage, pLiteral) == 0) \
		{ \
			Key = EMessageKey::pKey; \
			Domain = FindMessageMetadata(Key)->m_Domain; \
			return true; \
		}
		QM_HUD_NOTIFICATION_STATIC_UPSTREAM_RULES(QM_TRY_UPSTREAM_LITERAL)
#undef QM_TRY_UPSTREAM_LITERAL

#define QM_TRY_ALIAS_LITERAL(pLiteral, pKey) \
		if(str_comp(pMessage, pLiteral) == 0) \
		{ \
			Key = EMessageKey::pKey; \
			Domain = FindMessageMetadata(Key)->m_Domain; \
			return true; \
		}
		QM_HUD_NOTIFICATION_STATIC_ALIAS_RULES(QM_TRY_ALIAS_LITERAL)
#undef QM_TRY_ALIAS_LITERAL

		Key = EMessageKey::None;
		return false;
	}
```

Keep the existing function names in place if needed to minimize churn.

- [ ] **Step 6: Populate `m_MessageKey` and localized text from the catalog**

When a static rule matches, set:

```cpp
Analysis.m_MessageKey = Key;
str_copy(Analysis.m_aLocalizedText, Localize(CanonicalMessageText(Key)), sizeof(Analysis.m_aLocalizedText));
```

and route/class/domain from `FindMessageMetadata(Key)`.

- [ ] **Step 7: Run the static notification tests**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
E:\Coding\DDNet\QmClient\cmake-build-release\testrunner.exe --gtest_filter="*QmHudNotifications*Static*:*QmHudNotificationRules*Whisper*:*QmHudNotificationRules*ShowAll*:*QmHudNotificationRules*RescueDisabled*:*QmHudNotificationRules*UnknownEmote*"
```

Expected: all touched static tests pass.

- [ ] **Step 8: Commit**

```powershell
git add src/game/client/components/qmclient/hud_notification_rules.cpp src/game/client/components/qmclient/hud_notification_static_upstream_rules.h src/game/client/components/qmclient/hud_notification_static_alias_rules.h src/test/qm_hud_notifications_test.cpp
git commit -m "refactor(hud): split static upstream and alias rules"
```

## Task 3: Route Dynamic Rules Through Semantic Keys

**Files:**
- Modify: `src/game/client/components/qmclient/hud_notification_rules.cpp`
- Modify: `src/game/client/components/qmclient/hud_notification_catalog.h`
- Modify: `src/game/client/components/qmclient/hud_notification_catalog.cpp`
- Modify: `src/test/qm_hud_notifications_test.cpp`

- [ ] **Step 1: Add a failing dynamic semantic-key test**

Add to `src/test/qm_hud_notifications_test.cpp`:

```cpp
TEST(QmHudNotificationRules, DynamicTeamMessageUsesSemanticRouteAndCanonicalText)
{
	const auto Analysis = QmHudNotifications::AnalyzeServerMessage("'Alpha' joined team 5", QmHudNotifications::ESoloPrompt::None);
	EXPECT_EQ(Analysis.m_Domain, QmHudNotifications::EServerMessageDomain::Team);
	EXPECT_EQ(Analysis.m_Class, QmHudNotifications::EServerMessageClass::Prompt);
	EXPECT_STREQ(Analysis.m_aLocalizedText, "'Alpha' 加入了 5 队");
	EXPECT_FALSE(Analysis.m_UseFallbackLocalization);
}
```

This is a characterization test. It should remain green after the refactor, and if a temporary regression occurs, stop and fix before moving on.

- [ ] **Step 2: Add semantic-key variants for dynamic families**

Extend `EMessageKey` with dynamic message families such as:

```cpp
TeamJoined,
TeamInviteReceived,
SwapRequestSent,
SwapRequestTimedOut,
RescueModeCurrent,
TimerPositionCurrent,
```

For these keys, the catalog text should be format templates, for example:

```cpp
"'%s' 加入了 %s 队"
"你已向 %s 发出交换请求。输入 /cancelswap 可取消"
```

- [ ] **Step 3: Introduce a formatter helper for semantic keys with placeholders**

Add to `hud_notification_catalog.cpp` a helper with a stable small parameter surface:

```cpp
void FormatMessageText(EMessageKey Key, char *pBuf, size_t BufSize, const char *pValueA = "", const char *pValueB = "", const char *pValueC = "");
```

Implement it with a `switch(Key)` and `str_format(...)`, using canonical templates from the catalog.

- [ ] **Step 4: Refactor one dynamic analyzer at a time**

In `hud_notification_rules.cpp`, convert dynamic branches from:

```cpp
str_format(Analysis.m_aLocalizedText, ..., Localize("..."), aValueA);
SetLocalizedAnalysis(..., Analysis.m_aLocalizedText);
```

to:

```cpp
Analysis.m_MessageKey = EMessageKey::SwapRequestSent;
FormatMessageText(Analysis.m_MessageKey, Analysis.m_aLocalizedText, sizeof(Analysis.m_aLocalizedText), aValueA);
const auto *pMeta = FindMessageMetadata(Analysis.m_MessageKey);
SetLocalizedAnalysis(Analysis, pMeta->m_Route, pMeta->m_Class, pMeta->m_Domain, Analysis.m_aLocalizedText);
```

Convert these dynamic groups in separate micro-steps:

- Team join / invite / lock / unlock
- Team 0 enable / disable
- Swap request / timeout / cancel / completion
- Rescue mode current / set
- Timer/race-time informational prompts

Run focused tests after each group, not only at the end.

- [ ] **Step 5: Run the dynamic tests after each converted group**

Run after every micro-step:

```powershell
E:\Coding\DDNet\QmClient\cmake-build-release\testrunner.exe --gtest_filter="*QmHudNotificationRules*Team*:*QmHudNotificationRules*Swap*:*QmHudNotificationRules*Timer*:*QmHudNotificationRules*Rescue*"
```

Expected: no regression in existing dynamic parsing tests.

- [ ] **Step 6: Commit**

```powershell
git add src/game/client/components/qmclient/hud_notification_catalog.h src/game/client/components/qmclient/hud_notification_catalog.cpp src/game/client/components/qmclient/hud_notification_rules.cpp src/test/qm_hud_notifications_test.cpp
git commit -m "refactor(hud): route dynamic notifications through semantic keys"
```

## Task 4: Rebase Blacklist and Formatting on Semantic Metadata

**Files:**
- Modify: `src/game/client/components/qmclient/hud_notification_rules.cpp`
- Modify: `src/test/qm_hud_notifications_test.cpp`

- [ ] **Step 1: Add a failing blacklist/classification equivalence test**

Add:

```cpp
TEST(QmHudNotifications, UpstreamAndAliasMessagesShareBlacklistAndRoutingBehavior)
{
	const auto English = QmHudNotifications::AnalyzeServerMessage("Unknown emote... Say /emote", QmHudNotifications::ESoloPrompt::None);
	const auto Chinese = QmHudNotifications::AnalyzeServerMessage("未知表情。输入 /emote 查看帮助", QmHudNotifications::ESoloPrompt::None);

	EXPECT_EQ(English.m_Route, Chinese.m_Route);
	EXPECT_EQ(English.m_Class, Chinese.m_Class);
	EXPECT_EQ(English.m_Domain, Chinese.m_Domain);
	EXPECT_EQ(English.m_UseFallbackLocalization, Chinese.m_UseFallbackLocalization);
}
```

- [ ] **Step 2: Run the test to verify behavior before refactoring**

Run:

```powershell
E:\Coding\DDNet\QmClient\cmake-build-release\testrunner.exe --gtest_filter="*QmHudNotifications*Blacklist*:*QmHudNotifications*RoutingBehavior*"
```

Expected: passes or exposes the exact branch that still depends on raw text.

- [ ] **Step 3: Replace raw-text classification paths with metadata lookups where possible**

In `AnalyzeServerMessage`, after any successful static or dynamic semantic match:

```cpp
const auto *pMeta = FindMessageMetadata(Analysis.m_MessageKey);
if(pMeta != nullptr)
{
	SetLocalizedAnalysis(Analysis, pMeta->m_Route, pMeta->m_Class, pMeta->m_Domain, Analysis.m_aLocalizedText, Analysis.m_SoloPrompt);
}
```

For exclusion logic, prefer:

```cpp
if(Analysis.m_MessageKey != EMessageKey::None && FindMessageMetadata(Analysis.m_MessageKey)->m_ExcludeFromNotifications)
```

before falling back to literal-only helper checks.

- [ ] **Step 4: Keep help/example suppression as a separate literal/shape path**

Do not try to force `Usage:` / `Example:` / `Bad:` / practice command lists into `EMessageKey`. Keep that path separate because it is not a stable user-facing semantic notification family.

This step is complete only when you explicitly confirm in code comments or surrounding structure that:

- semantic-key path handles stable notification messages
- help/example suppression remains literal/shape-based

- [ ] **Step 5: Run the full HUD notification suite**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
E:\Coding\DDNet\QmClient\cmake-build-release\testrunner.exe --gtest_filter="*QmHudNotifications*:*QmHudNotificationRules*"
```

Expected: all HUD notification tests pass.

- [ ] **Step 6: Commit**

```powershell
git add src/game/client/components/qmclient/hud_notification_rules.cpp src/test/qm_hud_notifications_test.cpp
git commit -m "refactor(hud): base notification routing on semantic metadata"
```

## Task 5: Remove or Minimize the Old Static Rule Layer

**Files:**
- Modify: `src/game/client/components/qmclient/hud_notification_rules.cpp`
- Modify or delete: `src/game/client/components/qmclient/hud_notification_static_rules.h`
- Modify: `CMakeLists.txt` if include dependencies change

- [ ] **Step 1: Identify the remaining dependency on the old static rule header**

Search:

```powershell
rg -n "hud_notification_static_rules|QM_HUD_NOTIFICATION_STATIC_" src/game/client/components/qmclient
```

Expected: only the new staged compatibility points remain.

- [ ] **Step 2: Replace remaining uses or convert the old header into a compatibility shim**

Choose one of these exact outcomes:

- delete `hud_notification_static_rules.h` entirely if no includes remain, or
- keep it as a thin wrapper that includes `hud_notification_static_upstream_rules.h` and `hud_notification_static_alias_rules.h`

Do not keep the old mixed macro table alive in parallel.

- [ ] **Step 3: Run compile verification**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
```

Expected: build passes without include or macro reference errors.

- [ ] **Step 4: Commit**

```powershell
git add src/game/client/components/qmclient/hud_notification_rules.cpp src/game/client/components/qmclient/hud_notification_static_rules.h src/game/client/components/qmclient/hud_notification_static_upstream_rules.h src/game/client/components/qmclient/hud_notification_static_alias_rules.h CMakeLists.txt
git commit -m "refactor(hud): remove mixed static notification rule table"
```

## Task 6: Final Verification and Evidence Capture

**Files:**
- Modify: `docs/superpowers/plans/2026-06-07-hud-notification-rule-reorg.md`

- [ ] **Step 1: Run focused affected tests**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
E:\Coding\DDNet\QmClient\cmake-build-release\testrunner.exe --gtest_filter="*Score*:*QmHudNotifications*:*RaceHelper*"
```

Expected: all affected tests pass.

- [ ] **Step 2: Run client build**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
```

Expected: `DDNet.exe` links successfully.

- [ ] **Step 3: Run documentation checks because this plan file changed**

Run:

```powershell
python qmclient_scripts/gate/check_docs.py
```

Expected: pass with no doc drift or link errors.

- [ ] **Step 4: Record evidence in this plan file**

Append to the bottom of this file:

```text
Command: <exact command>
Result: <pass/fail and key output>
Scope: <what this proves>
Gaps: <what was not verified>
```

Record one block per command from this task.

- [ ] **Step 5: Commit**

```powershell
git add docs/superpowers/plans/2026-06-07-hud-notification-rule-reorg.md
git commit -m "docs(plan): record hud notification rule reorg verification"
```

## Self-Review

- Spec coverage:
  - Upstream baseline vs local alias split: covered by Tasks 2 and 5.
  - Semantic catalog introduction: covered by Task 1.
  - Dynamic parser unification: covered by Task 3.
  - Blacklist/routing rebasing: covered by Task 4.
  - Verification and evidence capture: covered by Task 6.
- Placeholder scan:
  - Removed vague “refactor the rules” wording and replaced it with exact files, commands, and code skeletons.
  - Every task contains explicit files, commands, and expected outcomes.
- Type consistency:
  - Semantic type names are consistently `EMessageKey`, `SMessageMetadata`, `FindMessageMetadata`, `CanonicalMessageText`, and `FormatMessageText`.
  - Dynamic formatting helper name is fixed across later tasks.

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-06-07-hud-notification-rule-reorg.md`. Two execution options:

1. Subagent-Driven (recommended) - I dispatch a fresh subagent per task, review between tasks, fast iteration
2. Inline Execution - Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?

## Verification Evidence

Command: `cmd /c .\qmclient_scripts\cmake-windows.cmd --build cmake-build-release --target testrunner -j 14`
Result: pass; `testrunner.exe` rebuilt successfully after the HUD notification refactor changes.
Scope: Proves the affected C++ test target still compiles and links with the new catalog, semantic metadata path, and minimized static compatibility layer.
Gaps: Does not prove runtime behavior by itself.

Command: `cmd /c .\cmake-build-release\testrunner-task6.exe --gtest_filter="*Score*:*QmHudNotifications*:*RaceHelper*"`
Result: pass; 47 tests from 7 suites passed, including `QmHudNotifications`, `RaceHelper`, `Sql/Score`, and related score/team score cases.
Scope: Proves the affected HUD notification behavior, race helper parsing, and score-related localized outputs stayed correct after the refactor.
Gaps: Executed via a copied test binary because `cmake-build-release/testrunner.exe` was intermittently locked by another local process.

Command: `cmd /c .\qmclient_scripts\cmake-windows.cmd --build cmake-build-release --target game-client -j 14`
Result: pass; `DDNet.exe` linked successfully.
Scope: Proves the client target integrates the new HUD notification sources and still builds end-to-end.
Gaps: No interactive client launch or visual HUD inspection was performed in this turn.

Command: `python qmclient_scripts/gate/check_docs.py`
Result: pass; governance doc consistency check reported AGENTS / CLAUDE sync and no broken doc entrypoints.
Scope: Proves the modified plan/governance docs remain consistent with repo doc checks.
Gaps: This only verifies documented entry consistency, not gameplay behavior.

Command: `cmd /c .\qmclient_scripts\cmake-windows.cmd --build cmake-build-release --target testrunner -j 14`
Result: pass; `testrunner.exe` relinked successfully after the final `TimeoutCodeSet` / legacy compatibility regression-test tightening.
Scope: Proves the final post-review HUD notification test adjustments still compile and link cleanly.
Gaps: Build evidence only; behavior is covered by the focused test command below.

Command: `cmd /c .\cmake-build-release\testrunner-task6.exe --gtest_filter="*QmHudNotifications*Static*:*QmHudNotifications*LegacyStaticCompatibilityLayerExcludesMigratedSemanticStatics*"`
Result: pass; 4/4 tests passed, including `LegacyStaticCompatibilityLayerExcludesMigratedSemanticStatics`.
Scope: Proves the final static semantic / legacy compatibility regression guard passes after tightening the Chinese `TimeoutCodeSet` alias assertion.
Gaps: This is still a focused regression slice, not the entire test suite.
