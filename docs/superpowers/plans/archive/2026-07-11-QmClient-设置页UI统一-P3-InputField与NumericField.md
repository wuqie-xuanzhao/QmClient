> **已归档，禁止作为实现依据。** 本文保留为历史记录；当前实现请以活动 spec/plan/skill 为准。

# QmClient 设置页 UI 统一 P3 InputField 与 NumericField Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把设置页普通/搜索/清除/icon/多行文本、整数/小数/`∞`、单位与 slider+input 收口到 `ui_widget::InputField(...)` 和 `ui_widget::NumericField(...)`，并让 delay update 成为数值控件的 commit policy 而不是旧绘制回退开关。

**Architecture:** `InputField` 只建立一次 shell/content rect，再把文本、光标、IME、选择和搜索 hotkey 委托给现有 `CUi` edit logic，所有 affordance 都从同一 layout 计算。`NumericField` 持有可复用的 staged state，把配置整数按明确 divisor/precision 映射为整数或小数显示；slider、文本和 `∞` 共用一次 parse/format/commit，`ON_RELEASE_OR_SUBMIT` 只延迟写配置，不改变视觉路径。

**Tech Stack:** C++、QmUi `UiForms`、DDNet `CLineInput`/`CUi`、GoogleTest、CMake/MSVC。

> **执行状态（2026-07-12）：** 自动化实现、独立只读审查和 `default` gate 已完成；真实客户端的视觉与交互矩阵（IME、不同 UI scale、窄卡片、长本地化、拖动 release）待人工验收反馈。P3 在该反馈收口前保持进行中。

## Global Constraints

- P1 theme/card 和 P2 deck/registry/model 必须完成；本计划不得创建页面私有 input shell、focus ring 或数值状态 map。
- 输入非激活/激活背景颜色相同；active 只增加与 field 同圆角、宽度至少 `2.0 * UiScale` 的外框 focus ring。
- 无 leading icon 或 clear action 时不预留 slot；有 affordance 时 slot 为 field height 的正方形，文本 rect 从 slot 外边界开始。
- `CUi::DoEditBox*` 只负责文字/光标/选区/IME/编辑状态，调用时 `m_DrawBackground=false`；设置页面不得直接调用它们。
- Controls 垃圾桶始终是独立 destructive action button，不映射为 `InputField` clear affordance。
- 数值配置继续使用现有整数存储；小数通过 `m_DisplayDivisor`/`m_Precision` 映射，不改变配置文件字段类型或序列化格式。
- `∞` 的存储值由 `m_InfiniteStoredValue` 明确给出；不能假定所有控件都用 `0` 表示无限。
- `SCROLLBAR_OPTION_DELAYUPDATE` 只在兼容入口转换为 `EInputCommitPolicy::ON_RELEASE_OR_SUBMIT`，不得调用 `CUi::DoScrollbarOption(...)` 或旧 `DoScrollbarH(...)` 分支。
- slider 正常最小轨道宽度为 `96.0 * UiScale`；不足时先缩减 label 分配，再切两行。触发两行或最小字号必须返回 layout feedback 并进入验收记录。
- P3 只保证设置页不再使用 legacy/direct 路径；非卡片菜单上的临时 forwarding alias 在 P7 删除。
- 同一 `cmake-build-release` 目标串行；版本更新留给 P7。

---

## File Structure

- Modify: `src/game/client/QmUi/UiForms.h/.cpp` — 唯一 input shell、layout helper、numeric state/format/commit/slider。
- Modify: `src/game/client/QmUi/UiTokens.h` — input slot/inset/focus/track/row fallback token。
- Modify: `src/game/client/ui.h/.cpp` — 把 QmClient/TClient 两份文件局部 multiline edit logic 收口为唯一 `CUi::DoEditBoxMultiLine(...)`。
- Modify: `src/game/client/components/menus.h/.cpp` — `DoSettingsScrollbarOption` 只适配 `NumericField`；state map 从 `CLineInputNumber` 升级为 `SNumericFieldState`。
- Modify: `src/game/client/components/menus_settings.cpp` — 设置页 input/numeric call sites。
- Modify: `src/game/client/components/menus_settings_controls.cpp` — Controls 搜索、鼠标/摇杆数值与独立删除 action。
- Modify: `src/game/client/components/qmclient/menus_qmclient.cpp` — 密码与 Qm 数值字段。
- Modify: `src/game/client/components/tclient/menus_tclient.cpp` — TClient edit/slider helper 只转发公共 primitive。
- Modify: `src/test/QmAnimTest.cpp` — input/numeric pure behavior。
- Modify: `src/test/qmclient_monitoring_test.cpp` — 生产路径删除断言。

---

### Task 1: 用一个 InputField shell 统一 layout、focus 与 affordance

**Files:**
- Modify: `src/game/client/QmUi/UiForms.h`
- Modify: `src/game/client/QmUi/UiForms.cpp`
- Modify: `src/game/client/QmUi/UiTokens.h`
- Modify: `src/game/client/ui.h`
- Modify: `src/game/client/ui.cpp`
- Modify: `src/game/client/components/qmclient/menus_qmclient.cpp`
- Modify: `src/game/client/components/tclient/menus_tclient.cpp`
- Test: `src/test/QmAnimTest.cpp`

**Interfaces:**
- Consumes: P1 `IUiContext::m_pTheme`、`CLineInput`、field rect。
- Produces: `SInputFieldLayout ResolveInputFieldLayout(...)`、`SInputFieldResult InputField(...)`、`SInputFieldOptions`。

- [ ] **Step 1: Write failing input layout tests**

```cpp
TEST(InputField, AffordanceSlotsOnlyExistWhenRequested)
{
	const CUIRect Rect{10.0f, 20.0f, 300.0f, 36.0f};
	const SInputFieldLayout Plain = ResolveInputFieldLayout(Rect, false, false, 1.0f);
	const SInputFieldLayout Both = ResolveInputFieldLayout(Rect, true, true, 1.0f);
	EXPECT_FLOAT_EQ(Plain.m_LeadingSlot.w, 0.0f);
	EXPECT_FLOAT_EQ(Plain.m_ClearSlot.w, 0.0f);
	EXPECT_FLOAT_EQ(Plain.m_ContentRect.x, Rect.x + ui_token::input::CONTENT_INSET);
	EXPECT_FLOAT_EQ(Both.m_LeadingSlot.w, Rect.h);
	EXPECT_FLOAT_EQ(Both.m_ClearSlot.w, Rect.h);
	EXPECT_GT(Both.m_ContentRect.x, Plain.m_ContentRect.x);
	EXPECT_LT(Both.m_ContentRect.w, Plain.m_ContentRect.w);
}

TEST(InputField, FocusRingUsesOuterShellWithoutChangingContentRect)
{
	const CUIRect Rect{10.0f, 20.0f, 300.0f, 36.0f};
	const SInputFieldLayout Layout = ResolveInputFieldLayout(Rect, true, true, 1.25f);
	EXPECT_LT(Layout.m_FocusRingRect.x, Rect.x);
	EXPECT_LT(Layout.m_FocusRingRect.y, Rect.y);
	EXPECT_GT(Layout.m_FocusRingRect.w, Rect.w);
	EXPECT_TRUE(Rect.Inside({Layout.m_ContentRect.x, Layout.m_ContentRect.y}));
}

TEST(InputField, MultilineDefaultsToTopLeftWithoutChangingSingleLineDefault)
{
	SInputFieldOptions Options;
	EXPECT_EQ(ResolveInputFieldTextAlign(Options), TEXTALIGN_ML);
	Options.m_Mode = EInputFieldMode::MULTILINE;
	EXPECT_EQ(ResolveInputFieldTextAlign(Options), TEXTALIGN_TL);
	Options.m_TextAlign = TEXTALIGN_MC;
	EXPECT_EQ(ResolveInputFieldTextAlign(Options), TEXTALIGN_MC);
}
```

- [ ] **Step 2: Run tests to verify red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=InputField.*
```

Expected: compile FAIL because the unified layout/API does not exist.

- [ ] **Step 3: Define the exact InputField API**

```cpp
enum class EInputFieldMode
{
	TEXT,
	SEARCH,
	MULTILINE,
};

enum class EInputTextStyle
{
	SMALL,
	BODY,
};

enum class EInputCommitPolicy
{
	LIVE,
	ON_RELEASE_OR_SUBMIT,
};

struct SInputFieldOptions
{
	const char *m_pPlaceholder = nullptr;
	const char *m_pLeadingIcon = nullptr;
	EInputFieldMode m_Mode = EInputFieldMode::TEXT;
	EInputTextStyle m_TextStyle = EInputTextStyle::BODY;
	EInputCommitPolicy m_CommitPolicy = EInputCommitPolicy::LIVE;
	bool m_Clearable = false;
	bool m_SearchHotkeyEnabled = false;
	bool m_ReadOnly = false;
	int m_Corners = IGraphics::CORNER_ALL;
	// -1 表示按 mode 解析：单行默认 ML，multiline 默认 TL。
	int m_TextAlign = -1;
};

struct SInputFieldLayout
{
	CUIRect m_ShellRect;
	CUIRect m_FocusRingRect;
	CUIRect m_LeadingSlot;
	CUIRect m_ContentRect;
	CUIRect m_ClearSlot;
};

SInputFieldLayout ResolveInputFieldLayout(const CUIRect &Rect, bool HasLeadingIcon, bool Clearable, float UiScale);
int ResolveInputFieldTextAlign(const SInputFieldOptions &Options);
SInputFieldResult InputField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const SInputFieldOptions &Options);

bool CUi::DoEditBoxMultiLine(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, float LineSpacing, int TextAlign, const SEditBoxRenderOptions &Options = {});
```

`SInputFieldResult` 保留 `m_Changed`、`m_Committed`、`m_Submitted`、`m_Deactivated`、`m_Cleared`，并增加 `m_InvalidInput`；shell 先绘制 P1 theme 的 `m_InputSurface`，active 时只在 `m_FocusRingRect` 绘制 `m_FocusRing`。

`CUi::DoEditBoxMultiLine(...)` 的 body 由当前 QmClient/TClient 两份相同的文件局部 `DoEditBoxMultiLine(CUi *, ...)` 移入 `ui.cpp`：保留 active/hot item、mouse selection、IME restart、clip、`CLineInput::Render(...)` 和 scroll-offset reset；只在 `Options.m_DrawBackground` 为真时绘制背景。移入后删除两份 file-local 定义，页面不再直接调用它们。

`ResolveInputFieldTextAlign(...)` 是唯一默认对齐解释：

```cpp
int ResolveInputFieldTextAlign(const SInputFieldOptions &Options)
{
	if(Options.m_TextAlign >= 0)
		return Options.m_TextAlign;
	return Options.m_Mode == EInputFieldMode::MULTILINE ? TEXTALIGN_TL : TEXTALIGN_ML;
}
```

因此从 QmClient/TClient 文件局部 helper 迁移的多行调用在不写 override 时仍为 `TEXTALIGN_TL`；若业务确实需要其他对齐，必须显式设置 `m_TextAlign`。

- [ ] **Step 4: Route every mode through the same content rect**

```cpp
CUi::SEditBoxRenderOptions RenderOptions;
RenderOptions.m_DrawBackground = false;
const int TextAlign = ResolveInputFieldTextAlign(Options);
bool Changed = false;
if(Options.m_Mode == EInputFieldMode::SEARCH)
	Changed = Ctx.m_pUi->DoEditBox_Search(pInput, &Layout.m_ContentRect, FontSize, Options.m_SearchHotkeyEnabled, RenderOptions);
else if(Options.m_Mode == EInputFieldMode::MULTILINE)
	Changed = Ctx.m_pUi->DoEditBoxMultiLine(pInput, &Layout.m_ContentRect, FontSize, ui_token::input::MULTILINE_LINE_SPACING * Ctx.m_UiScale, TextAlign, RenderOptions);
else
	Changed = Ctx.m_pUi->DoEditBox(pInput, &Layout.m_ContentRect, FontSize, Options.m_Corners, {}, TextAlign, RenderOptions);
```

leading icon 与 clear button 只使用 `Layout.m_LeadingSlot`/`Layout.m_ClearSlot`；不再调用 `DrawLegacyTextFieldPlate(...)`。

- [ ] **Step 5: Keep temporary aliases as forwarding-only**

`TextFieldEx`、`SearchFieldEx`、`ClearableTextFieldEx`、`IconTextFieldEx` 暂时只允许构造 `SInputFieldOptions` 后调用 `InputField(...)`；删除 `LegacyTextFieldEx` 和 `DrawLegacyTextFieldPlate`。forwarding alias 标注“P7 非卡片菜单迁移后删除”，不得包含绘制代码。

- [ ] **Step 6: Run tests and commit**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=InputField.*
```

Expected: input tests PASS。

```powershell
git add src/game/client/QmUi/UiForms.h src/game/client/QmUi/UiForms.cpp src/game/client/QmUi/UiTokens.h src/game/client/ui.h src/game/client/ui.cpp src/game/client/components/qmclient/menus_qmclient.cpp src/game/client/components/tclient/menus_tclient.cpp src/test/QmAnimTest.cpp
git commit -m "refactor(settings-ui): 统一 InputField 外壳" -m "refactor: 统一 icon、清除、搜索、多行与 focus 几何" -m "test: 覆盖 affordance slot 与外框不变量"
```

### Task 2: 实现整数、小数、∞ 与单位共用的 NumericField

**Files:**
- Modify: `src/game/client/QmUi/UiForms.h`
- Modify: `src/game/client/QmUi/UiForms.cpp`
- Create: `src/game/client/QmUi/UiFormLogic.h`
- Create: `src/game/client/QmUi/UiFormLogic.cpp`
- Modify: `CMakeLists.txt`
- Test: `src/test/QmAnimTest.cpp`

**Interfaces:**
- Consumes: `InputField(...)`、现有 `IScrollbarScale`、整数配置存储。
- Produces: `SNumericFieldState`、`SNumericFieldOptions`、`SNumericFieldResult NumericField(...)`；无 UI 依赖的 format/parse/layout/commit helper 独立在 `UiFormLogic.h/.cpp`。

- [ ] **Step 1: Write failing numeric tests**

```cpp
TEST(NumericField, FormatsAndParsesIntegerDecimalAndInfinity)
{
	SNumericValueFormat Integer;
	Integer.m_DisplayDivisor = 1;
	Integer.m_Precision = 0;
	EXPECT_EQ(FormatNumericFieldValue(42, Integer), "42");

	SNumericValueFormat Decimal;
	Decimal.m_DisplayDivisor = 100;
	Decimal.m_Precision = 2;
	EXPECT_EQ(FormatNumericFieldValue(125, Decimal), "1.25");
	int Stored = 0;
	EXPECT_TRUE(ParseNumericFieldValue("-3.50", Decimal, -1000, 1000, &Stored));
	EXPECT_EQ(Stored, -350);

	Decimal.m_AllowInfinite = true;
	Decimal.m_InfiniteStoredValue = 0;
	EXPECT_TRUE(ParseNumericFieldValue("∞", Decimal, -1000, 1000, &Stored));
	EXPECT_EQ(Stored, 0);
	EXPECT_EQ(FormatNumericFieldValue(0, Decimal), "∞");
}

TEST(NumericField, DelayPolicyCommitsOnlyOnReleaseSubmitOrBlur)
{
	SInputFieldResult Editing;
	Editing.m_Changed = true;
	EXPECT_FALSE(NumericFieldShouldCommit(EInputCommitPolicy::ON_RELEASE_OR_SUBMIT, false, Editing));
	Editing.m_Submitted = true;
	EXPECT_TRUE(NumericFieldShouldCommit(EInputCommitPolicy::ON_RELEASE_OR_SUBMIT, false, Editing));
	Editing.m_Submitted = false;
	Editing.m_Deactivated = true;
	EXPECT_TRUE(NumericFieldShouldCommit(EInputCommitPolicy::ON_RELEASE_OR_SUBMIT, false, Editing));
	EXPECT_TRUE(NumericFieldShouldCommit(EInputCommitPolicy::ON_RELEASE_OR_SUBMIT, true, {}));
}

TEST(NumericField, FallsBackToTwoRowsBeforeCollapsingSliderTrack)
{
	const SNumericFieldLayout Wide = ResolveNumericFieldLayout({0.0f, 0.0f, 500.0f, 36.0f}, true, true, 1.0f);
	const SNumericFieldLayout Narrow = ResolveNumericFieldLayout({0.0f, 0.0f, 260.0f, 36.0f}, true, true, 1.0f);
	EXPECT_FALSE(Wide.m_TwoRows);
	EXPECT_GE(Wide.m_SliderRect.w, 96.0f);
	EXPECT_TRUE(Narrow.m_TwoRows);
	EXPECT_GE(Narrow.m_SliderRect.w, 96.0f);
}
```

- [ ] **Step 2: Run tests to verify red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=NumericField.*
```

Expected: compile FAIL because numeric contract is absent.

- [ ] **Step 3: Define exact numeric state and options**

```cpp
struct SNumericValueFormat
{
	int m_DisplayDivisor = 1;
	int m_Precision = 0;
	bool m_AllowInfinite = false;
	int m_InfiniteStoredValue = 0;
};

struct SNumericFieldState
{
	CLineInputBuffered<64> m_Input;
	int m_PendingStoredValue = 0;
	int m_LastSyncedStoredValue = 0;
	bool m_HasPendingValue = false;
	bool m_HasSyncedValue = false;
	bool m_SliderWasActive = false;
};

struct SNumericFieldOptions
{
	const char *m_pLabel = nullptr;
	const char *m_pUnit = nullptr;
	const char *m_pMaxText = nullptr;
	const IScrollbarScale *m_pScale = nullptr;
	SNumericValueFormat m_Format;
	EInputCommitPolicy m_CommitPolicy = EInputCommitPolicy::LIVE;
	bool m_NoClampValue = false;
	float m_UiScale = 1.0f;
};

enum class ENumericLayoutFeedback
{
	NONE,
	TWO_ROWS,
	TRACK_AT_MINIMUM,
};

struct SNumericFieldLayout
{
	CUIRect m_LabelRect;
	CUIRect m_SliderRect;
	CUIRect m_InputRect;
	CUIRect m_UnitRect;
	bool m_TwoRows = false;
	ENumericLayoutFeedback m_Feedback = ENumericLayoutFeedback::NONE;
};

struct SNumericFieldResult
{
	SInputFieldResult m_Input;
	ENumericLayoutFeedback m_LayoutFeedback = ENumericLayoutFeedback::NONE;
	bool m_ValueChanged = false;
	bool m_Committed = false;
};

// 以上 pure 数值类型与下面四个 helper 定义在 UiFormLogic.h。
std::string FormatNumericFieldValue(int StoredValue, const SNumericValueFormat &Format);
bool ParseNumericFieldValue(const char *pText, const SNumericValueFormat &Format, int StoredMin, int StoredMax, int *pStoredValue);
bool NumericFieldShouldCommit(EInputCommitPolicy Policy, bool SliderReleased, const SInputFieldResult &InputResult);
SNumericFieldLayout ResolveNumericFieldLayout(const CUIRect &Rect, bool HasLabel, bool HasUnit, float UiScale);
// Stateful/client-only types and renderer stay in UiForms.h/.cpp.
SNumericFieldResult NumericField(const IUiContext &Ctx, SNumericFieldState &State, const void *pId, int *pStoredValue, int StoredMin, int StoredMax, const CUIRect &Rect, const SNumericFieldOptions &Options);
```

`m_Precision` 限制为 `0..3`，`m_DisplayDivisor` 必须大于 `0`；invalid input 设置 `m_InvalidInput=true` 且不覆盖 config。`m_pUnit` 只占 suffix rect，不写入 input string。当 input/slider 均未 active 且没有 pending value 时，若 `*pStoredValue != m_LastSyncedStoredValue` 就重新 format 到 `m_Input`；active 编辑期不用外部配置刷新覆盖用户输入。实现文件显式 include `<algorithm>`、`<cerrno>`、`<cctype>`、`<climits>`、`<cmath>`、`<cstdlib>`、`<cstring>` 和 `<limits>`。

`UiFormLogic.h` 拥有 `SNumericValueFormat`、`ENumericLayoutFeedback`、`SNumericFieldLayout`、`FormatNumericFieldValue(...)`、`ParseNumericFieldValue(...)`、`NumericFieldShouldCommit(...)` 和 `ResolveNumericFieldLayout(...)`；它只依赖 POD/result 定义与 `ui_rect.h`，不 include `ui.h`、不绘制、不访问 `CUi`。`UiForms.cpp` 只保留 client-only `InputField(...)`/`NumericField(...)` 绘制和交互协调，调用 pure helper。

- [ ] **Step 4: Implement the complete helpers and one staged commit path**

```cpp
std::string FormatNumericFieldValue(int StoredValue, const SNumericValueFormat &Format)
{
	if(Format.m_DisplayDivisor <= 0)
		return {};
	if(Format.m_AllowInfinite && StoredValue == Format.m_InfiniteStoredValue)
		return "\xe2\x88\x9e";

	const int Precision = std::clamp(Format.m_Precision, 0, 3);
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%.*f", Precision, StoredValue / static_cast<double>(Format.m_DisplayDivisor));
	return aBuf;
}

bool ParseNumericFieldValue(const char *pText, const SNumericValueFormat &Format, int StoredMin, int StoredMax, int *pStoredValue)
{
	if(pText == nullptr || pStoredValue == nullptr || Format.m_DisplayDivisor <= 0 || StoredMin > StoredMax)
		return false;

	const char *pBegin = pText;
	while(*pBegin != '\0' && std::isspace(static_cast<unsigned char>(*pBegin)))
		++pBegin;
	const char *pEnd = pBegin + std::strlen(pBegin);
	while(pEnd > pBegin && std::isspace(static_cast<unsigned char>(pEnd[-1])))
		--pEnd;
	const std::string Text(pBegin, pEnd);
	if(Text.empty())
		return false;
	if(Format.m_AllowInfinite && Text == "\xe2\x88\x9e")
	{
		*pStoredValue = Format.m_InfiniteStoredValue;
		return true;
	}

	errno = 0;
	char *pParseEnd = nullptr;
	const double DisplayValue = std::strtod(Text.c_str(), &pParseEnd);
	if(errno == ERANGE || pParseEnd == Text.c_str() || *pParseEnd != '\0' || !std::isfinite(DisplayValue))
		return false;
	const double ScaledValue = DisplayValue * static_cast<double>(Format.m_DisplayDivisor);
	if(ScaledValue < static_cast<double>(std::numeric_limits<int>::min()) || ScaledValue > static_cast<double>(std::numeric_limits<int>::max()))
		return false;
	const int RoundedValue = static_cast<int>(std::llround(ScaledValue));
	*pStoredValue = std::clamp(RoundedValue, StoredMin, StoredMax);
	return true;
}

bool NumericFieldShouldCommit(EInputCommitPolicy Policy, bool SliderReleased, const SInputFieldResult &InputResult)
{
	if(Policy == EInputCommitPolicy::LIVE)
		return true;
	return SliderReleased || InputResult.m_Submitted || InputResult.m_Deactivated;
}

SNumericFieldLayout ResolveNumericFieldLayout(const CUIRect &Rect, bool HasLabel, bool HasUnit, float UiScale)
{
	const float Scale = maximum(0.5f, UiScale);
	const float Gap = 8.0f * Scale;
	const float InputWidth = 88.0f * Scale;
	const float UnitWidth = HasUnit ? 56.0f * Scale : 0.0f;
	const float LabelWidth = HasLabel ? minimum(140.0f * Scale, Rect.w * 0.32f) : 0.0f;
	const float MinimumSliderWidth = 96.0f * Scale;
	const float RowHeight = maximum(32.0f * Scale, Rect.h);
	const int VisibleBlocks = 2 + (HasLabel ? 1 : 0) + (HasUnit ? 1 : 0);
	const float TotalGaps = maximum(0, VisibleBlocks - 1) * Gap;
	const float OneRowSliderWidth = Rect.w - LabelWidth - InputWidth - UnitWidth - TotalGaps;

	SNumericFieldLayout Layout{};
	if(OneRowSliderWidth >= MinimumSliderWidth)
	{
		float X = Rect.x;
		if(HasLabel)
		{
			Layout.m_LabelRect = {X, Rect.y, LabelWidth, RowHeight};
			X += LabelWidth + Gap;
		}
		Layout.m_SliderRect = {X, Rect.y, OneRowSliderWidth, RowHeight};
		X += OneRowSliderWidth + Gap;
		Layout.m_InputRect = {X, Rect.y, InputWidth, RowHeight};
		X += InputWidth;
		if(HasUnit)
		{
			X += Gap;
			Layout.m_UnitRect = {X, Rect.y, UnitWidth, RowHeight};
		}
		Layout.m_Feedback = OneRowSliderWidth <= MinimumSliderWidth + 0.01f ? ENumericLayoutFeedback::TRACK_AT_MINIMUM : ENumericLayoutFeedback::NONE;
		return Layout;
	}

	Layout.m_TwoRows = true;
	const float TopRightWidth = minimum(Rect.w, InputWidth + (HasUnit ? Gap + UnitWidth : 0.0f));
	Layout.m_LabelRect = HasLabel ? CUIRect{Rect.x, Rect.y, maximum(0.0f, Rect.w - TopRightWidth - Gap), RowHeight} : CUIRect{};
	Layout.m_InputRect = {Rect.x + maximum(0.0f, Rect.w - TopRightWidth), Rect.y, minimum(InputWidth, TopRightWidth), RowHeight};
	if(HasUnit)
		Layout.m_UnitRect = {Layout.m_InputRect.x + Layout.m_InputRect.w + Gap, Rect.y, maximum(0.0f, TopRightWidth - Layout.m_InputRect.w - Gap), RowHeight};
	Layout.m_SliderRect = {Rect.x, Rect.y + RowHeight + Gap, maximum(Rect.w, MinimumSliderWidth), RowHeight};
	Layout.m_Feedback = Rect.w + 0.01f < MinimumSliderWidth ? ENumericLayoutFeedback::TRACK_AT_MINIMUM : ENumericLayoutFeedback::TWO_ROWS;
	return Layout;
}

SNumericFieldResult NumericField(const IUiContext &Ctx, SNumericFieldState &State, const void *pId, int *pStoredValue, int StoredMin, int StoredMax, const CUIRect &Rect, const SNumericFieldOptions &Options)
{
	SNumericFieldResult Result{};
	if(Ctx.m_pUi == nullptr || pId == nullptr || pStoredValue == nullptr || StoredMin > StoredMax || Options.m_Format.m_DisplayDivisor <= 0)
		return Result;

	const bool HasLabel = Options.m_pLabel != nullptr && Options.m_pLabel[0] != '\0';
	const bool HasUnit = Options.m_pUnit != nullptr && Options.m_pUnit[0] != '\0';
	const SNumericFieldLayout Layout = ResolveNumericFieldLayout(Rect, HasLabel, HasUnit, Options.m_UiScale);
	Result.m_LayoutFeedback = Layout.m_Feedback;
	if(HasLabel)
		Ctx.m_pUi->DoLabel(&Layout.m_LabelRect, Options.m_pLabel, ui_token::font::BODY * Options.m_UiScale, TEXTALIGN_ML);
	if(HasUnit)
		Ctx.m_pUi->DoLabel(&Layout.m_UnitRect, Options.m_pUnit, ui_token::font::SMALL * Options.m_UiScale, TEXTALIGN_ML);

	const bool SliderWasActiveThisFrame = Ctx.m_pUi->CheckActiveItem(pId);
	if(!State.m_Input.IsActive() && !SliderWasActiveThisFrame && !State.m_HasPendingValue && (!State.m_HasSyncedValue || State.m_LastSyncedStoredValue != *pStoredValue))
	{
		const std::string Text = FormatNumericFieldValue(*pStoredValue, Options.m_Format);
		State.m_Input.Set(Text.c_str());
		State.m_LastSyncedStoredValue = *pStoredValue;
		State.m_HasSyncedValue = true;
	}

	const int DisplayedValue = State.m_HasPendingValue ? State.m_PendingStoredValue : *pStoredValue;
	const bool DisplayedInfinite = Options.m_Format.m_AllowInfinite && DisplayedValue == Options.m_Format.m_InfiniteStoredValue;
	const int SliderMax = Options.m_Format.m_AllowInfinite && StoredMax < INT_MAX ? StoredMax + 1 : StoredMax;
	const int SliderDisplayedValue = DisplayedInfinite ? SliderMax : std::clamp(DisplayedValue, StoredMin, StoredMax);
	const float SliderRelative = Options.m_pScale != nullptr ?
		Options.m_pScale->ToRelative(SliderDisplayedValue, StoredMin, SliderMax) :
		(SliderMax == StoredMin ? 0.0f : (SliderDisplayedValue - StoredMin) / static_cast<float>(SliderMax - StoredMin));
	const float NewSliderRelative = Ctx.m_pUi->DoScrollbarH(pId, &Layout.m_SliderRect, SliderRelative);
	const int NewSliderAbsolute = Options.m_pScale != nullptr ?
		Options.m_pScale->ToAbsolute(NewSliderRelative, StoredMin, SliderMax) :
		StoredMin + static_cast<int>(std::lround(NewSliderRelative * (SliderMax - StoredMin)));
	const bool SliderChanged = NewSliderAbsolute != SliderDisplayedValue;
	const int SliderStoredValue = Options.m_Format.m_AllowInfinite && NewSliderAbsolute == SliderMax ?
		Options.m_Format.m_InfiniteStoredValue : std::clamp(NewSliderAbsolute, StoredMin, StoredMax);
	const bool SliderActive = Ctx.m_pUi->CheckActiveItem(pId);
	const bool SliderReleased = State.m_SliderWasActive && !SliderActive;
	State.m_SliderWasActive = SliderActive;

	const bool ShowMaxText = Options.m_pMaxText != nullptr && !State.m_Input.IsActive() && !DisplayedInfinite && DisplayedValue == StoredMax;
	std::string SavedInput;
	if(ShowMaxText)
	{
		SavedInput = State.m_Input.GetString();
		State.m_Input.Set(Options.m_pMaxText);
	}
	SInputFieldOptions InputOptions;
	InputOptions.m_TextAlign = TEXTALIGN_MC;
	const bool InputWasActive = State.m_Input.IsActive();
	const SInputFieldResult InputResult = InputField(Ctx, &State.m_Input, Layout.m_InputRect, InputOptions);
	Result.m_Input = InputResult;
	if(ShowMaxText)
	{
		State.m_Input.Set(SavedInput.c_str());
		if(!InputWasActive && State.m_Input.IsActive())
			State.m_Input.SelectAll();
	}

	int CandidateStoredValue = State.m_HasPendingValue ? State.m_PendingStoredValue : DisplayedValue;
	bool HasCandidate = State.m_HasPendingValue;
	if(SliderChanged)
	{
		CandidateStoredValue = SliderStoredValue;
		HasCandidate = true;
	}
	if(InputResult.m_Changed)
	{
		int ParsedStoredValue = DisplayedValue;
		const int ParseMin = Options.m_NoClampValue ? INT_MIN : StoredMin;
		const int ParseMax = Options.m_NoClampValue ? INT_MAX : StoredMax;
		if(!ParseNumericFieldValue(State.m_Input.GetString(), Options.m_Format, ParseMin, ParseMax, &ParsedStoredValue))
		{
			Result.m_Input.m_InvalidInput = true;
			return Result;
		}
		CandidateStoredValue = ParsedStoredValue;
		HasCandidate = true;
	}

	const bool Commit = NumericFieldShouldCommit(Options.m_CommitPolicy, SliderReleased, InputResult);
	if(HasCandidate && Commit)
	{
		if(*pStoredValue != CandidateStoredValue)
		{
			*pStoredValue = CandidateStoredValue;
			Result.m_ValueChanged = true;
		}
		State.m_HasPendingValue = false;
		State.m_LastSyncedStoredValue = *pStoredValue;
		State.m_HasSyncedValue = true;
		const std::string Text = FormatNumericFieldValue(*pStoredValue, Options.m_Format);
		State.m_Input.Set(Text.c_str());
		Result.m_Committed = true;
		Result.m_Input.m_Committed = true;
	}
	else if(HasCandidate)
	{
		State.m_PendingStoredValue = CandidateStoredValue;
		State.m_HasPendingValue = true;
		if(SliderChanged)
		{
			const std::string Text = FormatNumericFieldValue(CandidateStoredValue, Options.m_Format);
			State.m_Input.Set(Text.c_str());
		}
	}
	return Result;
}
```

slider 与 input 都更新 `CandidateStoredValue`；`∞` 是 format/parse 结果，不是另一个 slider branch。

- [ ] **Step 5: Run tests and commit**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=NumericField.*
```

Expected: all numeric tests PASS。

构建登记必须同步完成：`QmUi/UiFormLogic.cpp/.h` 加入既有 QmUi client source list，仅 `QmUi/UiFormLogic.cpp` 加入 `TESTS_EXTRA`。不得把 `UiForms.cpp` 加入 `TESTS_EXTRA`，因为它依赖完整 `CUi` 绘制链。`QmAnimTest.cpp` 的 `NumericField.*` 只调用 `UiFormLogic` pure API；`NumericField(...)` 生产接线由 Task 3/4 的结构测试和 `game-client` 构建验证。

```powershell
git add CMakeLists.txt src/game/client/QmUi/UiForms.h src/game/client/QmUi/UiForms.cpp src/game/client/QmUi/UiFormLogic.h src/game/client/QmUi/UiFormLogic.cpp src/test/QmAnimTest.cpp
git commit -m "feat(settings-ui): 统一 NumericField" -m "feat: 支持整数、小数、无限值、单位与延迟提交" -m "test: 覆盖格式、提交策略和两行保底"
```

### Task 3: 移除 delay-update 旧绘制回退

**Files:**
- Modify: `src/game/client/components/menus.h`
- Modify: `src/game/client/components/menus.cpp:1114-1186`
- Modify: `src/game/client/components/tclient/menus_tclient.cpp:730-800`
- Modify: `src/game/client/components/qmclient/menus_qmclient.cpp:2260-2320`
- Test: `src/test/qmclient_monitoring_test.cpp`

**Interfaces:**
- Consumes: Task 2 `SNumericFieldState`/`NumericField(...)`。
- Produces: 所有设置 slider helper 都只转换 options 并调用公共 numeric；`SCROLLBAR_OPTION_DELAYUPDATE` 不再改变绘制路径。

- [ ] **Step 1: Write failing source-contract test**

```cpp
TEST(QmMonitoringHelpers, DelayUpdateStaysOnNumericFieldPath)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Body = ExtractSourceFunctionBody(Menus, "bool CMenus::DoSettingsScrollbarOption(int Page, int Tab, int Subtab, const char *pTextId, const void *pId, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, const IScrollbarScale *pScale, unsigned Flags, const char *pSuffix, const char *pMaxText)");
	ASSERT_FALSE(Body.empty());
	EXPECT_NE(Body.find("EInputCommitPolicy::ON_RELEASE_OR_SUBMIT"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::NumericField("), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoScrollbarOption("), std::string::npos);
	EXPECT_EQ(Body.find("ui_widget::SliderInputField("), std::string::npos);
}
```

- [ ] **Step 2: Run test to verify red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.DelayUpdateStaysOnNumericFieldPath
```

Expected: FAIL because delay update still returns `Ui()->DoScrollbarOption(...)`.

- [ ] **Step 3: Replace state map and compatibility mapping**

```cpp
ui_widget::SNumericFieldState *CMenus::GetSettingsNumericFieldState(const void *pId)
{
	auto &pState = m_vpSettingsNumericFieldStates[pId];
	if(!pState)
		pState = std::make_unique<ui_widget::SNumericFieldState>();
	return pState.get();
}

Options.m_CommitPolicy = (Flags & CUi::SCROLLBAR_OPTION_DELAYUPDATE) != 0 ?
	ui_widget::EInputCommitPolicy::ON_RELEASE_OR_SUBMIT :
	ui_widget::EInputCommitPolicy::LIVE;
```

`DoSettingsScrollbarOption(...)`、TClient/QmClient slider helper 全部调用 `NumericField(...)`；删除 `DoSettingsSliderInputField(...)`、`GetSettingsSliderInput(...)`、`SSliderInputFieldOptions` 和 `SliderInputField(...)`。

- [ ] **Step 4: Run focused tests and build**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=NumericField.*:QmMonitoringHelpers.DelayUpdateStaysOnNumericFieldPath
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
```

Expected: tests PASS，`game-client` build 退出码 `0`。

- [ ] **Step 5: Commit delay policy migration**

```powershell
git add src/game/client/components/menus.h src/game/client/components/menus.cpp src/game/client/components/tclient/menus_tclient.cpp src/game/client/components/qmclient/menus_qmclient.cpp src/game/client/QmUi/UiForms.h src/game/client/QmUi/UiForms.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "fix(settings-ui): 收口数值延迟提交路径" -m "fix: delay update 不再回退旧滑条绘制" -m "test: 禁止设置页绕过 NumericField"
```

### Task 4: 清退设置页 direct/legacy input 调用

**Files:**
- Modify: `src/game/client/components/menus_settings.cpp`
- Modify: `src/game/client/components/menus_settings_controls.cpp`
- Modify: `src/game/client/components/qmclient/menus_qmclient.cpp`
- Modify: `src/game/client/components/tclient/menus_tclient.cpp`
- Modify: `src/test/qmclient_monitoring_test.cpp`

**Interfaces:**
- Consumes: Tasks 1–3 公共 input/numeric。
- Produces: 设置页生产文件只调用 `InputField(...)`/`NumericField(...)`；Controls destructive action 保持独立。

- [ ] **Step 1: Write failing deletion inventory test**

```cpp
TEST(QmMonitoringHelpers, SettingsPagesUseOnlyUnifiedInputAndNumericFields)
{
	struct SSettingsScope
	{
		const char *m_pPath;
		std::vector<const char *> m_vFunctions;
	};
	const std::array<SSettingsScope, 4> aScopes = {{
		{"src/game/client/components/menus_settings.cpp", {"CMenus::RenderSettingsGeneral", "CMenus::RenderSettingsPlayer", "CMenus::RenderSettingsTee", "CMenus::RenderSettingsGraphics", "CMenus::RenderSettingsSound", "CMenus::RenderSettingsDDNet", "CMenus::RenderSettingsAppearance"}},
		{"src/game/client/components/menus_settings_controls.cpp", {"CMenusSettingsControls::Render"}},
		{"src/game/client/components/qmclient/menus_qmclient.cpp", {"CMenus::RenderSettingsQmClientContent"}},
		{"src/game/client/components/tclient/menus_tclient.cpp", {"CMenus::RenderSettingsTClient"}},
	}};
	for(const SSettingsScope &Scope : aScopes)
	{
		const std::string Source = ReadRepoFile(Scope.m_pPath);
		std::string SettingsBodies;
		for(const char *pFunction : Scope.m_vFunctions)
		{
			const std::string Body = ExtractSourceFunctionBody(Source, pFunction);
			ASSERT_FALSE(Body.empty()) << Scope.m_pPath << ": " << pFunction;
			SettingsBodies += Body;
		}
		for(const char *pForbidden : {
			"TextFieldEx(", "SearchFieldEx(", "ClearableTextFieldEx(", "IconTextFieldEx(", "LegacyTextFieldEx(",
			"TextField(", "SearchField(", "ClearableTextField(", "IconTextField(", "SliderInputField(",
			"Ui()->DoEditBox(", "Ui()->DoEditBox_Search(", "DoEditBoxMultiLine(Ui(),", "static bool DoEditBoxMultiLine("})
			EXPECT_EQ(SettingsBodies.find(pForbidden), std::string::npos) << Scope.m_pPath << ": " << pForbidden;
	}
	const std::string Ui = ReadRepoFile("src/game/client/ui.cpp");
	EXPECT_NE(Ui.find("bool CUi::DoEditBoxMultiLine("), std::string::npos);
	const std::string Forms = ReadRepoFile("src/game/client/QmUi/UiForms.cpp");
	EXPECT_NE(Forms.find("m_pUi->DoEditBoxMultiLine("), std::string::npos);
}
```

检查只截取设置页入口函数 body，不会误伤 P7 尚未迁移的非卡片菜单。若 QmClient/TClient 入口在 P0 合并后拆分，必须把新的设置子页函数逐个加入 `m_vFunctions`，不得退回整文件误报或留下 alias 豁免。

- [ ] **Step 2: Run test to verify red**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=QmMonitoringHelpers.SettingsPagesUseOnlyUnifiedInputAndNumericFields
```

Expected: FAIL at TClient direct edit、QmClient password fields and any remaining legacy/icon calls.

- [ ] **Step 3: Migrate text fields with explicit options**

密码字段使用 `EInputFieldMode::TEXT`、不 clear、无 leading icon；搜索使用 `SEARCH` + clear + search hotkey；多行规则编辑使用 `MULTILINE`。示例：

```cpp
ui_widget::SInputFieldOptions PasswordOptions;
PasswordOptions.m_pPlaceholder = Localize("Password");
PasswordOptions.m_TextStyle = ui_widget::EInputTextStyle::BODY;
PasswordOptions.m_Mode = ui_widget::EInputFieldMode::TEXT;
ui_widget::InputField(SettingsUiContext("qm_axiom_password"), &s_AxiomLoginPassword, PasswordEditRect, PasswordOptions);
```

所有 context 使用 P1 `SettingsUiContext(...)`；不得在页面自行画 plate/focus/icon。

- [ ] **Step 4: Keep Controls trash as a separate action**

绑定删除仍使用独立 button ID 与 destructive style：

```cpp
if(GameClient()->m_Menus.DoButton_MenuIcon(&Bind.m_KeyResetButton, FontIcons::FONT_ICON_TRASH_CAN, &DeleteRect, BUTTONFLAG_LEFT))
	Bind.m_ToBeDeleted = true;
```

不把 `DeleteRect` 传入 `InputField`，也不使用 `m_Clearable` 表示删除绑定。

- [ ] **Step 5: Run deletion test and all focused behavior tests**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=InputField.*:NumericField.*:QmMonitoringHelpers.DelayUpdateStaysOnNumericFieldPath:QmMonitoringHelpers.SettingsPagesUseOnlyUnifiedInputAndNumericFields
```

Expected: all tests PASS。

- [ ] **Step 6: Commit settings-page migration**

```powershell
git add src/game/client/components/menus_settings.cpp src/game/client/components/menus_settings_controls.cpp src/game/client/components/qmclient/menus_qmclient.cpp src/game/client/components/tclient/menus_tclient.cpp src/test/qmclient_monitoring_test.cpp
git commit -m "refactor(settings-ui): 清退设置页旧输入路径" -m "refactor: 普通、搜索、密码、多行与数值统一走公共 primitive" -m "test: 保留 Controls 独立删除语义并禁止 direct edit"
```

### Task 5: P3 全量验证、人工矩阵与只读审查

**Files:**
- Modify: none unless review findings require a scoped fix
- Test: all input/numeric tests, full C++ regression, docs and default gate

**Interfaces:**
- Consumes: Tasks 1–4。
- Produces: P5/P6 可直接使用的稳定 input/numeric contract。

- [x] **Step 1: Run serial automated verification**

Run:

```powershell
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe --gtest_filter=InputField.*:NumericField.*:QmMonitoringHelpers.DelayUpdateStaysOnNumericFieldPath:QmMonitoringHelpers.SettingsPagesUseOnlyUnifiedInputAndNumericFields
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
cmd /c qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
python qmclient_scripts/gate/check_docs.py
python qmclient_scripts/gate/check_gate.py --mode default
git diff --check
```

Expected: all commands exit `0`。

- [ ] **Step 2: Execute manual input matrix**

```text
普通/密码/搜索/clear/icon/多行：placeholder、光标、选区、IME、提交、失焦
focus：背景不变，仅统一粗外框；75%/100%/125% UI scale 圆角与 inset 同源
整数/小数/∞：键盘输入、slider、modifier wheel、非法文本、单位与最大值文案
delay commit：拖动期间预览连续，release 才写配置；提交/失焦行为一致
窄卡片/长中文/德文：先两行，不出现球状轨道、文字覆盖或控件逃逸
Controls：清除搜索只清文本，垃圾桶只删除绑定
```

Expected: 每项记录页面、viewport、UI scale、语言和结果；`TWO_ROWS`/`TRACK_AT_MINIMUM` feedback 与具体 stable ID 一起记录。

- [x] **Step 3: Dispatch independent read-only review**

review 重点：`CLineInput` 生命周期、IME/selection 未破坏、invalid parse 不写配置、divisor 溢出、`∞` 映射、delay pending state、NoClamp 语义、focus 不双绘、settings 文件 direct-call 删除。等待完整 findings-first 报告。

Expected: P0/P1 finding 修复并重跑 Step 1；报告返回前 P3 不完成。

---

## Self-review

- Spec coverage: 覆盖普通/搜索/clear/icon/多行、IME/focus/content rect、整数/小数/∞/单位、delay commit、slider 最小轨道、两行保底和 Controls destructive action。
- Marker scan: 未发现未决占位、省略实现或未定义类型。
- Type consistency: P5–P7 只使用 `ui_widget::InputField(...)`、`SInputFieldOptions`、`ui_widget::NumericField(...)`、`SNumericFieldState`、`SNumericFieldOptions` 和 `SNumericFieldResult`。
- Scope boundary: 非卡片菜单 forwarding alias 最终删除在 P7；ColorPicker 专用颜色轨道不改；协议/配置字段类型不变。
