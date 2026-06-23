---
title: QmClient 待办整合规格（半 plan 式）
date: 2026-06-20
status: draft
source: 用户 backlog（#8 ~ #20，共 13 个主任务）
scope: 只调研、只落文档，不动代码
---

## 背景

本规格整合 2026-06-20 用户提交的 13 个主任务（编号沿用用户原始编号 #8 ~ #20），
覆盖 BUG 修复、UI/UX 增强、动画改进、平台系统能力和 BestClient 功能移植。

本次产出**仅为调研 + 规格文档**，不改任何代码。每个任务给出：
现状（引用具体文件:行号）、根因/缺口、设计方向、影响范围、风险与依赖、验收标准。

所有引用的文件:行号均来自本轮 `read`/`search` 实际看到的内容，未定位的会标注「未定位」。

## 目标

1. 为 13 个任务建立可执行的规格基线，后续每个任务可独立拆出小 plan/spec 实现。
2. 明确任务间的耦合（尤其 #10 通知栏贴边 与 #14.2 聊天框贴边的共用 Helpers）。
3. 给出优先级分组和执行顺序建议，遵循「一次一个功能」原则。
4. 为 #20 BestClient 移植提供分步实现计划（4 个子功能顺序）。

## 非目标

1. 不在本轮实现任何代码。
2. 不修改协议、物理、预测、demo 格式等上游保护区域（除非任务明确要求且获批准）。
3. 不逐条修改通知文案（#17 只定方向，具体文案改动留给后续小规格）。
4. 不做 PS blend mode 的完整数学推导（#11 只列推荐实现项和接入点）。

---

## 优先级分组与执行顺序

按风险/耦合/独立性分成五批，**每批内部任务可并行，批次间串行**。

### 第一批 — 低风险快速收口（每个 < 1 天，互不依赖）

| 任务 | 类型 | 说明 |
|------|------|------|
| #19 | BUG 修复 | 表情按键修饰键防误触，单文件 <30 行 |
| #15 | BUG 修复 | 旁观者 ID 卡片在禅模式 + 旁观组合下消失，纯逻辑 |
| #14.4 | 文案 | 聊天系统消息去掉 `***` 前缀 |
| #17 | 增强 | 通知栏文本语义化扫描，文案为主 |

### 第二批 — 中等独立功能（每个 1-2 天，互不依赖）

| 任务 | 类型 | 说明 |
|------|------|------|
| #9 | 增强 | 皮肤列表按更新时间排序 + 按钮压缩 |
| #14.1 | BUG + UI | 翻译按钮位置 + 二级面板关闭 BUG |
| #14.3 | UI | 聊天框滚动条（左/右跟随、变窄、新组件） |
| #13 | 增强 | 皮肤切换过渡动画多样化 |
| #12 | 增强 | 切枪动画高级配置 |

### 第三批 — 结构性改动（必须先设计共用 Helpers，强耦合）

| 任务 | 类型 | 说明 |
|------|------|------|
| #10 | BUG + 架构 | 通知栏贴边回归 + HUD 编辑器/渲染一致性 |
| #14.2 | 增强 | 聊天框贴边适配（复用 #10 的 Helpers） |
| #14.5 | 增强 | 聊天框弹性动画 + 批次淡出 + 可配置 |

**#10 和 #14.2 必须一起做**，共用一套贴边 Helpers（见下方「共用贴边 Helpers 设计」）。

### 第四批 — 需要调研先行

| 任务 | 类型 | 说明 |
|------|------|------|
| #11 | 增强 | 资源编辑器 PS blend modes（先调研再实现） |
| #8 | BUG 修复 | 滚动抖动根治（先定位根因） |
| #20 | 移植 | BestClient 4 子功能（先写分步 spec） |

### 第五批 — 需要批准/平台能力调研

| 任务 | 类型 | 说明 |
|------|------|------|
| #18 | 增强 | IME 样式可视化选择（IME 管线敏感） |
| #16 | 增强 | 进程高优先级（跨平台 spike） |

---

## 8. 滚动抖动根治

### 现状

QmClient 设置页（`src/game/client/components/qmclient/menus_qmclient.cpp`）和 TClient
设置页（`src/game/client/components/tclient/menus_tclient.cpp`）在滚动时出现抖动。

滚动状态管理集中在 `src/game/client/components/settings_resource_jobs.cpp`：
- `SettingsBuildFrameContext`（:392）构建每帧资源上下文，含 `m_ScrollActive`、
  `m_JumpScrollActive`、`m_PostScrollRecoveryFrames`。
- `SettingsScrollInteractionCooldown`（:1248 附近）和 `SettingsScrollInteractionRecovery`
  管理「滚动冷却帧」和「滚动后恢复帧」。
- 皮肤列表在 `menus_settings.cpp:2514-2517` 调用这两个函数，恢复帧数 = 2。

### 根因 / 缺口（top-2 假设）

**假设 A（最可能）：资源加载预算阶梯跳变**
滚动期间资源加载预算被压到最低（`settings_resource_jobs.cpp:421-448`，
finalize 16 / gpu upload 4），滚动结束后的 2 帧恢复期跳到 48/192，再跳回正常。
这种**预算阶梯式跳变**导致每帧加载/上传的资源数突变 → 可见项目逐批出现 → 视觉抖动。

**假设 B：布局重算与滚动不同步**
`m_PostScrollRecoveryFrames > 0` 期间，`SettingsResourceSharedHeavyBudget`（:1262-1264）
返回 RecoveryBudget 而非 NormalBudget，导致恢复期有额外重算，可能引起可见项高度
（`Line.m_aYOffset[OffsetType]`，chat.cpp 同类机制）重新测量造成的位移。

> **验证方法**：开 perf debug（`gs_TeeListDrainPerfSession`），观察抖动帧的
> `finalize_count` / `gpu_upload` 是否突变；或临时把 `RecoveryFrames` 设为 0 看抖动是否消失。

### 设计方向

1. 平滑预算过渡：滚动结束时不阶梯跳变，改为线性/缓动插值到正常预算。
2. 或：增加恢复帧数（2 → N），让跳变更平缓。
3. 需要先确认是假设 A 还是 B（只读 perf 日志定位），再决定改预算曲线还是改布局时机。

### 影响范围

- `src/game/client/components/settings_resource_jobs.cpp`（预算函数）
- 可能涉及 `menus_settings.cpp`、`menus_qmclient.cpp`、`tclient/menus_tclient.cpp` 的滚动调用

### 风险与依赖

- 滚动性能优化框架是已存在的复杂系统（见归档 spec
  `2026-06-08-页面性能优化框架设计.md`），改动预算曲线可能影响整体性能。
- 必须先定位根因，不能盲目调参。

### 验收标准

1. QmClient 和 TClient 设置页滚动时无可见抖动。
2. 滚动结束后的资源加载不出现「突然蹦出一批」的视觉跳变。
3. 现有滚动性能（帧率、加载延迟）不回归。

---

## 9. 皮肤列表按更新时间排序 + 按钮压缩

### 现状

排序比较器：`CSkins::CSkinListEntry::operator<`（`src/game/client/components/skins.cpp:734-762`）：
```
if(m_Favorite && !Other.m_Favorite) return true;   // 收藏优先
if(!m_Favorite && Other.m_Favorite) return false;
// 然后按 name 和 colorKey 排序
```
**没有 mtime 字段**。`CSkinListEntry`（`skins.h:294-325`）只有 `m_Favorite`、
`m_Name`、`m_ColorKey`、`m_SelectedMain/Dummy`。

皮肤列表 UI：`menus_settings.cpp:2296`，`s_ListBox.DoStart(50.0f, size, 4, 2, ...)`，
**每行 4 个项目**，间距 2。每项含收藏按钮 + 队列按钮，用户反馈「太挤」。

收藏机制：`m_Favorites`（`std::set<std::string>`），`AddFavorite`/`RemoveFavorite`/
`IsFavorite`（skins.cpp:2813-2842），触发 `m_SkinList.ForceRefresh()`。

### 根因 / 缺口

1. 缺少 mtime 排序：`CSkinContainer`（skins.h:99-165）和 `CSkinListEntry` 都没有
   文件修改时间字段。
2. 按钮太挤：每行 4 个，加上收藏/队列图标，视觉密度过高。

### 设计方向

1. **mtime 字段**：在 `CSkinContainer` 加 `m_LastModified`（文件 mtime，扫描时获取），
   透传到 `CSkinListEntry`。
2. **排序比较器**：优先级 = 收藏 > mtime（新→旧）> name。修改 `operator<`。
3. **按钮压缩**：考虑每行 3 个（降低密度），或缩小收藏/队列图标尺寸，
   或把图标叠加在预览图右上角而非并排。
4. **配置项**：新增 `qm_skin_sort_mode`（0=按时间, 1=按名字），放栖梦设置页。

### 影响范围

- `src/game/client/components/skins.h`（CSkinContainer / CSkinListEntry 加字段）
- `src/game/client/components/skins.cpp`（operator<、扫描时记录 mtime、MakeSkinListEntry）
- `src/game/client/components/menus_settings.cpp`（按钮布局调整）
- `src/engine/shared/config_variables_qmclient.h`（新增 sort_mode 配置）

### 风险与依赖

- mtime 获取需要在皮肤目录扫描（`SkinScan`，skins.cpp:766）时调用 `storage->GetFileTime`
  或等价 API，需确认 DDNet storage 层支持。
- 排序变化不能破坏收藏置顶语义。
- 按钮布局改动要回归测试设置页性能（预热、虚拟化框架）。

### 验收标准

1. 收藏皮肤始终在最前。
2. 非收藏皮肤按更新时间从新到旧排列。
3. 按钮不再拥挤，视觉舒适。
4. 皮肤列表性能（加载、滚动）不回归。

---

## 10. 通知栏贴边回归 + HUD 编辑器/实际渲染一致性

> 本任务与 #14.2 强耦合，**必须一起做**，共用一套贴边 Helpers。

### 现状（来自 HudEdgeSnap agent 完整调研）

**通知栏边距已生效但编辑器预览不反映边距**：
- `qm_hud_notifications_edge_margin`（`config_variables_qmclient.h:173`，默认 8）通过
  `InsetAnchoredRect`（`hud_notifications.h:153-160`）在变换前空间手动偏移 `AnchorRect`。
- 但 `BeginTransform`（`hud_notifications.cpp:229`）以**原始** `AnchorRect`（未含边距）
  做变换。
- `RenderNotifications`（:230）在变换后空间渲染含边距偏移的 `RenderBaseRect`。
- 结果：编辑器 `UpdateVisibleRect`（hud_editor.cpp:88-115）记录的拖拽框在变换后空间
  表现为「贴屏幕边缘」，而实际渲染位置距边缘 = EdgeMargin。

**这就是「编辑器里贴边了但实际渲染贴不到边」的根因。**

### 根因 / 缺口

1. 编辑器变换空间和实际渲染空间的边距语义不一致：边距在渲染侧手动施加，
   但编辑器的 `ComputeTransformPlacement`（hud_editor.cpp:326-417）和
   `ClampStateToScreen`（:294-319）不知道边距的存在。
2. 通知栏私有 `InsetAnchoredRect` 和 `ResolveHorizontalFlow`（hud_notifications.h:100-106）
   是局部实现，没有提升为通用 Helpers。
3. 聊天框（#14.2）完全没有贴边能力，硬编码 `x=0`。

### 设计方向（共用贴边 Helpers）

在 `hud_editor.h` 的 `QmHudEditor` 命名空间新增通用贴边设施：

```cpp
namespace QmHudEditor {
    // 边距描述：上下左右各方向的贴边保留距离
    struct SEdgeMargin {
        float m_Left = 0.0f, m_Right = 0.0f, m_Top = 0.0f, m_Bottom = 0.0f;
    };

    // 在变换前空间施加边距，返回调整后的 rect
    CUIRect ApplyEdgeMargin(const CUIRect &Rect, const SEdgeMargin &Margin);

    // 水平流向：贴左时左→右生长，贴右时右→左生长
    enum class EHorizontalFlow { LeftToRight, RightToLeft };
    EHorizontalFlow ResolveHorizontalFlow(const CUIRect &VisibleRect, float ScreenWidth);

    // 已有的 SnapAxisToScreenEdges / SnapAxisToGuides 可在交互时复用
}
```

集成点：
- `ClampStateToScreen` / `ComputeTransformPlacement` 接收 `SEdgeMargin`，
  使 Editor State 本身包含边距信息 → 编辑器预览和实际渲染语义一致。
- 通知栏私有 `InsetAnchoredRect` 改为调用 `ApplyEdgeMargin`。
- 聊天框新增 `qm_chat_edge_margin` 配置，调用同一套 Helpers。

### 影响范围

- `src/game/client/components/hud_editor.cpp` / `hud_editor.h`（核心 Helpers）
- `src/game/client/components/qmclient/hud_notifications/hud_notifications.cpp` / `.h`
- `src/game/client/components/chat.cpp`（#14.2）
- `src/engine/shared/config_variables_qmclient.h`（新增 `qm_chat_edge_margin`）
- 所有使用 `BeginTransform` 的 HUD 元素（hud.cpp 中 13+ 处）需确认不受影响

### 风险与依赖

- 这是结构性改动，影响所有 HUD 元素的编辑器交互。
- 现有测试：`src/test/qm_hud_notifications_test.cpp`（:1063-1093 EdgeMarginInsetsOnly…）
  和 `QmHudEditorGeometry` snap 测试，改动后必须全绿。
- 通知栏的左右生长方向逻辑（`ResolveHorizontalFlow`）迁移到通用层时要保持行为一致。

### 验收标准

1. 通知栏在编辑器里拖到边缘时，预览框和实际渲染位置一致（都贴边或都有边距）。
2. `qm_hud_notifications_edge_margin` 生效：贴边时背景框与屏幕边有配置的距离。
3. 聊天框可贴左/贴右，贴右时从右向左生长。
4. 上下左右贴边都正常工作。
5. 现有 HUD 元素（计时器、计分板等）的编辑器行为不回归。

---

## 11. 资源编辑器混合颜色模式、叠加模式增强

### 现状

皮肤染色当前是**替换式**：`CTeeRenderInfo::ApplyColors(useCustomColor, colorBody, colorFeet)`
直接把皮肤纹理颜色替换成自定义颜色（见 `skins.cpp`、`ghost.cpp:381`、
`nameplates.cpp:1744` 等调用点）。

Frankenstein 编辑器（`src/game/client/components/menus_assets_editor.cpp:2027`）
用 tint（染色）方式给部件上色，只有「染色/替换」一种模式，没有 PS blend mode。

### 根因 / 缺口

没有任何 Multiply / Screen / Overlay / SoftLight / HardLight 等混合模式。
染色数学是 `color_cast<ColorRGBA>(ColorHSLA(colorValue))`（HSL→RGB 替换），
不是 per-pixel blend。

### 设计方向

**先调研 PS blend modes，再挑选对皮肤染色有意义的子集实现。**

推荐实现的 blend modes（对皮肤染色有意义，非全部 PS 模式）：
| 模式 | 公式（C_result = f(C_base, C_blend)） | 用途 |
|------|----------------------------------------|------|
| Normal（替换） | C_blend | 当前已有 |
| Multiply | C_base × C_blend | 加深、阴影 |
| Screen | 1 - (1-C_base)(1-C_blend) | 提亮 |
| Overlay | C_base<0.5 ? 2×base×blend : 1-2(1-base)(1-blend) | 增强对比 |
| SoftLight | 基于 Penoline 公式 | 柔和叠加 |
| HardLight | Overlay 的 blend/base 互换 | 强烈叠加 |

接入点：在 `ApplyColors` 或渲染管线层（`players.cpp` 的皮肤渲染）加入 blend 运算。
新增配置 `qm_skin_blend_mode`（枚举），在 Frankenstein 编辑器和设置页暴露。

### 影响范围

- `src/game/client/components/skins.cpp` / `skins.h`（染色函数）
- `src/game/client/components/menus_assets_editor.cpp`（编辑器 UI）
- `src/game/client/components/players.cpp`（渲染管线，可能）
- `src/engine/shared/config_variables_qmclient.h`（blend_mode 配置）

### 风险与依赖

- blend mode 数学必须准确，建议先写单元测试验证每个模式的边界值。
- 如果在渲染管线层做 per-pixel blend，性能要评估（可能需要 shader）。
- 不能破坏 DDNet 原生皮肤的默认渲染（Normal 模式 = 当前行为）。

### 验收标准

1. 至少实现 Multiply / Screen / Overlay 三种 blend mode。
2. Frankenstein 编辑器可切换 blend mode 并实时预览。
3. 默认（Normal）模式与当前染色效果完全一致，无回归。
4. 每个 blend mode 有单元测试覆盖边界值。

---

## 12. 切枪动画可调整高级配置（视觉效果）

### 现状（已完整精读）

**切枪动画** ≠ 弹道辅助线，是两个不同的东西：

**切枪动画**（`players.cpp:894-918`，`RenderHand`/武器渲染段）：
- 配置：`QmWeaponSwitchAnim`（开关，config_variables_qmclient.h:133）+ `QmWeaponSwitchAnimScope`（范围 0=自己/1=本地+分身/2=所有玩家，:134）
- 实现：`players.cpp:905-916`，当检测到武器切换（`m_aWeaponSwitchLastWeapons[ClientId] != Player.m_Weapon`）时启动动画。
- **硬编码参数**（players.cpp:907-915）：
- `SwitchAnimDuration = 0.3f`（动画时长，:907）
- 位移幅度 `Direction * 40.0f`（滑入距离，:914）
- 旋转幅度 `pi * 2.0f`（旋转一圈，:915）
- easing `1 - (1-p)³`（easeOutCubic，:913）

**弹道辅助线**（`weapon_trajectory.cpp`，grenade/shotgun/laser 轨迹预测）：
- 配置：`QmWeaponTrajectory`（模式 0/1/2）+ `Color`/`Width`/`Alpha`（:129-132）
- 这个已较完整，不在本次「切枪动画」范围。

设置页 UI：`menus_qmclient.cpp:5443-5468`，已有开关 + 范围下拉，但**没有时长/位移/旋转/easing 的高级配置**。

### 根因 / 缺口

切枪动画的 4 个视觉参数全部硬编码（时长 0.3s、位移 40px、旋转 2π、easeOutCubic），用户要「可调整高级配置」。

### 设计方向

1. 新增配置变量（config_variables_qmclient.h）：
- `qm_weapon_switch_anim_duration`（时长，0.05-2.0s，默认 0.3）
- `qm_weapon_switch_anim_distance`（滑入距离，0-100，默认 40）
- `qm_weapon_switch_anim_rotation`（旋转弧度，0-4π，默认 2π）
- `qm_weapon_switch_anim_easing`（easing 枚举：0=easeOutCubic 当前，1=easeOutBack 弹性，2=linear，3=easeInOutQuad）
2. `players.cpp:907-915` 的硬编码常量替换为配置读取。
3. 设置页（`menus_qmclient.cpp:5447` 的 `if(g_Config.m_QmWeaponSwitchAnim)` 块内）增加「高级配置」子区。

### 影响范围
- `src/game/client/components/players.cpp:907-915`（4 个硬编码参数→配置）
- `src/game/client/components/qmclient/menus_qmclient.cpp:5447-5468`（设置页加高级配置）
- `src/engine/shared/config_variables_qmclient.h`（4 个新配置）
- 可能新增 easing 工具函数（easeOutBack 等，复用 `jelly_tee.cpp` 的弹性实现）

### 风险与依赖
- 低风险，纯视觉参数化，不影响游戏逻辑。
- easing 枚举扩展要确保默认值（easeOutCubic）与当前效果完全一致。
- 武器切换动画与 Gores 自动切枪联动时也要正常（Gores 会触发武器切换）。

### 验收标准
1. 切枪动画的时长、位移、旋转、easing 可独立配置。
2. 设置页有「切枪动画高级配置」子区。
3. 默认值与当前效果完全一致。
4. Gores 自动切枪时动画正常。

---

## 13. 皮肤切换的过渡动画效果多样化

### 现状（已完整精读）

皮肤切换动画**已有 5 种类型**，不是单一的——用户反馈「大差不差」可能是因为
参数都偏保守（缩放幅度 0.02-0.06，位移 8-18px，差异确实不明显）。

**已有 5 种类型**（`render.h:241-323` `ComputeSkinChangeTransitionBlend`，`render.cpp:263-285` 渲染）：

| 枚举 | 下拉名 | 效果 | 关键参数（均硬编码） |
|------|--------|------|---------------------|
| 0 GHOST_POP | Afterimage pop（残影弹出） | sin 震荡 + 缩放 + 残影 alpha | 缩放 0.94+0.06·EaseOut+0.05·Pop，alpha 0.18+0.82 |
| 1 FADE_SCALE | Smooth fade（平滑淡入淡出） | 交叉淡入 + 轻缩放 | 缩放 0.88+0.12·EaseOut |
| 2 SLIDE_LEFT | Slide left（左滑） | 旧皮左移 14px，新皮右入 18px | 位移 -14/+18 |
| 3 SPIN_POP | Spin pop（旋转弹出） | 旋转 + 缩放 + Pop | 旋转 -0.18/+0.20 rad，缩放 +0.03·Pop |
| 4 THEME_SWITCH | Brightness shift（亮度切换） | 垂直位移上下分开 | 位移 ±8px y |

配置：`QmSkinChangeTransitionType`（类型 0-4）+ `QmSkinChangeTransitionMs`（时长 0-2000ms）。
easing 统一用 `EaseOut = 1-(1-p)³`（:246），`Pop = sin(Progress·π)`（:248）。

设置页 UI：`menus_qmclient.cpp:3972-3996`，已有类型下拉 + 时长滑块。

### 根因 / 缺口

1. **视觉参数全硬编码**：每种类型的缩放幅度、位移距离、旋转角度、alpha 曲线都是魔法数字
   （如 `0.06f`、`-14.0f`、`0.20f`），不可配置 → 类型间差异「大差不差」。
2. **easing 单一**：只有 easeOutCubic，缺弹性（easeOutBack/elastic）、bounce 等。
3. **缺更多类型**：没有粒子爆发、故障/glitch、弹性回弹等视觉冲击力强的效果。

### 设计方向

1. **参数化现有类型**：把每种类型的关键系数提取为配置（或一组统一的「强度」配置），
   让用户可调缩放/位移/旋转幅度。
2. **新增 easing 选项**：`qm_skin_change_transition_easing`（枚举：easeOutCubic 当前 / easeOutBack 弹性 / bounce / linear）。
3. **新增类型**：
- `Glitch`（故障）：随机偏移 + RGB 分离
- `Elastic pop`（弹性回弹）：easeOutElastic + 大幅缩放
- `Particle burst`（粒子爆发）：切换瞬间喷射粒子（复用粒子系统）
4. 复用 `jelly_tee.cpp` 的弹性动画基础设施。

### 影响范围
- `src/game/client/render.h:241-323`（`ComputeSkinChangeTransitionBlend` 参数化 + 新类型）
- `src/game/client/render.cpp:263-285`（渲染新类型）
- `src/game/client/components/qmclient/menus_qmclient.cpp:3972-3996`（设置页加新类型 + 参数滑块）
- `src/engine/shared/config_variables_qmclient.h`（easing 配置 + 可能的强度配置）

### 风险与依赖
- 纯客户端视觉，不影响游戏逻辑。
- 新类型（尤其 glitch / particle burst）有额外渲染开销，需评估性能。
- 参数化现有类型时，默认值必须与当前硬编码值完全一致。

### 验收标准
1. 至少新增 2-3 种视觉差异明显的过渡类型（glitch / elastic / particle）。
2. 现有 5 种类型的视觉参数可调（强度滑块）。
3. easing 可选（至少 easeOutCubic + easeOutBack）。
4. 默认值与当前效果一致。
5. 设置页可切换并实时预览。

---

## 14. 聊天框改进（5 个子项）

### 14.1 翻译按钮位置 + 二级面板关闭 BUG

#### 现状

翻译按钮位置：`chat.cpp:2513`
```cpp
CUIRect TranslateButtonRect = {ClippingRect.x + ClippingRect.w + TranslateButtonGap, ...};
```
按钮在 `ClippingRect.w` **右侧**（+ TranslateButtonGap）。用户要移到左边（ALL:/chat: 左边）。

翻译面板关闭 BUG 根因：`PopupLanguageMenu`（`chat.cpp:3397-3534`）**永远返回
`POPUP_KEEP_OPEN`**（:3533），没有任何分支返回 `POPUP_CLOSE_CURRENT`。
关闭路径只有：
- `OnInput`（:1102-1108）ESC 键 → `CloseLanguageMenu()`
- 外部点击（:1093-1098）

但面板**没有关闭按钮、没有点标题关闭、没有点击面板外区域的可靠关闭**。
`DoPopupMenu`（:3170）注册后，popup 自身不会主动关闭。

#### 根因 / 缺口

1. 翻译按钮硬编码在右侧，需移到左侧（`ClippingRect.x - TranslateButtonSize - Gap`）。
2. 面板关闭依赖外部 ESC/点击，但聊天场景下这些可能被拦截；面板缺主动关闭入口。
3. 面板 UI（字体比例、横向宽度）不美观：`MenuWidth = 240.0f`（:3135）写死，
   `FontSize = 7.5f`（:3407）写死。

#### 设计方向

1. 翻译按钮移到输入框前缀（ALL:/Chat:）左侧。
2. 面板增加关闭按钮（右上角 ×），或点击标题栏关闭。
3. 面板宽度/字体改为相对值（基于聊天框尺寸缩放）。
4. 确保二级面板的输入态不遮挡关键操作（输入框、滚动条）。

#### 影响范围
- `src/game/client/components/chat.cpp`（按钮位置、面板关闭、面板 UI）
- `src/game/client/components/chat.h`（如有状态字段）

#### 验收标准
1. 翻译按钮在 ALL:/Chat: 左侧。
2. 打开翻译面板后可通过按钮/ESC/点外部可靠关闭。
3. 面板字体比例和宽度美观。
4. 面板不遮挡聊天输入。

---

### 14.2 聊天框贴边适配（共用 Helpers，见 #10）

#### 现状

聊天框**完全缺乏贴边能力**：`chat.cpp:2388-2389` 硬编码 `ChatRect = {0.0f, 50.0f, ...}`，
即 x=0（左对齐），无 `InsetAnchoredRect`、无 `PreviewTransform`、无边距配置、无贴右逻辑。

`BeginTransform(EHudEditorElement::Chat, ChatRect)`（:2389）只用了基础变换。

#### 根因 / 缺口

聊天框没有接入 HUD 编辑器的贴边体系，无法贴右。

#### 设计方向

复用 #10 的共用贴边 Helpers：
1. 聊天框新增 `qm_chat_edge_margin` 配置。
2. `ChatRect` 的 x 改为基于贴边方向计算（贴左 x=margin，贴右 x=ScreenWidth-width-margin）。
3. 贴右时文本对齐和滚动条位置（见 #14.3）都要跟随翻转。
4. 用 `ResolveHorizontalFlow` 决定生长方向。

#### 影响范围
- `src/game/client/components/chat.cpp`
- `src/engine/shared/config_variables_qmclient.h`（`qm_chat_edge_margin`）
- 依赖 #10 的 Helpers 完成

#### 验收标准
1. 聊天框可在 HUD 编辑器里贴左/贴右。
2. 贴右时从右向左生长，文本右对齐。
3. 与 #10 的通知栏共用同一套 Helpers。

---

### 14.3 聊天框滚动条（左/右跟随、变窄、新组件）

#### 现状

滚动条：`chat.cpp:2561`
```cpp
CUIRect ScrollbarRect = {ChatRect.w - CHAT_SCROLLBAR_WIDTH - CHAT_SCROLLBAR_MARGIN, ...};
```
- 位置：**右侧**（`ChatRect.w - width`）
- 宽度：`CHAT_SCROLLBAR_WIDTH = 5.0f`（chat.cpp:46）
- 边距：`CHAT_SCROLLBAR_MARGIN = 2.0f`（:47）
- 是聊天框**自定义实现**（`m_ScrollbarDragging`，:2588-2606），不是设置菜单的滚动条组件。

用户要：学习 Best Client 放左边（贴右时放右边）；变窄；考虑用栖梦页面滚动条或新建组件。

#### 根因 / 缺口

1. 滚动条硬编码在右侧，不跟随贴边方向。
2. 宽度 5.0f 可能仍偏宽（用户主观）。
3. 当前是 chat 私有实现，未复用通用滚动条组件。

#### 设计方向

1. 滚动条位置跟随 #14.2 的贴边方向：贴左→滚动条在左，贴右→滚动条在右。
2. 评估栖梦页面滚动条组件（搜索 `QmScroll`/栖梦 scroll region），或新建轻量聊天滚动条组件。
3. 宽度可配置或减小到 3-4px。

#### 影响范围
- `src/game/client/components/chat.cpp`（滚动条位置、宽度）
- 可能的新滚动条组件文件
- 依赖 #14.2 贴边方向确定

#### 验收标准
1. 贴左时滚动条在左，贴右时在右。
2. 滚动条宽度合适（不抢聊天空间）。
3. 拖拽、滚轮交互正常。

---

### 14.4 聊天框系统信息去掉 `***`

#### 现状

`***` 前缀在 `chat.cpp:1443`：
```cpp
str_format(aBuf, sizeof(aBuf), "*** %s", pMsg->m_pMessage);
Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat/server", aBuf, ...);
```
这是**控制台（F1）输出**的前缀，不是聊天框渲染。聊天框渲染本身不加 `***`。

#### 根因 / 缺口

用户看到的 `***` 来自 F1 控制台的服务端消息日志。去掉即可（或改为更现代的标记）。

#### 设计方向

1. 去掉 `"*** %s"` 前缀，直接打印消息，或用更轻量的标记（如 `› `）。
2. 确认聊天框渲染（非控制台）是否也有 `***`——根据代码搜索，聊天框渲染不加，
   但需确认 `RenderLine` 路径。

#### 影响范围
- `src/game/client/components/chat.cpp:1443`（单行）

#### 验收标准
1. F1 控制台的系统消息不再有 `***` 前缀。
2. 聊天框渲染不受影响。

---

### 14.5 聊天框动画（弹性、批次淡出、可配置）

#### 现状

消失动画根因已定位：
- `CalculateCutOffOffsetX`（`chat.cpp:450-453`）：
  ```cpp
  return -CHAT_ANIM_SLIDE_OUT_OFFSET * EaseInQuad(CutOffT);
  ```
  返回**负 X 偏移**（向左滑），`CHAT_ANIM_SLIDE_OUT_OFFSET = 60.0f`（chat.h:39）。
- `EaseInQuad`（chat.cpp:440-443）= `t * t`，加速左移。
- `CalculateCutOffAlpha`（:445-448）= `1 - EaseInQuad(t)`，同时淡出。

**用户看到的「往左移动一点」就是 `-60 * EaseInQuad` 的左滑。**

常量全部硬编码：
- `CHAT_ANIM_SLIDE_OUT_OFFSET = 60.0f`（chat.h:39）
- `CHAT_ANIM_CUTOFF_DURATION = 0.3f`（:40）

#### 根因 / 缺口

1. 消失动画是「左滑 + 淡出」，用户要「批次淡出」（不左滑）。
2. 无弹性（easeOutBack / elastic）。
3. 动画参数全硬编码，不可配置。
4. 没有在栖梦-视觉页面暴露配置。

#### 设计方向

1. 去掉 `CalculateCutOffOffsetX` 的左滑，或改为可配置（默认关闭左滑）。
2. 消失动画改为「批次淡出」：多条消息按批（上边的先）逐批 alpha 渐变。
3. 加入弹性 easing（easeOutBack），复用 `jelly_tee.cpp` 或新增 easing 工具。
4. 新增配置（栖梦-视觉页面）：
   - `qm_chat_anim_slide_out`（是否左滑，默认 0）
   - `qm_chat_anim_fade_duration`（淡出时长）
   - `qm_chat_anim_easing`（easing 类型枚举）

#### 影响范围
- `src/game/client/components/chat.cpp`（动画函数、常量）
- `src/game/client/components/chat.h`（常量→配置）
- `src/game/client/components/qmclient/menus_qmclient.cpp`（栖梦-视觉页配置项）
- `src/engine/shared/config_variables_qmclient.h`（新增配置）

#### 验收标准
1. 消失动画默认是淡出（不左滑），可配置开启左滑。
2. 多条消息按批次淡出（上边先消失）。
3. 动画有弹性感（easeOutBack 或类似）。
4. 栖梦-视觉页面可配置动画参数。
5. 弹出动画（显示时）也丝滑。

---

## 15. 旁观者 ID 卡片消失（禅模式回归，issue #161）

### 现状

GitHub #161 标 CLOSED，但**回归确实存在**。

旁观卡片渲染：`CHud::RenderSpectatorHud`（`hud.cpp:5270-5338`），显示
`Following %d: %s`（:5306）。

可见性判断：`hud.cpp:5852-5952`
```cpp
const bool MainHudVisible = g_Config.m_ClShowhud != 0;
const bool FocusSpectatorHudVisible = ShouldRenderFocusSpectatorHud(
    SpecInfo.m_Active, m_ClShowhudSpectator != 0,
    MainHudVisible, m_QmFocusMode != 0, m_QmFocusModeHideHud != 0);
if(MainHudVisible) { ... RenderSpectatorHud(); }
else if(FocusSpectatorHudVisible) { RenderSpectatorHud(); }
```

`ShouldRenderFocusSpectatorHud`（`modes.cpp:85-88`）：
```cpp
return SpectatorActive && SpectatorHudEnabled && !MainHudVisible
       && ShouldHideFocusHud(FocusActive, HideHud);
```

### 根因

禅模式 + 旁观 + `m_QmFocusModeHideHud` 开启时：
- `MainHudVisible` 可能为 false（若 `m_ClShowhud=0`）
- `FocusSpectatorHudVisible` 要求 `SpectatorHudEnabled`（`m_ClShowhudSpectator != 0`）
- 若用户在禅模式下设了 `m_ClShowhudSpectator=0`，或 `m_QmFocusModeHideHud` 与
  `m_ClShowhud` 的组合导致两个分支都不进 → **卡片消失**

逻辑缺陷：`ShouldRenderFocusSpectatorHud` 的条件组合没有覆盖「禅模式想保留旁观卡片」
的合理场景。旁观卡片属于「即使隐藏 HUD 也应保留」的关键信息。

### 设计方向

1. 修正 `ShouldRenderFocusSpectatorHud`：禅模式下旁观卡片应始终可见
   （只要 `SpectatorActive && SpectatorHudEnabled`），不受 `HideHud` 影响。
2. 或：新增 `m_QmFocusModeKeepSpectatorId` 配置，默认 true，保证旁观卡片不被禅模式隐藏。
3. 测试覆盖所有组合：禅模式开/关 × HideHud 开/关 × 旁观开/关 × cl_showhud 各值。

### 影响范围

- `src/game/client/components/qmclient/modes.cpp`（:85-88 逻辑修正）
- `src/game/client/components/qmclient/modes.h`
- 可能新增配置项

### 风险与依赖

- 纯逻辑 bug，低风险。
- 要确认修正后不破坏禅模式「隐藏其他 HUD」的语义——只保留旁观卡片，其他照常隐藏。

### 验收标准

1. 禅模式 + 旁观时，被旁观者 ID 卡片始终可见。
2. 禅模式仍正确隐藏其他 HUD（计分板、名字等，按配置）。
3. 所有 Focus/ShowHud/Spectator 组合有测试覆盖。

---

## 16. DDNet 客户端进程高优先级（issue #164）

### 现状（已确认：Windows 已实现，缺其他平台 + 热重载 + UI）

**配置项已存在**：`QmProcessHighPriority` / `qm_process_high_priority`
（`src/engine/shared/config_variables_qmclient.h:16`，默认 0，描述「仅 Windows：启动时将进程优先级设置为高」）。

**Windows 实现已存在**：`src/engine/client/client.cpp:5931-5939`
```cpp
#if defined(CONF_FAMILY_WINDOWS)
    if(g_Config.m_QmProcessHighPriority)
    {
        if(SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
            log_info("client", "applied Windows high priority class");
        else
            log_error("client", "failed to apply Windows high priority class (error=%lu)", GetLastError());
    }
#endif
```
 仅在**启动时**应用一次，无热重载。
 用的是 `HIGH_PRIORITY_CLASS`（非 `ABOVE_NORMAL`）。
 没有设置页 UI 开关（配置只能改 settings 文件）。

### 根因 / 缺口

issue #164 要求 4 项，当前只满足第 1 项的一半：
1. ✅（部分）UI 配置项——配置变量有，但无设置页开关
2. ❌ 热重载——启动时一次性，配置变更不生效
3. ❌ 其他平台（Mac/Linux/Android）——完全未实现
4. ❓ 网络优先级保留——需确认 TOS/QoS 不受影响（ProcessPriority agent 搜索了 `tos`/`QoS` 但未及确认细节）

### 设计方向

1. **设置页 UI**：在栖梦或系统工具页加 `QmProcessHighPriority` 开关（最小改动）。
2. **热重载**：注册 config chain callback（`Console()->Chain`），配置变更时重新调用 `SetPriorityClass`。
3. **平台扩展**：
   + Linux/macOS：`setpriority(PRIO_PROCESS, 0, nice_value)`——非 root 只能降低不能提高，需降级处理。
   + Android：`Process.setThreadPriority`（JNI）——后台限制严格，可能无效。
4. **优先级档位**：考虑用 `ABOVE_NORMAL_PRIORITY_CLASS` 替代 `HIGH_PRIORITY_CLASS`（更安全，避免系统卡顿风险）。
5. **网络优先级**：确认 `m_Tos`（DDNet 网络层 TOS 设置）和进程优先级独立，互不影响。

### 影响范围
- `src/engine/client/client.cpp:5931-5939`（抽出为可热重载的函数，加 config chain）
- 平台层 `src/base/system.cpp`（新增 Linux/macOS/Android 封装）
- `src/game/client/components/qmclient/menus_qmclient.cpp`（设置页开关）
- 网络层确认（`m_Tos`/QoS 不受影响）

### 风险与依赖
- 触及平台系统层，属于需批准区域，但 Windows 部分已实现降低了风险。
- Windows `HIGH_PRIORITY_CLASS` 若游戏死循环会锁死系统，建议降为 `ABOVE_NORMAL`。
- Linux/macOS 非 root 无法提升，需明确文档说明或静默降级。
- Android 限制最多，可能只对前台有效。

### 验收标准
1. Windows 可通过设置页开关切换，热重载生效。
2. 网络优先级（TOS/QoS）不受影响。
3. 非 Windows 平台有降级处理或明确提示。
4. 切换不影响游戏稳定性。

---

## 17. 通知栏提示文本语义化增强（说人话）

### 现状（来自 NotifAndBC agent）

通知本地化引擎完善：`hud_notification_rules.cpp`（1239 行）+ 4 个静态规则宏表
（`hud_notification_static_rules.h` 等）覆盖约 120 条 DDNet 原始消息→本地化映射。

### 根因 / 缺口（4 类不自然翻译）

NotifAndBC agent 识别的典型：
1. **英文直译 + 术语堆砌**：如 `"Team can't be saved while a dragger is active"`
2. **被动/无人称**：如 `"Show the team top 5 is not allowed on this server"`（8 变体）
3. **命令式**：如 `"Unknown emote... Say /emote"`
4. **缺主语**：如 `"You can hammer hit others"`

### 设计方向

1. 逐类扫描 `hud_notification_static_rules.h`（120 条），标注需改的。
2. 重写为口语化中文（主语明确、主动语态、去术语）。
3. 不改规则匹配逻辑，只改本地化文本。
4. 走 i18n 流程：`extract_strings.py` → `generate_all.py` → `validate.py`。

### 影响范围

- `src/game/client/components/qmclient/hud_notifications/hud_notification_static_rules.h`
- `qmclient_scripts/languages_qmclient/`（翻译产物）
- 可能 `hud_notification_static_alias_rules.h`

### 风险与依赖

- 纯文案，低风险。
- 不能改英文 key（会破坏匹配），只改本地化值。

### 验收标准

1. 4 类不自然翻译全部修正。
2. 规则匹配行为不变。
3. i18n 校验全绿。

---

## 18. IME 样式可视化选择（栖梦-视觉页面）

### 现状（已确认：IME 三模式体系已实现，缺可视化配置入口）

**QmClient 已有完整的三模式 IME 体系**（前一轮调研遗漏，本轮修正）：

配置变量（`src/engine/shared/config_variables_qmclient.h`）：
- `QmImeAutoManage` / `qm_ime_auto_manage`（:31，默认 1）：根据文本输入焦点自动启用/关闭 IME
- `QmNewIme` / `qm_new_ime`（:32，默认 1）：启用新版 IME 候选栏

三状态机 `QmImeComputeCandidateRenderAction`（`src/game/client/qm_ime_manager.h:17-23`）：
- `VALIDATE_ONLY`：用系统原生 IME（非 Windows 平台，`QmImeShouldUseSystemCandidateUi()` 返回 true）
- `LEGACY`：旧版候选栏（`m_QmNewIme == 0`，调 `CLineInput::RenderLegacyCandidates`）
- `POPUP`：新版候选弹窗（`m_QmNewIme == 1`，调 `CQmImeCandidatePopup::Render`）

平台策略（`src/engine/shared/qm_ime_policy.h`）：
- Windows 强制自绘候选词（`QmImeShouldUseSystemCandidateUi()` 返回 false）
- 非 Windows 平台用系统原生 IME UI

渲染调度（`qm_ime_manager.cpp:140-161`）：`RenderCandidatePopup()` 根据状态机选 LEGACY/POPUP/VALIDATE_ONLY。

SDL 集成（`input.cpp:111-115`）：
- `SDL_HINT_IME_INTERNAL_EDITING = "0"`
- `SDL_HINT_IME_SHOW_UI` 由 `QmImeShouldUseSystemCandidateUi()` 控制
- Windows 下通过 `WM_IME_NOTIFY` + `ImmGetCandidateListW` 自绘候选词（input.cpp:922-980）

候选弹窗实现：`src/game/client/qm_ime_candidate_popup.cpp`（完整自绘候选词窗口，含主题 `qm_theme::SImeTheme`）。

测试：`src/test/qm_ime_platform_test.cpp`（验证平台策略和状态机）。

### 根因 / 缺口

用户要的三种模式**实际都已存在**：
- DDNet 旧版 IME = `QmNewIme=0`（LEGACY 路径）
- 新版 IME = `QmNewIme=1`（POPUP 路径）
- 输入法自带 = 非 Windows 平台的 VALIDATE_ONLY（Windows 被强制自绘）

**真正的 gap 只有两个**：
1. `QmNewIme` 配置**没有在栖梦-视觉页面可视化**（用户核心诉求）——配置项存在但设置页没有开关。
2. Windows 无法选「输入法自带」——`QmImeShouldUseSystemCandidateUi()` 硬编码 false，
   导致 Windows 用户即使想用微信输入法等原生 UI 也用不了。

### 设计方向

1. 在栖梦-视觉页面暴露 `QmNewIme` 开关（旧版 vs 新版候选栏），这是最小改动，满足核心诉求。
2. 若要 Windows 支持「输入法自带」：把 `QmImeShouldUseSystemCandidateUi()` 改为可配置
   （新增 `qm_ime_use_system_ui`），而非硬编码 false。
   注意：Windows 自绘候选词（`WM_IME_NOTIFY` 处理）和系统原生 UI 互斥，切换时要正确设置
   `SDL_HINT_IME_SHOW_UI`。
3. 可考虑统一为三选一枚举 `qm_ime_style`（0=旧版自绘, 1=新版弹窗, 2=系统原生），
   替代 `QmNewIme` + 平台硬编码的组合，逻辑更清晰。

### 影响范围
- `src/game/client/components/qmclient/menus_qmclient.cpp`（栖梦-视觉页加 IME 配置区）—— 主要改动
- `src/engine/shared/qm_ime_policy.h`（若开放 Windows 系统原生选项）
- `src/engine/client/input.cpp:112`（`SDL_HINT_IME_SHOW_UI` 跟随新配置）
- 可能 `src/engine/shared/config_variables_qmclient.h`（若新增枚举配置）

### 风险与依赖
- 低风险：暴露已有配置项到设置页，几乎无代码风险。
- 中风险：Windows 开放系统原生 IME 需正确处理 `SDL_HINT_IME_SHOW_UI` 切换，
   否则可能出现两个候选词窗口（自绘 + 原生）叠加。
- 已有测试 `qm_ime_platform_test.cpp` 覆盖状态机，改动后需保证全绿。

### 验收标准
1. 栖梦-视觉页面可切换旧版/新版 IME 候选栏。
2.（可选）Windows 可选择系统原生 IME，不出现候选词窗口叠加。
3. 切换不影响游戏其他输入（按键、聊天）。
4. `qm_ime_platform_test.cpp` 全绿。
---

## 19. 表情按键修正（Win/Command/Meta 防误触）

### 现状

`src/game/client/components/emoticon.cpp:22-36` 的 `ConKeyEmoticon`：
```cpp
void CEmoticon::ConKeyEmoticon(IConsole::IResult *pResult, void *pUserData) {
    ...
    if(!pSelf->GameClient()->m_Snap.m_SpecInfo.m_Active && ...) {
        if(pSelf->GameClient()->m_BindWheel.IsActive())
            pSelf->m_Active = false;
        else
            pSelf->m_Active = pResult->GetInteger(0) != 0;  // 只看按键状态
    }
}
```
全仓搜索 `ModifierIsPressed.*emote` / `emoticon.*modif` 均无匹配。

### 根因 / 缺口

**功能未实现**。`+emote` 按下时没有检查修饰键（Win/Command/Meta/Control），
导致 `Win+Shift+S` 截图等快捷键误触发表情菜单。

### 设计方向

在 `ConKeyEmoticon` 的**按下边沿**（`GetInteger(0)` 从 0→1）检查修饰键：
```cpp
if(pResult->GetInteger(0) != 0) {
    // 按下边沿，若修饰键按下则拒绝激活
    if(pSelf->Input()->ModifierIsPressed() || pSelf->Input()->KeyIsPressed(KEY_LWIN) || ...)
        return;  // 不激活
}
```

### 影响范围

- `src/game/client/components/emoticon.cpp`（单文件，<30 行）

### 风险与依赖

- 低风险，单文件小改动。
- TDD：先写失败测试（修饰键按下时 +emote 不激活）。

### 验收标准

1. 按 Win/Command/Meta/Control + 表情键时，表情菜单不弹出。
2. 单独按表情键时，菜单正常弹出。
3. 截图（Win+Shift+S）不再误触发表情。

---

## 20. BestClient 功能移植（4 子功能，分步实现）

### 来源

BestClient 源码：`docs/dyl/BestClient/`（用户已 clone）。
BC 配置变量：`docs/dyl/BestClient/src/shared/config_variables_bestclient.h`。

> **重要**：#20 应先写分步 spec（本文档即是基线），再按子功能顺序逐个实现，
> 一次一个 commit，不要混在一起。

---

### 20.1 简短服务器名（KoG gores + Axiom + CHN DDR）

#### 现状（已确认：BC 有 KoG，无 Axiom/CHN）

**KoG（国外 KoG gores 服）— BC 已实现：**
- 配置项：`docs/dyl/BestClient/src/engine/shared/config_variables_bestclient.h:83` `BcUseShortKogServerName` / `bc_use_short_kog_server_name`（默认 0）
- 简短化逻辑：`docs/dyl/BestClient/src/game/client/components/menus_browser.cpp:141-215` `GetServerbrowserDisplayName()`：
- 1. 匹配：`GameType` 含 "gores" **且** 名字含 "kog"（:144）
- 2. 找独立 "kog" 词（前后非字母数字），截取之后的实际名（:156-168）
- 3. 跳分隔符（空格/`|`/`*`/`-`/`:`/`[`/`]`，:150-152）
- 4. 去掉 `[kog.tw]` 后缀（:173-179）
- 5. 处理 `#数字 - 地图名` 格式重组（:181-212）
- 效果：`[kog.tw] #1 - MapName` → `MapName`
- UI 开关：`menus_bestclient.cpp:5709`

**Axiom（国服）— BC 无，QmClient 独有需求：**
- 识别方式：通过 **community ID** 含 "axiom"（`src/game/client/components/qmclient/axiom_auto_login.cpp:72`）
- 现有自动登录已能识别 Axiom 社区，但**没有服务器名简短化**。
- 命名规律（用户提供样本）：`Axiom <城市> <难度> - <服务器ID> <地图名>`
- 样本：`Axiom 北京 普通 - CHN1O 钩累死`
- 简短化目标：剥掉 `Axiom <城市> <难度> - <服务器ID> ` 前缀，保留 `钩累死`（地图名）；或保留 `CHN1O 钩累死`。
- 规则：找 ` - ` 分隔符，取右侧；再剥掉服务器 ID token（`CHN\d+\w*`），保留地图名。

**CHN DDR（国服 DDR 服）— BC 无，QmClient 独有需求：**
- 代码里无任何 CHN DDR 服务器引用（搜索 `CHN.*DDR` / `DDR.*CHN` 均无命中）。
- 命名规律（用户提供样本）：`DDNet CHN<数字> <城市> - <难度> <地图名>`
- 样本：`DDNet CHN7 西安 - Moderate 中阶`
- 简短化目标：剥掉 `DDNet CHN<数字> <城市> - <难度> ` 前缀，保留 `中阶`（地图名）；或保留 `CHN7 中阶`。
- 规则：找 ` - ` 分隔符，取右侧；再剥掉难度 token（`Moderate`/`Novice`/`Brutal` 等英文难度词），保留地图名。
- 识别方式：名字前缀含 `DDNet CHN`（或注册了对应 community ID，需确认）。

> **样本已补充**（2026-06-20）：上述规律基于用户提供的实际服务器名。实现时需多采样几个确认模式稳定性，特别是难度词枚举（`普通`/`Moderate`/`Novice`/`Brutal`/`中阶` 等中英混杂）。

#### 设计方向

统一抽象为「服务器显示名简短化」框架，支持多个服务器族（KoG / Axiom / CHN DDR）：

1. 移植 BC 的 `GetServerbrowserDisplayName` 作为 KoG 规则。
2. 新增 Axiom 规则：匹配 community ID 含 "axiom" 的服务器，按其命名规律简短化（规则待样本确认）。
3. 新增 CHN DDR 规则：匹配 CHN DDR 服（识别方式待定），按其命名规律简短化。
4. 配置项（放 Gores 模块设置页，用户要求）：
- `qm_short_kog_server_name`（对应 BC，含 KoG gores）
- `qm_short_axiom_server_name`（Axiom 国服）
- `qm_short_chn_ddr_server_name`（CHN DDR 国服）
- 或合并为一个 bitmask 配置 `qm_short_server_names`（位掩码：bit0=KoG, bit1=Axiom, bit2=CHN DDR）。
5. 简短化逻辑放 `menus_browser.cpp`，复用 BC 的 `GetServerbrowserDisplayName` 结构，为每个族单独写匹配+截取规则。

#### 影响范围
- `src/game/client/components/menus_browser.cpp`（显示名简化，新增多族规则）
- `src/game/client/components/qmclient/menus_qmclient.cpp`（Gores 设置页开关）
- `src/engine/shared/config_variables_qmclient.h`（新增配置）
- 可能复用 `axiom_auto_login.cpp` 的 `IsAxiomCommunity()` 判断 Axiom 服

#### 风险
- 低风险，纯显示名处理，不影响服务器通信或玩法。
- Axiom / CHN DDR 的命名规律需用户提供样本才能设计准确匹配。
- 不同族的识别方式不同（KoG 靠名字+GameType，Axiom 靠 community ID），框架要能容纳多种识别策略。
---

### 20.2 3D 表情（加阴影）

#### 现状（NotifAndBC agent，已定位 BC 源码）

BC 实现：`docs/dyl/BestClient/src/game/client/components/players.cpp:1119-1139`，
`bc_emoticon_shadow` —— 在表情精灵前画一个黑色偏移半透明副本
（`EmoticonShadowOffsetX/Y = 2.0`，`EmoticonShadowOpacity = 0.75`）。

QmClient 现状：`src/game/client/components/players.cpp:1164-1219` 表情渲染，
**没有阴影 pass**。

#### 设计方向

移植 BC 的阴影 pass 到 QmClient `players.cpp` 表情渲染段：
1. 在表情精灵绘制前，用黑色 + 偏移 + 半透明再画一遍。
2. 配置 `qm_emoticon_shadow`（开关），放栖梦-视觉页。

#### 影响范围
- `src/game/client/components/players.cpp`（:1164-1219 段，~30 行）
- 配置变量

#### 风险
- 低风险，纯渲染增强，~30 行单文件。

#### 验收标准
1. 表情有阴影，视觉上 3D 感。
2. 可在栖梦-视觉页开关。

---

### 20.3 电影级摄像机（Demo、观战增强）

#### 现状（NotifAndBC agent，已定位 BC 源码）

BC 实现：`docs/dyl/BestClient/src/game/client/components/camera.cpp:468-478`，
`m_CinematicCameraSmoothing`（bool）+ `m_CinematicCameraPosition`（vec2），
`FollowSpeed = 8.0f` lerp 平滑跟随，仅在 spectator freeview（`CAMTYPE_SPEC`）生效。

BC 头文件：`camera.h:67-68` 成员声明。
BC 配置：`config_variables_bestclient.h:45` `bc_cinematic_camera`。

QmClient 现状：`camera.cpp` 是**上游核心**，含大量 Qm 扩展（drift、dynamic FOV、
zoom、smoothing），**没有 cinematic camera**。

#### 设计方向

把 BC 的 cinematic 平滑跟随合并进 QmClient 的 `OnRender()`：
1. 新增 `qm_cinematic_camera` 配置 + `m_CinematicCameraPosition` 成员。
2. 在 spectator freeview 路径加入 lerp 平滑（FollowSpeed 可配）。
3. **必须不破坏** QmClient 已有的 drift / dynamic FOV / smoothing。

#### 影响范围
- `src/game/client/components/camera.cpp` / `camera.h`（**上游核心，高风险**）
- 配置变量

#### 风险

**高风险**：`camera.cpp` 是上游保护区域，QmClient 已有大量扩展。
合并 BC 改动必须：
- 不破坏现有 drift/FOV/smoothing。
- 只在 spectator freeview 生效。
- 充分回归测试观战、demo 回放。
- 建议最后做，且单独 commit + 充分测试。

#### 验收标准
1. 观战 freeview 时有电影感平滑跟随。
2. drift / dynamic FOV / 现有 smoothing 不回归。
3. demo 回放正常。

---

### 20.4 3D Particles（线框渲染 + 简单透视投影）

#### 现状（NotifAndBC agent，关键澄清）

**BC 的 3D Particles 和 QmClient 已有的背景 3D 粒子完全不同，可共存：**

| | BC `C3DParticles` | QmClient `CBackgroundParticles` |
|---|---|---|
| 源码 | `docs/dyl/BestClient/src/game/client/components/bestclient/3d_particles.cpp` | `src/game/client/components/tclient/background_particles.cpp` |
| 渲染 | 线框（`IGraphics::LinesDraw`） | 纹理形状（cube/heart/sphere） |
| 投影 | 简单透视（`ProjectPoint`，`PROJ_DIST=600`） | 无透视（静态背景） |
| 行为 | 跟随相机中心，有速度/物理/粒子碰撞 | 静态装饰 |
| 配置 | `bc_3d_particles*`（bestclient.h:327-336） | `qm_3d_particles*`（config_qmclient.h:335-354） |

BC 的 `C3DParticles` 头文件：`3d_particles.h`，`SParticle`（vec3 pos/vel/rot）+ `RenderParticles`。

#### 设计方向

把 BC 的 `C3DParticles` 作为**新组件**移植到 QmClient（不替换现有背景粒子）：
1. 复制 `3d_particles.cpp` / `.h` 到 `src/game/client/components/qmclient/`。
2. 配置变量从 `bc_3d_particles*` 改名 `qm_weapon_3d_particles*`（区分背景粒子）。
3. 注册新组件，在武器/钩子粒子效果处调用。
4. 设置页新增配置区（区分于现有 3D 背景粒子）。

#### 影响范围
- 新增 `src/game/client/components/qmclient/3d_particles.cpp` / `.h`
- 组件注册（`gameclient.cpp`）
- 配置变量
- 设置页 UI

#### 风险
- 中风险，新组件不破坏现有，但要与背景粒子明确区分避免混淆。
- 透视投影数学要验证正确。

#### 验收标准
1. 武器/钩子粒子有线框 3D 效果 + 透视投影。
2. 不影响现有背景 3D 粒子。
3. 可独立开关。

---

## 共用贴边 Helpers 设计（#10 + #14.2）

> 此节是 #10 和 #14.2 的共享设计基础，两个任务必须基于此协同实现。

### 目标

提供一套 HUD 编辑器通用的贴边设施，让所有可编辑 HUD 元素（通知栏、聊天框、
未来更多）共享一致的贴边语义，消除「编辑器贴边了但实际渲染贴不到」的根因。

### 设计

**新增（`hud_editor.h` 的 `QmHudEditor` 命名空间）**：
```cpp
struct SEdgeMargin {
    float m_Left = 0.0f, m_Right = 0.0f, m_Top = 0.0f, m_Bottom = 0.0f;
};
CUIRect ApplyEdgeMargin(const CUIRect &Rect, const SEdgeMargin &Margin);
enum class EHorizontalFlow { LeftToRight, RightToLeft };
EHorizontalFlow ResolveHorizontalFlow(const CUIRect &VisibleRect, float ScreenWidth);
```

**集成**：
1. `ClampStateToScreen` / `ComputeTransformPlacement` 接收 `SEdgeMargin`，
   使 Editor State 包含边距 → 编辑器预览反映边距。
2. 通知栏私有 `InsetAnchoredRect`（hud_notifications.h:153-160）改为调用
   通用 `ApplyEdgeMargin`。
3. 通知栏私有 `ResolveHorizontalFlow`（hud_notifications.h:100-106）迁移为通用版。
4. 聊天框新增 `qm_chat_edge_margin`，调用同一套。

### 现有可复用基础设施

`QmHudEditor` 命名空间已有 `SnapAxisToScreenEdges`、`SnapAxisToGuides`（hud_editor.h:28-85）
可在编辑器交互时复用。

### 测试

现有测试 `src/test/qm_hud_notifications_test.cpp`（:1063-1093）和
`QmHudEditorGeometry` snap 测试，改动后必须全绿，并新增 Helpers 的单元测试。

---

## BC 移植分步实现计划

按风险从低到高、独立性排序：

1. **#20.2 3D 表情阴影**（低风险，~30 行单文件）→ 先做
2. **#20.4 3D Particles**（中风险，新组件，不破坏现有）→ 其次
3. **#20.1 简短服务器名**（KoG 已有 BC 实现可移植；Axiom/CHN DDR 需用户提供服务器名样本后设计匹配规则）→ 依赖样本，可与 #20.2 并行准备
4. **#20.3 电影摄像机**（高风险，上游核心）→ 最后，单独 commit + 充分测试

每个子功能独立 commit，不混合。
- #16 进程优先级（Windows 已实现 `client.cpp:5931-5939`，缺其他平台+热重载+UI）、#18 IME（三模式体系已实现 `qm_ime_manager.h`，缺设置页可视化入口）、#20.1 KoG（BC 已实现 `menus_browser.cpp:141-215`，Axiom/CHN DDR 需用户提供服务器名样本）均已实地确认，剩余待确认项见各任务「需用户补充」标注。
---

## 附：本轮调研方法说明

- 并行派发 8 个只读 explore subagent，因 token 并发限流（group mix 仅允许 3 并发）
  导致 6 个失败，2 个成功（HudEdgeSnap、NotifAndBC）。
- 失败的任务由主代理串行补完（chat.cpp 精读、skins 排序、滚动抖动机制等）。
- 所有文件:行号引用均来自实际 `read`/`search`，未命中的标注「未定位」或「需确认」。
- #16 进程优先级、#18 IME 需后续 spike 确认；#20.1 KoG 已确认 BC 实现（`menus_browser.cpp:141-215`），Axiom/CHN DDR 需用户提供服务器名样本。
---
## 全客户端动画硬编码审计与可配置化（frontend design 视角）

> 本节是对 #12、#13 动画任务的延伸：盘点全客户端的动画硬编码，建议可配置化项，并从 frontend design 角度提出新动画机会。

### 现有动画基础设施（已成熟，可复用）

**QmUi 动画运行时**（`src/game/client/QmUi/`）：
- `QmAnim.h` / `QmAnim.cpp`：`EUiAnimDriver`（TWEEN/SPRING）、`SUiSpringConfig`（阻尼/弹性）、`EEasing` 枚举
- `EEasing` 已有 7 种：LINEAR / EASE_IN / EASE_OUT / EASE_IN_OUT / EASE_OUT_QUART / EASE_OUT_BACK（弹性回弹）/ EASE_IN_OUT_CUBIC / CUBIC_BEZIER（`QmAnim.cpp:56-79`）
- `QmAnimCurves.h`：预设曲线常量 `ui_curve::STANDARD`(0.30s) / `EMPHASIZED`(0.50s) / `DECELERATE`(0.25s) / `ACCELERATE`(0.20s) / `BOUNCE_OUT`(0.35s EASE_OUT_BACK)
- `QmMotion.h`：`ApplyMotionLevel` 按 `QmUiMotionLevel`（0=关闭/1=降低/2=完整）缩放时长和阻尼
- `CUiV2AnimationRuntime`：统一的动画值解析（`ResolveUiAnimValue` / `ResolveAnimatedLayoutValueEx`）

**弹性 Tee 动画**（`jelly_tee.cpp`）：`CQmJelly`，已有配置 `QmJellyTee`/`QmJellyTeeStrength`/`QmJellyTeeDuration`/`QmJellyTeeOthers`。

**现有可配置动画**（config_variables_qmclient.h）：
- `QmUiMotionLevel`（:20）全局动效强度
- `QmSkinChangeTransitionType` + `QmSkinChangeTransitionMs`（:126-127）
- `QmWeaponSwitchAnim` + `QmWeaponSwitchAnimScope`（:133-134）
- `QmHudNotificationsAnimType` + `QmHudNotificationsAnimMs`（:170-171）
- `QmChatBubbleAnimation`（:388）
- `QmPieMenuScale` / `QmPieMenuOpacity`（pie_menu.cpp）
- `QmJellyTee*` 系列

### 硬编码动画参数盘点（按子系统）

**HUD 当前武器**（hud.cpp:40-43）：切换时长 0.14f、峰值进度 0.45f、峰值缩放 1.32f、静止缩放 1.2f，全硬编码。easing（hud.cpp:45-57 `HudWeaponSwitchEase`）自定义曲线。建议：`qm_hud_weapon_switch_*`

**聊天 CutOff**（chat.h:39-40）：左滑偏移 60.0f、时长 0.3f，全硬编码。`EaseInQuad`（chat.cpp:440-442）= t*t 固定。建议：`qm_chat_anim_slide_offset` / `_duration` / `_easing`

**切枪动画 #12**（players.cpp:907-915）：时长 0.3f、位移 40.0f、旋转 2π、easeOutCubic，全硬编码。详见 #12 章节

**皮肤切换 blend #13**（render.h:246-319）：每种类型的缩放/位移/旋转/alpha 系数（~20 个魔法数字），全硬编码。详见 #13 章节

**通知栏动画**（hud_notifications.cpp:16-318）：`EaseOutCubic` 内联、滑入偏移 32.0f（:317），部分硬编码（动画时长已配）。建议：偏移可配

**Pie 菜单**（pie_menu.h:61-64）：时长 0.08f、最小缩放 0.85f、高亮缩放 1.25f，全硬编码。建议：`qm_pie_menu_anim_*`

**菜单页面切换**（menus.cpp:104-106）：时长 0.18f、alpha 最大 0.12f、tab hover 0.10f，全硬编码。建议：统一到 MotionLevel

**媒体岛/HUD 布局动画**（hud.cpp:770,1090-1092,3552-3614）：各处 0.08f-0.18f 时长 + EASE_OUT，全硬编码 inline。建议：统一到 MotionLevel

**名字板聊天气泡**（nameplates.cpp:2075-2124）：alpha/transform 时长 0.16-0.20f、填充速度 0.035f/字符，全硬编码。建议：`qm_chat_bubble_*`

**BindWheel**（bindwheel.cpp:161-291）：`QuadEaseInOut` 自定义曲线、动画相位，硬编码。建议：`qm_bindwheel_*`

**Console 状态变化**（console.cpp:2005）：`m_StateChangeDuration = 0.1f`，硬编码，低优先级

**统计**：约 60+ 个硬编码动画参数散布在 12+ 个文件，大多时长集中在 0.08-0.30s，easing 以 EASE_OUT 为主。

### 可配置化建议（分优先级）

**第一优先（用户直接感知，高价值）：**
1. 切枪动画（#12）：4 个参数（时长/位移/旋转/easing）
2. 皮肤切换 blend（#13）：每类型的强度系数 + easing 枚举
3. 聊天消失动画（#14.5）：左滑偏移 + easing + 时长（左滑可关闭）
4. HUD 当前武器图标：切换时长 + 峰值缩放

**第二优先（体验优化）：**
5. 通知栏滑入偏移（32.0f → 可配）
6. Pie 菜单开合时长 + 缩放
7. 名字板聊天气泡动画时长

**第三优先（系统性，建议统一治理而非逐个配）：**
8. 菜单/HUD 布局动画 → 已有 `QmUiMotionLevel` 全局缩放，建议不逐个暴露，而是让 MotionLevel 更细粒度（如分「页面切换/微交互/HUD布局」三个子档）
9. 把散落的 inline easing 统一改用 `QmAnimCurves.h` 预设曲线

### Frontend Design 视角：还可以加的动画

当前缺失的动画机会（参考现代 UI/UX 设计语言）：

**高价值（用户每局都触发）：**
- 死亡/复活动画：当前直接消失/出现，可加淡出+缩小/淡入+弹出
- 钩中/脱离反馈：钩子连接成功时 tee 轻微弹性形变（复用 jelly_tee 的 impulse）
- 得分/完成提示动画：计分板数字变化时的滚动动画（number tick）
- HUD 元素入场：游戏开始时 HUD 元素依次滑入（staggered entrance）

**中价值（设置/菜单体验）：**
- 设置项展开/折叠：子区展开时的 height 动画（当前可能瞬时）
- 下拉框/弹窗：spring 弹性出现（已有 BOUNCE_OUT 曲线可用）
- 按钮按下反馈：scale 微缩 + 回弹（press → 0.96 → 1.0）
- Toast/通知：进入时从边缘滑入 + 轻微过冲（overshoot）

**低价值（锦上添花）：**
- 皮肤预览旋转：设置页 tee 预览可缓慢自转
- Demo 进度条：拖动时的惯性滑动
- 服务器列表行 hover：背景色过渡 + 轻微位移

### 统一治理方向（非本批，但建议规划）

与其给每个硬编码参数都加一个 config（会产生 60+ 配置项，设置页爆炸），建议：
1. **游戏内动画**（切枪/皮肤/HUD武器/死亡复活）→ 逐个暴露配置，放栖梦-视觉页
2. **UI 布局动画**（菜单/页面/HUD布局/气泡）→ 统一到 `QmUiMotionLevel` 的子档位，不逐个暴露
3. **新建「动画总控」设置区**：栖梦-视觉页加一个子区，集中游戏内动画配置（切枪/皮肤/死亡/复活），UI 动画走 MotionLevel

### 影响范围（统一治理）
- `src/game/client/render.h` / `render.cpp`（皮肤切换 blend 参数化）
- `src/game/client/components/players.cpp`（切枪 + 死亡复活）
- `src/game/client/components/hud.cpp`（HUD 武器 + 元素入场）
- `src/game/client/components/chat.cpp` / `chat.h`（聊天动画）
- `src/game/client/components/qmclient/hud_notifications/`（通知动画）
- `src/game/client/components/pie_menu.cpp` / `.h`（pie 菜单）
- `src/game/client/QmUi/QmMotion.h` / `QmAnimCurves.h`（MotionLevel 子档位）
- `src/engine/shared/config_variables_qmclient.h`（新配置）
- `src/game/client/components/qmclient/menus_qmclient.cpp`（栖梦-视觉页配置区）

### 风险与依赖
- 动画参数化是系统性工程，建议分批做：先做第一优先（4 项），再评估。
- 新增动画（死亡/复活/HUD入场）有渲染开销，需性能评估。
- `QmUiMotionLevel` 子档位拆分会影响所有 UI 动画调用点，改动面大。
- 统一用 `QmAnimCurves.h` 预设曲线时，要保证默认行为不变。

### 验收标准（第一优先批次）
1. 切枪/皮肤切换/聊天消失/HUD武器 4 组动画参数全部可配置。
2. 每组默认值与当前硬编码完全一致。
3. 栖梦-视觉页有「游戏内动画」配置子区。
4. 至少新增 1 种新动画（建议：死亡淡出缩小 或 HUD 元素入场）。

---
## 第二批待办整合（用户补充 TODO，2026-06-20）

> 本节整合用户在动画审计后补充的一批 TODO。每项标注现状（已实现/部分/全新）、是否触及上游保护区域、优先级。
> 与 docs/dyl/QmClient_docs/现有现状页交叉引用，避免重复规划。

### B1. 名字板钩子进度条（中优先级，部分已有基础）

**现状**：
- 无尽钩图标已实现（`hud.cpp:4062-4068` `m_EndlessHookOffset`，`m_EndlessHook` 状态）
- 强弱钩：资源注册有别名（`assets_resource_registry.cpp:56`），但名字板**没有**强弱钩贴图显示
- 钩子时长：tuning 层 `HookDuration`（默认 1.25s），钩住玩家最大 1.2s
- **没有**钩子进度条（圆形加载条）渲染

**子项**：
1. 钩子时间进度条（圆形）——显示在钩子释放者 Tee 旁。需读取 `HookDuration` 调谐参数计算进度
2. 无限钩进度条替换为 ♾️ 符号——复用现有无尽钩图标逻辑
3. 进度条颜色渐变（绿→黄→红），低时间闪烁
4. 强弱钩显示自己——显示自己的强弱钩贴图（而非数字）

**约束**：触及上游物理区域（HookDuration 调谐）。相关现状页：`docs/dyl/QmClient_docs/规划/钩子进度状态.html`

**影响范围**：`nameplates.cpp`（名字板渲染）、`hud.cpp`（无尽钩）、可能 `players.cpp`。⚠️ 物理敏感，需先处理 HookDuration 调谐、0.7 负 tick 裁剪。

### B2. HUD 编辑器增强（中优先级，部分已有）

**现状**：
- 吸附已有：`SnapAxisToScreenEdges`、`SnapAxisToGuides`（hud_editor.cpp:780-781）、边缘锚定（:782-784）
- **没有**红绿虚线视觉反馈（当前吸附无视觉指示）
- **没有** Flexbox 拖拽排列（当前是单个元素独立拖拽）

**子项**：
1. 拖拽排列 HUD 元素位置和大小（Flexbox 布局）——需重构 HUD 编辑器的布局模型
2. 红、绿虚线拖拽吸附增强——在 `SnapAxisToGuides` 命中时绘制虚线视觉

**影响范围**：`hud_editor.cpp` / `hud_editor.h`（核心）、`hud.cpp`（所有 BeginTransform 调用点）。相关现状页：`docs/dyl/QmClient_docs/规划/HUD视觉辅助状态.html`

**风险**：Flexbox 是大改动，影响所有 HUD 元素布局。建议先做红绿虚线（小改动），Flexbox 另行调研。

### B3. 武器命中反馈（中优先级，全新）

**现状**：钩子/手枪命中时无特定粒子反馈。准星不显示路径中的表面/玩家是否可钩中。

**子项**：
1. 钩子命中时产生特定粒子反馈（复用粒子系统）
2. 准星显示路径中的表面、玩家是否可钩中——联合弹道辅助线代码（`weapon_trajectory.cpp`）

**影响范围**：`weapon_trajectory.cpp`、`effects.cpp`（粒子）、可能 `hud.cpp`（准星）。⚠️ 物理敏感（钩子命中判定）。

**注**：与第一批复盘里 frontend design 建议的「钩中反馈」一致，可合并实现。

### B4. 聊天气泡漫画样式（低优先级，全新）

**现状**：聊天气泡配置有 `QmChatBubbleTextColor` 等（config_qmclient.h:387），有消失动画（`QmChatBubbleAnimation`），但**没有**样式枚举（漫画气泡/圆角矩形/无背景等）。

**子项**：
1. 聊天气泡大小、颜色、透明度自定义（部分已有颜色，缺大小/透明度）
2. 漫画气泡样式（带尾巴指向说话者）

**影响范围**：`nameplates.cpp:2075-2124`（气泡渲染）、`config_variables_qmclient.h`（样式枚举）。相关现状页：`docs/dyl/QmClient_docs/规划/聊天UI状态.html`

### B5. 自动跟随好友服务器（中优先级，数据源已就绪）

**现状**：跟随逻辑未实现，但**数据源和跳转 API 都已就绪**——好友列表本身就显示好友当前服务器（`CFriendItem::ServerInfo()`，`menus_browser.cpp:2155`），双击好友即可 `Connect()` 加入（`menus_browser.cpp:2568-2575`）：
```cpp
if(ButtonResult == 1 && Friend.ServerInfo())
{
    str_copy(g_Config.m_UiServerAddress, Friend.ServerInfo()->m_aAddress);
    if(Ui()->DoDoubleClickLogic(pListItemId))
        Connect(g_Config.m_UiServerAddress);  // 双击好友直接加入其服务器
}
```
`CServerInfo` 由 serverbrowser 每次刷新维护，`Friend.ServerInfo()->m_aAddress` 就是好友当前服务器地址。**不需要 presence 服务或服务器协议改动。**

**子项**：
1. 可设定时间再跟踪好友 IP 然后跳转，避免频繁跳转，需 UI 文本提示「正在保持跟随」
2. 好友超时后（跟到服务器后）继续跟随跳转，上限 2 次，避免被耍

**实现路径**：轮询目标好友的 `ServerInfo()->m_aAddress`，检测变化 → 用户设定的延迟计时器 → `Connect(addr)` 跳转 → 跳转计数器上限 2 次 → UI 文本提示「正在跟随 XXX」。所有依赖（数据源、跳转、好友进入广播 `qmclient.cpp:155`）均已存在。

**影响范围**：`src/game/client/components/menus_browser.cpp`（好友列表轮询）、新增跟随状态机（建议放 `qmclient/`）、UI 提示。相关现状页：`docs/dyl/QmClient_docs/规划/服务器收藏状态.html`

### B6. 马甲注册与识别（中优先级，双类型，客户端打基础 + 服务端由远程仓库主实现）

**需求重述**（用户三轮澄清后定稿）：马甲分两种类型，UI 和数据流完全不同。类型 A 保持完整功能（不退化），客户端代码先行，服务端接口由远程仓库主（中心服务 `42.194.185.210` 维护者）实现。

#### 识别体系（已存在，复用，无需新建）

中心服务已有完整的稳定身份识别链：
- `machine_hash`：本机硬件/随机回退生成的 sha256（`qmclient.cpp:1402-1456`），稳定标识一台机器
- `qid`：服务端按 `machine_hash` 签发的稳定用户 ID，改名不变
- `/qm/token` 接口返回 auth_token（也接受 `qid` 字段名，`qmclient.cpp:1587-1594`）
- `/qm/users.json` 每个用户条目带 `qid` 字段（`qmclient_utils.cpp:130-132`）
- 客户端运行时 `MarkQ1menGSyncClient(ClientId, ..., pQid, ...)` 把 qid 绑到游戏内 ClientId（`qmclient.cpp:1641`、`gameclient.h:1201`）
- `GetQ1menGClientQid(ClientId)` 从 ClientId 反查 qid（`gameclient.h:1203`）——**识别链已闭合：名字 → qid → ClientId**

> **关键结论**：DDNet 名字能随便改，名字不能当主键，但 `qid` 稳定。用户注册时填的是名字，客户端内部把名字转成 qid 注册，保证绑定关系跨改名稳定。

#### 类型 A：公告马甲（alt_account）——主动注册，所有 QmClient 用户可见

玩家自己声明「我是 XXX 的马甲」，注册后所有 QmClient 用户都能看到标注。

**UI 流程（极简，用户填名字即可，无发码/无验证码）**：
1. **马甲侧（设置页 → QmClient → 马甲管理 → 我是马甲）**：
   - 一个 `DoEditBox` 输入主号名（复用 Axiom 密码输入的极简模式，但不隐藏，`menus_qmclient.cpp:3549-3568`）
   - 一个 `DoEditBox` 输入自己的马甲名（即"我对外显示的马甲身份名"，默认=当前游戏名）
   - 按钮「注册马甲」
   - 显示当前注册状态：「已注册为主号 XXX 的马甲」/「未注册」
   - 按钮「取消注册」
2. **主号侧（无需 UI 操作）**：主号什么都不用做——任何 QmClient 用户都能在名字板/计分板看到「[XXX的马甲]」标注。主号可在设置页查看「谁把我标为马甲」（只读列表），但不需要审批。

> 用户明确要求：UI 不要复杂，不要发码/绑定码（用户看不见），就填主号名和马甲名即可。

**注册时客户端做的事（名字 → qid 转换）**：
1. 用户填的主号名 → 客户端在 `/qm/users.json` 在线名单里按名字查到主号的 `qid`（`m_vLocalServerMarks`，`qmclient_utils.cpp:126-132`）
2. 客户端调 `POST /qm/alt/register`（携带自己的 auth_token + 主号 qid + 马甲名）→ 服务端记录 `{ alt_qid: 我的qid, main_qid: 主号qid, alt_name: 马甲名 }`
3. 主号不在线时无法立即注册（查不到 qid）——客户端提示「主号当前不在线，请稍后重试」或「请主号先上线一次」。这是无服务端名单的唯一限制，可接受。

**数据流（复用现有 `/qm/*` 服务，新增字段/接口）**：
- 上报：`/qm/report` 的每个 player 对象（`qmclient.cpp:1526-1537`）增加可选 `alt_of` 字段（主号 qid，未注册则为空/省略）——服务端如不识别此字段会忽略，不影响现有功能
- 下发：`/qm/users.json` 每个用户条目增加可选 `alt_of` 字段（主号 qid）和 `alt_name` 字段（马甲显示名）；客户端解析时缓存 qid→主号qid 映射（`qmclient_utils.cpp:130-143` 加两行解析）
- 客户端渲染时：`GetQ1menGClientQid(ClientId)` 拿到该玩家 qid → 查缓存 → 标注主号名

**服务端需新增接口（由远程仓库主 / 中心服务维护者实现，本开发者不实现服务端代码）**：
- `POST /qm/alt/register`（body: `auth_token` + `main_qid` + `alt_name`）→ 记录主从关系
- `POST /qm/alt/unregister`（body: `auth_token`）→ 解除
- `GET /qm/alt/list?auth_token=...` → 查询某用户的马甲列表（主号侧只读展示用）
- `/qm/users.json` 扩展：每个用户条目可选带 `alt_of`（主号 qid）和 `alt_name`

> **明确标注：以上 4 个服务端改动由远程仓库主实现，不在本 spec 的客户端实现范围内。客户端代码先按此契约写好调用，服务端未就绪时功能静默不可用（注册按钮提示「服务暂不可用」），不报错、不影响其他功能。**

**客户端实现范围（本开发者做）**：
1. 「马甲管理」设置区 UI（`menus_qmclient.cpp`，极简两输入框）
2. 名字→qid 转换逻辑（复用 `/qm/users.json` 查询，`qmclient_utils.cpp`）
3. `/qm/alt/register` 调用封装（`qmclient.cpp`，HTTP POST，带 auth_token）
4. `/qm/users.json` 解析扩展：读 `alt_of` / `alt_name` 字段（`qmclient_utils.cpp:130-143`）
5. 缓存 + 渲染：名字板（`nameplates.cpp`）/计分板（`scoreboard.cpp`）标注「[主号名的马甲]」，复用 warlist 的标注布局模式
6. 配置项：`QmAltEnabled`（开关）、`QmAltMainName`（主号名）、`QmAltName`（马甲名）
7. 服务端未就绪时的优雅降级（HTTP 404/超时 → 提示服务不可用，不崩溃）

#### 类型 B：私人标注（private_tag）——你标注别人，仅自己可见（类似 warlist）

你给任意玩家打私人标签「这是 XXX 的马甲」，仅自己可见，不上报中心服务。

**实现：复用 TClient warlist 机制**。warlist 是现有完整的私人标注系统：
- `CWarList` 组件（`warlist.h/cpp`），本地存储 `qmclient_warlist.cfg`（`config_domains.h:12`）
- 支持自定义分组（`CWarType`）、名字/部落标注、原因文本、颜色
- 渲染钩子已遍布：名字板（`nameplates.cpp:601-607`）、计分板（`scoreboard.cpp:1292`）、聊天（`chat.cpp:2200`）、旁观菜单、指示器（`player_indicator.cpp:110`）
- 控制台命令 `+war_name`/`+war_clan`（`warlist.cpp:55-69`）

**实现选择（推荐后者）**：
- 方案 1：在 warlist 里加一个「马甲」内置分组（复用 `CWarType`，reason 字段存主号名）
- 方案 2（推荐）：新建 `CPrivateTag` 组件，复用 warlist 的存储和渲染模式，但语义独立（避免「敌对」和「马甲」混在一个列表里）。存 `tag_target → label`，名字板显示 label

**UI**：右键玩家名（复用好友右键弹窗模式 `menus_browser.cpp:2924`）添加「标注为 XXX 的马甲」。

#### 验收标准
1. **类型 A（服务端就绪后）**：马甲填主号名 + 马甲名 → 注册成功 → 所有 QmClient 用户在名字板/计分板看到「[主号名]的马甲」
2. **类型 A（服务端未就绪）**：注册按钮提示「服务暂不可用」，不报错，不影响其他功能，UI 正常显示
3. **类型 A 改名稳定性**：主号或马甲改名后，因绑定基于 qid 而非名字，关系不丢失
4. **类型 B**：能给任意玩家打私人标签、仅自己可见、名字板/计分板显示
5. 两类标注可共存（同一玩家可能既有公告马甲标注又有私人标注）

#### 影响范围
- 客户端（本开发者）：`menus_qmclient.cpp`（马甲管理 UI）、`qmclient.cpp`（注册调用 + report 字段）、`qmclient_utils.cpp`（users.json 解析扩展）、`nameplates.cpp`/`scoreboard.cpp`（渲染）、`config_variables_qmclient.h`（配置）
- 类型 B：新增 `private_tag.cpp` 或扩展 `warlist.cpp`
- 服务端（远程仓库主实现）：`/qm/alt/register`、`/qm/alt/unregister`、`/qm/alt/list`、`/qm/users.json` 扩展

#### 风险
- **类型 A 依赖远程仓库主实现服务端**——客户端代码先行按契约写好，服务端未就绪则优雅降级。这是明确的设计取舍，非缺陷
- 主号不在线时无法注册（查不到 qid）——可接受限制，或后续考虑服务端按名字缓存 qid（需服务端改动）
- 私人标注用名字匹配，改名会失效（类型 A 用 qid 不受影响）
- 任何人都能声明是某人的马甲（无主号审批）——用户明确接受此设计，简化优先于防冒认。如后续需要防冒认，再加服务端白名单

### B7. 好友分类（大部分已实现，仅需增强 UI）

**现状**：**已完整实现！** `CFriends` 有分类增删改查全套（`friends.cpp:81-398`）：
- 控制台命令：`friend_category_add`/`rename`/`remove`/`set_friend_category`
- 浏览器页面有分类弹窗（`m_FriendsCategoryPopupContext`）、拖拽排序（`s_CategoryDragState`）
- 分类数据结构 `m_aCategory[64]`（friends.h:14）

**剩余缺口**（用户 TODO 中的「改善」）：
1. 浏览器页面分类管理更方便（当前可能有但交互不够顺）
2. 添加好友时分类选择更显眼

**影响范围**：`menus_browser.cpp`（分类 UI）。相关现状页：`docs/dyl/QmClient_docs/规划/服务器收藏状态.html`、`收藏浏览状态.html`

### B8. 持久化统计数据（Ranks）（中优先级，全新）

**现状**：当前每局重置，玩家统计仅会话内有效。搜索 `PersistStat`/`StatRank` 无本地持久化命中。

**子项**：
1. 本地 JSON/Toml 持久化存储，支持跨局统计（保存数量：限制和无限）
2. 统计面板导出增强（图表可视化）
3. Kog 组队落水死亡榜（记录同一 Team 所有成员数据，Team 0 不生效）

**影响范围**：新增持久化文件、`statboard` 组件、`qmclient/` 工具。相关现状页：`docs/dyl/QmClient_docs/规划/持久统计状态.html`、`本地比赛历史.html`

**注**：现状页已拆成 sidecar 小规格，Kog 死亡榜需组队数据记录。

### B9. 投票换图功能增强（低优先级，部分已有基础）

**现状**：DDNet 已有 `BrIndicateFinished`（已完成地图标记）和 `random_unfinished_map`。投票换图面板（`menus_ingame.cpp` PAGE_CALLVOTE）当前**不显示**已完成地图图标。

**需求**：投票换图面板显示已完成地图（同服务器列表中的已完成图标）。

**影响范围**：`menus_ingame.cpp`（投票换图面板渲染）。需复用浏览器列表的已完成图标逻辑。

### B 批优先级建议

| 优先级 | 项目 | 说明 |
--------|------|------|
 高 | B1 钩子进度条 | 物理敏感但价值高，有现状页 |
 高 | B3 武器命中反馈 | 全新，与动画审计 frontend design 合并 |
 中 | B2 HUD 编辑器增强 | 先做红绿虚线（小），Flexbox 另行调研 |
 中 | B8 持久统计 | 有现状页，sidecar 小规格 |
 中 | B7 好友分类增强 | 已实现，仅 UI 改善 |
 低 | B4 聊天气泡漫画样式 | 纯视觉 |
 低→中 | B5 自动跟随好友 | ✅ 数据源就绪（Friend.ServerInfo()），无需协议/presence |
 低 | B9 投票换图增强 | 小改动 |
 低→中 | B6 马甲注册 | 双类型：公告马甲（名字填+qid绑定，客户端先行，服务端由远程仓库主实现）+ 私人标注（复用warlist）|

---
## 第二轮深度调研结论（2026-06-20）

> 针对用户反馈「标了调研先行的就该去调研」，本轮对 B1/B5/B6/#8/#20.1/B7 做了代码级可行性确认，并修正了 B6 的误判。

### B5 自动跟随好友服务器 — ✅ 可行（数据源已就绪，第二轮纠错）

**第二轮纠错**：之前写「用 `/qm/users.json` presence 服务」是**错误的**。用户正确指出——好友列表本来就显示好友在哪个服务器，双击就能加入，这是 serverbrowser 现有功能，根本不需要 presence 服务。

**正确的数据源**：`CFriendItem::ServerInfo()`（`menus_browser.cpp:2155`）持有好友当前 `CServerInfo`，serverbrowser 每次刷新维护。双击好友即 `Connect(Friend.ServerInfo()->m_aAddress)` 加入（`menus_browser.cpp:2568-2575`）。

**实现路径**：轮询目标好友的 `ServerInfo()->m_aAddress` 检测变化 → 用户设定延迟计时器 → `Connect(addr)` → 跳转计数上限 2 次 → UI 提示「正在跟随 XXX」。所有依赖已就绪，无需协议改动、无需 presence 服务。

**教训**：调研时应先看现有功能（好友列表点击加入）是怎么实现的，而不是去找额外的服务。

### B6 马甲注册与识别 — ✅ 可行（修正：不触及 DDNet 协议，走 QmClient 中心服务）

**之前误判**：标注「触及服务器协议，需批准」。**错误。**

**调研结论**：不需要 DDNet 服务器协议支持。Axiom 已有「主号/分身」概念（`config_variables_qmclient.h:274-275` `QmAxiomLoginPassword`=主号、`QmAxiomDummyLoginPassword`=分身），说明 QmClient 用户体系已区分主号与分身。马甲关系可走 QmClient 自有中心服务：
- `/qm/report` 已上报玩家状态，扩展上报「主号标识」字段
- `/qm/users.json` 下发时携带马甲关系映射
- 客户端识别到目标玩家是某主号的马甲后，在名字板/计分板标注「XXX 的马甲」

**修正后约束**：不再是「需批准」的上游协议改动，而是 QmClient 中心服务 + 客户端显示的常规功能。唯一的外部依赖是中心服务端需要增加马甲关系的存储/查询接口（由 QmClient 服务端维护者实现，非 DDNet 上游）。

### B1 名字板钩子进度条 — ✅ 可行（有近似限制）

**调研结论**：钩子时长和钩住状态客户端可读，进度条可显示（近似值）。

**关键代码证据**：
- `gamecore.cpp:268`：钩住瞬间 `m_HookTick = SERVER_TICK_SPEED * (1.25f - m_Tuning.m_HookDuration)`，`HookDuration` 是 tuning 参数
- `gameclient.h:962`：客户端可读 `GetTuning(TuneZone)->m_HookDuration`
- snapshot 广播 `m_HookedPlayer`（`server/entities/character.cpp:1372` `pData->m_HookedPlayer = m_Core.HookedPlayer()`）和 `m_HookState`（ghost.cpp:43 证明可读）
- 无尽钩状态：`m_EndlessHook`（hud.cpp:4062-4068 已用于无尽钩图标）

**限制**：⚠️ `m_HookTick`（剩余钩 ticks）**不在 `CNetObj_Character` snapshot 里广播**，是 core 内部状态。进度条只能**估算**——记录 `HookedPlayer` 从 -1 变为有效 id 的那一 tick 作为起点，用 `1.25f - HookDuration` 作为总时长倒计时。这是近似值（服务端 tick 延迟），但视觉上够用。

**强弱钩**：`assets_resource_registry.cpp:56` 有「强弱钩」资源别名，但没有名字板贴图显示逻辑。B1 子项 4（显示自己的强弱钩贴图）需新增渲染。

**UI 实现方案**（用户追问「进度条找什么图标/UI，怎么实现」）：
1. **进度条渲染**：复用 `CUi::RenderProgressSpinner`（`ui.cpp:1848-1886`）——它是现成的圆形弧形进度条，传 `SProgressSpinnerProperties.m_Progress`（0.0~1.0）即画对应比例的填充弧。位置：钩子释放者 Tee 头顶或旁边（类似聊天气泡位置 `nameplates.cpp`），半径约 TeePhysicalSize 的 0.6-0.8 倍。
2. **进度计算**：`progress = 1.0 - (elapsed_ticks / total_ticks)`，`total_ticks = SERVER_TICK_SPEED * (1.25f - HookDuration)`。`elapsed_ticks` 从本地记录的钩住 tick 起算（近似，因 `m_HookTick` 不广播）。
3. **颜色渐变（绿→黄→红）**：`RenderProgressSpinner` 的 `Props.m_Color` 按 progress 插值 HSL（绿 120°→黄 60°→红 0°）。低时间（progress < 0.25）加 alpha 闪烁（`0.5 + 0.5*sin(time*10)`）。
4. **无限钩**：progress 条替换为 ♾️ 文本或复用现有 `m_EndlessHookOffset` 图标（`hud.cpp:4078,4065`），不画进度弧。
5. **新增配置**：`QmHookProgressBar`（开关）、`QmHookProgressBarScope`（仅自己/仅他人/全部，类似 `QmWeaponSwitchAnimScope`）、`QmHookProgressBarSize`。

### #8 滚动抖动根治 — 假设 A 确认为主因

**调研结论**：通过精读 `settings_resource_jobs.cpp:416-451`，确认**假设 A（预算阶梯跳变）是主因**。

**代码证据**：预算在三个阶段间离散跳变：
- finalize（:416-425）：滚动 16 → 恢复 48 → 正常 64
- gpu upload units（:427-438）：滚动 4 → 恢复 8 → 正常 12
- gpu upload limiter（:440-451）：滚动 96 → 恢复 192 → 正常 288

恢复帧只有 2 帧（`menus_settings.cpp:2514-2517`），阶段边界资源数突变 → 抖动。

假设 B（布局重算，`SettingsResourceSharedHeavyBudget` :1263 恢复期用 RecoveryBudget）也有贡献，但不是主因。

**根治方向明确**：把离散跳变（16→48→64）改为连续插值——在 `m_PostScrollRecoveryFrames` 倒数时用 `lerp(scroll_budget, normal_budget, t)` 而非阶梯值。改 `settings_resource_jobs.cpp:416-451` 的三个函数即可，不需要 perf 调试定位（根因已确认）。

### #20.1 简短服务器名 — 难度词枚举已存在，不需额外样本

**调研结论**：难度词检测逻辑已存在于 DDNet 和 QmClient（`menus_browser.cpp:85-91` 和 `menus_qmclient.cpp:5846-5855` 是完全相同的代码）：
```cpp
if(str_find_nocase(pText, "Novice")) return "Novice";
if(str_find_nocase(pText, "Moderate")) return "Moderate";
if(str_find_nocase(pText, "Brutal")) return "Brutal";
if(str_find_nocase(pText, "Insane")) return "Insane";
```

**缺口确认**：现有检测**只识别英文难度词**。CHN DDR 样本 `Moderate 中阶` 含中文「中阶」，Axiom 样本 `普通` 全中文。需要补全中文别名（普通=Novice，中阶=Moderate，野蛮=Brutal，疯狂=Insane）。

**修正**：之前标的「Axiom/CHN DDR 需用户提供服务器名样本才能设计准确匹配」**不再成立**——难度词枚举是固定的小集合，用户提供的两个样本（`Axiom 北京 普通 - CHN1O 钩累死`、`DDNet CHN7 西安 - Moderate 中阶`）已足够确定模式。KoG 已有 BC 实现（`menus_browser.cpp:141-215`）。

### B7 好友分类 — 修正：功能完整但入口隐蔽（非「已完整实现」）

**之前误判**：标「已完整实现」。**不够准确。**

**精确现状**：
- 后端：完整（`friends.cpp:81-398` 增删改查 + 控制台命令）
- 管理 UI：**存在但入口隐蔽**——唯一触发方式是**右键点击分类标题**（`menus_browser.cpp:2329-2337` `HeaderResult == 2`），弹出含「添加分类/重命名/删除」的菜单（`PopupFriendsCategory` :2924-3022）
- 添加好友时：有分类下拉（`:2899` DoDropDown），但不够显眼

**关键问题**：分类标题渲染（:2316-2337）**没有任何视觉提示「右键管理」**——没有齿轮图标、没有 tooltip。用户（包括本次调研的用户）必须自己发现这个隐藏交互。这正是用户原始 TODO「添加好友时分类选择更显眼（需考虑 UI 实现）」的真实痛点。

**B7 应聚焦**：
1. 在分类标题加可见的「管理」入口（齿轮/三点图标按钮），或加 tooltip 提示「右键管理分类」
2. 空分类状态下加「点击此处创建分类」引导
3. 添加好友对话框里的分类下拉增强（显眼的「+ 新建分类」选项）
