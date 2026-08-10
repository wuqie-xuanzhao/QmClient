# 设置页渐进式渲染重构设计

> **文档已过时** — 本文档内容不再反映当前代码状态，仅供参考。

## 背景

QmClient 的 TClient 设置页（`RenderSettingsTClientSettings`）和 QmClient 控制页（`RenderSettingsQmClient`）在首次打开或切换 Tab 时存在明显卡顿。当前"首帧延迟 + Compact 渲染"方案存在架构缺陷，用户体验比静态渲染更差。

## 现状分析

### 现有方案

```
BeginDeferredTClientSettings() → gs_TClientSettingsDeferredFrames = 6
每帧 FinishDeferredTClientSettingsFrame() → --countdown

各 section 按硬编码阈值判断是否 Compact:
  CompactVisualFont     = frames >= 6
  CompactVisualNameplate= frames >= 5
  CompactHudSection     = frames >= 4
  CompactEffects+TeeStatusBar = frames >= 3
  ...
  frames == 0 → 全部解锁 → 卡顿帧
```

### 问题诊断

1. **全局同步计数器**：20+ 个 section 的命运绑在 `gs_TClientSettingsDeferredFrames` 一根弦上。阈值撞车意味着多个重量级 section 在同一帧从 Compact→Full。
2. **不可控的解锁节奏**：计数器单调递减，无法根据实际渲染成本动态调整。某个 1ms 的 section 和另一个 15ms 的 section 共享相同的"第 N 帧解锁"逻辑。
3. **代码重复**：`menus_tclient.cpp` 和 `menus_qmclient.cpp` 各自复制了完全相同的 deferred 管理逻辑。
4. **控制与渲染耦合**：deferred 判断散落在渲染 Lambda 内部，每次新增 section 都需要同时修改控制逻辑和渲染逻辑。

## 目标

1. **消除卡顿**：单帧 UI 渲染时间稳定在目标帧预算内（默认 5ms，60fps 对应 16.6ms，留出充足余量给游戏渲染）
2. **渐进式可见**：section 按优先级逐步从占位→完整过渡，用户可感知进度
3. **消除重复**：两个文件共享同一套 section 管理管线
4. **可扩展**：新增 section 只需注册，不需要改控制逻辑
5. **可观测**：内置 profiling，记录每个 section 的渲染耗时

## 设计

### 核心概念：Section 注册表 + 统一 Loader

每个 settings page 的 section 从"硬编码 lambda 调用"变为"注册表条目"。一个统一的 `CSectionLoader` 按帧预算逐 section 推进渲染。

### Section 状态机

```
                 BeginLoader()
                      │
                      ▼
              ┌───────────────┐
              │   MEASURING   │  ← 只调用 measure lambda 获取高度
              └───────┬───────┘
                      │
                      ▼
              ┌───────────────┐
              │    COMPACT    │  ← 调用 compact render lambda（标题+摘要行）
              └───────┬───────┘
                      │ (帧预算允许 & 在解锁序列中轮到)
                      ▼
              ┌───────────────┐
              │     FULL      │  ← 调用完整 render lambda
              └───────────────┘
```

状态转换由 `CSectionLoader::Process()` 每帧驱动。转换策略：
- **MEASURING→COMPACT**：立即（无渲染成本，仅高度计算）
- **COMPACT→FULL**：受帧预算 + 每次最多 N 个 section 的限制

### 数据结构

```cpp
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
    int m_Priority;                // 解锁优先级（视口内=0，近视口=1，远视口=2+）
    ESettingsSectionState m_State;
    float m_CachedHeight;          // measure 结果缓存

    // 三个回调：measure / compact render / full render
    // 返回值：section 高度
    std::function<float(CUIRect &)> m_MeasureFn;
    std::function<float(CUIRect &)> m_RenderCompactFn;
    std::function<float(CUIRect &)> m_RenderFullFn;
};

class CSectionLoader
{
public:
    void Register(std::vector<SSettingsSection> vSections); // 一次性注册
    void Begin(CUIRect MainView, float TimeBudgetMs);
    void Process();
    bool IsComplete() const;            // 所有 section 是否都已 FULL
    void Reset();

    // 预热：在 loading 阶段按会话缓存精准预渲染
    // SessionCache: 上次会话的 tab/scroll 状态，nullptr 表示无缓存
    // TimeBudgetMs: 每帧预热时间预算（建议 2-4ms）
    // 返回 false 表示预热尚未完成，调用方应下帧继续调用
    bool Warmup(const struct SSessionUiCache *pCache, float TimeBudgetMs);
    void InvalidateCache();              // 强制失效所有 FBO 缓存（语言切换、窗口 resize）
    void SetDirtyByConfig(const void *pConfigVar);  // 配置变更时标记相关 section 为脏
    bool IsWarmupComplete() const;

    void SaveSessionCache(struct SSessionUiCache &Cache) const;

    const char *GetPerfReport() const;  // profiling 摘要

private:
    std::vector<SSettingsSection> m_vSections;
    bool m_bInitialized;
    int m_CurrentIndex;                 // 当前处理到的 section 索引
    double m_BudgetPerFrameMs;

    bool m_bWarmupActive;               // 是否在预热阶段
    int m_WarmupIndex;                  // 预热进度
};
```

### 帧处理流程

```
每帧 Process():
  1. 如果 m_bInitialized == false:
     - 所有 section 入队，状态 = UNINITIALIZED
     - m_bInitialized = true

  2. 按优先级排序 m_vSections（视口优先级动态调整）

  3. 帧预算循环:
     while (剩余预算 > 0 && 还有 section 未处理):
       section = m_vSections[m_CurrentIndex++]

       if section.m_State == UNINITIALIZED:
         section.m_CachedHeight = section.m_MeasureFn()
         section.m_State = MEASURING

       elif section.m_State == MEASURING:
         如果视口内:
           section.m_State = COMPACT  // 先显示标题行
         否则（远视口）:
           跳过（不占预算）

       elif section.m_State == COMPACT:
         如果帧预算允许且本轮解锁计数未达上限:
           section.m_State = FULL
           开始计时 section 的 full render
           预算 -= 实际耗时
           break  // 每帧只解锁 1-2 个 section
         else:
           渲染 compact 版本

       elif section.m_State == FULL:
         渲染 full 版本（此时成本已在解锁帧摊还）

  4. 如果所有可见 section 都已 FULL，标记 IsComplete()
     此时后续帧直接跳过 Process()，走快速路径
```

### 视口优先级

```cpp
int ComputeViewportPriority(const CUIRect &SectionRect, const CUIRect &Viewport)
{
    if (SectionRect 在视口内)  return 0;   // 最高
    if (SectionRect 在视口上下 200px 内) return 1;
    return 2 + static_cast<int>(abs distance / 200);  // 递增
}
```

视口内 section 优先解锁，用户立即看到变化；远远超出视口的 section 保持 COMPACT 直到用户滚动靠近。

### 渲染整合

现有 `RenderSettingsTClientSettings` 的调用模型从：

```cpp
// 旧：硬编码 Lambda 调用
LayoutVisualFontSection(Column, true);
LayoutInputSection(Column, true);
// ... 20+ 个
```

变为：

```cpp
// 新：注册表 + 统一 Loader
static CSectionLoader s_SettingsLoader;
static std::once_flag s_SettingsInit;

std::call_once(s_SettingsInit, [&]() {
    s_SettingsLoader.Register({
        {"Visual: Font & Cursor",  0, LayoutFontMeasure,     LayoutFontCompact,     LayoutFontFull},
        {"Visual: Nameplates",     0, LayoutNameplateMeasure, LayoutNameplateCompact, LayoutNameplateFull},
        {"Visual: Effects",        1, LayoutEffectsMeasure,    LayoutEffectsCompact,   LayoutEffectsFull},
        {"Input",                  0, LayoutInputMeasure,     LayoutInputCompact,     LayoutInputFull},
        {"Anti-Latency",           0, LayoutAntiLatencyMeasure, LayoutAntiLatencyCompact, LayoutAntiLatencyFull},
        // ...
    });
});

s_SettingsLoader.Begin(MainView, 5.0 /* ms budget */);
s_SettingsLoader.Process();
```
### 启动预热（Pre-Warming）

上述渐进式渲染解决了打开设置页时的卡顿，但**首次文本渲染**的瓶颈仍然存在——中文字体字形光栅化（glyph rasterization）在第一次渲染某个字时需要生成纹理，这本身很耗时。预热策略在游戏加载阶段提前触发字形的光栅化，让设置页打开时连 COMPACT 渲染都是秒开的。

#### 为什么之前的预热方案会卡死

上次尝试在加载阶段调用完整渲染路径，触发了所有 section 的全量字体光栅化。中文字库有数千个常用字形，全量光栅化在单帧内完成必然导致无响应。而实际上每个设置页实际用到的汉字只有几十个。

#### 精准预热：基于会话缓存的按需预热

**原理**：只预热**上次会话实际渲染过**的 section 中的文本。字体光栅化缓存（glyph atlas）是持久化的——一旦某个汉字的字形被光栅化过，后续渲染就是零成本纹理查询。

**会话状态缓存**（~100 bytes，写入配置或独立文件）：
```
struct SessionUiCache {
    int m_LastTClientTab;      // 上次打开的 TClient tab
    int m_LastQmTab;           // 上次打开的 QmClient tab
    float m_LastScrollY;       // 上次 scroll 位置
};
```

**预热流程**（在 `CGameClient::OnInit()` 资源加载完成后，loading 阶段内执行）：

```
CSectionLoader::Warmup(SessionCache, TimeBudgetPerFrameMs = 3.0):
  加载 SessionCache，确定上次活跃的 tab 和视口内的 section 列表

  每帧循环:
    while (剩余预热预算 > 0 && 还有 section 待预热):
      section = 按视口优先级取下一个

      如果是远视口 section:
        只调用 measure（不渲染文字，跳过）
        continue

      如果是视口内或近视口 section:
        调用 compact render（渲染标题 + 摘要文本行）
        // 这一行会触发所有用到的字形光栅化
        // 光栅化结果写入 glyph atlas 缓存，后续永久加速

      预热预算 -= 实际耗时
      if 实际耗时 > 1ms:
        // 这个 section 的 compact render 在预热状态下已经很贵
        // 说明首次字形光栅化正在进行，那就让它完成
        // 但不继续预热下一个，留给下帧
        break
```

**与渐进式渲染的协同**：

1. 启动预热：用 COMPACT 路径渲染上次视口内的 section → 字形缓存已热
2. 用户打开设置页：CSectionLoader 正常渐进渲染 → MEASURING 几乎零成本（高度计算不碰文字），COMPACT 渲染时字形已缓存，瞬间完成
3. FULL 解锁：按帧预算逐 section 解锁，单帧负载可预期

预热阶段的时间预算（3ms）比正常渲染（5ms）更保守，因为 loading 期间游戏仍在渲染背景和 UI，不能占用太多帧时间。

#### 配置项

```
MACRO_CONFIG_INT(QmUiPreWarm, qm_ui_prewarm, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "启动时预热上次打开的设置页文本")
```

默认开启。用户如遇到兼容问题可关闭回退到纯渐进式渲染。


### 稳态缓存（Steady-State Caching）

渐进式渲染和预热解决了**打开瞬间**的卡顿。但设置页在稳态（所有 section 都已 FULL）下，每帧仍然重渲所有可见 section 的文本标签。当用户快速滚动时，即使 `IsSectionVisible` 裁剪了不可见 section，视口内的 section 仍在逐帧做文本排版和 Draw Call。

稳态缓存做两件事：

#### 1. 配置脏标记（Config Dirty Tracking）

每个 section 注册时需要列出它依赖的配置项。渲染时，Loader 计算这些配置项的哈希，与上一帧比对：

```
struct SSettingsSection {
    // ... 原有字段
    std::vector<const int *> m_DependencyConfigInts;   // 依赖的 int 配置
    std::vector<const unsigned *> m_DependencyConfigCols; // 依赖的 color 配置
    uint64_t m_LastConfigHash;                           // 上一帧的配置哈希
    bool m_bDirty;                                       // 本帧是否需要重渲
};
```

`Process()` 中：如果 section 处于 FULL 状态且 `m_bDirty == false` 且 FBO 缓存有效 → **跳过渲染 lambda 调用**，直接 blit 缓存的纹理。

配置变更检测的开销极低：每个 section 只做一次 FNV-1a 哈希（或用更简单的 XOR），在 Process() 的预算循环外完成，不影响渲染预算。

唯一例外：编辑框（`CLineInput`）所在的 section 始终标记为 dirty，因为文本内容和光标需要逐帧更新。

#### 2. 静态文本 FBO 缓存

DDNet 渲染管线已有 `IGraphics::RenderToTexture` 能力。每个 section 在首次 FULL 渲染完成后，将其渲染结果捕获到一个 FBO：

```
IGraphics::CTextureHandle m_CachedTexture;  // section 内容的 FBO
CUIRect m_CachedRect;                       // FBO 对应的屏幕区域
```

后续帧中，如果 section 未被标记为 dirty：
- 不调用渲染 lambda
- 直接将 `m_CachedTexture` blit 到 `m_CachedRect` 位置
- 成本降为：1 次纹理绑定 + 1 次四边形 Draw Call

FBO 缓存的失效条件：
- section 的配置值变更（检测到 dirty）
- 窗口大小变化（rect 不再匹配）
- 语言切换

#### 内存估算

以最重的 HUD section（~600px 高 × ~400px 宽）为例：
- FBO: 400×600×4 = 960KB（RGBA8）
- 20 个 section 同时缓存 ≈ 8-12MB

实际只缓存视口内 + 近视口的 section（~4-6 个），总计 < 4MB，对现代硬件零压力。

#### 与渐进式渲染的关系

三层叠加，互不冲突：
1. **预热层**：消除冷启动的字形光栅化（loading 阶段）
2. **渐进层**：控制解锁节奏，单帧 ≤5ms（打开瞬间）
3. **缓存层**：稳态静默，只 blit 纹理（打开后）

现有的 `Layout*Section(Column, bool Render)` Lambda 几乎不需要修改——它的 `Render=false` 就是 Compact 行为，`Render=true` 就是 Full 行为。Compact 可以进一步简化（只显示标题行和摘要文本）。

### 对 qmclient 菜单的复用

`CSectionLoader` 是通用组件，`menus_qmclient.cpp` 的 `RenderSettingsQmClient` 同样注册自己的 section 列表即可，零额外 plumbing。

### Profiling 支持

Loader 内置计时，通过 `g_Config.m_QmPerfDebug` 开关：
```
[perf/section_loader] tab=settings sections=18 full=12/18 budget_used_ms=4.2
[perf/section_loader] tab=settings section=Visual:Effects full_render_ms=3.1
```

超过阈值的 section 单独记录，便于发现性能回归。

## 改动范围

### 新增文件

| 文件 | 说明 |
|------|------|
| `src/game/client/components/section_loader.h` | CSectionLoader 声明 |
| `src/game/client/components/section_loader.cpp` | CSectionLoader 实现 |
| `src/test/section_loader_test.cpp` | 单元测试 |

### 修改文件

| 文件 | 改动 |
|------|------|
| `menus_tclient.cpp` | 移除匿名区 deferred 全局变量和函数，`RenderSettingsTClientSettings` 改用 CSectionLoader；添加会话缓存保存/加载逻辑 |
| `menus_qmclient.cpp` | 同上，移除 `gs_QmVisualDeferredFrames` 等，改用 CSectionLoader |
| `gameclient.cpp` | `OnUpdate()` 中调用 `CSectionLoader::Warmup()`，loading 阶段逐帧推进预热 |
| `config_variables_qmclient_extra.h` | 新增 `QmUiPreWarm` 配置项 |
| `CMakeLists.txt` | 添加 `section_loader.cpp` 和 `section_loader_test.cpp` |

### 不移改

- 单个 section 的 Layout Lambda 保持不变（它们的双态设计已经是正确的）
- `IsSectionVisible` / `SkipSection` 视口裁剪保持不变
- 高度缓存机制保持不变

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| Section 注册顺序错误导致布局错位 | 每个 section 返回自己的高度，Loader 自动推进 Column 位置，不依赖手动 `HSplitTop` |
| 某个 section 的 measure 和 full render 高度不一致 | 单元测试验证：同 section 的 measure 高度 == full render 高度 |
| 帧预算在不同机器上表现不一 | 默认 5ms 足够保守（60fps 帧周期 16.6ms）；设为 `g_Config` 可调项 |
| 重构面大，引入回归 | 分步实施：先 tclient，后 qmclient；每步都跑全量测试门禁 |
| 预热导致 loading 阶段耗时增加 | 预热仅在 loading 空闲帧执行（不阻塞资源加载）；可配置关闭 |
| 会话缓存与当前窗口布局不一致 | 预热只做字形光栅化，不依赖布局结果；布局在 `Process()` 时重新计算 |


## 验收标准

1. 打开 TClient Settings 页时，前 3 帧内可见 section 出现在视口内，后续 section 逐帧渐进出现
2. 单帧 UI 渲染时间稳定在 ~5ms 内（通过 `m_QmPerfDebug` 观察）
3. 所有现有 section 的功能与交互不变（每个 widget 的点击、拖拽、输入行为不变）
4. 打开 QmClient 控制页同样满足以上标准
5. 滚动时，离开视口的 section 不会引发额外渲染开销
6. 新增 section 只需在注册表加一行，不碰 Loader 代码
7. 单元测试覆盖：section 状态机转换、高度一致性、帧预算截断
8. 开启 `qm_ui_prewarm` 后，loading 阶段每帧预热耗时不超过 3ms
9. 会话缓存文件在设置页关闭时正确写入，下次启动可恢复
10. 稳态下滚动时，视口内 section 如配置未变更，不再逐帧重渲（profiling 确认 `Full Render` 调用数为 0，仅 FBO blit）
11. FBO 缓存失效时机正确：配置变更、窗口 resize、语言切换后自动重建