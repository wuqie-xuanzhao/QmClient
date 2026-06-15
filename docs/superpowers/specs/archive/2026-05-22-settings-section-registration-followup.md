# 设置页 Section 注册模式 — 后续规格

> **文档已过时** — 本文档内容不再反映当前代码状态，仅供参考。

> 状态：规划中 | 上一阶段：`2026-05-22-settings-progressive-render-plan.md` (轻量帧计数模式已完成)

## 目标

将当前轻量级帧计数模式升级为完整 Section 注册模式，实现：
1. 每 section 独立的 measure/compact/full 三态渲染
2. 视口优先级驱动的逐 section 解锁（不再全局统一倒计时）
3. 配置脏标记跳过无变化 section 的重渲
4. 游戏启动时基于会话缓存的预热（`gameclient.cpp` 集成）

## 当前状态

- `CSectionLoader` 已实现完整的四态机 + 会话缓存 + 脏标记 + 轻量帧计数
- `menus_tclient.cpp` 仅使用轻量帧计数模式（`BeginLightweight`），旧 `Layout*Section` lambdas 未改动
- `QmUiPreWarm` 配置项已就位
- 768 全量测试通过，零回归

## 分步规划

### 阶段 A：单 Section 试注册（低风险，验证可行性）

**范围：** 仅迁移 `LayoutVisualFontSection` 一个 section。

**改动：**
- 从 `LayoutVisualFontSection(Column, bool Render)` 派生三层 lambda：
  - `MeasureFn`：调用 `LayoutVisualFontSection(Column, false)`，收集高度不渲染
  - `RenderCompactFn`：渲染摘要版（标题 + 当前字体的纯文本名）
  - `RenderFullFn`：调用 `LayoutVisualFontSection(Column, true)` 并 `DeferHeavyControls=false`
- `CSectionLoader::Register()` 注册该 section
- `Process()` 每帧驱动状态转换

**依赖项配置：** `[&g_Config.m_TcCustomFont]` 等

**验证：**
1. 单元测试：注册单 section，验证 MEASURING→COMPACT→FULL 完整路径
2. 手动：打开设置页→TClient Settings，观察 VisualFont section 从摘要渐变为完整交互

### 阶段 B：全部 Section 迁移

将阶段 A 模式推广到所有 `Layout*Section` lambdas（约 11 个）。

**风险点：** 每个 `Layout*Section` lambda 内部有 subsection 级的 `DeferXxxHeavyControls` 逻辑，需要决定：
- 方案 1：保留 subsection 级 defer（section 级 FULL 后，subsection 仍可 defer）
- 方案 2：section 级 FULL 后 section 内全部释放

建议方案 1，向后兼容。

### 阶段 C：启用配置脏标记

每个 section 注册时传入 `m_DependencyConfigInts` / `m_DependencyConfigCols`，CSectionLoader 自动比对哈希。

### 阶段 D：游戏启动预热集成

**文件：** `src/game/client/gameclient.cpp`

**改动：**
```cpp
// OnInit() 末尾
if(g_Config.m_QmUiPreWarm) {
    CSectionLoader::LoadSessionCache(s_UiSessionCache, "qmclient/session_ui_cache", Storage());
    s_UiWarmupPending = true;
}

// OnUpdate() 的 loading 阶段
if(s_UiWarmupPending && m_ClientState != IClient::STATE_ONLINE) {
    // 预热必须在 UI init 之后、但又不阻塞 loading
    s_UiWarmupPending = !s_TClientSettingsLoader.Warmup(&s_UiSessionCache, 3.0f);
}

// 离开设置页时保存会话
SaveTClientSessionCache(Storage(), s_CurCustomTab, ScrollOffset.y);

// HandleLanguageChanged()
s_TClientSettingsLoader.InvalidateCache();
s_QmSettingsLoader.InvalidateCache();
```

## 时间线建议

| 阶段 | 工作量 | 风险 |
|------|--------|------|
| A：单 Section 试注册 | 小（1-2 小时） | 低，可随时回退 |
| B：全部 Section 迁移 | 中（3-4 小时） | 中，需逐 section 验证 |
| C：配置脏标记 | 小（<1 小时） | 低，已在单元测试中验证 |
| D：启动预热集成 | 中（1-2 小时） | 中，需确保不阻塞 loading |

总计约 6-9 小时。建议分两个 sprint：
- Sprint 1：A + C（最核心价值：逐 section 解锁 + 脏标记）
- Sprint 2：B + D（完整推广 + 预热）
