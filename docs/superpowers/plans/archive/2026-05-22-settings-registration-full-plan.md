# 设置页 Section 注册模式 — 全量实现计划

> **文档已过时** — 本文档内容不再反映当前代码状态，仅供参考。

> **前置状态**：`2026-05-22-settings-progressive-render-plan.md` 的 Task 1 (CSectionLoader 核心) 已完成。
> 本计划覆盖剩余全部工作，将轻量帧计数升级为完整 section 注册 + 脏标记 + 预热。

**架构**：CSectionLoader 四态机 (UNINITIALIZED→MEASURING→COMPACT→FULL) + 视口优先级 + 会话缓存 I/O + 配置脏标记

---

## 前置修复 (当前 session)

- [ ] **修复 3.1**：`Process()` 中 section y-position 累积前序高度
- [ ] **修复 3.2**：`m_pWarmupCache` 声明+赋值（已做，待验证编译）

---

## 任务 1：删除旧 deferred 代码并修复集成

**文件**：`src/game/client/components/tclient/menus_tclient.cpp`

**当前问题**：新旧两套倒计时机制并行运行。`gs_TClientSettingsDeferredFrames` 设 6 后因 `FinishDeferredTClientSettingsFrame()` 无调用点而永不递减，导致 `ShouldDeferTClientVisualStage()` 永远返回 true，重型控件永不渲染。

- [ ] **Step 1**：删除 4 个旧全局变量
  - `gs_TClientSettingsDeferredFrames`
  - `gs_TClientTabDeferredFrames`
  - `gs_TClientDeferredTab`
  - `gs_QmVisualDeferredFrames`

- [ ] **Step 2**：删除 9 个旧函数
  - `BeginDeferredTClientSettings()` → 已由 `s_SettingsLoader.BeginLightweight(6, 5.0f)` 替代
  - `BeginDeferredTClientTab()` → 已由 `s_SettingsLoader.BeginLightweight()` 替代
  - `ShouldDeferTClientVisualStage()` → 替换调用点为 `s_SettingsLoader.GetFramesRemaining()`
  - `ShouldDeferTClientTabContent()` → 同
  - `GetDeferredTClientTabFrames()` → 同
  - `FinishDeferredTClientSettingsFrame()` → 已由 `s_SettingsLoader.Process()` 替代
  - `FinishDeferredTClientTabFrame()` → 同
  - `BeginDeferredQmVisualTab()` → 后续 qmclient 迁移时替换
  - `ShouldDeferQmVisualHeavyStage()` → 同上
  - `FinishDeferredQmVisualFrame()` → 同上

- [ ] **Step 3**：替换 `ShouldDeferTClientVisualStage()` 的 4 个调用点（第 1021-1024 行）
  ```cpp
  // 旧：
  const bool DeferVisualFontHeavyControls = ShouldDeferTClientVisualStage(ScrollOffset.y, 3);
  // 新：
  const bool DeferVisualFontHeavyControls = s_SettingsLoader.GetFramesRemaining() >= 3 && absolute(ScrollOffset.y) <= 1.0f;
  ```
  对 `DeferVisualNameplateHeavyControls`、`DeferVisualEffectsHeavyControls`、`DeferRightHeavySections` 同样处理。

- [ ] **Step 4**：删除所有对已删除函数的调用（`RenderSettingsTClient` 中的 `BeginDeferredTClientSettings`、`FinishDeferredTClientTabFrame` 等）

- [ ] **Step 5**：构建验证 → `run_cxx_tests` 全量通过

- [ ] **Step 6**：Commit

---

## 任务 2：修复 Process() y-position 累积

**文件**：`src/game/client/components/section_loader.h`, `section_loader.cpp`

**问题**：所有 section 的 `SectionRect.y` 都用 `m_MainView.y`，`ComputeViewportPriority` 对所有 section 返回相同优先级。

- [ ] **Step 1**：在私有成员中添加 `float m_AccumulatedY = 0.0f;`
- [ ] **Step 2**：`Begin()` 中重置 `m_AccumulatedY = m_MainView.y`
- [ ] **Step 3**：`Process()` 中每次 measure/compact/full 渲染后累加：
  ```cpp
  SectionRect.y = m_AccumulatedY;
  // ... render ...
  m_AccumulatedY += Section.m_CachedHeight;
  ```
- [ ] **Step 4**：`ComputeViewportPriority()` 使用 SectionRect 的实际 y（而非固定 `m_MainView.y`）
- [ ] **Step 5**：更新单元测试以验证 y-position 累积
- [ ] **Step 6**：Commit

---

## 任务 3：单 Section 试注册 (LayoutVisualFontSection)

**文件**：`src/game/client/components/tclient/menus_tclient.cpp`

**策略**：先迁移一个 section 验证完整管线，再推广到全部。

- [ ] **Step 1**：在匿名区定义 static CSectionLoader 和注册状态（替换旧 `s_SettingsLoader` 的轻量模式）
  ```cpp
  static CSectionLoader s_TClientSettingsLoader;
  static bool s_TClientSettingsRegistered = false;
  ```

- [ ] **Step 2**：从 `LayoutVisualFontSection` 派生三层 lambda：
  - `MeasureFn`：调用布局逻辑但不渲染文字，仅返回高度
  - `RenderCompactFn`：渲染摘要（当前字体名 + 简单标签）
  - `RenderFullFn`：完整交互 UI
  - 传入 `m_DependencyConfigInts`：`[&g_Config.m_TcCustomFont]`

- [ ] **Step 3**：在 `RenderSettingsTClientSettings` 中替换 VisualFontSection 的旧式调用：
  ```cpp
  // 首次注册
  if(!s_TClientSettingsRegistered) {
      s_TClientSettingsLoader.Register({ visualFontSection });
      s_TClientSettingsRegistered = true;
  }
  // 每帧
  s_TClientSettingsLoader.m_ScrollY = ScrollOffset.y;
  if(!s_TClientSettingsLoader.IsComplete())
      s_TClientSettingsLoader.Process();
  ```
  其他 section 保持旧式调用不变（混合模式过渡）。

- [ ] **Step 4**：构建 + 运行全量测试

- [ ] **Step 5**：手动验收 — 打开 TClient Settings，观察 Visual Font section 从摘要渐变为完整

- [ ] **Step 6**：Commit

---

## 任务 4：全部 Section 迁移

**文件**：`src/game/client/components/tclient/menus_tclient.cpp`

**目标**：将任务 3 的模式推广到所有 ~11 个 `Layout*Section` lambda。

每个 section 需处理：
1. 从 `Layout*Section(Column, bool Render)` 派生 measure/compact/full
2. 列出依赖的 `g_Config.m_*` 字段
3. 保留 subsection 级的 DeferXxxHeavyControls 逻辑（在 RenderFullFn 内部）

**Section 清单** (TClient Settings)：
- Visual: Font & Cursor
- Visual: Nameplate
- Visual: Effects & Particles
- Visual: Tee Status Bar
- Visual: Tile Outlines
- HUD & UI
- Chat & Messages
- Audio & Sound
- Misc & Advanced

**风险控制**：
- 每个 section 单独 commit，出现问题时独立 revert
- 每 2-3 个 section 跑一次全量测试
- 保留旧 `Layout*Section` lambda 直到迁移完成（防止未迁移的依赖断裂）

- [ ] **Step 1**：注册所有 section 到 s_TClientSettingsLoader
- [ ] **Step 2**：删除旧式直接调用，改为 Process() 驱动
- [ ] **Step 3**：处理所有依赖配置项
- [ ] **Step 4**：分批 commit，每批 2-3 section + 测试

---

## 任务 5：启用配置脏标记

**文件**：`section_loader.h`（已有），`menus_tclient.cpp`（填充依赖项）

**背景**：`CSectionLoader` 已实现 `ComputeConfigHash()` 和 `m_DependencyConfigInts/Cols`，只需在各 section 注册时传入正确的依赖指针。

- [ ] **Step 1**：为每个已注册 section 填入准确的 `m_DependencyConfigInts/Cols`
- [ ] **Step 2**：在 `SetDirtyByConfig()` 被调用（配置变更时）验证脏标记传播
- [ ] **Step 3**：新增测试：修改配置 → 对应 section 重渲，不相关 section 跳过
- [ ] **Step 4**：Commit

---

## 任务 6：游戏启动预热集成

**文件**：`src/game/client/gameclient.cpp`, `gameclient.h`

**前提**：`QmUiPreWarm` 配置项已在 `config_variables_qmclient_extra.h` 中定义。

- [ ] **Step 1**：在 `gameclient.h` 添加成员
  ```cpp
  class CSectionLoader;
  struct SSessionUiCache;
  // ...
  std::unique_ptr<CSectionLoader> m_pUiWarmupLoader;
  SSessionUiCache m_UiSessionCache;
  bool m_UiWarmupPending = false;
  ```

- [ ] **Step 2**：`OnInit()` 末尾加载会话缓存
  ```cpp
  if(g_Config.m_QmUiPreWarm) {
      CSectionLoader::LoadSessionCache(m_UiSessionCache, "qmclient/session_ui_cache", Storage());
      m_UiWarmupPending = true;
  }
  ```

- [ ] **Step 3**：`OnUpdate()` loading 阶段逐帧预热
  ```cpp
  if(m_UiWarmupPending && Client()->State() != IClient::STATE_ONLINE) {
      m_UiWarmupPending = !m_pUiWarmupLoader->Warmup(&m_UiSessionCache, 3.0f);
  }
  ```

- [ ] **Step 4**：`OnLanguageChange()` 中调用 `InvalidateCache()`

- [ ] **Step 5**：离开设置页时保存会话缓存

- [ ] **Step 6**：手动验收 — 重启客户端，首次打开设置页无卡顿

- [ ] **Step 7**：Commit

---

## 任务 7：QmClient 设置页迁移

**文件**：`src/game/client/components/qmclient/menus_qmclient.cpp`

与任务 3-4 相同模式，针对 `RenderSettingsQmClient`：

- [ ] **Step 1**：删除 qmclient 的 deferred 全局变量/函数
- [ ] **Step 2**：创建 section 注册并替换渲染循环
- [ ] **Step 3**：Commit

---

## 任务 8：集成验证与收尾

- [ ] 完整构建（clean-first）
- [ ] `run_cxx_tests` 全量通过
- [ ] `check_gate.py default` 通过
- [ ] 手动验收：首次打开、Tab 切换、滚动、配置变更、重启预热

---

## 依赖图

```
前置修复 (3.1+3.2)
    │
    ▼
任务 1 (删除旧代码)
    │
    ▼
任务 2 (y-position 修复)
    │
    ├────── 任务 3 (单 Section 试注册)
    │           │
    │           ▼
    │       任务 4 (全部 Section 迁移) ──→ 任务 5 (脏标记)
    │           │
    │           ▼
    └────── 任务 6 (预热集成)
                │
                ▼
            任务 7 (qmclient 迁移)
                │
                ▼
            任务 8 (集成验证)
```

任务 2 可与任务 1 并行；任务 4 和 5 有顺序依赖；任务 6 依赖任务 4 完成后的 CSectionLoader 实例。
