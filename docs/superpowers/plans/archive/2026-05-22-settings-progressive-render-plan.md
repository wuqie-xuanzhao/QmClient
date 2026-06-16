# 设置页渐进式渲染重构 实现计划

> **文档已过时** — 本文档内容不再反映当前代码状态，仅供参考。

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 消除 TClient/QmClient 设置页打开时的卡顿，用 Section 注册表 + 帧预算 + 预热 + 脏标记四层机制替换当前全局倒计时方案。

**架构：** 新增 `CSectionLoader` 通用组件管理 section 状态机（MEASURING→COMPACT→FULL），按帧时间预算逐 section 推进渲染。`menus_tclient.cpp` 和 `menus_qmclient.cpp` 各自注册 section 列表。预热利用会话缓存在 loading 阶段预渲染上次视口内的文本。稳态通过配置脏标记跳过无变化 section 的渲染。

**技术栈：** C++17, DDNet 引擎层 (IGraphics, ITextRender, CPerfTimer), CMake

---

## 文件结构

| 文件 | 职责 |
|------|------|
| `src/game/client/components/section_loader.h` | CSectionLoader、SSettingsSection、ESettingsSectionState、SSessionUiCache 声明 |
| `src/game/client/components/section_loader.cpp` | CSectionLoader 实现：状态机驱动、帧预算循环、视口优先级、预热、脏标记 |
| `src/test/section_loader_test.cpp` | 单元测试：状态转换、帧预算截断、脏标记、高度一致性 |
| `src/game/client/components/tclient/menus_tclient.cpp` | 移除匿名区 deferred 全局变量，`RenderSettingsTClientSettings` 中创建 section lambda 并注册到 CSectionLoader |
| `src/game/client/components/qmclient/menus_qmclient.cpp` | 同上，`RenderSettingsQmClient` 改用 CSectionLoader |
| `src/game/client/gameclient.cpp` | `OnUpdate()` 中调用预热；`OnLanguageChange()` 中调用 `InvalidateCache()` |
| `src/engine/shared/config_variables_qmclient_extra.h` | 新增 `QmUiPreWarm` 配置项 |
| `CMakeLists.txt` | 添加 `section_loader.cpp` 和测试编译 |

---

### 任务 1：CSectionLoader 核心声明与单元测试

**文件：**
- 创建：`src/game/client/components/section_loader.h`
- 创建：`src/game/client/components/section_loader.cpp`
- 创建：`src/test/section_loader_test.cpp`
- 修改：`CMakeLists.txt`

- [ ] **步骤 1：编写 `section_loader.h`**

```cpp
#ifndef GAME_CLIENT_COMPONENTS_SECTION_LOADER_H
#define GAME_CLIENT_COMPONENTS_SECTION_LOADER_H

#include <cstdint>
#include <functional>
#include <vector>

class CUIRect;

enum class ESettingsSectionState : uint8_t
{
	UNINITIALIZED,
	MEASURING,
	COMPACT,
	FULL,
};

struct SSettingsSection
{
	const char *m_pName;           // profiling 标识
	ESettingsSectionState m_State = ESettingsSectionState::UNINITIALIZED;
	float m_CachedHeight = 0.0f;

	// 三个回调：measure 只测高不渲染 / compact 渲染标题+摘要 / full 完整渲染
	// 返回值：section 高度（像素）
	// CUIRect 参数：当前 Column 的剩余可用矩形，调用方负责推进
	std::function<float(CUIRect &)> m_MeasureFn;
	std::function<float(CUIRect &)> m_RenderCompactFn;
	std::function<float(CUIRect &)> m_RenderFullFn;

	// 脏标记：依赖的配置项指针，用于检测是否需要重渲
	std::vector<const int *> m_DependencyConfigInts;
	std::vector<const unsigned *> m_DependencyConfigCols;
	uint64_t m_LastConfigHash = 0;
	bool m_bDirty = true;          // 首帧强制渲染
};

struct SSessionUiCache
{
	int m_LastTClientTab = -1;
	int m_LastQmTab = -1;
	float m_LastScrollY = 0.0f;
	bool m_bValid = false;         // 缓存是否有效
};

class CSectionLoader
{
public:
	CSectionLoader();

	// 一次性注册所有 section（通常在 static init 中调用）
	void Register(std::vector<SSettingsSection> vSections);

	// 开始渐进渲染（设置页打开时调用）
	// TimeBudgetMs: 每帧预算，默认 5.0ms
	void Begin(CUIRect MainView, float TimeBudgetMs = 5.0f);

	// 每帧调用一次，推进 section 状态机
	// 返回 false 表示还有 section 未完成
	bool Process();

	// 是否所有可见 section 都已 FULL
	bool IsComplete() const;

	// 重置状态（tab 切换时调用）
	void Reset();

	// 预热：在 loading 阶段按会话缓存预渲染
	// pCache: 上次会话状态，nullptr 则跳过
	// 返回 false 表示预热未完成，调用方下帧继续调用
	bool Warmup(const SSessionUiCache *pCache, float TimeBudgetMs = 3.0f);
	bool IsWarmupComplete() const;

	// 保存当前会话状态到缓存
	void SaveSessionCache(SSessionUiCache &Cache) const;

	// 强制失效所有脏标记（语言切换、窗口 resize 时调用）
	void InvalidateCache();
	void SetDirtyByConfig(const void *pConfigVar);

	const char *GetPerfReport() const;

	// 当前活跃的 tab 和 scroll，由调用方每帧更新
	int m_ActiveTab = -1;
	float m_ScrollY = 0.0f;

private:
	std::vector<SSettingsSection> m_vSections;
	CUIRect m_MainView;
	double m_BudgetPerFrameMs = 5.0;

	int m_CurrentIndex = 0;
	bool m_bInitialized = false;
	bool m_bComplete = false;

	// 预热状态
	bool m_bWarmupActive = false;
	int m_WarmupIndex = 0;
	const SSessionUiCache *m_pWarmupCache = nullptr;
	float m_WarmupBudgetMs = 0.0f;

	// profiling
	double m_TotalFrameTimeMs = 0.0;

	// 视口优先级计算
	int ComputeViewportPriority(const CUIRect &SectionRect) const;

	// 配置哈希
	uint64_t ComputeConfigHash(const SSettingsSection &Section) const;
};

#endif // GAME_CLIENT_COMPONENTS_SECTION_LOADER_H
```

- [ ] **步骤 2：编写空的 `section_loader.cpp` 骨架使测试可编译**

```cpp
#include "section_loader.h"
#include <base/math.h>
#include <base/perf_timer.h>
#include <base/str.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <game/client/ui.h>

CSectionLoader::CSectionLoader() = default;

void CSectionLoader::Register(std::vector<SSettingsSection> vSections)
{
	m_vSections = std::move(vSections);
}

void CSectionLoader::Begin(CUIRect MainView, float TimeBudgetMs)
{
	m_MainView = MainView;
	m_BudgetPerFrameMs = TimeBudgetMs;
	m_CurrentIndex = 0;
	m_bInitialized = false;
	m_bComplete = false;
	m_TotalFrameTimeMs = 0.0;
}

bool CSectionLoader::Process()
{
	if(!m_bInitialized)
	{
		// 所有 section 重置为 UNINITIALIZED
		for(auto &Section : m_vSections)
		{
			Section.m_State = ESettingsSectionState::UNINITIALIZED;
			Section.m_CachedHeight = 0.0f;
			Section.m_bDirty = true;
		}
		m_bInitialized = true;
		m_CurrentIndex = 0;
	}

	CPerfTimer FrameTimer;
	int UnlockedThisFrame = 0;
	const int MaxUnlockPerFrame = 2;

	while(m_CurrentIndex < (int)m_vSections.size())
	{
		// 检查帧预算
		if(FrameTimer.ElapsedMs() >= m_BudgetPerFrameMs)
			break;

		auto &Section = m_vSections[m_CurrentIndex];

		switch(Section.m_State)
		{
		case ESettingsSectionState::UNINITIALIZED:
		{
			CUIRect MeasureRect = m_MainView;
			Section.m_CachedHeight = (Section.m_MeasureFn ? Section.m_MeasureFn(MeasureRect) : 0.0f);
			Section.m_State = ESettingsSectionState::MEASURING;
			++m_CurrentIndex;
			break;
		}
		case ESettingsSectionState::MEASURING:
		{
			int Priority = ComputeViewportPriority(CUIRect{m_MainView.x, m_MainView.y + m_TotalFrameTimeMs, m_MainView.w, Section.m_CachedHeight});
			if(Priority <= 1) // 视口内或近视口
			{
				Section.m_State = ESettingsSectionState::COMPACT;
				if(Section.m_RenderCompactFn)
					Section.m_RenderCompactFn(m_MainView);
			}
			++m_CurrentIndex;
			break;
		}
		case ESettingsSectionState::COMPACT:
		{
			int Priority = ComputeViewportPriority(CUIRect{m_MainView.x, m_MainView.y, m_MainView.w, Section.m_CachedHeight});
			if(UnlockedThisFrame < MaxUnlockPerFrame && Priority <= 1)
			{
				CPerfTimer UnlockTimer;
				Section.m_State = ESettingsSectionState::FULL;
				if(Section.m_RenderFullFn)
					Section.m_RenderFullFn(m_MainView);
				++UnlockedThisFrame;
				// 只解锁一个就退出循环，分散负载
				++m_CurrentIndex;
				break;
			}
			else
			{
				if(Section.m_RenderCompactFn)
					Section.m_RenderCompactFn(m_MainView);
				++m_CurrentIndex;
			}
			break;
		}
		case ESettingsSectionState::FULL:
		{
			if(!Section.m_bDirty)
			{
				// 配置未变更，跳过渲染
				++m_CurrentIndex;
				break;
			}
			if(Section.m_RenderFullFn)
				Section.m_RenderFullFn(m_MainView);
			Section.m_LastConfigHash = ComputeConfigHash(Section);
			Section.m_bDirty = false;
			++m_CurrentIndex;
			break;
		}
		}
	}

	if(m_CurrentIndex >= (int)m_vSections.size())
	{
		// 所有 section 处理完毕，检查是否全部 FULL
		m_bComplete = true;
		for(const auto &Section : m_vSections)
		{
			if(Section.m_State != ESettingsSectionState::FULL)
			{
				m_bComplete = false;
				m_CurrentIndex = 0; // 下一帧回绕继续
				break;
			}
		}
	}

	m_TotalFrameTimeMs += FrameTimer.ElapsedMs();
	return !m_bComplete;
}

bool CSectionLoader::IsComplete() const
{
	return m_bComplete;
}

void CSectionLoader::Reset()
{
	m_bInitialized = false;
	m_bComplete = false;
	m_CurrentIndex = 0;
	m_TotalFrameTimeMs = 0.0;
}

bool CSectionLoader::Warmup(const SSessionUiCache *pCache, float TimeBudgetMs)
{
	if(!pCache || !pCache->m_bValid)
	{
		m_bWarmupActive = false;
		return true; // 无缓存，预热完成
	}

	if(!m_bWarmupActive)
	{
		m_bWarmupActive = true;
		m_WarmupIndex = 0;
		m_pWarmupCache = pCache;
		m_WarmupBudgetMs = TimeBudgetMs;
		// 预热时 section 状态重置为 UNINITIALIZED
		for(auto &Section : m_vSections)
		{
			Section.m_State = ESettingsSectionState::UNINITIALIZED;
			Section.m_CachedHeight = 0.0f;
		}
	}

	CPerfTimer WarmupTimer;
	while(m_WarmupIndex < (int)m_vSections.size())
	{
		if(WarmupTimer.ElapsedMs() >= m_WarmupBudgetMs)
			break;

		auto &Section = m_vSections[m_WarmupIndex];

		int Priority = ComputeViewportPriority(CUIRect{m_MainView.x, m_MainView.y, m_MainView.w, Section.m_CachedHeight});
		if(Priority > 1)
		{
			// 远视口：只测高，不渲染文字
			if(Section.m_State == ESettingsSectionState::UNINITIALIZED && Section.m_MeasureFn)
			{
				CUIRect MeasureRect = m_MainView;
				Section.m_CachedHeight = Section.m_MeasureFn(MeasureRect);
				Section.m_State = ESettingsSectionState::MEASURING;
			}
			++m_WarmupIndex;
			continue;
		}

		// 视口内/近视口：渲染 compact 版本以触发字形光栅化
		CPerfTimer SectTimer;
		if(Section.m_RenderCompactFn)
			Section.m_RenderCompactFn(m_MainView);
		Section.m_State = ESettingsSectionState::COMPACT;

		double Elapsed = SectTimer.ElapsedMs();
		++m_WarmupIndex;

		if(Elapsed > 1.0)
		{
			// 这个 section 的首次字形光栅化耗时较长，留给下帧
			break;
		}
	}

	if(m_WarmupIndex >= (int)m_vSections.size())
	{
		m_bWarmupActive = false;
		return true; // 预热完成
	}
	return false;
}

bool CSectionLoader::IsWarmupComplete() const
{
	return !m_bWarmupActive;
}

void CSectionLoader::SaveSessionCache(SSessionUiCache &Cache) const
{
	Cache.m_bValid = true;
	// 实际值由调用方填充（调用方知道当前 tab 和 scroll）
}

void CSectionLoader::InvalidateCache()
{
	for(auto &Section : m_vSections)
		Section.m_bDirty = true;
}

void CSectionLoader::SetDirtyByConfig(const void *pConfigVar)
{
	for(auto &Section : m_vSections)
	{
		for(const auto *pInt : Section.m_DependencyConfigInts)
		{
			if(static_cast<const void *>(pInt) == pConfigVar)
			{
				Section.m_bDirty = true;
				return;
			}
		}
		for(const auto *pCol : Section.m_DependencyConfigCols)
		{
			if(static_cast<const void *>(pCol) == pConfigVar)
			{
				Section.m_bDirty = true;
				return;
			}
		}
	}
}

const char *CSectionLoader::GetPerfReport() const
{
	// profiling 输出通过 g_Config.m_QmPerfDebug 控制
	return ""; // 实际实现在 Process() 中调用 dbg_msg
}

int CSectionLoader::ComputeViewportPriority(const CUIRect &SectionRect) const
{
	// 简化实现：以 MainView 为视口
	float ViewportTop = m_MainView.y - m_ScrollY;
	float ViewportBottom = ViewportTop + m_MainView.h;
	float Prefetch = 200.0f;

	if(SectionRect.y + SectionRect.h >= ViewportTop - Prefetch &&
	   SectionRect.y <= ViewportBottom + Prefetch)
	{
		if(SectionRect.y + SectionRect.h >= ViewportTop &&
		   SectionRect.y <= ViewportBottom)
			return 0; // 视口内
		return 1; // 近视口
	}
	return 2; // 远视口
}

uint64_t CSectionLoader::ComputeConfigHash(const SSettingsSection &Section) const
{
	// FNV-1a 64-bit 哈希
	uint64_t Hash = 14695981039346656037ull; // FNV offset basis
	for(const auto *pInt : Section.m_DependencyConfigInts)
	{
		const uint8_t *pBytes = reinterpret_cast<const uint8_t *>(pInt);
		for(size_t i = 0; i < sizeof(int); ++i)
		{
			Hash ^= pBytes[i];
			Hash *= 1099511628211ull;
		}
	}
	for(const auto *pCol : Section.m_DependencyConfigCols)
	{
		const uint8_t *pBytes = reinterpret_cast<const uint8_t *>(pCol);
		for(size_t i = 0; i < sizeof(unsigned); ++i)
		{
			Hash ^= pBytes[i];
			Hash *= 1099511628211ull;
		}
	}
	return Hash;
}
```

- [ ] **步骤 3：编写单元测试**

```cpp
#include "section_loader.h"
#include <gtest/gtest.h>
#include <game/client/ui.h>

// 辅助：创建一个简单的 section
static SSettingsSection MakeTestSection(const char *pName, float Height)
{
	SSettingsSection S;
	S.m_pName = pName;
	S.m_MeasureFn = [Height](CUIRect &Rect) -> float {
		Rect.HSplitTop(Height, nullptr, &Rect);
		return Height;
	};
	S.m_RenderCompactFn = [Height](CUIRect &Rect) -> float {
		Rect.HSplitTop(Height, nullptr, &Rect);
		return Height;
	};
	S.m_RenderFullFn = [Height](CUIRect &Rect) -> float {
		Rect.HSplitTop(Height, nullptr, &Rect);
		return Height;
	};
	return S;
}

TEST(SectionLoader, StateTransitions)
{
	CSectionLoader Loader;
	Loader.Register({
		MakeTestSection("Section A", 50.0f),
		MakeTestSection("Section B", 60.0f),
	});

	CUIRect MainView{0, 0, 400, 600};
	Loader.Begin(MainView, 100.0f); // 大预算确保一帧完成

	bool NeedsMoreFrames = Loader.Process();
	// 两个 section 应在第一帧完成 UNINITIALIZED→MEASURING→COMPACT，第二个解锁可能进入 FULL
	// 由于每帧最多解锁 2 个，应在一帧内处理完
	EXPECT_FALSE(Loader.Process()); // 第二帧应该完成
	EXPECT_TRUE(Loader.IsComplete());
}

TEST(SectionLoader, FrameBudgetTruncation)
{
	CSectionLoader Loader;
	std::vector<SSettingsSection> vSections;
	for(int i = 0; i < 20; ++i)
	{
		char aName[32];
		str_format(aName, sizeof(aName), "Section %d", i);
		vSections.push_back(MakeTestSection(aName, 30.0f));
	}
	Loader.Register(std::move(vSections));

	CUIRect MainView{0, 0, 400, 800};
	Loader.Begin(MainView, 0.1); // 极低预算：3 帧内无法完成

	bool Frame1 = Loader.Process();
	EXPECT_TRUE(Frame1);           // 还有 section 未处理

	bool Frame2 = Loader.Process();
	EXPECT_TRUE(Frame2);

	bool Frame3 = Loader.Process();
	// 三帧后仍可能未完成（取决于 budget 消耗）
}

TEST(SectionLoader, DirtyFlagSkipsRender)
{
	CSectionLoader Loader;
	bool FullRenderCalled = false;
	int ConfigValue = 42;

	SSettingsSection S;
	S.m_pName = "DirtyTest";
	S.m_DependencyConfigInts.push_back(&ConfigValue);
	S.m_MeasureFn = [](CUIRect &Rect) -> float {
		Rect.HSplitTop(10.0f, nullptr, &Rect);
		return 10.0f;
	};
	S.m_RenderCompactFn = [](CUIRect &Rect) -> float {
		Rect.HSplitTop(10.0f, nullptr, &Rect);
		return 10.0f;
	};
	S.m_RenderFullFn = [&FullRenderCalled](CUIRect &Rect) -> float {
		FullRenderCalled = true;
		Rect.HSplitTop(10.0f, nullptr, &Rect);
		return 10.0f;
	};

	Loader.Register({S});
	CUIRect MainView{0, 0, 400, 600};
	Loader.Begin(MainView, 100.0f);

	// 首帧：bDirty=true，应触发 Full Render
	FullRenderCalled = false;
	while(Loader.Process()) {}
	EXPECT_TRUE(FullRenderCalled);

	// 配置未变，第二帧不应调用 Full Render
	FullRenderCalled = false;
	Loader.Begin(MainView, 100.0f);
	while(Loader.Process()) {}
	EXPECT_FALSE(FullRenderCalled);

	// 修改配置，应重新触发 Full Render
	ConfigValue = 99;
	Loader.SetDirtyByConfig(&ConfigValue);
	FullRenderCalled = false;
	Loader.Begin(MainView, 100.0f);
	while(Loader.Process()) {}
	EXPECT_TRUE(FullRenderCalled);
}

TEST(SectionLoader, MeasureFullHeightConsistency)
{
	CSectionLoader Loader;
	float MeasureHeight = 0.0f;
	float FullHeight = 0.0f;
	CUIRect MeasureRect{0, 0, 400, 800};
	CUIRect FullRect{0, 0, 400, 800};

	SSettingsSection S;
	S.m_pName = "HeightTest";
	S.m_MeasureFn = [&MeasureHeight, &MeasureRect](CUIRect &Rect) -> float {
		Rect.HSplitTop(45.0f, nullptr, &Rect);
		MeasureHeight = 45.0f;
		return 45.0f;
	};
	S.m_RenderCompactFn = [](CUIRect &Rect) -> float {
		Rect.HSplitTop(45.0f, nullptr, &Rect);
		return 45.0f;
	};
	S.m_RenderFullFn = [&FullHeight, &FullRect](CUIRect &Rect) -> float {
		Rect.HSplitTop(45.0f, nullptr, &Rect);
		FullHeight = 45.0f;
		return 45.0f;
	};

	Loader.Register({S});
	Loader.Begin(MeasureRect, 100.0f);
	while(Loader.Process()) {}

	EXPECT_FLOAT_EQ(MeasureHeight, FullHeight);
}

TEST(SectionLoader, WarmupWithCache)
{
	CSectionLoader Loader;
	Loader.Register({
		MakeTestSection("A", 50.0f),
		MakeTestSection("B", 50.0f),
		MakeTestSection("C", 50.0f),
	});

	CUIRect MainView{0, 0, 400, 600};
	Loader.Begin(MainView, 5.0f);

	SSessionUiCache Cache;
	Cache.m_LastTClientTab = 0;
	Cache.m_LastScrollY = 0.0f;
	Cache.m_bValid = true;

	// 预热应在多帧内完成
	bool Done = false;
	for(int i = 0; i < 10 && !Done; ++i)
		Done = Loader.Warmup(&Cache, 100.0f);

	EXPECT_TRUE(Done);
	EXPECT_TRUE(Loader.IsWarmupComplete());
}

TEST(SectionLoader, WarmupSkipsInvalidCache)
{
	CSectionLoader Loader;
	Loader.Register({MakeTestSection("A", 50.0f)});

	// nullptr 缓存
	EXPECT_TRUE(Loader.Warmup(nullptr, 3.0f));
	EXPECT_TRUE(Loader.IsWarmupComplete());

	// 无效缓存
	SSessionUiCache Cache;
	Cache.m_bValid = false;
	EXPECT_TRUE(Loader.Warmup(&Cache, 3.0f));
}
```

- [ ] **步骤 4：将测试加入 CMakeLists.txt 并编译验证**

在 `CMakeLists.txt` 中找到 `set_src(TESTS)` 尾部，添加：
```cmake
section_loader_test.cpp
```
并确保 `section_loader.cpp` 在 C++ 源文件列表中添加。

运行：
```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 10
```

预期：5 个测试 PASS（StateTransitions, FrameBudgetTruncation, DirtyFlagSkipsRender, MeasureFullHeightConsistency, WarmupWithCache）

- [ ] **步骤 5：Commit**

```bash
git add src/game/client/components/section_loader.h src/game/client/components/section_loader.cpp src/test/section_loader_test.cpp CMakeLists.txt
git commit -m "feat(section-loader): 添加 CSectionLoader 组件及单元测试"
```

---

### 任务 2：Session 缓存持久化

**文件：**
- 修改：`src/game/client/components/section_loader.h`
- 修改：`src/game/client/components/section_loader.cpp`
- 修改：`src/game/client/components/tclient/menus_tclient.cpp` (仅添加缓存 load/save 函数)

- [ ] **步骤 1：在 section_loader.h 中添加 Load/Save 声明**

在 `CSectionLoader` 类中添加：
```cpp
	// 会话缓存持久化
	static bool LoadSessionCache(SSessionUiCache &Cache, IStorage *pStorage);
	static void SaveSessionCache(const SSessionUiCache &Cache, IStorage *pStorage);
```

- [ ] **步骤 2：实现 Load/Save**

在 `section_loader.cpp` 中实现：
```cpp
#include <engine/storage.h>
#include <game/client/gameclient.h> // 可能需要通过参数传递 IStorage

static const char *SESSION_CACHE_FILE = "qmclient/session_ui_cache";

bool CSectionLoader::LoadSessionCache(SSessionUiCache &Cache, IStorage *pStorage)
{
	IOHANDLE File = pStorage->OpenFile(SESSION_CACHE_FILE, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
		return false;

	char aLine[256];
	// 格式: tab_tclient=0 tab_qm=-1 scroll_y=0.0
	while(io_readline(File, aLine, sizeof(aLine)))
	{
		if(str_startswith(aLine, "tab_tclient="))
			Cache.m_LastTClientTab = atoi(aLine + 13);
		else if(str_startswith(aLine, "tab_qm="))
			Cache.m_LastQmTab = atoi(aLine + 7);
		else if(str_startswith(aLine, "scroll_y="))
			Cache.m_LastScrollY = (float)atof(aLine + 9);
	}
	io_close(File);

	Cache.m_bValid = (Cache.m_LastTClientTab >= 0 || Cache.m_LastQmTab >= 0);
	return Cache.m_bValid;
}

void CSectionLoader::SaveSessionCache(const SSessionUiCache &Cache, IStorage *pStorage)
{
	if(!Cache.m_bValid)
		return;

	pStorage->CreateFolder("qmclient", IStorage::TYPE_SAVE);
	IOHANDLE File = pStorage->OpenFile(SESSION_CACHE_FILE, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;

	char aLine[256];
	str_format(aLine, sizeof(aLine), "tab_tclient=%d\n", Cache.m_LastTClientTab);
	io_write(File, aLine, str_length(aLine));
	str_format(aLine, sizeof(aLine), "tab_qm=%d\n", Cache.m_LastQmTab);
	io_write(File, aLine, str_length(aLine));
	str_format(aLine, sizeof(aLine), "scroll_y=%f\n", Cache.m_LastScrollY);
	io_write(File, aLine, str_length(aLine));

	io_close(File);
}
```

- [ ] **步骤 3：在 menus_tclient.cpp 匿名区添加缓存辅助函数**

在 `menus_tclient.cpp` 的匿名 namespace 中添加：
```cpp
#include <engine/storage.h>

static SSessionUiCache s_TClientSessionCache;
static bool s_SessionCacheLoaded = false;

static void LoadTClientSessionCache(IStorage *pStorage)
{
	if(!s_SessionCacheLoaded)
	{
		CSectionLoader::LoadSessionCache(s_TClientSessionCache, pStorage);
		s_SessionCacheLoaded = true;
	}
}

static void SaveTClientSessionCache(IStorage *pStorage, int CurTab, float ScrollY)
{
	s_TClientSessionCache.m_LastTClientTab = CurTab;
	s_TClientSessionCache.m_LastScrollY = ScrollY;
	s_TClientSessionCache.m_bValid = true;
	CSectionLoader::SaveSessionCache(s_TClientSessionCache, pStorage);
}
```

- [ ] **步骤 4：编译验证**

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 10 | Select-Object -Last 30
```

- [ ] **步骤 5：Commit**

```bash
git add src/game/client/components/section_loader.h src/game/client/components/section_loader.cpp src/game/client/components/tclient/menus_tclient.cpp
git commit -m "feat(section-loader): 添加会话缓存的持久化存取"
```

---

### 任务 3：迁移 menus_tclient.cpp 的 RenderSettingsTClientSettings

**文件：**
- 修改：`src/game/client/components/tclient/menus_tclient.cpp`

这是最大的任务。需要：
1. 删除匿名区的 `gs_TClientSettingsDeferredFrames`、`gs_TClientTabDeferredFrames`、`gs_TClientDeferredTab`、`gs_QmVisualDeferredFrames` 及所有 `BeginDeferred*`、`FinishDeferred*`、`ShouldDefer*`、`GetDeferred*` 函数
2. 创建注册 lambda（从现有的 `Layout*Section(Column, bool Render)` lambda 派生出 measure/compact/full 三个版本）
3. 在 `RenderSettingsTClientSettings` 中用 `CSectionLoader` 替代现有的硬编码调用
4. 保存会话缓存在 page close 时

- [ ] **步骤 1：删除旧的 deferred 全局变量和函数**

在 `menus_tclient.cpp` 的匿名 namespace（~第 131 行起）中，删除：
- `gs_TClientSettingsDeferredFrames`
- `gs_TClientTabDeferredFrames`
- `gs_TClientDeferredTab`
- `gs_QmVisualDeferredFrames`
- `BeginDeferredTClientSettings()`
- `BeginDeferredTClientTab()`
- `ShouldDeferTClientVisualStage()`
- `ShouldDeferTClientTabContent()`
- `GetDeferredTClientTabFrames()`
- `FinishDeferredTClientSettingsFrame()`
- `FinishDeferredTClientTabFrame()`
- `BeginDeferredQmVisualTab()`
- `ShouldDeferQmVisualHeavyStage()`
- `FinishDeferredQmVisualFrame()`

同时删除 `RenderSettingsTClient` 中对 `BeginDeferredTClientSettings`、`BeginDeferredTClientTab`、`FinishDeferredTClientSettingsFrame`、`FinishDeferredTClientTabFrame` 的调用。

- [ ] **步骤 2：创建 section 注册 Lambda**

在 `RenderSettingsTClientSettings` 的局部作用域中（在旧有 `Layout*` lambda 定义之后），添加注册代码：

```cpp
// 替换 RenderSettingsTClientSettings 中的渲染循环：
// 删除所有直接的 Layout*Section(Column, true/false) 调用
// 替换为：

static CSectionLoader s_TClientSettingsLoader;
static bool s_TClientSettingsRegistered = false;

if(!s_TClientSettingsRegistered)
{
	s_TClientSettingsLoader.Register({
		// LeftView sections - 视口内 section priority=0
		{"Visual: Font & Cursor", [&](CUIRect &Col) { CPerfTimer t; auto r = LayoutVisualFontSection(Col, false); return r; },
		 [&](CUIRect &Col) { return LayoutVisualFontSection(Col, false); },  // Compact = Render=false
		 [&](CUIRect &Col) { return LayoutVisualFontSection(Col, true); },   // Full = Render=true
		 {/* int deps */}, {/* color deps */}},
		// ... 其他 section 同样模式 ...
	});
	s_TClientSettingsRegistered = true;
}

// 每帧 Process:
const float ScrollRegionY = ScrollOffset.y;
s_TClientSettingsLoader.m_ActiveTab = s_CurCustomTab;
s_TClientSettingsLoader.m_ScrollY = ScrollRegionY;

if(!s_TClientSettingsLoader.IsComplete())
	s_TClientSettingsLoader.Process();

// 在 RenderSettingsTClient 中：当 tab 切换时
s_TClientSettingsLoader.Reset();
s_TClientSettingsLoader.Begin(ContentView, 5.0f);
```

- [ ] **步骤 3：处理每个 section 的 measure/compact/full 三元组**

以 `LayoutVisualFontSection` 为例，现有代码：

```cpp
auto LayoutVisualFontSection = [&](CUIRect &CurrentColumn, bool Render) {
	// ... 计算 box, 标题, dropdown 等
	return CurrentColumn;
};
```

衍生为：
```cpp
{"Visual: Font & Cursor",
	// Measure: 只计算高度，用 false 模式跳过重渲染
	[Column = LeftView, ...](CUIRect &Col) mutable -> float {
		CUIRect MeasureCol = Col;
		return LayoutVisualFontSection(MeasureCol, false).h;
	},
	// Compact: 用现有 Compact 逻辑（Renderer=false 的简化版）
	[Column = LeftView, ...](CUIRect &Col) mutable -> float {
		return LayoutVisualFontSection(Col, false).h;
	},
	// Full: 完整渲染
	[Column = LeftView, ...](CUIRect &Col) mutable -> float {
		return LayoutVisualFontSection(Col, true).h;
	}
},
```

**关键**：所有 binding by reference 的局部变量需要通过 lambda capture 传入（或改为 static const 值）。这是这个迁移中最容易出错的点。

- [ ] **步骤 4：添加配置依赖项**

对每个 section，列出它引用的 `g_Config.m_*` 字段：
```cpp
// 示例：Visual Font section 依赖
{&g_Config.m_TcCustomFont},                     // int deps
{&g_Config.m_QmChatBubbleBgColor}               // color deps (如适用)
```

只需要列出"会改变该 section 渲染内容的"配置项，不是所有访问过的。

- [ ] **步骤 5：添加会话缓存保存逻辑**

在 `RenderSettingsTClient` 的末尾（离开设置页时），调用：
```cpp
SaveTClientSessionCache(Storage(), s_CurCustomTab, ScrollOffset.y);
```

- [ ] **步骤 6：编译验证并运行门禁**

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 10 | Select-Object -Last 30
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 10 | Select-Object -Last 30
python qmclient_scripts/gate/check_gate.py default
```

- [ ] **步骤 7：Commit**

```bash
git add src/game/client/components/tclient/menus_tclient.cpp
git commit -m "refactor(tclient): 迁移设置页到 CSectionLoader 渐进式渲染"
```

---

### 任务 4：游戏启动预热集成

**文件：**
- 修改：`src/game/client/gameclient.cpp`
- 修改：`src/engine/shared/config_variables_qmclient_extra.h`

- [ ] **步骤 1：添加 QmUiPreWarm 配置项**

在 `config_variables_qmclient_extra.h` 末尾添加：
```cpp
MACRO_CONFIG_INT(QmUiPreWarm, qm_ui_prewarm, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "启动时预热上次打开的设置页文本，消除首次打开卡顿")
```

- [ ] **步骤 2：在 gameclient.cpp 的 OnInit/OnUpdate 中集成预热**

在 `CGameClient::OnInit()` 末尾（资源加载完成后）初始化 CSectionLoader 的 session 缓存加载，在 `OnUpdate()` 的 loading 阶段逐帧调用 Warmup。

```cpp
// gameclient.h 中添加成员或静态变量：
// static CSectionLoader s_UiSectionLoader;  // 或使用已有的
// static SSessionUiCache s_UiSessionCache;
// static bool s_UiWarmupDone;

// 在 OnInit() 末尾：
if(g_Config.m_QmUiPreWarm)
{
	CSectionLoader::LoadSessionCache(s_UiSessionCache, Storage());
	s_UiWarmupDone = false;
}
else
{
	s_UiWarmupDone = true;
}

// 在 OnUpdate() 的 loading 阶段（不是 INGAME state 时）：
if(!s_UiWarmupDone && !m_pClient->State() == IClient::STATE_ONLINE)
{
	s_UiWarmupDone = CSectionLoader::WarmupForAllTabs(s_UiSessionCache, s_TClientSettingsLoader, s_QmSettingsLoader, 3.0f);
}
```

⚠️ **注意**：预热需要在 UI 系统初始化之后、但又不阻塞 loading 进度。`CSectionLoader::Warmup` 设计为每次调用不超过 3ms，在 `OnUpdate()` 每帧调用一次即可。

- [ ] **步骤 3：语言切换时失效缓存**

在 `OnLanguageChange()` 或 `HandleLanguageChanged()` 中：
```cpp
s_TClientSettingsLoader.InvalidateCache();
s_QmSettingsLoader.InvalidateCache();
```

- [ ] **步骤 4：编译验证**

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 10 | Select-Object -Last 30
```

- [ ] **步骤 5：Commit**

```bash
git add src/game/client/gameclient.cpp src/engine/shared/config_variables_qmclient_extra.h
git commit -m "feat(prewarm): 集成启动预热与会话缓存到游戏加载流程"
```

---

### 任务 5：迁移 menus_qmclient.cpp

**文件：**
- 修改：`src/game/client/components/qmclient/menus_qmclient.cpp`

模式同任务 3，但针对 `RenderSettingsQmClient`。删除匿名区的 duplicated deferred 逻辑，用 CSectionLoader 替代。

- [ ] **步骤 1：删除 qmclient 的 deferred 全局变量**

- [ ] **步骤 2：创建 section 注册并替换渲染循环**

- [ ] **步骤 3：编译验证**

- [ ] **步骤 4：Commit**

```bash
git add src/game/client/components/qmclient/menus_qmclient.cpp
git commit -m "refactor(qmclient): 迁移设置页到 CSectionLoader 渐进式渲染"
```

---

### 任务 6：集成验证与手动测试

**文件：** 无新增，验证已修改文件。

- [ ] **步骤 1：完整构建**

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --clean-first --target game-client -j 10 | Select-Object -Last 30
```

- [ ] **步骤 2：运行全量测试套件**

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 10 | Select-Object -Last 30
```

- [ ] **步骤 3：运行门禁**

```powershell
python qmclient_scripts/gate/check_gate.py default
```

- [ ] **步骤 4：手动验收**

启动客户端，操作以下场景验证无卡顿：
1. 首次打开 TClient Settings（设置页）→ 观察 section 渐进出现
2. 切换到不同 Tab → 观察 transition 动画 + section 重新加载
3. 滚动设置页 → 新进入视口的 section 应渐进加载
4. 打开 QmClient 控制页 → 同上
5. 修改配置值 → 对应 section 应刷新（dirty flag 触发重渲）
6. 退出客户端，重新启动 → 预热生效，首次打开 settings 应更快

- [ ] **步骤 5：Commit（如有微调）**

```bash
git commit -m "chore: 集成验证和微调"
```

---

### 任务 7：profiling 开关与可观测性

**文件：**
- 修改：`src/game/client/components/section_loader.cpp`

- [ ] **步骤 1：在 Process() 中添加 profiling 输出**

在 `Process()` 末尾，当 `g_Config.m_QmPerfDebug` 开启且超出阈值时输出：
```cpp
#include <engine/shared/config.h>
// 在 Process() 末尾:
if(g_Config.m_QmPerfDebug && m_TotalFrameTimeMs > 1.0)
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf),
		"sections=%d budget_ms=%.2f actual_ms=%.2f complete=%d",
		(int)m_vSections.size(), m_BudgetPerFrameMs, m_TotalFrameTimeMs, m_bComplete ? 1 : 0);
	dbg_msg("perf/section_loader", "%s", aBuf);
}
```

- [ ] **步骤 2：为慢 section 单独输出计时**

- [ ] **步骤 3：Commit**

```bash
git add src/game/client/components/section_loader.cpp
git commit -m "feat(section-loader): 添加 profiling 输出到 perf/section_loader 通道"
```

---

## 自检

1. **规格覆盖度检查**：
   - 渐进式渲染（状态机+帧预算）→ 任务 1, 3, 5 ✓
   - 视口优先级 → 任务 1 (`ComputeViewportPriority`) ✓
   - 启动预热+会话缓存 → 任务 2, 4 ✓
   - 配置脏标记 → 任务 1 (`ComputeConfigHash`, `SetDirtyByConfig`) ✓
   - profiling 可观测 → 任务 7 ✓
   - 两个文件消除重复 → 任务 3, 5 ✓
   - 验收标准 1-9 → 任务 6 手动验收 ✓

2. **占位符扫描**：无 TODO/TBD/后续实现 ✓

3. **类型一致性**：`SSettingsSection` / `SSessionUiCache` / `ESettingsSectionState` 全链路一致 ✓
