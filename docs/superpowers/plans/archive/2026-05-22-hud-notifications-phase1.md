# HUD 通知第一阶段实现计划

> **文档已过时** — 本文档内容不再反映当前代码状态，仅供参考。

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 实现右侧 HUD 通知系统，把 solo 进入/离开提示和可选 Echo 消息迁移到 HUD toast，并保留 F1 控制台日志。

**架构：** 新增 `CQmHudNotifications` 客户端组件，负责通知队列、solo 状态变化检测、左侧聊天抑制判断和 HUD 渲染。`CChat` 在添加左侧聊天行前询问该组件是否迁移消息；`CHudEditor` 把通知作为一个可拖动 HUD 模块。

**技术栈：** C++17、DDNet/QmClient `CComponent`、现有 config 宏、现有 HUD 编辑器布局、`Localize(...)`、现有 C++ gtest/testrunner。

---

## 文件结构

- 创建：`src/game/client/components/qmclient/hud_notifications.h`
  - 定义 `CQmHudNotifications`、通知类型、动画类型、已知 solo 文案匹配 helper。
- 创建：`src/game/client/components/qmclient/hud_notifications.cpp`
  - 实现队列、solo 状态检测、聊天抑制、Echo 迁移、HUD 渲染。
- 修改：`CMakeLists.txt`
  - 把新组件和新测试加入构建。
- 修改：`src/game/client/gameclient.h`
  - 增加 `CQmHudNotifications m_QmHudNotifications` 成员。
- 修改：`src/game/client/gameclient.cpp`
  - 注册新组件，保证它能接收 snapshot/render，并在聊天之前可被调用。
- 修改：`src/game/client/components/chat.cpp`
  - 在 `NETMSGTYPE_SV_CHAT` 的 `AddLine(...)` 前做 HUD 通知迁移判断。
  - 在 `Echo(...)` 中做 Echo-to-HUD 迁移。
- 修改：`src/game/client/components/hud_editor.h`
  - 增加 `EHudEditorElement::HudNotifications`。
- 修改：`src/game/client/components/hud_editor.cpp`
  - 增加 token `hud_notifications`，使位置可保存/恢复。
- 修改：`src/engine/shared/config_variables_qmclient.h`
  - 增加 HUD 通知相关配置变量。
- 修改：`src/game/client/components/qmclient/menus_qmclient.cpp`
  - 在栖梦 HUD/聊天设置区暴露开关、颜色、文本大小、动画、停留时间、最大条数、兼容模式。
- 修改：`data/languages/simplified_chinese.txt`
  - 增加固定系统提示中文翻译。
- 创建：`src/test/qm_hud_notifications_test.cpp`
  - 覆盖已知 solo 文案匹配、动画参数 clamp、队列上限 helper。

## 任务 1：新增配置变量和纯 helper 测试

**文件：**
- 修改：`src/engine/shared/config_variables_qmclient.h`
- 创建：`src/game/client/components/qmclient/hud_notifications.h`
- 创建：`src/game/client/components/qmclient/hud_notifications.cpp`
- 创建：`src/test/qm_hud_notifications_test.cpp`
- 修改：`CMakeLists.txt`

- [ ] **步骤 1：添加失败测试**

在 `src/test/qm_hud_notifications_test.cpp` 增加：

```cpp
#include <gtest/gtest.h>

#include <game/client/components/qmclient/hud_notifications.h>

TEST(QmHudNotifications, MatchesKnownSoloPrompts)
{
	EXPECT_EQ(QmHudNotifications::MatchKnownSoloPrompt("You are now in a solo part"), QmHudNotifications::ESoloPrompt::Enter);
	EXPECT_EQ(QmHudNotifications::MatchKnownSoloPrompt("You are now out of the solo part"), QmHudNotifications::ESoloPrompt::Leave);
	EXPECT_EQ(QmHudNotifications::MatchKnownSoloPrompt("你现在处于单人区域"), QmHudNotifications::ESoloPrompt::Enter);
	EXPECT_EQ(QmHudNotifications::MatchKnownSoloPrompt("你现在已离开单人区域"), QmHudNotifications::ESoloPrompt::Leave);
	EXPECT_EQ(QmHudNotifications::MatchKnownSoloPrompt("regular server message"), QmHudNotifications::ESoloPrompt::None);
}

TEST(QmHudNotifications, ClampsVisibleCount)
{
	EXPECT_EQ(QmHudNotifications::ClampVisibleCount(-1), 1);
	EXPECT_EQ(QmHudNotifications::ClampVisibleCount(0), 1);
	EXPECT_EQ(QmHudNotifications::ClampVisibleCount(3), 3);
	EXPECT_EQ(QmHudNotifications::ClampVisibleCount(20), 8);
}

TEST(QmHudNotifications, ClampsTiming)
{
	EXPECT_EQ(QmHudNotifications::ClampHoldMs(200), 500);
	EXPECT_EQ(QmHudNotifications::ClampHoldMs(2500), 2500);
	EXPECT_EQ(QmHudNotifications::ClampHoldMs(30000), 10000);
	EXPECT_EQ(QmHudNotifications::ClampAnimationMs(0), 0);
	EXPECT_EQ(QmHudNotifications::ClampAnimationMs(9000), 2000);
}
```

- [ ] **步骤 2：运行测试验证失败**

运行：

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 10
```

预期：编译失败，报 `hud_notifications.h` 或 `QmHudNotifications` 未定义。

- [ ] **步骤 3：添加配置变量**

在 `src/engine/shared/config_variables_qmclient.h` 的 HUD/Input overlay 附近加入：

```cpp
// HUD Notifications / HUD 通知
MACRO_CONFIG_INT(QmHudNotificationsSolo, qm_hud_notifications_solo, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "启用 solo 右侧 HUD 通知")
MACRO_CONFIG_INT(QmHudNotificationsEcho, qm_hud_notifications_echo, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "把 echo 消息显示为右侧 HUD 通知")
MACRO_CONFIG_INT(QmHudNotificationsCompatSolo, qm_hud_notifications_compat_solo, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "兼容自定义服务器 solo 提示")
MACRO_CONFIG_COL(QmHudNotificationsBgColor, qm_hud_notifications_bg_color, 0x99000000, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "HUD 通知背景颜色")
MACRO_CONFIG_COL(QmHudNotificationsTextColor, qm_hud_notifications_text_color, 0xFFFFFFFF, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "HUD 通知文字颜色")
MACRO_CONFIG_INT(QmHudNotificationsTextSize, qm_hud_notifications_text_size, 14, 8, 32, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HUD 通知文字大小")
MACRO_CONFIG_INT(QmHudNotificationsHoldMs, qm_hud_notifications_hold_ms, 2500, 500, 10000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HUD 通知停留时间（毫秒）")
MACRO_CONFIG_INT(QmHudNotificationsAnimType, qm_hud_notifications_anim_type, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HUD 通知动画类型（0=淡入滑入 1=仅淡入 2=无动画）")
MACRO_CONFIG_INT(QmHudNotificationsAnimMs, qm_hud_notifications_anim_ms, 220, 0, 2000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HUD 通知动画时间（毫秒）")
MACRO_CONFIG_INT(QmHudNotificationsMaxVisible, qm_hud_notifications_max_visible, 3, 1, 8, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HUD 通知最大显示数量")
```

- [ ] **步骤 4：实现纯 helper**

创建 `src/game/client/components/qmclient/hud_notifications.h`：

```cpp
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_HUD_NOTIFICATIONS_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_HUD_NOTIFICATIONS_H

#include <game/client/component.h>
#include <game/client/ui_rect.h>

#include <deque>

namespace QmHudNotifications
{
enum class ESoloPrompt
{
	None,
	Enter,
	Leave,
};

ESoloPrompt MatchKnownSoloPrompt(const char *pMessage);
int ClampVisibleCount(int Value);
int ClampHoldMs(int Value);
int ClampAnimationMs(int Value);
} // namespace QmHudNotifications

class CQmHudNotifications : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnRelease() override;
	void OnNewSnapshot() override;
	void OnRender() override;

	bool QueueEcho(const char *pMessage);
	bool ShouldSuppressServerChat(const char *pMessage);

private:
	enum class EKind
	{
		SoloEnter,
		SoloLeave,
		Echo,
	};

	struct SNotification
	{
		EKind m_Kind = EKind::Echo;
		char m_aText[256] = {};
		int64_t m_StartTime = 0;
	};

	std::deque<SNotification> m_vNotifications;
	bool m_HasLastSolo = false;
	bool m_LastSolo = false;
	QmHudNotifications::ESoloPrompt m_PendingCompatPrompt = QmHudNotifications::ESoloPrompt::None;
	int64_t m_PendingCompatUntil = 0;

	void Queue(EKind Kind, const char *pText);
	void QueueSolo(QmHudNotifications::ESoloPrompt Prompt);
	bool LocalSoloState(bool &Solo) const;
	void RenderNotifications(const CUIRect &BaseRect, bool Preview);
};

#endif
```

创建 `src/game/client/components/qmclient/hud_notifications.cpp`，先实现 helper：

```cpp
#include "hud_notifications.h"

#include <base/system.h>

namespace QmHudNotifications
{
ESoloPrompt MatchKnownSoloPrompt(const char *pMessage)
{
	if(pMessage == nullptr)
		return ESoloPrompt::None;
	if(str_comp(pMessage, "You are now in a solo part") == 0 || str_comp(pMessage, "你现在处于单人区域") == 0)
		return ESoloPrompt::Enter;
	if(str_comp(pMessage, "You are now out of the solo part") == 0 || str_comp(pMessage, "你现在已离开单人区域") == 0)
		return ESoloPrompt::Leave;
	return ESoloPrompt::None;
}

int ClampVisibleCount(int Value)
{
	return std::clamp(Value, 1, 8);
}

int ClampHoldMs(int Value)
{
	return std::clamp(Value, 500, 10000);
}

int ClampAnimationMs(int Value)
{
	return std::clamp(Value, 0, 2000);
}
} // namespace QmHudNotifications
```

- [ ] **步骤 5：把源文件加入构建**

在 `CMakeLists.txt` 客户端组件列表中把 `components/qmclient/hud_notifications.cpp` 放在其他 `qmclient` 组件附近。

在测试列表中加入 `qm_hud_notifications_test.cpp`。

- [ ] **步骤 6：运行测试验证通过**

运行：

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 10
cmake-build-release\testrunner.exe --gtest_filter=QmHudNotifications.*
```

预期：`QmHudNotifications.*` 全部 PASS。

- [ ] **步骤 7：Commit**

```powershell
git add src/engine/shared/config_variables_qmclient.h src/game/client/components/qmclient/hud_notifications.h src/game/client/components/qmclient/hud_notifications.cpp src/test/qm_hud_notifications_test.cpp CMakeLists.txt
git commit -m "feat: add HUD notification helpers"
```

## 任务 2：注册 HUD 通知组件并实现队列

**文件：**
- 修改：`src/game/client/gameclient.h`
- 修改：`src/game/client/gameclient.cpp`
- 修改：`src/game/client/components/qmclient/hud_notifications.cpp`

- [ ] **步骤 1：补充组件成员**

在 `gameclient.h` 的 QmClient 组件区域加入 include 和成员：

```cpp
#include <game/client/components/qmclient/hud_notifications.h>
```

```cpp
CQmHudNotifications m_QmHudNotifications;
```

- [ ] **步骤 2：注册组件**

在 `CGameClient::OnConsoleInit()` 的 `m_vpAll` 列表中，把 `&m_QmHudNotifications` 放在 `&m_QmMonitoring` 附近，并早于 `&m_Chat` 注册：

```cpp
&m_QmClient,
&m_QmMonitoring,
&m_QmHudNotifications,
&m_TClient,
```

这样 `CChat` 可以通过 `GameClient()->m_QmHudNotifications` 查询迁移规则。

- [ ] **步骤 3：实现队列基础逻辑**

在 `hud_notifications.cpp` 实现：

```cpp
void CQmHudNotifications::OnReset()
{
	m_vNotifications.clear();
	m_HasLastSolo = false;
	m_LastSolo = false;
	m_PendingCompatPrompt = QmHudNotifications::ESoloPrompt::None;
	m_PendingCompatUntil = 0;
}

void CQmHudNotifications::OnRelease()
{
	OnReset();
}

void CQmHudNotifications::Queue(EKind Kind, const char *pText)
{
	if(pText == nullptr || pText[0] == '\0')
		return;
	SNotification Notification;
	Notification.m_Kind = Kind;
	str_copy(Notification.m_aText, pText);
	Notification.m_StartTime = time_get();
	m_vNotifications.push_back(Notification);
	while((int)m_vNotifications.size() > QmHudNotifications::ClampVisibleCount(g_Config.m_QmHudNotificationsMaxVisible))
		m_vNotifications.pop_front();
}
```

- [ ] **步骤 4：编译验证**

运行：

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 10
```

预期：`DDNet.exe` 链接成功。

- [ ] **步骤 5：Commit**

```powershell
git add src/game/client/gameclient.h src/game/client/gameclient.cpp src/game/client/components/qmclient/hud_notifications.cpp
git commit -m "feat: register HUD notification component"
```

## 任务 3：实现 solo 状态触发和左侧聊天抑制

**文件：**
- 修改：`src/game/client/components/qmclient/hud_notifications.cpp`
- 修改：`src/game/client/components/chat.cpp`

- [ ] **步骤 1：实现 local solo 状态读取**

在 `CQmHudNotifications::LocalSoloState` 中只读取本地角色状态，不读取聊天文本：

```cpp
bool CQmHudNotifications::LocalSoloState(bool &Solo) const
{
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0 || LocalId >= MAX_CLIENTS)
		return false;
	if(!GameClient()->m_aClients[LocalId].m_Active)
		return false;
	Solo = GameClient()->m_aClients[LocalId].m_Solo;
	return true;
}
```

- [ ] **步骤 2：实现 solo 转换入队**

在 `OnNewSnapshot()` 中：

```cpp
void CQmHudNotifications::OnNewSnapshot()
{
	bool Solo = false;
	if(!LocalSoloState(Solo))
	{
		m_HasLastSolo = false;
		return;
	}

	if(!m_HasLastSolo)
	{
		m_HasLastSolo = true;
		m_LastSolo = Solo;
		return;
	}

	if(Solo == m_LastSolo)
		return;

	const auto Prompt = Solo ? QmHudNotifications::ESoloPrompt::Enter : QmHudNotifications::ESoloPrompt::Leave;
	if(g_Config.m_QmHudNotificationsSolo)
		QueueSolo(Prompt);
	if(g_Config.m_QmHudNotificationsCompatSolo)
	{
		m_PendingCompatPrompt = Prompt;
		m_PendingCompatUntil = time_get() + time_freq() / 2;
	}
	m_LastSolo = Solo;
}
```

实现 `QueueSolo`：

```cpp
void CQmHudNotifications::QueueSolo(QmHudNotifications::ESoloPrompt Prompt)
{
	if(Prompt == QmHudNotifications::ESoloPrompt::Enter)
		Queue(EKind::SoloEnter, Localize("You are now in a solo part"));
	else if(Prompt == QmHudNotifications::ESoloPrompt::Leave)
		Queue(EKind::SoloLeave, Localize("You are now out of the solo part"));
}
```

- [ ] **步骤 3：实现 server chat 抑制**

在 `ShouldSuppressServerChat` 中：

```cpp
bool CQmHudNotifications::ShouldSuppressServerChat(const char *pMessage)
{
	if(!g_Config.m_QmHudNotificationsSolo)
		return false;

	const auto Known = QmHudNotifications::MatchKnownSoloPrompt(pMessage);
	if(Known != QmHudNotifications::ESoloPrompt::None)
		return true;

	if(!g_Config.m_QmHudNotificationsCompatSolo)
		return false;
	if(m_PendingCompatPrompt == QmHudNotifications::ESoloPrompt::None)
		return false;
	if(time_get() > m_PendingCompatUntil)
	{
		m_PendingCompatPrompt = QmHudNotifications::ESoloPrompt::None;
		return false;
	}

	m_PendingCompatPrompt = QmHudNotifications::ESoloPrompt::None;
	return true;
}
```

- [ ] **步骤 4：在聊天入口分流**

在 `CChat::OnMessage` 中 `AddLine(...)` 前插入：

```cpp
if(pMsg->m_ClientId == SERVER_MSG && GameClient()->m_QmHudNotifications.ShouldSuppressServerChat(pMsg->m_pMessage))
	return;
```

保留 `StoreSave(...)` 行为的决策：solo 提示不是 save 文本，可以在 suppress 后直接 return；F1 控制台日志仍由 console/logger 路径保留，不依赖 `CChat::AddLine(...)`。

- [ ] **步骤 5：运行测试和构建**

运行：

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 10
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 10
```

预期：测试通过，`DDNet.exe` 链接成功。

- [ ] **步骤 6：Commit**

```powershell
git add src/game/client/components/qmclient/hud_notifications.cpp src/game/client/components/chat.cpp
git commit -m "feat: route solo prompts to HUD notifications"
```

## 任务 4：实现 Echo 迁移

**文件：**
- 修改：`src/game/client/components/qmclient/hud_notifications.cpp`
- 修改：`src/game/client/components/chat.cpp`

- [ ] **步骤 1：实现 Echo 入队接口**

```cpp
bool CQmHudNotifications::QueueEcho(const char *pMessage)
{
	if(!g_Config.m_QmHudNotificationsEcho)
		return false;
	Queue(EKind::Echo, pMessage);
	return true;
}
```

- [ ] **步骤 2：在 `CChat::Echo` 中迁移**

在两个 `Echo` overload 的 `AddLine(CLIENT_MSG, ...)` 前加：

```cpp
if(GameClient()->m_QmHudNotifications.QueueEcho(pString))
	return;
```

对 `Echo(const char *pString, bool ForceVisible)` 也使用同样判断。Echo 迁移后不进入左侧聊天；Echo 内容不调用 `Localize(...)`。

- [ ] **步骤 3：构建验证**

运行：

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 10
```

预期：`DDNet.exe` 链接成功。

- [ ] **步骤 4：手动验证**

运行客户端后在控制台执行：

```text
echo hud notification smoke
```

预期：

- 默认配置下 Echo 仍显示在左侧聊天。
- 设置 `qm_hud_notifications_echo 1` 后，Echo 显示为右侧 HUD 通知，左侧聊天不重复显示。
- F1 控制台仍可看到 Echo 命令输出路径中的原始信息。

- [ ] **步骤 5：Commit**

```powershell
git add src/game/client/components/qmclient/hud_notifications.cpp src/game/client/components/chat.cpp
git commit -m "feat: route echo messages to HUD notifications"
```

## 任务 5：实现 HUD 渲染和动画

**文件：**
- 修改：`src/game/client/components/qmclient/hud_notifications.cpp`
- 修改：`src/game/client/components/qmclient/hud_notifications.h`

- [ ] **步骤 1：实现时间进度计算**

在 `hud_notifications.cpp` 添加局部 helper：

```cpp
static float EaseOutCubic(float t)
{
	t = std::clamp(t, 0.0f, 1.0f);
	return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}
```

- [ ] **步骤 2：实现通知生命周期清理**

在 `OnRender()` 开头，根据 `HoldMs + 2 * AnimMs` 清理过期通知：

```cpp
const int HoldMs = QmHudNotifications::ClampHoldMs(g_Config.m_QmHudNotificationsHoldMs);
const int AnimMs = QmHudNotifications::ClampAnimationMs(g_Config.m_QmHudNotificationsAnimMs);
const int64_t Now = time_get();
const int64_t Lifetime = (int64_t)(HoldMs + 2 * AnimMs) * time_freq() / 1000;
while(!m_vNotifications.empty() && Now - m_vNotifications.front().m_StartTime > Lifetime)
	m_vNotifications.pop_front();
```

- [ ] **步骤 3：实现默认 HUD rect 和编辑器 transform**

默认 rect：

```cpp
CUIRect DefaultRect;
DefaultRect.w = 210.0f;
DefaultRect.h = 80.0f;
DefaultRect.x = 300.0f * Graphics()->ScreenAspect() - DefaultRect.w - 10.0f;
DefaultRect.y = 190.0f;
```

渲染时包裹：

```cpp
const bool Preview = GameClient()->m_HudEditor.IsActive() && m_vNotifications.empty();
const auto Scope = GameClient()->m_HudEditor.BeginTransform(EHudEditorElement::HudNotifications, DefaultRect);
RenderNotifications(DefaultRect, Preview);
GameClient()->m_HudEditor.EndTransform(Scope);
```

- [ ] **步骤 4：实现通知绘制**

`RenderNotifications` 要：

- 使用 `color_cast<ColorRGBA>(ColorHSLA(g_Config.m_QmHudNotificationsBgColor, true))` 读取背景色。
- 使用 `g_Config.m_QmHudNotificationsTextSize` 作为字号。
- 每条通知计算文本宽度，背景宽度限制在默认 rect 宽度内。
- 背景使用 `CUIRect::Draw(..., IGraphics::CORNER_ALL, 6.0f)`。
- 文本使用 `TextRender()->Text(...)` 或 text cursor 渲染。
- 动画类型：
  - `0`：alpha + 右侧偏移。
  - `1`：只 alpha。
  - `2`：alpha 恒为 1，偏移为 0。

核心循环形状：

```cpp
for(int i = (int)vVisible.size() - 1; i >= 0; --i)
{
	const SNotification &Notification = *vVisible[i];
	const float Alpha = ...;
	const float OffsetX = ...;
	CUIRect Box = {BaseRect.x + OffsetX, Y, BoxW, BoxH};
	Box.Draw(ApplyAlpha(BgColor, Alpha), IGraphics::CORNER_ALL, 6.0f);
	TextRender()->TextColor(ApplyAlpha(TextColor, Alpha));
	TextRender()->Text(Box.x + PaddingX, Box.y + PaddingY, FontSize, Notification.m_aText, TextMaxWidth);
}
```

- [ ] **步骤 5：构建验证**

运行：

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 10
```

预期：`DDNet.exe` 链接成功。

- [ ] **步骤 6：Commit**

```powershell
git add src/game/client/components/qmclient/hud_notifications.h src/game/client/components/qmclient/hud_notifications.cpp
git commit -m "feat: render HUD notification toasts"
```

## 任务 6：接入 HUD 编辑器

**文件：**
- 修改：`src/game/client/components/hud_editor.h`
- 修改：`src/game/client/components/hud_editor.cpp`
- 修改：`src/game/client/components/qmclient/hud_notifications.cpp`

- [ ] **步骤 1：新增 HUD 编辑器元素**

在 `EHudEditorElement` 中 `InputOverlay` 后、`Count` 前加入：

```cpp
HudNotifications,
```

- [ ] **步骤 2：新增 token**

在 `CHudEditor::ElementToken` 中加入：

```cpp
case EHudEditorElement::HudNotifications: return "hud_notifications";
```

- [ ] **步骤 3：确保预览可见**

`CQmHudNotifications::OnRender()` 在 HUD 编辑器激活且队列为空时，用 sample 文本绘制预览：

```cpp
if(Preview)
{
	SNotification PreviewNotification;
	PreviewNotification.m_Kind = EKind::SoloEnter;
	str_copy(PreviewNotification.m_aText, Localize("You are now in a solo part"));
	PreviewNotification.m_StartTime = time_get();
	RenderNotifications(DefaultRect, true);
	return;
}
```

实现时不要把 sample push 到 `m_vNotifications`，只在渲染函数内临时使用。

- [ ] **步骤 4：手动验证**

运行客户端，打开 HUD 编辑器：

- 预期能看到 HUD 通知模块示例。
- 拖动后退出 HUD 编辑器，`qm_hud_editor_layout` 包含 `hud_notifications` token。
- 重开客户端或重新进入 HUD 编辑器后位置保持。

- [ ] **步骤 5：Commit**

```powershell
git add src/game/client/components/hud_editor.h src/game/client/components/hud_editor.cpp src/game/client/components/qmclient/hud_notifications.cpp
git commit -m "feat: add HUD notification editor module"
```

## 任务 7：接入栖梦设置页

**文件：**
- 修改：`src/game/client/components/qmclient/menus_qmclient.cpp`

- [ ] **步骤 1：在 HUD 模块区域新增控件**

在 `QMCLIENT_SETTINGS_TAB_HUD` 的 HUD 相关卡片里加入：

```cpp
DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_QmHudNotificationsSolo, Localize("Show solo prompts as HUD notifications"), &g_Config.m_QmHudNotificationsSolo, &Row, LgLineHeight);
DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_QmHudNotificationsEcho, Localize("Show echo messages as HUD notifications"), &g_Config.m_QmHudNotificationsEcho, &Row, LgLineHeight);
DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_QmHudNotificationsCompatSolo, Localize("Compat custom server solo prompts"), &g_Config.m_QmHudNotificationsCompatSolo, &Row, LgLineHeight);
```

- [ ] **步骤 2：添加样式控件**

使用现有 `DoLine_ColorPicker` 和 `RenderSliderWithValueInput`：

```cpp
DoLine_ColorPicker(&s_QmHudNotificationBgColorId, LgLineHeight, LgBodySize, LgLineSpacing, &CardContent, Localize("Notification background"), &g_Config.m_QmHudNotificationsBgColor, ColorRGBA(0.0f, 0.0f, 0.0f, 0.6f), false, nullptr, true);
DoLine_ColorPicker(&s_QmHudNotificationTextColorId, LgLineHeight, LgBodySize, LgLineSpacing, &CardContent, Localize("Notification text"), &g_Config.m_QmHudNotificationsTextColor, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), false, nullptr, true);
RenderSliderWithValueInput(&s_QmHudNotificationTextSizeInputId, ControlColValue, &g_Config.m_QmHudNotificationsTextSize, 8, 32);
RenderSliderWithValueInput(&s_QmHudNotificationHoldInputId, ControlColValue, &g_Config.m_QmHudNotificationsHoldMs, 500, 10000, "ms");
RenderSliderWithValueInput(&s_QmHudNotificationAnimInputId, ControlColValue, &g_Config.m_QmHudNotificationsAnimMs, 0, 2000, "ms");
RenderSliderWithValueInput(&s_QmHudNotificationMaxVisibleInputId, ControlColValue, &g_Config.m_QmHudNotificationsMaxVisible, 1, 8);
```

动画类型用现有分段/下拉模式；如果当前区域没有合适 helper，用三个 radio-like buttons 绑定 `0/1/2`，文案为：

```cpp
Localize("Fade and slide")
Localize("Fade only")
Localize("No animation")
```

- [ ] **步骤 3：构建验证**

运行：

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 10
```

预期：`DDNet.exe` 链接成功。

- [ ] **步骤 4：手动验证**

在栖梦设置页修改：

- 背景颜色和 alpha。
- 文本大小。
- 动画类型。
- 停留时间。
- 最大条数。

预期：右侧 HUD 通知立即使用新设置，界面文字不溢出控件。

- [ ] **步骤 5：Commit**

```powershell
git add src/game/client/components/qmclient/menus_qmclient.cpp
git commit -m "feat: add HUD notification settings"
```

## 任务 8：加入本地化文本

**文件：**
- 修改：`data/languages/simplified_chinese.txt`

- [ ] **步骤 1：添加翻译条目**

在 solo 相关区域附近加入：

```text
You are now in a solo part
== 你现在处于单人区域

You are now out of the solo part
== 你现在已离开单人区域
```

- [ ] **步骤 2：运行本地化/构建验证**

运行：

```powershell
git diff --check
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 10
```

预期：无 whitespace 错误，`DDNet.exe` 链接成功。

- [ ] **步骤 3：Commit**

```powershell
git add data/languages/simplified_chinese.txt
git commit -m "feat: localize HUD solo notifications"
```

## 任务 9：端到端验证和审查准备

**文件：**
- 不要求新增文件；只在发现前面任务缺陷时修改对应文件。

- [ ] **步骤 1：运行完整验证**

运行：

```powershell
git diff --check
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 10
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 10
```

预期：

- `git diff --check` 无输出。
- `run_cxx_tests` 通过。
- `game-client` 链接 `DDNet.exe`。

- [ ] **步骤 2：手动 smoke**

启动客户端，至少验证：

- 默认配置下进入 solo tile：右侧出现本地化进入提示，左侧不显示已知 solo 提示，F1 控制台仍保留原始日志。
- 默认配置下离开 solo tile：右侧出现本地化离开提示。
- `qm_hud_notifications_echo 0`：`echo abc` 仍按原行为进入左侧聊天。
- `qm_hud_notifications_echo 1`：`echo abc` 进入右侧 HUD，左侧不重复。
- HUD 编辑器能拖动通知模块并保存位置。
- 修改背景颜色、文本大小、动画类型、最大条数后，新通知使用新设置。

- [ ] **步骤 3：派发只读代码审查子代理**

任务完成后派发一个只读审查子代理，明确要求：

```text
只读审查当前分支的 HUD 通知第一阶段实现。重点检查是否符合 docs/superpowers/specs/2026-05-22-hud-notifications-chat-interactions-design.md 和 docs/superpowers/plans/2026-05-22-hud-notifications-phase1.md；不要修改文件；不要派发子代理；输出中文简短报告。
```

审查必须使用 `/chinese-code-review` 或 `/code-review-excellence` 对应技能。子代理未返回报告前，不得声称审查完成。报告处理完后及时关闭子代理。

- [ ] **步骤 4：最终 Commit**

如果审查后有修复，修复并重新运行步骤 1 验证，然后提交：

```powershell
git status --short
git add <changed files>
git commit -m "feat: add HUD notification toasts"
```

## 不在本计划内

- 聊天历史滚动和滚动条。
- 聊天点击复制或选择复制。
- F1 控制台滚动条。
- 普通玩家聊天迁移到右侧 HUD。
- 通知点击复制。
- 每类通知单独位置。
