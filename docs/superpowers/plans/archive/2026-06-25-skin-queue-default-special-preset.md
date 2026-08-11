> **已归档，禁止作为实现依据。** 本文保留为历史记录；当前实现请以活动 spec/plan/skill 为准。

# 皮肤队列默认/特殊预设收口 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让皮肤队列预设拥有一个不可删除但可编辑的默认预设，并把“服务器轮换全部玩家皮肤”收敛成一条独立的特殊预设，同时补一个清空皮肤队列的图标按钮。

**Architecture:** 预设列表保留现有线性结构，但初始化时强制注入一个默认预设，并将服务器轮换映射到另一条特殊预设。UI 仍然沿用当前设置页的预设条，只是在删除、清空、服务器轮换入口上收紧语义。队列内容匹配和当前/已应用状态保持分离，避免把“内容相等”误当成“最后应用来源”。

**Tech Stack:** C++、现有 QmClient 设置页 UI、现有 skins 配置保存链、gtest 结构化源码断言测试。

---

### Task 1: 补预设模型和初始化

**Files:**
- Modify: `src/game/client/components/skins.h`
- Modify: `src/game/client/components/skins.cpp`
- Test: `src/test/skins_test.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
TEST(Skins, QueuePresetsStartWithDefaultEditablePresetAndSpecialServerPreset)
{
	std::ifstream HeaderFile(TestSourcePath("src/game/client/components/skins.h"));
	ASSERT_TRUE(HeaderFile.good());
	std::stringstream HeaderBuffer;
	HeaderBuffer << HeaderFile.rdbuf();
	const std::string Header = HeaderBuffer.str();

	std::ifstream SourceFile(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(SourceFile.good());
	std::stringstream SourceBuffer;
	SourceBuffer << SourceFile.rdbuf();
	const std::string Source = SourceBuffer.str();

	EXPECT_NE(Header.find("bool HasDefaultSkinQueuePreset() const"), std::string::npos);
	EXPECT_NE(Header.find("bool IsServerSkinQueuePreset(size_t PresetIndex) const"), std::string::npos);
	EXPECT_NE(Source.find("m_vSkinQueuePresets.clear();"), std::string::npos);
	EXPECT_NE(Source.find("AddSkinQueuePreset(\""), std::string::npos);
	EXPECT_NE(Source.find("server"), std::string::npos);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake-build-release\\testrunner.exe --gtest_filter=Skins.QueuePresetsStartWithDefaultEditablePresetAndSpecialServerPreset`
Expected: FAIL because the default/special preset helpers and initialization do not exist yet.

- [ ] **Step 3: Write minimal implementation**

```cpp
class CSkins
{
public:
	bool HasDefaultSkinQueuePreset() const;
	bool IsServerSkinQueuePreset(size_t PresetIndex) const;
private:
	size_t m_DefaultSkinQueuePresetIndex = 0;
	size_t m_ServerSkinQueuePresetIndex = 1;
};

CSkins::CSkins()
{
	m_vSkinQueuePresets.clear();
	AddSkinQueuePreset("Current queue", 0);
	m_DefaultSkinQueuePresetIndex = 0;
	AddSkinQueuePreset("Server rotation", 0);
	m_ServerSkinQueuePresetIndex = 1;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake-build-release\\testrunner.exe --gtest_filter=Skins.QueuePresetsStartWithDefaultEditablePresetAndSpecialServerPreset`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/game/client/components/skins.h src/game/client/components/skins.cpp src/test/skins_test.cpp
git commit -m "fix(qmclient): add default skin queue preset"
```

### Task 2: 禁止删除默认预设并让服务器预设独立

**Files:**
- Modify: `src/game/client/components/skins.cpp`
- Modify: `src/game/client/components/menus_settings.cpp`
- Test: `src/test/skins_test.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
TEST(Skins, DefaultPresetCannotBeRemovedAndServerPresetIsSkippedFromManualDelete)
{
	std::ifstream SourceFile(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(SourceFile.good());
	std::stringstream SourceBuffer;
	SourceBuffer << SourceFile.rdbuf();
	const std::string Source = SourceBuffer.str();

	EXPECT_NE(Source.find("if(PresetIndex == m_DefaultSkinQueuePresetIndex)"), std::string::npos);
	EXPECT_NE(Source.find("return false;"), std::string::npos);
	EXPECT_NE(Source.find("IsServerSkinQueuePreset(PresetIndex)"), std::string::npos);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake-build-release\\testrunner.exe --gtest_filter=Skins.DefaultPresetCannotBeRemovedAndServerPresetIsSkippedFromManualDelete`
Expected: FAIL because remove-guards are not present yet.

- [ ] **Step 3: Write minimal implementation**

```cpp
bool CSkins::RemoveSkinQueuePreset(size_t PresetIndex, int Dummy)
{
	if(PresetIndex == m_DefaultSkinQueuePresetIndex || IsServerSkinQueuePreset(PresetIndex))
	{
		return false;
	}
	...
}
```

In the settings UI:

```cpp
const bool CanDeletePreset = HasSelectedPreset &&
	!GameClient()->m_Skins.IsDefaultSkinQueuePreset((size_t)ActivePresetIndex) &&
	!GameClient()->m_Skins.IsServerSkinQueuePreset((size_t)ActivePresetIndex);
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake-build-release\\testrunner.exe --gtest_filter=Skins.DefaultPresetCannotBeRemovedAndServerPresetIsSkippedFromManualDelete`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/game/client/components/skins.cpp src/game/client/components/menus_settings.cpp src/test/skins_test.cpp
git commit -m "fix(qmclient): protect default skin queue preset"
```

### Task 3: 改服务器轮换为特殊预设入口

**Files:**
- Modify: `src/game/client/components/menus_settings.cpp`
- Modify: `src/game/client/components/skins.cpp`
- Modify: `src/engine/shared/config_variables_qmclient.h`
- Test: `src/test/skins_test.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
TEST(Skins, ServerRotationUsesDedicatedPresetAndNoLongerNeedsGlobalToggle)
{
	std::ifstream ConfigFile(TestSourcePath("src/engine/shared/config_variables_qmclient.h"));
	ASSERT_TRUE(ConfigFile.good());
	std::stringstream ConfigBuffer;
	ConfigBuffer << ConfigFile.rdbuf();
	const std::string Config = ConfigBuffer.str();

	std::ifstream MenusFile(TestSourcePath("src/game/client/components/menus_settings.cpp"));
	ASSERT_TRUE(MenusFile.good());
	std::stringstream MenusBuffer;
	MenusBuffer << MenusFile.rdbuf();
	const std::string Menus = MenusBuffer.str();

	EXPECT_EQ(Config.find("QmSkinQueueRotateMap"), std::string::npos);
	EXPECT_EQ(Config.find("QmDummySkinQueueRotateMap"), std::string::npos);
	EXPECT_NE(Menus.find("Server rotation"), std::string::npos);
	EXPECT_NE(Menus.find("Apply this preset to the server queue"), std::string::npos);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake-build-release\\testrunner.exe --gtest_filter=Skins.ServerRotationUsesDedicatedPresetAndNoLongerNeedsGlobalToggle`
Expected: FAIL because rotate-map toggle still exists.

- [ ] **Step 3: Write minimal implementation**

```cpp
// remove QmSkinQueueRotateMap/QmDummySkinQueueRotateMap
// add a dedicated preset row in the preset UI
if(DoSettingsButton_Menu(..., Localize("Server rotation"), ...))
{
	GameClient()->m_Skins.SelectSkinQueuePreset(GameClient()->m_Skins.ServerSkinQueuePresetIndex(QueueDummy), QueueDummy);
	GameClient()->m_Skins.ApplySkinQueuePreset(GameClient()->m_Skins.ServerSkinQueuePresetIndex(QueueDummy), QueueDummy);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake-build-release\\testrunner.exe --gtest_filter=Skins.ServerRotationUsesDedicatedPresetAndNoLongerNeedsGlobalToggle`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/game/client/components/menus_settings.cpp src/game/client/components/skins.cpp src/engine/shared/config_variables_qmclient.h src/test/skins_test.cpp
git commit -m "fix(qmclient): make server skin rotation a preset"
```

### Task 4: 增加清空皮肤队列按钮

**Files:**
- Modify: `src/game/client/components/skins.h`
- Modify: `src/game/client/components/skins.cpp`
- Modify: `src/game/client/components/menus_settings.cpp`
- Test: `src/test/skins_test.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
TEST(Skins, SkinQueueCanBeClearedFromPresetEditor)
{
	std::ifstream MenusFile(TestSourcePath("src/game/client/components/menus_settings.cpp"));
	ASSERT_TRUE(MenusFile.good());
	std::stringstream MenusBuffer;
	MenusBuffer << MenusFile.rdbuf();
	const std::string Menus = MenusBuffer.str();

	EXPECT_NE(Menus.find("tee-clear-skin-queue"), std::string::npos);
	EXPECT_NE(Menus.find("Clear skin queue"), std::string::npos);
	EXPECT_NE(Menus.find("GameClient()->m_Skins.ClearSkinQueue(QueueDummy);"), std::string::npos);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake-build-release\\testrunner.exe --gtest_filter=Skins.SkinQueueCanBeClearedFromPresetEditor`
Expected: FAIL because there is no clear helper/button.

- [ ] **Step 3: Write minimal implementation**

```cpp
void CSkins::ClearSkinQueue(int Dummy)
{
	m_aSkinQueue[Dummy].clear();
	m_aAppliedSkinQueuePresetIndex[Dummy] = -1;
	SkinQueueIndexVar(Dummy) = 0;
	m_aSkinQueueElapsed[Dummy] = 0ns;
	m_aSkinQueueLastUpdate[Dummy].reset();
	m_SkinList.ForceRefresh();
}
```

UI:

```cpp
static CButtonContainer s_ClearQueueButton;
if(DoButton_MenuIcon(&s_ClearQueueButton, ICON_TRASH, &ClearQueueButtonRect, BUTTONFLAG_LEFT))
{
	GameClient()->m_Skins.ClearSkinQueue(QueueDummy);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake-build-release\\testrunner.exe --gtest_filter=Skins.SkinQueueCanBeClearedFromPresetEditor`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/game/client/components/skins.h src/game/client/components/skins.cpp src/game/client/components/menus_settings.cpp src/test/skins_test.cpp
git commit -m "fix(qmclient): add skin queue clear action"
```

### Task 5: 收口回归验证

**Files:**
- Modify: none
- Test: existing gates

- [ ] **Step 1: Run the focused tests**

Run:
```bash
cmake-build-release\\testrunner.exe --gtest_filter=Skins.*
```
Expected: PASS.

- [ ] **Step 2: Run the repo quick gate**

Run:
```bash
python qmclient_scripts/gate/check_gate.py --mode quick
```
Expected: PASS.

- [ ] **Step 3: Review for leftover compatibility or UI drift**

Check that:
```cpp
QmSkinQueueRotateMap
QmDummySkinQueueRotateMap
```
are removed from config and UI paths, while the server preset remains a normal selectable/editable preset entry in the preset list.

- [ ] **Step 4: Commit**

```bash
git add src/engine/shared/config_variables_qmclient.h src/game/client/components/menus_settings.cpp src/game/client/components/skins.cpp src/game/client/components/skins.h src/test/skins_test.cpp
git commit -m "fix(qmclient): finalize skin queue preset flow"
```

---

**Self-review**
- Scope covered: default preset, server special preset, preset deletion guard, clear queue action, tests, gate.
- No placeholders left in steps.
- Function names and config keys are consistent with the current codebase terms.
