# 聊天与控制台交互第二阶段实现计划

> **文档已过时** — 本文档内容不再反映当前代码状态，仅供参考。

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 增强左侧聊天框和 F1 控制台交互：聊天历史可滚动并带滚动条，聊天消息可复制，F1 控制台增加可拖动滚动条且保留现有选择、链接和导出行为。

**架构：** 聊天侧在 `CChat` 内补一个独立的历史滚动/选择状态，复用当前行准备和渲染管线，不替换聊天布局。控制台侧在现有 `m_BacklogCurLine`、选择复制、链接点击和导出模式基础上补滚动条拖拽逻辑。

**技术栈：** C++17、DDNet/QmClient `CChat`/`CGameConsole`、现有 `IInput` 鼠标状态、`ITextRender` 选择计算、`Input()->SetClipboardText(...)`、BestClient 参考实现 `docs/dyl/BestClient`。

---

## 文件结构

- 修改：`src/game/client/components/chat.h`
  - 增加聊天历史滚动、滚动条拖拽、鼠标选择/复制状态。
- 修改：`src/game/client/components/chat.cpp`
  - 增加聊天历史滚轮滚动、滚动条渲染/拖拽、点击/选择复制、布局重建条件。
- 修改：`src/game/client/components/console.h`
  - 增加 F1 控制台滚动条拖拽状态。
- 修改：`src/game/client/components/console.cpp`
  - 在现有 backlog 渲染区域补滚动条，保留选择、链接点击、导出模式。
- 修改：`src/engine/shared/config_variables_qmclient.h`
  - 增加少量阶段二配置开关。
- 修改：`src/game/client/components/qmclient/menus_qmclient.cpp`
  - 在栖梦聊天/HUD 设置附近增加交互开关。
- 创建：`src/test/qm_chat_interactions_test.cpp`
  - 测试纯 helper：滚动 clamp、滚动条值和 backlog line 映射、点击拖拽阈值。
- 修改：`CMakeLists.txt`
  - 加入测试源。

## 参考边界

参考 `docs/dyl/BestClient` 的行为，不直接替换 QmClient 当前实现。

参考点：

- `docs/dyl/BestClient/src/game/client/components/chat.h`
  - `m_BacklogCurLine`
  - `m_ScrollbarDragging`
  - `m_WantsSelectionCopy`
- `docs/dyl/BestClient/src/game/client/components/chat.cpp`
  - `CHAT_SCROLLBAR_WIDTH`
  - `RenderTextLine(..., std::string *pSelectionString)`
  - `OnPrepareLines(y, m_BacklogCurLine, ...)`
  - `Input()->SetClipboardText(...)`
- `docs/dyl/BestClient/src/game/client/components/console.h`
  - `m_ScrollbarDragging`
  - `m_ScrollbarDragOffset`
- `docs/dyl/BestClient/src/game/client/components/console.cpp`
  - `CONSOLE_SCROLLBAR_WIDTH`
  - backlog scrollbar block around line 1457

QmClient 当前差异：

- `src/game/client/components/console.cpp` 已经有 selection/copy/link/export 逻辑，阶段二只补滚动条。
- `src/game/client/components/chat.cpp` 已有翻译按钮和输入框鼠标逻辑，阶段二不能让聊天历史点击覆盖输入框、翻译按钮或语言菜单。
- 阶段一 HUD 通知不在本计划中修改。

## 任务 1：新增配置与滚动 helper 测试

**文件：**
- 修改：`src/engine/shared/config_variables_qmclient.h`
- 创建：`src/test/qm_chat_interactions_test.cpp`
- 修改：`CMakeLists.txt`
- 修改：`src/game/client/components/chat.h`
- 修改：`src/game/client/components/chat.cpp`

- [ ] **步骤 1：添加失败测试**

创建 `src/test/qm_chat_interactions_test.cpp`：

```cpp
#include <gtest/gtest.h>

#include <game/client/components/chat.h>

TEST(QmChatInteractions, ClampBacklogLine)
{
	EXPECT_EQ(CChat::ClampBacklogLine(-3, 10, 4), 0);
	EXPECT_EQ(CChat::ClampBacklogLine(0, 10, 4), 0);
	EXPECT_EQ(CChat::ClampBacklogLine(6, 10, 4), 6);
	EXPECT_EQ(CChat::ClampBacklogLine(7, 10, 4), 6);
	EXPECT_EQ(CChat::ClampBacklogLine(20, 10, 4), 6);
}

TEST(QmChatInteractions, ScrollbarValueToBacklogLine)
{
	EXPECT_EQ(CChat::ScrollbarValueToBacklogLine(1.0f, 12), 0);
	EXPECT_EQ(CChat::ScrollbarValueToBacklogLine(0.0f, 12), 12);
	EXPECT_EQ(CChat::ScrollbarValueToBacklogLine(0.5f, 12), 6);
}

TEST(QmChatInteractions, ClickDragThreshold)
{
	EXPECT_TRUE(CChat::IsCopyClickDrag(vec2(10.0f, 10.0f), vec2(12.0f, 12.0f)));
	EXPECT_FALSE(CChat::IsCopyClickDrag(vec2(10.0f, 10.0f), vec2(30.0f, 10.0f)));
}
```

- [ ] **步骤 2：运行测试验证失败**

运行：

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 10
```

预期：编译失败，提示 `ClampBacklogLine` 等 helper 不存在。

- [ ] **步骤 3：添加配置变量**

在 `src/engine/shared/config_variables_qmclient.h` 的聊天或 HUD 通知附近加入：

```cpp
// Chat and console interactions / 聊天与控制台交互
MACRO_CONFIG_INT(QmChatHistoryScroll, qm_chat_history_scroll, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "启用聊天历史滚动")
MACRO_CONFIG_INT(QmChatHistoryScrollbar, qm_chat_history_scrollbar, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "显示聊天历史滚动条")
MACRO_CONFIG_INT(QmChatClickCopy, qm_chat_click_copy, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "点击或选择复制聊天消息")
MACRO_CONFIG_INT(QmConsoleScrollbar, qm_console_scrollbar, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "显示 F1 控制台滚动条")
```

- [ ] **步骤 4：添加 helper 声明和实现**

在 `CChat` public 区加入：

```cpp
static int ClampBacklogLine(int Line, int TotalLines, int VisibleLines);
static int ScrollbarValueToBacklogLine(float Value, int MaxScroll);
static bool IsCopyClickDrag(vec2 Press, vec2 Release);
```

在 `chat.cpp` 中实现：

```cpp
int CChat::ClampBacklogLine(int Line, int TotalLines, int VisibleLines)
{
	const int MaxScroll = maximum(0, TotalLines - VisibleLines);
	return std::clamp(Line, 0, MaxScroll);
}

int CChat::ScrollbarValueToBacklogLine(float Value, int MaxScroll)
{
	return std::clamp((int)std::round((1.0f - std::clamp(Value, 0.0f, 1.0f)) * MaxScroll), 0, MaxScroll);
}

bool CChat::IsCopyClickDrag(vec2 Press, vec2 Release)
{
	return length(Release - Press) <= 5.0f;
}
```

- [ ] **步骤 5：加入测试构建并验证**

在 `CMakeLists.txt` 测试源里加入 `qm_chat_interactions_test.cpp`。

运行：

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 10
cmake-build-release\testrunner.exe --gtest_filter=QmChatInteractions.*
```

预期：`QmChatInteractions.*` 全部 PASS。

- [ ] **步骤 6：Commit**

```powershell
git add src/engine/shared/config_variables_qmclient.h src/game/client/components/chat.h src/game/client/components/chat.cpp src/test/qm_chat_interactions_test.cpp CMakeLists.txt
git commit -m "feat: add chat interaction helpers"
```

## 任务 2：聊天历史滚动状态

**文件：**
- 修改：`src/game/client/components/chat.h`
- 修改：`src/game/client/components/chat.cpp`

- [ ] **步骤 1：添加状态字段**

在 `CChat` 私有字段中加入：

```cpp
int m_BacklogCurLine = 0;
bool m_ScrollbarDragging = false;
float m_ScrollbarDragOffset = 0.0f;
std::optional<vec2> m_LastMousePos;
```

在 `Reset()`、`ClearLines()`、`OnRelease()` 相关清理路径中重置：

```cpp
m_BacklogCurLine = 0;
m_ScrollbarDragging = false;
m_ScrollbarDragOffset = 0.0f;
m_LastMousePos.reset();
```

- [ ] **步骤 2：新消息到来时保持历史锁定**

在 `AddLine(...)` 的成功新增行后：

```cpp
if(m_BacklogCurLine > 0)
	m_BacklogCurLine = minimum(m_BacklogCurLine + 1, MAX_LINES - 1);
```

这样用户滚到历史时，新消息不会把视图拉回底部；如果用户在底部，仍跟随最新消息。

- [ ] **步骤 3：滚轮处理**

在 `OnInput(const IInput::CEvent &Event)` 中，当满足以下条件时处理鼠标滚轮：

- `g_Config.m_QmChatHistoryScroll != 0`
- 聊天显示中或聊天输入激活。
- 语言菜单未打开。
- 鼠标在聊天历史区域，不在输入框和翻译按钮内。

伪代码：

```cpp
if(Event.m_EventType == IInput::FLAG_PRESS && Event.m_Key == KEY_MOUSE_WHEEL_UP)
	m_BacklogCurLine = ClampBacklogLine(m_BacklogCurLine + 1, MAX_LINES, CountVisibleLines(m_BacklogCurLine));
if(Event.m_EventType == IInput::FLAG_PRESS && Event.m_Key == KEY_MOUSE_WHEEL_DOWN)
	m_BacklogCurLine = ClampBacklogLine(m_BacklogCurLine - 1, MAX_LINES, CountVisibleLines(m_BacklogCurLine));
```

如果现有输入事件结构对滚轮使用不同 key，按当前项目 `console.cpp` 中 PageUp/PageDown/scroll input 的写法适配。

- [ ] **步骤 4：渲染从历史偏移开始**

在 `OnRender()` 渲染消息循环处，把原先从最新行开始的循环改为以 `m_BacklogCurLine` 为偏移：

```cpp
for(int i = m_BacklogCurLine; i < MAX_LINES; i++)
{
	CLine &Line = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
	...
}
```

同时修改 `OnPrepareLines(...)`，让它接受或读取当前 `m_BacklogCurLine`，确保 text container 的高度、截断和翻译按钮位置对应当前视图。

- [ ] **步骤 5：构建验证**

运行：

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 10
```

预期：`DDNet.exe` 链接成功。

- [ ] **步骤 6：手动 smoke**

进入服务器或用本地 echo 生成多条聊天：

- 鼠标滚轮向上后，聊天显示旧消息。
- 新消息到来时，历史视图不跳到底部。
- 滚到底部后，新消息继续显示。
- 翻译按钮和聊天输入框仍可点击。

- [ ] **步骤 7：Commit**

```powershell
git add src/game/client/components/chat.h src/game/client/components/chat.cpp
git commit -m "feat: add chat history scrolling"
```

## 任务 3：聊天滚动条

**文件：**
- 修改：`src/game/client/components/chat.h`
- 修改：`src/game/client/components/chat.cpp`

- [ ] **步骤 1：定义滚动条常量**

在 `chat.cpp` 局部常量区域加入：

```cpp
static constexpr float CHAT_SCROLLBAR_WIDTH = 5.0f;
static constexpr float CHAT_SCROLLBAR_MARGIN = 2.0f;
```

- [ ] **步骤 2：计算总行数和可见行数**

增加私有 helper：

```cpp
int CountInitializedLines() const;
int CountVisibleLinesFrom(int BacklogLine) const;
```

实现时只统计 `m_Initialized` 且未被 Focus/Gores 过滤的行，避免滚动条高度和实际可见内容不一致。

- [ ] **步骤 3：绘制滚动条**

在聊天历史渲染前或后加入滚动条块：

```cpp
const int TotalLines = CountInitializedLines();
const int VisibleLines = CountVisibleLinesFrom(m_BacklogCurLine);
const int MaxScroll = maximum(0, TotalLines - VisibleLines);
if(g_Config.m_QmChatHistoryScrollbar && MaxScroll > 0)
{
	CUIRect ScrollbarRect = {x + ChatOpenOffsetX - CHAT_SCROLLBAR_WIDTH - CHAT_SCROLLBAR_MARGIN, LogTop, CHAT_SCROLLBAR_WIDTH, LogHeight};
	...
}
```

手柄位置使用：

```cpp
const float Current = 1.0f - (float)m_BacklogCurLine / (float)MaxScroll;
```

- [ ] **步骤 4：实现拖拽**

拖拽规则：

- 鼠标按在 rail 或 handle 内开始拖拽。
- 按住时更新 `m_BacklogCurLine = ScrollbarValueToBacklogLine(NewValue, MaxScroll)`。
- 拖拽时禁用聊天文本选择状态，避免滚动条拖动变成复制。
- 鼠标释放时结束拖拽。

- [ ] **步骤 5：构建和手动验证**

运行：

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 10
```

手动验证：

- 消息少于可见容量时不显示滚动条。
- 消息超出容量时显示滚动条。
- 拖动手柄能稳定切换历史位置。
- 关闭 `qm_chat_history_scrollbar 0` 后滚动条不显示，滚轮滚动仍可用。

- [ ] **步骤 6：Commit**

```powershell
git add src/game/client/components/chat.h src/game/client/components/chat.cpp
git commit -m "feat: add chat history scrollbar"
```

## 任务 4：聊天点击/选择复制

**文件：**
- 修改：`src/game/client/components/chat.h`
- 修改：`src/game/client/components/chat.cpp`

- [ ] **步骤 1：添加选择状态字段**

在 `CLine` 中加入：

```cpp
int m_SelectionStart = -1;
int m_SelectionEnd = -1;
CUIRect m_RenderedTextRect{};
```

在 `CChat` 中加入：

```cpp
bool m_MouseIsPress = false;
vec2 m_MousePress = vec2(0.0f, 0.0f);
vec2 m_MouseRelease = vec2(0.0f, 0.0f);
bool m_HasSelection = false;
bool m_WantsSelectionCopy = false;
```

在 reset/clear 路径归零。

- [ ] **步骤 2：捕获聊天历史鼠标 press/release**

在 `OnRender()` 中读取聊天坐标系鼠标位置，规则：

- 输入框内不触发历史复制。
- 翻译按钮/语言 tag hover 时不触发历史复制。
- 滚动条拖拽时不触发历史复制。
- `g_Config.m_QmChatClickCopy == 0` 时不触发。

按下：

```cpp
m_MouseIsPress = true;
m_MousePress = MousePos;
m_MouseRelease = MousePos;
m_HasSelection = false;
```

释放：

```cpp
m_MouseIsPress = false;
m_MouseRelease = MousePos;
```

- [ ] **步骤 3：让文本渲染计算选择范围**

在渲染单行文本时，参考当前 `console.cpp` 的 `STextCursor` 选择模式和 BestClient 的 `RenderTextLine`：

```cpp
if(ChatInteractionActive && (m_MouseIsPress || m_HasSelection || m_WantsSelectionCopy))
{
	LineCursor.m_CalculateSelectionMode = TEXT_CURSOR_SELECTION_MODE_CALCULATE;
	LineCursor.m_PressMouse = m_MousePress;
	LineCursor.m_ReleaseMouse = m_MouseRelease;
}
```

渲染后保存：

```cpp
Line.m_SelectionStart = LineCursor.m_SelectionStart;
Line.m_SelectionEnd = LineCursor.m_SelectionEnd;
```

- [ ] **步骤 4：实现 Ctrl+C 复制选择**

在 `CChat::OnInput` 中，当 `Ctrl+C` 且 `m_HasSelection` 时：

```cpp
m_WantsSelectionCopy = true;
return true;
```

在渲染循环中按可见顺序收集被选中的 UTF-8 子串：

```cpp
std::string SelectionString;
...
if(Line.m_SelectionStart >= 0 && Line.m_SelectionEnd >= 0 && Line.m_SelectionStart != Line.m_SelectionEnd)
{
	const int SelectionMin = minimum(Line.m_SelectionStart, Line.m_SelectionEnd);
	const int SelectionMax = maximum(Line.m_SelectionStart, Line.m_SelectionEnd);
	const size_t OffUTF8Start = str_utf8_offset_chars_to_bytes(Line.m_aText, SelectionMin);
	const size_t OffUTF8End = str_utf8_offset_chars_to_bytes(Line.m_aText, SelectionMax);
	SelectionString.insert(0, std::string(Line.m_aText + OffUTF8Start, OffUTF8End - OffUTF8Start) + (SelectionString.empty() ? "" : "\n"));
}
```

复制：

```cpp
if(m_WantsSelectionCopy && !SelectionString.empty())
	Input()->SetClipboardText(SelectionString.c_str());
m_WantsSelectionCopy = false;
```

- [ ] **步骤 5：实现单击复制整条消息**

释放时如果 `IsCopyClickDrag(m_MousePress, m_MouseRelease)` 且没有非空选择：

- 找到鼠标所在的可见聊天行。
- 复制 `Line.m_aText`。
- 不复制玩家名和时间戳，第一版只复制消息内容。

如果用户希望后续复制完整显示文本，再单独扩展；不要在本任务混入格式选项。

- [ ] **步骤 6：构建和手动验证**

运行：

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 10
```

手动验证：

- 单击聊天消息复制消息文本。
- 拖选多行后 Ctrl+C 复制选区。
- 点击翻译按钮不复制消息。
- 点击输入框不复制历史消息。
- 拖动滚动条不复制消息。
- 中文文本复制不截断 UTF-8。

- [ ] **步骤 7：Commit**

```powershell
git add src/game/client/components/chat.h src/game/client/components/chat.cpp
git commit -m "feat: add chat message copy interaction"
```

## 任务 5：F1 控制台滚动条

**文件：**
- 修改：`src/game/client/components/console.h`
- 修改：`src/game/client/components/console.cpp`

- [ ] **步骤 1：添加滚动条状态**

在 `CGameConsole::CInstance` 中加入：

```cpp
bool m_ScrollbarDragging = false;
float m_ScrollbarDragOffset = 0.0f;
```

在 `Reset()`、`ClearBacklog()`、`SetLogFilter(...)` 中清理：

```cpp
m_ScrollbarDragging = false;
m_ScrollbarDragOffset = 0.0f;
```

- [ ] **步骤 2：定义滚动条常量**

在 `console.cpp` 顶部局部常量区域加入：

```cpp
static constexpr float CONSOLE_SCROLLBAR_WIDTH = 18.0f;
static constexpr float CONSOLE_SCROLLBAR_MARGIN = 5.0f;
```

- [ ] **步骤 3：预留右侧宽度**

在计算 console text width 的位置，若 `g_Config.m_QmConsoleScrollbar` 开启且有足够内容：

```cpp
Width -= (CONSOLE_SCROLLBAR_WIDTH + CONSOLE_SCROLLBAR_MARGIN);
```

必须确保输入框、completion 和 backlog text 不被滚动条覆盖。

- [ ] **步骤 4：渲染滚动条**

在 backlog 渲染循环前，使用当前 `m_BacklogCurLine`、`m_LinesRendered` 和 backlog 总行数估算 `MaxScroll`。

参考 BestClient 的 console scrollbar block，但保留当前 QmClient 的：

- `m_ChatExportMode`
- link click
- selection copy
- search match highlight
- topbar filter buttons

滚动条拖拽时：

```cpp
pConsole->m_BacklogCurLine = NewLine;
pConsole->m_BacklogLastActiveLine = pConsole->m_BacklogCurLine;
pConsole->m_HasSelection = false;
pConsole->m_MouseIsPress = false;
```

- [ ] **步骤 5：构建和手动验证**

运行：

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 10
```

手动验证：

- F1 控制台内容较少时不显示滚动条。
- 内容较多时显示滚动条。
- 拖动滚动条可切换历史位置。
- Ctrl+C 复制选区仍可用。
- Ctrl+点击链接仍可用。
- Chat export mode 的选中/导出仍可用。
- `qm_console_scrollbar 0` 后隐藏滚动条。

- [ ] **步骤 6：Commit**

```powershell
git add src/game/client/components/console.h src/game/client/components/console.cpp
git commit -m "feat: add console backlog scrollbar"
```

## 任务 6：栖梦设置页接入

**文件：**
- 修改：`src/game/client/components/qmclient/menus_qmclient.cpp`

- [ ] **步骤 1：添加交互开关**

在栖梦设置的聊天或 HUD 区域加入：

```cpp
DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_QmChatHistoryScroll, Localize("Enable chat history scrolling"), &g_Config.m_QmChatHistoryScroll, &Row, LgLineHeight);
DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_QmChatHistoryScrollbar, Localize("Show chat history scrollbar"), &g_Config.m_QmChatHistoryScrollbar, &Row, LgLineHeight);
DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_QmChatClickCopy, Localize("Click chat messages to copy"), &g_Config.m_QmChatClickCopy, &Row, LgLineHeight);
DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_QmConsoleScrollbar, Localize("Show F1 console scrollbar"), &g_Config.m_QmConsoleScrollbar, &Row, LgLineHeight);
```

- [ ] **步骤 2：构建验证**

运行：

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 10
```

预期：`DDNet.exe` 链接成功。

- [ ] **步骤 3：手动验证**

打开栖梦设置页：

- 四个开关显示完整，不挤出控件。
- 关闭聊天滚动后滚轮不再滚动聊天历史。
- 关闭聊天滚动条后滚动条隐藏。
- 关闭点击复制后点击聊天不改剪贴板。
- 关闭 F1 控制台滚动条后控制台滚动条隐藏。

- [ ] **步骤 4：Commit**

```powershell
git add src/game/client/components/qmclient/menus_qmclient.cpp
git commit -m "feat: add chat interaction settings"
```

## 任务 7：端到端验证和审查准备

**文件：**
- 不要求新增文件；只在发现缺陷时修改对应实现文件。

- [ ] **步骤 1：完整验证**

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

- [ ] **步骤 2：交互 smoke**

启动客户端并验证：

- 聊天消息超过可见区域后，鼠标滚轮可浏览旧消息。
- 聊天滚动条显示并可拖拽。
- 新消息到来时，用户锁在历史位置不会被拉回底部。
- 单击聊天消息复制内容。
- 拖选聊天文本后 Ctrl+C 复制选区。
- 翻译按钮、语言菜单、聊天输入框不被复制逻辑抢事件。
- F1 控制台滚动条可拖拽。
- F1 控制台选区复制、链接点击、chat export mode 仍工作。

- [ ] **步骤 3：派发只读代码审查子代理**

任务完成后派发一个只读审查子代理，明确要求：

```text
只读审查当前分支的聊天与控制台交互第二阶段实现。重点检查是否符合 docs/superpowers/specs/2026-05-22-hud-notifications-chat-interactions-design.md 和 docs/superpowers/plans/2026-05-22-chat-console-interactions-phase2.md；重点关注聊天输入/翻译按钮/F1 控制台选择和链接行为是否被破坏；不要修改文件；不要派发子代理；输出中文简短报告。
```

审查必须使用 `/chinese-code-review` 或 `/code-review-excellence` 对应技能。子代理未返回报告前，不得声称审查完成。报告处理完后及时关闭子代理。

- [ ] **步骤 4：最终 Commit**

如果审查后有修复，修复并重新运行步骤 1 验证，然后提交：

```powershell
git status --short
git add <changed files>
git commit -m "feat: add chat and console interactions"
```

## 不在本计划内

- 右侧 HUD 通知第一阶段实现。
- 通知点击复制。
- 聊天复制格式选项。
- F1 控制台搜索功能重做。
- F1 控制台导出模式重做。
- BestClient 文件迁移或结构替换。
