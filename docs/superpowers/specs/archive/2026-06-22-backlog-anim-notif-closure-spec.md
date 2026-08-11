# Backlog 收口规格：#12 / #13 动画配置 + #17 通知文本语义化

- 创建：2026-06-22
- 状态：active
- 上游规格：`docs/superpowers/specs/2026-06-20-待办整合规格.html`
- 核验基线：HEAD `dff7a87fa9`（2026-06-22 `read` / `search` 实际核验）

## 背景与现状核验

第二轮只读核验（subagent + 主代理 `read`/`search`）确认了 backlog 整合规格与当前 HEAD 代码之间的实际差距：

### #12 切枪动画高级配置 —— 已完整实现，无需再动

| 验收项 | 状态 | 证据 |
| --- | --- | --- |
| 新增配置变量 | done | `config_variables_qmclient.h:140-143` `QmWeaponSwitchAnimDurationMs`（300，50-2000ms）/ `QmWeaponSwitchAnimDistance`（40，0-100）/ `QmWeaponSwitchAnimRotation`（360，0-1440°）/ `QmWeaponSwitchAnimEasing`（0，0-3） |
| 硬编码替换为配置读取 | done | `players.cpp:907-915`，所有旧魔法数字（0.3s / 40px / 2π / easeOutCubic）均替换为 `g_Config.m_Qm*` |
| 设置页暴露高级配置 | done | `menus_qmclient.cpp:5499-5560`，开关 + 范围 + duration/distance/rotation/easing 全部有 slider/dropdown |
| 默认值完全向后兼容 | done | 默认 300ms / 40 / 360° / easing 0 ≡ easeOutCubic，与旧硬编码值一致 |

命名差异（`_duration_ms` vs `_duration`、`_distance` vs `_offset`、deg vs rad）属实现细节，不影响功能或兼容性。本规格不再改动 #12。

### #13 皮肤切换过渡动画 —— 参数化已完成，仅缺新视觉类型

| 验收项 | 状态 | 证据 |
| --- | --- | --- |
| 参数化魔法数字至统一 Intensity 配置 | done | `render.h:281-361`，5 种类型均接入 `SkinChangeTransitionIntensityScale(Intensity)`；配置 `QmSkinChangeTransitionIntensity`（0-300%，默认 100）`config_variables_qmclient.h:131` |
| 配置体系 Type/Duration/Easing/Intensity | done | `config_variables_qmclient.h:127-131`，4 项齐全 |
| UI 暴露所有配置 | done | `menus_qmclient.cpp:3980-4054`，类型下拉 + duration + easing + intensity |
| 新增 2-3 种视觉差异明显的类型 | **todo** | `render.h:186-194` 仅有 `GHOST_POP/FADE_SCALE/SLIDE_LEFT/SPIN_POP/THEME_SWITCH` 5 种。spec 建议的 Glitch / Elastic / Particle 未实现 |

### #17 通知栏文本语义化 —— 机制完整，6 条译文需重写

通知语义引擎完整：`hud_notification_static_rules.h`（4 宏组）/ `upstream_rules.h` / `alias_rules.h` / `catalog.cpp` / `rules.cpp` 全部就位。第二轮 audit（159 条 key）结论：

- 153 条 OK（96.2%）
- 6 条 AWKWARD（需重写）
- 0 条 MISSING

AWKWARD 集中在两类问题：①「被处死」机器翻译腔（4 处）② 整句操作说明丢失（2 处）。

## 目标

1. **#13**：新增 2 种视觉差异明显的皮肤切换过渡类型 —— Glitch（故障抖动）与 Elastic（弹性缩放）。不实现 Particle（粒子消散），因为粒子需要额外的资源/emit 路径，超出本批「纯数学过渡」范围，留作后续小 spec。
2. **#17**：重写 6 条通知译文的简中版本，使其口语化、不丢操作说明。
3. 同步更新 backlog 整合规格的 #12/#13/#17 状态。
4. 完成验证：i18n 链、相关单测、quick gate。

## 非目标

- 不改动 #12 任何代码（已完整）。
- 不实现 Particle 粒子消散类型（超出纯过渡数学范围）。
- 不逐条重写 153 条 OK 译文（audit 已确认它们自然）。
- 不修改协议、物理、demo 格式等上游保护区域。

## 设计方向

### #13.1 Glitch（故障抖动）过渡类型

新增枚举 `SKIN_CHANGE_TRANSITION_GLITCH`。视觉特征：

- 旧皮肤向左/右高频随机偏移（伪故障位移），alpha 快速衰减
- 新皮肤从反向抖入，带轻微缩放抖动
- 使用 `sin(Progress * 高频)` 产生抖动，乘以 `IntensityScale` 控制幅度
- 与现有 5 种类型视觉差异明显（高频抖动是其他类型都没有的）

接入点：

- `render.h`：枚举新增 `SKIN_CHANGE_TRANSITION_GLITCH`，更新 `SKIN_CHANGE_TRANSITION_TYPE_COUNT`
- `render.h:281-361` `ComputeSkinChangeTransitionBlend`：新增 case
- `config_variables_qmclient.h:128`：`QmSkinChangeTransitionType` 的 max 从 4 改为 5
- `menus_qmclient.cpp:4001-4007`：下拉选项新增 `Localize("Glitch")`
- 测试：`skin_transition_test.cpp` 新增 `GlitchTypeAppliesJitterOffsets`

### #13.2 Elastic（弹性缩放）过渡类型

新增枚举 `SKIN_CHANGE_TRANSITION_ELASTIC`。视觉特征：

- 旧皮肤向中心收缩淡出（Y 轴压缩 + 缩小）
- 新皮肤弹性放大（overshoot 后回弹），复用 easing 框架的 `EASE_OUT_BACK`
- 强度由 `IntensityScale` 控制 overshoot 幅度
- 与 SPIN_POP 的差异：无旋转，纯 Y 轴压缩 + 弹性放大

接入点：同 Glitch，枚举值 / case / config max / UI 下拉 / 测试。

关键约束：alpha 必须落在 [0, 1] 可绘制范围（已有 `ElasticBackKeepsAlphaInDrawableRange` 测试约束，新类型同样必须满足）。

### #17 译文重写

6 条 AWKWARD 译文的简中重写（仅改 simplified_chinese，其他语言不在本批范围）：

| display_key | 旧译文 | 新译文 | 问题 |
| --- | --- | --- | --- |
| Your team was killed because it couldn't finish anymore and hasn't entered /practice mode | 你的队伍因已无法完赛且未进入 /practice 模式而被处死 | 队伍已无法完赛，又没开 /practice，已被判负清退 | ①② |
| Your team has been killed because it contains an invalid tee state | 你的队伍因包含无效 tee 状态而被处死 | 队伍中检测到不合法的 tee 状态，已被清除 | ① |
| Your team was unlocked by an unlock team tile | 你的队伍已被解锁队伍图块解除锁定 | 你的队伍已被地图上的解锁图块解开锁定 | ①② |
| Enter /practice mode or restart to avoid the entire team being killed in 60 seconds | 请输入 /practice 或重新开始，否则整队将在 60 秒后被处死 | 请输入 /practice 或重新开始，否则 60 秒后整队会被判负 | ② |
| Unknown argument. Check '/rescuemode list' | 未知 rescue 模式参数 | 未知参数，输入 /rescuemode list 查看帮助 | ①③ |
| You can see other players. To disable this use DDNet client and type /showothers | 你当前可以看到其他玩家 | 你现在能看到其他玩家。想关掉的话，在 DDNet 客户端里输入 /showothers | ③ |

改的是真相源 `qmclient_scripts/languages_qmclient/translations/i18n/qmclient.toml` 的 simplified_chinese 字段，然后跑生成链更新 `data/languages/simplified_chinese.txt`。

## 影响范围

- `src/game/client/render.h`（枚举 + 2 个 case）
- `src/engine/shared/config_variables_qmclient.h`（1 行 max 值）
- `src/game/client/components/qmclient/menus_qmclient.cpp`（下拉选项数组 + i18n source key）
- `src/test/skin_transition_test.cpp`（2 个新测试）
- `qmclient_scripts/languages_qmclient/translations/i18n/qmclient.toml`（6 条 simplified_chinese + 2 个新类型名 key）
- `data/languages/*.txt`（生成产物，由生成链产出）
- `docs/superpowers/specs/2026-06-20-待办整合规格.html`（状态同步）

## 风险与依赖

- 低风险：纯视觉数学参数 + 纯文案。不影响游戏逻辑、网络、物理。
- alpha 越界风险：Elastic 类型的 overshoot 可能让 alpha 短暂超 1，必须 clamp（已有 `ElasticBackKeepsAlphaInDrawableRange` 测试约束新类型同样要满足）。
- i18n 链一致性：改 toml 后必须跑 extract → generate → validate → review_duplicate，否则 toml 与 txt 漂移。

## 验收标准

- [ ] `render.h` 新增 `SKIN_CHANGE_TRANSITION_GLITCH` / `SKIN_CHANGE_TRANSITION_ELASTIC` 枚举值，`ComputeSkinChangeTransitionBlend` 有对应 case。
- [ ] `QmSkinChangeTransitionType` max 从 4 改为 6（5 个旧 + Glitch + Elastic）。

  注：枚举顺序为 GHOST_POP=0 / FADE_SCALE=1 / SLIDE_LEFT=2 / SPIN_POP=3 / THEME_SWITCH=4 / GLITCH=5 / ELASTIC=6，TYPE_COUNT=7。max=6。

- [ ] `menus_qmclient.cpp` 下拉选项数组新增 Glitch / Elastic 两项，顺序与枚举一致。
- [ ] `skin_transition_test.cpp` 新增测试覆盖两种新类型（偏移/缩放方向、alpha 在 [0,1]）。
- [ ] 6 条通知译文的 simplified_chinese 已在 toml 更新，`data/languages/simplified_chinese.txt` 经生成链同步。
- [ ] `qmclient.toml` 新增 Glitch / Elastic 两个新类型名的多语 key（保持生成链无新 source key 缺失）。
- [ ] i18n 链全绿：extract_strings / generate_all / validate / review_duplicate。
- [ ] run_cxx_tests 全绿，quick gate 通过。
- [ ] backlog 整合规格 #12 / #13 / #17 状态更新为已实现/收口。
