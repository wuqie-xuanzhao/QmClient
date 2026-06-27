# QmClient 碰撞箱模式 PRD

## 1. 背景

QmClient 当前已经存在 `Collision hitbox` 模块，入口在 `src/game/client/components/qmclient/collision_hitbox.cpp`，配置在 `src/engine/shared/config_variables_qmclient.h`。现有能力主要包括：

- 地图 Freeze / Death 外边界线显示。
- Tee 冻结点、死亡采样点显示。
- 盾牌拾取碰撞范围显示。
- 菜单开关与透明度、Freeze 颜色配置。

但当前模块更偏向“碰撞体积可视化”，还没有形成一个完整的“碰撞箱模式”。玩家想理解的是：Tee 的真实碰撞范围、Freeze/Death 触发点、锤子/枪/榴弹/激光/钩子等武器与 Tee 或地图的交互边界。因此需要把现有模块升级为一个可分层、可调试、可用于练习和教学的碰撞箱模式。

## 2. 目标

### 2.1 核心目标

提供一个客户端本地可视化模式，用来显示玩家在当前画面中最关心的碰撞与武器交互范围，帮助理解 DDNet/QmClient 中“为什么这里会中/不会中”“为什么这里会冻/会死/会钩中”。

### 2.2 用户收益

- 新手可以直观看到 Tee 真实碰撞点和地图危险边界。
- 练习玩家可以判断 Freeze、Death、Shield、Hook、Hammer、Grenade、Laser 的交互范围。
- 地图测试者可以快速发现实体边界与实际手感不一致的地方。
- 内容作者可以录制教学视频时打开 HitBox 层，解释碰撞逻辑。

### 2.3 非目标

- 不改变任何物理、预测、网络协议、demo 格式、地图行为。
- 不让玩家获得服务端没有发送的信息。
- 不实现服务端反作弊意义上的命中判定重写。
- 不保证所有 mod 的自定义武器都能完全准确显示，优先覆盖 DDNet/vanilla 常见逻辑。

## 3. 用户画像与场景

### 3.1 练习玩家

场景：玩家练习 edge、freeze skip、hammer fly、hook-through 等技巧，希望知道实际碰撞点在哪里。

需求：

- 看见自己 Tee 的真实物理范围。
- 看见 Freeze / Death 触发采样点。
- 看见锤子命中圆和目标 Tee 是否进入命中范围。

### 3.2 地图作者 / 测图玩家

场景：地图中某个 freeze/death/armor 位置看起来应该触发，但实际没有触发。

需求：

- 看见 tile 边界与 Tee 触发点。
- 看见 pickup 有效范围。
- 可临时只打开地图交互层，不显示武器层，避免画面过乱。

### 3.3 教学 / 视频作者

场景：录制教程时，需要解释武器命中、激光反弹、钩子距离等机制。

需求：

- 一键开启“教学预设”。
- 颜色清晰、透明度可调。
- 不污染日志，不影响性能。

## 4. 产品范围

### 4.0 已确认产品决策

- 功能正式命名为：`碰撞箱模式`。
- UI 需要保留旧 `Collision hitbox` 文案作为兼容提示，建议显示为 `碰撞箱模式 / Collision hitbox`。
- V1 默认显示武器层，不只显示地图/Tee/pickup。
- V1 允许显示所有玩家的武器交互，但实现上只处理屏幕内玩家/实体以控制性能。
- 需要提供快捷键支持，建议提供控制台绑定示例：`bind h toggle qm_hitbox_mode 0 1`；如果默认绑定快捷键，必须避免覆盖用户已有绑定。

### 4.1 V1 必做

#### A. 模式总开关

- 在 QmClient 模块页保留并升级当前 `Collision hitbox` 卡片为 `碰撞箱模式 / Collision hitbox`。
- 增加总开关：`显示碰撞箱模式`。
- 保留透明度配置。
- 保留 Freeze 颜色配置。

#### B. 分层显示

新增子开关：

- `Map hazard hitboxes`
  - 显示 Freeze / Deep Freeze / Death 边界。
  - 基于现有 `RenderTileHitboxes()`。
- `Tee hitboxes`
  - 显示 Tee 物理范围、中心点、Freeze/Death 采样点。
  - 基于现有 `RenderTeeHitboxes()` 扩展。
- `Pickup hitboxes`
  - 显示 Shield 等 pickup 有效范围。
  - 基于现有 `RenderPickupHitboxes()`。
- `Weapon interaction hitboxes`
  - 显示锤子、钩子、榴弹爆炸、激光/霰弹枪线段等交互范围。
  - V1 默认开启，覆盖屏幕内所有玩家。

#### C. Tee 物理范围显示

显示内容：

- Tee 物理半径：`CCharacterCore::PhysicalSize()`，当前源码常量为 `28.0f`。
- Tee 中心点。
- Freeze 触发中心采样点。
- Death 触发四点采样点，当前模块使用 `PhysicalSize() / 3.0f` 的四角采样。

显示规则：

- 本地 Tee 使用更高亮颜色。
- 分身使用同色系但略低透明度。
- 其他玩家根据 `cl_show_others_alpha` 继续衰减。
- 只显示屏幕内玩家。

#### D. 锤子交互范围

显示内容：

- 锤子命中圆心和命中半径。
- 命中目标 Tee 时高亮目标。
- 被 `HammerHitDisabled()` 禁用时显示为灰色/虚线或不显示。

实现依据：

- 复用 `CGameClient::GetPredictedHammerHitbox()`。
- 复用 `CGameClient::FindPredictedHammerHitTargets()`。
- 当前预测锤子命中点计算：方向向量乘以 `ProximityRadius * 0.75f`，命中半径为 `ProximityRadius * 0.5f`。

#### E. 榴弹爆炸范围

显示内容：

- 爆炸最大影响半径：当前预测世界 `CreateExplosion()` 使用 `135.0f`。
- 内圈半径：当前预测世界使用 `48.0f`。
- 圆心为爆炸点或预测 projectile 碰撞点。

显示规则：

- 只显示当前快照/预测中可见 projectile 的预测爆炸点。
- 对实际已发生 explosion event，可短暂显示淡出圆。

#### F. 激光 / 霰弹枪线段

显示内容：

- 激光/霰弹枪从 `From` 到 `To` 的线段。
- 碰撞点/反弹点。
- 与 Tee 相交的判定位置。

实现依据：

- `src/game/client/laser_data.cpp` 可提取快照激光数据。
- 预测层 `CLaser` 使用 `Collision()->IntersectLineTeleWeapon()` 处理武器线段与地图交互。

#### G. 钩子范围

显示内容：

- 当前 Tee 的 hook 起点。
- hook 方向线段。
- hook 与玩家碰撞近似范围。
- hookable / nohook tile 命中点。

实现依据：

- `CCharacterCore::Tick()` 中 hook 逻辑使用 `PhysicalSize()`、`IntersectLineTeleHook()`、玩家圆形交互。
- 客户端玩家渲染中已有 hook 线段和玩家相交辅助逻辑，可作为渲染参考。

### 4.2 V1 可选

- 显示枪子弹直线路径和最近碰撞点。
- 显示 ninja 近战范围。
- 显示每层图例 legend。
- 鼠标悬停某个 hitbox 时显示数值。
- 支持“只显示本地 / 本地+分身 / 全部玩家”，V1 默认“全部玩家”。

### 4.3 暂不做

- 服务端真实历史回放命中重算。
- 多 mod 自定义武器的完全适配。
- 将 HitBox 数据写入 demo。
- 网络同步 HitBox 状态给其他客户端。

## 5. UI / 交互设计

### 5.1 设置入口

入口位置：

- `QmClient` 设置页现有 `Collision hitbox` 模块卡片。

建议卡片标题：

- `碰撞箱模式`
- 兼容副标题：`Collision hitbox · Show Tee, map and weapon interaction hitboxes`

### 5.2 配置项

建议配置变量：

| 配置项 | 类型 | 默认 | 范围 | 说明 |
|---|---:|---:|---:|---|
| `qm_hitbox_mode` | int | 0 | 0-1 | 碰撞箱模式总开关 |
| `qm_hitbox_show_map` | int | 1 | 0-1 | 显示地图危险边界 |
| `qm_hitbox_show_tees` | int | 1 | 0-1 | 显示 Tee 碰撞体 |
| `qm_hitbox_show_pickups` | int | 1 | 0-1 | 显示 pickup 范围 |
| `qm_hitbox_show_weapons` | int | 1 | 0-1 | 显示武器交互，V1 默认开启 |
| `qm_hitbox_alpha` | int | 80 | 0-100 | 全局透明度 |
| `qm_hitbox_player_scope` | int | 2 | 0-2 | 玩家范围：0=本地，1=本地+分身，2=所有玩家 |
| `qm_hitbox_color_freeze` | color | 当前值 | - | Freeze 边界颜色 |
| `qm_hitbox_color_tee` | color | 默认青色 | - | Tee 碰撞体颜色 |
| `qm_hitbox_color_weapon` | color | 默认黄色 | - | 武器范围颜色 |

兼容建议：

- 现有 `qm_show_collision_hitbox` 可作为迁移来源。
- 现有 `qm_collision_hitbox_alpha` 可迁移到 `qm_hitbox_alpha`。
- 也可以不重命名，保持旧配置名并只新增子开关，减少迁移成本。

### 5.3 快捷键 / 控制台

建议提供控制台命令：

- `qm_hitbox_mode 1`
- `qm_hitbox_show_weapons 0`
- `qm_hitbox_player_scope 2`

需要提供快捷键支持：

- 推荐绑定示例：`bind h toggle qm_hitbox_mode 0 1`。
- 如果实现默认绑定，必须先检查是否会覆盖用户已有绑定；更稳妥的 V1 方案是在 UI 中提供“复制绑定命令”或“设置快捷键”入口。

## 6. 可视化规范

### 6.1 颜色建议

| 类型 | 颜色 | 说明 |
|---|---|---|
| 本地 Tee | 青色 | 主角信息优先 |
| 分身 Tee | 蓝色 | 与本地同组但区分 |
| 其他 Tee | 白色/灰色 | 不抢画面 |
| Freeze | 使用配置颜色，默认品红 | 沿用现有配置 |
| Death | 黑色或红黑 | 危险边界 |
| Shield pickup | 黄色 | 沿用当前实现 |
| Hammer | 橙色 | 近战范围 |
| Grenade explosion | 红色外圈 + 橙色内圈 | 外圈影响、内圈强力区 |
| Laser / Shotgun | 紫色/蓝色 | 线性武器 |
| Hook | 绿色 | hook 交互 |

### 6.2 层级顺序

推荐渲染顺序：

1. 地图危险边界。
2. pickup 范围。
3. 武器交互范围。
4. Tee 碰撞范围。
5. 命中高亮 / 文本提示。

### 6.3 抗干扰要求

- 默认透明度不超过 80%。
- 线宽不宜过粗。
- 大范围圆形只画边框，不填充。
- 如果同屏实体过多，优先降低非本地玩家武器层透明度或只绘制屏幕内最近实体；默认不直接隐藏，因为 V1 决策要求显示所有玩家武器交互。

## 7. 技术设计

### 7.1 现有模块改造

建议继续使用 `CCollisionHitbox` 作为组件，不新增平行组件，避免重复渲染和配置割裂。

文件范围：

- `src/game/client/components/qmclient/collision_hitbox.h`
- `src/game/client/components/qmclient/collision_hitbox.cpp`
- `src/engine/shared/config_variables_qmclient.h`
- `src/game/client/components/qmclient/menus_qmclient.cpp`
- 必要时更新 `CMakeLists.txt`

### 7.2 模块结构

建议拆分渲染函数：

- `RenderMapHitboxes()`
- `RenderTeeHitboxes()`
- `RenderPickupHitboxes()`
- `RenderHammerHitboxes()`
- `RenderExplosionHitboxes()`
- `RenderLaserHitboxes()`
- `RenderHookHitboxes()`
- `RenderLegend()` 可选

现有函数可以保留并渐进重命名，避免一次性大重构。

### 7.3 数据来源

| 数据 | 来源 | 可靠性 |
|---|---|---|
| 地图 tile | `Layers()` / `Collision()` | 高 |
| Tee 位置 | `m_aClients[].m_RenderPos` / predicted core | 高 |
| Tee 物理半径 | `CCharacterCore::PhysicalSize()` | 高 |
| 锤子范围 | `GetPredictedHammerHitbox()` | 高 |
| 锤子目标 | `FindPredictedHammerHitTargets()` | 高 |
| 榴弹爆炸 | projectile 预测 / explosion event | 中 |
| 激光线段 | `laser_data.cpp` / snap entities | 中高 |
| 钩子线段 | character core / render state | 中 |

### 7.4 兼容性要求

- 只读客户端已有状态，不修改 `CCharacterCore`、`CGameWorld`、`Collision()` 结果。
- 不新增网络消息。
- 不改变 demo 录制/播放格式。
- 在 demo playback 中尽量复用快照数据，预测不足时允许显示为“近似”。
- 对 sixup / 0.7 只做本地渲染兼容，不改变协议。

### 7.5 性能要求

- 默认关闭。
- 开启时只遍历屏幕内 tile / entity。
- 大地图缩放过远时继续使用现有范围裁剪逻辑。
- 武器层默认覆盖所有玩家，但只处理屏幕内玩家/实体，并对远距离或不可见实体做裁剪。
- 不在每帧分配大 vector，不反复读取整张地图。
- 地图 hitbox 缓存在 `OnMapLoad()` 构建，延续当前实现。

## 8. 验收标准

### 8.1 基础功能

- 开启 HitBox 模式后，能看到 Freeze/Death 边界、Tee 采样点、Shield 范围。
- 关闭总开关后不渲染任何 HitBox 内容。
- 单独关闭某一层后，该层不再渲染。

### 8.2 武器交互

- 使用锤子时，能看到锤子命中圆。
- 有目标进入锤子命中范围时，该目标高亮。
- 榴弹爆炸点显示外圈 `135.0f` 和内圈 `48.0f`。
- 激光/霰弹枪显示从 `From` 到 `To` 的线段和碰撞点。
- 钩子显示当前方向和可交互范围。

### 8.3 兼容性

- 普通联机、分身、demo playback 不崩溃。
- 开关状态保存到配置。
- 不影响玩家移动、命中、预测结果。
- 不影响服务器、demo、skin、map 格式。

### 8.4 性能

- 默认关闭时无额外可感知开销。
- 开启所有层后，在常规地图和 64 人场景中无明显掉帧。
- 超大地图或高缩放视角下仍能裁剪渲染范围。

## 9. 实施拆分

### Milestone 1：现有模块产品化

- 将 `Collision hitbox` UI 改为 `碰撞箱模式 / Collision hitbox`。
- 增加分层开关。
- 保留现有地图/Tee/pickup 渲染。
- 补充配置迁移或兼容旧配置。

### Milestone 2：Tee 与锤子交互

- Tee 物理圆显示。
- 本地/分身/其他玩家颜色区分。
- 接入 `GetPredictedHammerHitbox()`。
- 接入 `FindPredictedHammerHitTargets()`。

### Milestone 3：投射物与线性武器

- 榴弹爆炸圈。
- 激光/霰弹枪线段。
- hook 线段与 hookable/nohook 命中点。

### Milestone 4：可用性打磨

- 图例。
- 玩家范围模式：本地 / 本地+分身 / 所有玩家。
- 文案和本地化。
- 性能压测和异常场景处理。

## 10. 风险与注意事项

### 10.1 判定近似风险

部分武器交互只能基于客户端预测或快照近似显示。PRD 和 UI 文案应避免承诺“100% 服务端真实命中范围”，建议文案使用 `Predicted` / `Approx.`。

### 10.2 画面噪声

V1 已确认默认开启武器层，因此需要通过透明度、颜色、屏幕内裁剪和玩家范围配置控制画面噪声，而不是默认关闭武器层。

### 10.3 性能

武器层如果遍历全玩家、全 projectile、全 laser，每帧可能带来额外开销。V1 需要支持所有玩家，但必须限制为屏幕内玩家/实体，并避免每帧大规模临时分配。

### 10.4 DDNet 兼容性

不得把可视化逻辑写入 gamecore 或服务器逻辑。HitBox 模式只应存在于 client component 层。

## 11. 已确认问题

1. 命名使用 `碰撞箱模式`。
2. V1 默认显示武器层。
3. V1 允许显示所有玩家武器交互。
4. 需要快捷键支持，推荐绑定示例为 `bind h toggle qm_hitbox_mode 0 1`。
5. UI 保留旧 `Collision hitbox` 文案作为兼容提示。
