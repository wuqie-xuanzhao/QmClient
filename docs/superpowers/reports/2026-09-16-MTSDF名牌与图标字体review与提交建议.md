---
title: MTSDF 名牌与图标字体工作 review 与提交建议
date: 2026-09-16
status: active
local_head: 65891fa85b438057c4ad4491dfaa99d18b4da99b
branch: dyl_dev
---

# MTSDF 名牌与图标字体：完成度 review 与提交建议

本轮做两件事：把 Duotone 从「样式可选、单层 MTSDF」推进到真正可用的双色渲染（shader + 资源生成链
+ 图集重生成 + 回归测试），并对整条 MTSDF 名牌/字体工作做完整度 review 与提交判断。

结论先行：**功能已经闭环、可以提交**，但提交前有两件事必须做——**拆提交**（工作区混了至少两条互不
相关的工作流）和**真机各后端看一眼**（本轮只做到编译 + 语法 + 单测 + 单进程日志级验证，没有任何视觉
核对，视觉核对按约定由你完成）。

---

## 一、交付清单

### 1.1 Duotone 双色（本轮主交付）

| 层 | 改动 | 文件 |
| --- | --- | --- |
| 编码契约 | 新增 `qm_msdf_param` 命名空间，把 `w` 分量定义成三态、区间互不重叠 | `src/engine/graphics.h` |
| 编码实现 | 用契约替换原先会互相覆盖的两次赋值 | `src/engine/client/graphics_threaded.cpp` |
| 着色器 | 三个后端同阈值解码 + secondary 走真距离场 | `data/shader/textured_msdf.frag`、`data/shader/vulkan/textured_msdf.frag`、`data/shader/metal/qmclient.metal` |
| 生成链 | primary/secondary 分层独立求 SDF，不再把两层并集当 primary | `qmclient_scripts/qm_build_icon_msdf.py` |
| 资源 | 重新生成 duotone 图集（manifest 带 `secondary_mask: "alpha"`） | `data/qmclient/icons/qm_icons_duotone_msdf.{png,json}` |
| 回归测试 | 4 个编码不变量测试 + 1 个三后端阈值一致性合同测试 | `src/test/qm_msdf_param_encoding_test.cpp`（新）、`src/test/qm_icon_shader_contract_test.cpp` |

### 1.2 名牌矢量字体（MTSDF 只用于游戏内名牌）

- 随包字体 profile 门控：只有已验收的随包 profile 才走 MTSDF，其余字体始终 FreeType，
  避免同一字体在不同机器上结果不一致。
- 缺字形回退：CJK / 西里尔 / 谚文范围缺字形时**整条名牌**回退 FreeType；ASCII 外的装饰符号仍走
  MTSDF，避免一个冷门符号把整条名字拖回 FreeType。
- 删除全局失败锁 `s_NameplateMsdfForceFreeType`（一次失败就把整套名牌永久降级，行为不可预期）。
- 描边、渐变、彩虹、辉光四种效果在 MTSDF 路径统一实现，不再出现「同一名牌切换两种字体路径」。
- 删掉 LXGW WenKai 的运行时图集与对应 profile。

### 1.3 图标字体

- Phosphor 六样式：Thin / Light / Regular / Bold / Fill / Duotone，`qm_ui_icon_weight` 取值域 0–5。
- 设置页图标卡片从 4 段扩到 6 段（`s_aGraphicsIconWeightButtons[6]`、`{2, 0, 1, 3, 4, 5}`）。
- manifest 校验收紧：`kind == "mtsdf"` 且必须声明 `alpha_sdf`；新增可选 `secondary_mask`。

### 1.4 i18n 同步（本轮修复）

上一阶段把 UI 文案从「MSDF 铭牌」改成「铭牌矢量字体」时**只手改了生成产物** `data/languages/*.txt`，
维护源 `translations/i18n/*.toml` 没动。这会导致下一次 `generate_all.py` 把改动静默回滚。本轮补齐：
12 个 key × 12 语言，并删除 10 条已无源码引用的陈旧条目。详见第五节。

---

## 二、Duotone 的最终形态与能力边界

### 2.1 `w` 分量三态编码契约

`gMsdfParams.w` 同时承载「描边宽度」和「采样模式」两个语义，必须落在互不重叠的区间：

| 区间 | 模式 | 含义 |
| --- | --- | --- |
| `w > 0` | MSDF | `w` = 描边宽度（px） |
| `-0.001 < w < 0` | Duotone | 哨兵 `-0.0005`，Alpha 通道是 secondary 距离场 |
| `w <= -0.001` | TRUE_SDF | `w = -(描边宽度 + 0.001)`，Alpha 通道是 primary 真 SDF |

契约在 `src/engine/graphics.h` 的 `qm_msdf_param` 命名空间里用 `static_assert` 固定，三个后端
着色器共用同一阈值，并由合同测试锁住「不允许回退到旧阈值」。

**这是本轮修的 P0。** 修复前 Duotone 哨兵是 `-0.002`，与真 SDF 的描边编码 `-(outline + 0.001)`
正面冲突：名牌描边/辉光 pass 的 outline 在 0.5–2px 之间，算出的 `w ≈ -0.7 ~ -2.0` 会被判成
Duotone，而 Duotone 分支的 `Alpha = max(Opacity, Sample.a * 5) ≈ 1`，**整个字形 quad 变成实心块**。

### 2.2 资源语义

`render_duotone_field()` 把 duotone SVG 拆成两层分别渲染：

- `opacity < 0.5` 的层 = secondary → 写入 Alpha 通道的距离场；
- 其余层 = primary → 写入 RGB。

两层共享同一 `PX_RANGE`，所以着色器可以用同一个 `ScreenPxRange` 解码两者，缩放到任意尺寸都保持
锐利边缘（而不是把光栅覆盖当遮罩用）。95 个 duotone 图标里有 12 个本身没有 secondary 层，
生成器对这种情况返回全 0 场而不是中止。

### 2.3 明确的能力边界

**secondary 颜色不是独立配置项。** 当前实现里 secondary 配色由主 tint 向白偏移 55% 推导：

```glsl
const vec3 SecondaryColor = mix(Tint.rgb, vec3(1.0), 0.55);
```

也就是说 Duotone 现在是「同一色调的双层双色」，而不是「两种可独立指定的颜色」。真正的独立双色需要
新增一个 uniform + 一条配置 + 设置页控件，属于后续工作；着色器里已留注释标明。这是本轮**有意保留**
的边界，不是遗漏——现有 UI 只有单一「图标颜色」设置，加第二颜色需要同时设计交互。

---

## 三、验证证据

| 验证项 | 命令 / 范围 | 结果 | 覆盖了什么 / 没覆盖什么 |
| --- | --- | --- | --- |
| C++ 相关全量过滤测试 | `testrunner --gtest_filter="Qm*:*Msdf*:*Icon*:*Nameplate*:*Font*"` | 1184 项：1177 通过、5 跳过、2 失败 | 失败 2 项均为**既有**陈旧合同测试，见第六节 |
| 新增编码不变量测试 | `QmMsdfParamEncoding.*` + `QmIconShaderContract.*` + `QmIconAtlas.*` + `QmNameplateMsdf*.*` | 26 项：21 通过、5 跳过（跳过项要求「已发布的随包 MSDF profile」，当前无） | 锁住三态编码不重叠、三后端阈值一致、图集 RGBA 语义 |
| 图标卡片合同测试 | `QmNewUiMenuBranches/QmNewUiMenuRenderSurfaceContract.GraphicsIconCardSupportsDynamicCustomColorAndFourWeights` | 2/2 通过 | 本轮修正了「权重 4 项 → 6 项」后遗留的陈旧断言 |
| 图集生成脚本单测 | `py -3 -m unittest qmclient_scripts.tests.test_qm_build_icon_msdf` | 4/4 通过 | 提交的 msdf 图集是带 padding 的 RGBA 距离场；多形状 SVG 走光栅回退 |
| 客户端完整构建 | `cmake --build cmake-build-release --target game-client` | exit 0，223 步，链接成功（`DDNet.exe` 18:15） | 编译 + 链接，不是运行时行为 |
| 着色器语法 | 上一阶段用 glslang 校验 OpenGL / Vulkan 两个 `.frag`（含注入语法错误的对照实验） | 通过 | **本轮未重跑**；Metal 未做任何编译校验 |
| i18n 生成链 | `extract_strings.py --full` → `--write-back` → `generate_all.py` ×12 → `validate.py` | 提取新鲜；12 个语言文件重新生成；MTSDF 相关校验失败全部清除 | 见第五节 |
| 空白检查 | `git diff --check` | exit 0（仅 CRLF 策略提示，非空白错误） | — |
| 端到端（真实进程） | `e2e_qmclient.py cmake-build-release vector_font_and_icon_resources` | 场景断言全部通过；但**退出阶段间歇崩**（5 次尝试 3 次失败），崩溃在 NVIDIA 驱动内，非本工作流引入 | 证明随包名牌 + 图标 MTSDF 资源在真实进程里加载成功；崩溃问题见 6.4 |
| 退出阶段隔离探针 | 5 个配置组合各跑一次（基线 / 仅图标权重 / 仅名牌 MTSDF / 仅设置页 / 三项全开） | 5/5 干净退出（exit 0） | 说明崩溃与 MTSDF 配置组合无因果，是负载相关的驱动退出竞态 |

**没有做的验证（必须如实记录）**：

- **没有任何视觉核对。** 没有截图、没有看画面、没有比较外观。图标六样式与名牌描边/渐变/辉光的
  实际观感、Duotone 双色的深浅是否合适，全部未经验证。
- **没有跨后端真机运行。** OpenGL / Vulkan / Metal 三个后端只做到「着色器能编译 / 阈值一致」，
  没有在任一后端真机跑起来确认渲染结果一致。
- **没有跨平台验证。** 只在 Windows 上构建过。
- 本轮**未声称任何代码覆盖率百分比**——没有生成 line/function/branch 覆盖率报告。

---

## 四、用户侧体验风险

按「会不会被用户看见、看不看得懂」排序。

### R1（高）四个既有图标样式的图集被重新生成，外观可能整体位移

`qm_icons_{thin,regular,bold,fill}_msdf.png` 四张图集都被重新生成过：`msdfgen msdf` → `msdfgen mtsdf`，
并且生成器从 `convert("RGB")` 改成 `convert("RGBA")`，Alpha 通道从「恒为 255」变成真实 SDF
（PNG 体积 +12% 左右即为佐证）。

**这是必须的**：新着色器声明 `alpha_sdf` 后会把 Alpha 当距离场解码，如果 Alpha 还是全 255，
`clamp((1.0 - 0.5) * ScreenPxRange + 0.5, 0, 1) = 1` → 覆盖度恒为 1 → 所有图标变实心块。
所以资源与代码是同步的。

但副作用是：**六种样式的图标外观都可能和用户记忆里的不一样**（尤其是描边和抗锯齿边缘）。
这是最需要在真机上过一眼的一项。

### R2（高）名牌描边上限被放开，可能过粗

`MaxRepresentableOutline` 从 `min(max(0.0, 0.5 * ScreenPxRange - 0.5), 0.5)` 改成
`max(0.0, 0.5 * ScreenPxRange - 0.5)`——去掉了 0.5 的上限。以 `px_range = 6` 计，描边上限从 0.5
变成 2.5。这修的是「描边太细」，但**同一改动对所有走 MSDF/MTSDF 的文字生效**，如果用户的
`qm_nameplate_text_border_range` 设得偏大，现在会真的画到 2.5 而不是被静默压到 0.5，
观感上可能是「描边突然变粗/糊成一团」。建议真机上把描边、辉光各调几档看一遍。

### R3（中）Duotone 图标走的是 PIL 光栅回退，边缘质量与 msdfgen 路径不同

`requires_raster_sdf()` 对 `phosphor_duotone` 一律返回 true（部分 duotone SVG 的复合路径
msdfgen 无法稳健处理）。所以 Duotone 是「PIL 光栅化 + 自写距离变换」出来的场，
和其余五样式走 msdfgen 的场在边缘质量、锐度上可能有可感知差异。
**不选择 Duotone 的用户完全不受影响**，选了的用户可能觉得「这个样式糊一点」。

### R4（中）12 个 Duotone 图标没有 secondary 层

95 个 duotone 图标里有 12 个的 SVG 本来就没有 `opacity < 0.5` 的层。这些图标在 Duotone 样式下
和 Regular 几乎没区别。不是 bug，但用户可能觉得「选了双色调怎么没变」。

### R5（中）缺字形回退的粒度

CJK / 西里尔 / 谚文范围缺字形 → 整条名牌回退 FreeType。好处是不会出现白色方块，代价是
**一个生僻汉字就能让整条名字换字体**，同一屏里两条名牌字体不一致。ASCII 外的装饰符号
（emoji、特殊符号）仍走 MTSDF，所以这些符号用的是图集里的字形而不是系统字体。

### R6（中）i18n 校验仍是红的，但不是本工作流造成

`validate.py` 仍报 11 个语言各缺 4 条译文。这 4 条属于**观察者幽灵 / 空白贴图回退**功能，
全部在 HEAD 里就已存在（见第六节）。如果带着红校验提交，后面接手的人会误判成本次引入。

### R7（低）`qm_ui_icon_weight` 的值域扩到 0–5，旧配置文件的语义不变

默认值仍是 1（Bold），0–3 的含义未变，只是新增 4/5。旧配置文件不会因为升级而改变外观。
唯一需要注意的是配置文件里 `qm_ui_icon_weight` 的取值范围校验同步放宽了，不会再有「设 4 被夹回 3」。

### R8（低）调试日志措辞

`MTSDF icon atlas ready: weight=%s icons=%d` 只在 `qm_*_debug` 打开时输出，不影响普通用户。

---

## 五、本轮修复的 i18n 链不一致

### 问题

上一阶段把名牌相关文案从「MSDF 铭牌」改成「铭牌矢量字体」（`Localize("MSDF nameplate text")` →
`Localize("Vector nameplate text")` 等 5 处），但：

- `data/languages/*.txt`（生成产物）被**手工编辑**成新文案；
- `qmclient_scripts/languages_qmclient/translations/i18n/*.toml`（维护源）**没动**；
- `extracted_strings.txt`（提取缓存）也没重新提取，仍是旧 key。

后果：`generate_all.py` 一旦重跑，新文案会被静默回滚成「MSDF 铭牌」，而且手改产物会凭空多出
两条源码里已经不存在的孤儿条目（`Choose the font first, then enable vector nameplates…`）。

### 修复

1. `extract_strings.py --full` 重建提取缓存（原缓存已失效，增量跑不收敛）。
2. 补齐 12 个 key × 12 语言：`UI icon style`、`Light`、`Duotone`、
   `Qm UI icon style: 0=…4=Light, 5=Duotone`、`Use vector font rendering for nameplates`、
   `… when a bundled profile is available`、`Vector nameplate text`、`Bundled vector profile:`、
   `Available for this bundled font`、`Not available; FreeType is used`、
   `Only selected bundled fonts have vector nameplate atlases…`、`This font has no bundled vector profile…`。
   译文按 skill 的离线手工补译路径写草稿（本地翻译服务返回 402 余额不足），
   回填前逐条过了 `language_quality_failure` 预检（144/144 通过），并按
   `i18n_store.module_name_for_source` 核对了模块归属（menus 3 / qmclient 2 / tclient 7）。
3. 删除 10 条已无源码引用的陈旧条目（7 条 qmclient、2 条 tclient、1 条 menus）。
4. `generate_all.py` 逐语言重新生成 12 个运行时语言文件。

### 结果

- `FAIL: extracted_strings.txt is out of date` → 清除
- `FAIL: simplified_chinese.txt: missing_base_keys=[…]` → 清除
- 生成产物里不再有 MSDF 孤儿条目，新文案全部落盘
- `blocking violations: 0`

### 剩余（不属于本工作流，未修）

`validate.py` 仍报 11 个语言各缺 4 条译文，全部是已提交代码的既有缺口：

- `Show semi-transparent ghost tees for other players who are spectating`
- `Spectator ghost opacity`
- `Opacity of the ghost tees shown for players who are spectating (0 = fully transparent)`
- `Automatically fall back to the default asset when a custom asset sprite is fully transparent; …`

这 4 个 key 在 `HEAD` 里就已经存在于源码中（已用 `git grep HEAD` 逐个核对），属于观察者幽灵与
空白贴图回退功能。按 `qmclient-i18n-workflow` 的约定「局部任务不自动补译无关历史缺口」，
本轮**只报告不补译**。补齐它们需要的机制已经跑通，随时可以补。

---

## 六、既有问题（非本次引入，供决策）

### 6.1 两个源码合同测试在 HEAD 就已失败

| 测试 | 断言 | 证据 |
| --- | --- | --- |
| `QmNewUiMenuBranches.GraphicsFsaaSelectionDefersBackendReconfigure` | 要求 `CMenus::RenderSettingsGraphics` 里出现 `g_Config.m_GfxFsaaSamples = s_aFsaaSamples[FsaaSampleIndex];` | 该串在 HEAD 与工作区的 `menus_settings.cpp` 里**都不存在**，只存在于测试文件自身 |
| `QmNewUiMenuRenderEngineContract.GraphicsDriverCrashRecoveryUsesSafeStartupFallback` | 要求崩溃恢复函数里出现日志串 `resetting safe graphics settings without FSAA while preserving a desktop-sized display mode` | 该串在 HEAD 与工作区的 `client.cpp` 里**都不存在**，只存在于测试文件自身 |

两个测试文件本轮都没有被我改动过。这两条是「测试跟着旧实现写、实现改了没同步」的典型陈旧合同，
建议连同它们的归属工作流一起清理（要么改实现补回日志，要么删掉过时断言）。

### 6.2 TOML 维护源有大量未使用条目

用提取结果反查，`translations/i18n/*.toml` 里有 **483 条 key 已无源码引用**（本轮又清掉 10 条）。
这不是错误，`validate.py` 不因此失败，`review_duplicate_entries.py --show-unused` 是它的清理入口。
属于历史 backlog，本轮未扩大处理范围。

### 6.3 测试文件债务

`python qmclient_scripts/test_inventory.py` 报 `src/test/qm_new_ui_menu_branch_test.cpp`
lines=3911 / tests=158 / source_contract_references=304，远超「lines ≥ 1500 或
source_contract_references ≥ 20」的高债务阈值。本轮只替换了 1 行断言字符串，没有增长债务，
但这个文件该拆了。

### 6.4 Vulkan 退出期崩溃（NVIDIA 驱动，长期存在）

**现象**：Vulkan 后端下客户端退出时偶发 `0xC0000005`。转储栈：

```
nvoglv64.dll 访问违例（Reading from 0xFFFFFFFFFFFFFFFF）
DDNet.exe!CCommandProcessorFragment_Vulkan::CleanupVulkanSDL+0x84  backend_vulkan.cpp:6918  vkDestroyDevice
DDNet.exe!CCommandProcessorFragment_Vulkan::Cmd_PostShutdown+0x176  backend_vulkan.cpp:10342
DDNet.exe!CGraphicsBackend_SDL_GL::Shutdown                       backend_sdl.cpp:1473
DDNet.exe!CGraphics_Threaded::Shutdown                            graphics_threaded.cpp:4225
DDNet.exe!CKernel::Shutdown → main → PerformCleanup()              client.cpp:7250
```

栈里**没有任何 QmClient 业务代码**，崩溃点全在 `vkDestroyDevice` 内部，且已经过了
`crashdump_mark_shutdown_begin()`（`client.cpp:7248`）——也就是项目自己标注为「已知驱动故障、
按 shutdown 阶段忽略」的那一类。

**证明它是既有的**：`%APPDATA%/DDNet/dumps` 下有 **34 个签名完全一致的历史转储**，
时间跨度 2026-09-10 ~ 2026-09-15，全部早于本次工作（本次改动从 09-16 16:00 开始）。
`grep -l "CleanupVulkanSDL\|vkDestroyDevice\|PerformCleanup"` 命中全部 34 个。

**为什么 e2e 场景会被它打红**：`vector_font_and_icon_resources` 比其它场景多做不少 GPU 工作
（加载名牌 MTSDF 图集 + 图标图集 + 打开设置页渲染 + 开 debug 日志），命中驱动退出竞态的概率更高。
对照实验：

| 实验 | 结果 |
| --- | --- |
| `recording_without_connection` 场景 | exit 0 |
| `connection_failure_recovery` 场景 | exit 0 |
| 探针：基线（只启停） | exit 0 |
| 探针：仅 `qm_ui_icon_weight 1` | exit 0 |
| 探针：仅 `qm_nameplate_msdf 1` + debug | exit 0 |
| 探针：仅打开 `ui_page 16` | exit 0 |
| 探针：三项全开 + 设置页（等价于该场景） | exit 0 |
| 该场景本身，重复 5 次 | 3 次失败（2 次 `0xC0000005`、1 次 `WinError 32` 文件占用）、2 次通过 |

探针用**同一个二进制**跑与场景等价的配置组合却 5/5 通过，说明触发条件是负载/时序而非某个配置项。

**影响与建议**：这不是本轮引入的，也不阻塞 MTSDF 提交。但 `_quit_client()` 硬断言
`exit code == 0`，意味着这个场景在本机（Vulkan + NVIDIA）会间歇性变红，作为 gate 不可靠。
建议单独处理：要么让 `_quit_client` 接受「退出阶段已知驱动故障」的退出码，要么把该场景固定到
OpenGL 后端跑。**本轮不改**——改测试断言属于另一件事，且会掩盖真实信号。

### 6.5 转储目录里还有一个可疑项

`%APPDATA%/DDNet/dumps/QmClient_Crash/…_2026-09-16_17-09-50_49628_….RTP` 的 PID 49628 正是
当前仍在运行的 `DDNet.exe`。也就是说用户手上这个实例在 17:09 已经崩过一次。是否同一签名未逐一核对，
仅作提示。


---

## 七、提交建议

### 结论：可以提交，但要拆开

工作区目前有 92 项未提交改动，**至少混了三条互不相关的工作流**，不能一个提交打包：

| 分组 | 内容 | 建议 |
| --- | --- | --- |
| A. MTSDF 名牌 + 图标 + Duotone | 见 1.1–1.3 的文件清单 | 本轮交付，可提交 |
| B. i18n 同步 | `translations/i18n/{menus,qmclient,tclient}.toml`、`extracted_*.json/txt`、`data/languages/*.txt` | 建议与 A 同一个提交（A 的文案改动依赖它），或紧随其后 |
| C. 启动灰屏 / 菜单背景 | `client.cpp`、`menu_background.*`、`menus.cpp`、`maplayers.*`、`map_renderer.*`、`map.*`、`gameclient.cpp` | **不在本工作流内**，由对应负责人单独提交 |
| D. 其他零散改动 | `debughud.*`、`tclient/statusbar.cpp`、`tclient/tclient.cpp`、`serverbrowser.cpp`、`qmclient_scripts/integration/*`（部分） | 同上，逐个确认归属 |

判断依据是逐个看了 diff 内容，不是按文件名猜的。

### 提交前必须做的两件事

1. **真机各后端看一眼**（只有你能做）：至少确认
   - 六种图标样式在 OpenGL 下外观正常、Duotone 能看出双色；
   - 名牌描边 / 渐变 / 彩虹 / 辉光在 MTSDF 路径下正常，描边没有突然变粗；
   - 如果方便，切到 Vulkan 再确认一次（阈值已由合同测试锁死，但渲染结果没验证过）。
2. **决定两个既有失败测试怎么办**：它们会让 quick gate 一直是红的，无论是否本轮引入。

### 不阻塞提交，但需要单独排期的事

- `vector_font_and_icon_resources` 场景在本机（Vulkan + NVIDIA）间歇性变红，原因是 6.4 的驱动退出竞态，
  不是本轮引入。**不要在本次提交里顺手改它的断言**——那是另一个议题。
- i18n 里观察者幽灵 / 空白贴图回退的 4 个 key 缺 11 语言译文（6.1 之外的既有缺口）。
- `qm_new_ui_menu_branch_test.cpp` 的拆分。

### 建议的提交信息骨架

```
feat(qmclient): 完成 Duotone 双色图标与名牌矢量字体收尾

- 引入 qm_msdf_param 三态编码契约，修掉 Duotone 哨兵与真 SDF 描边编码冲突
  （旧哨兵 -0.002 会把带描边的名牌字形渲染成实心块）
- 三个后端着色器统一阈值解码，secondary 改为 Alpha 真距离场
- 图标生成链按 primary/secondary 分层独立求 SDF，重新生成 duotone 图集
- 补齐 12 个 i18n key × 12 语言，修掉「只手改生成产物」造成的不一致
```

---

## 八、需要你确认的清单

- [ ] 六种图标样式在 OpenGL 后端外观是否正常，Duotone 双色是否可见、深浅是否合适
- [ ] 名牌描边 / 辉光放开上限后是否过粗（R2）
- [ ] 是否接受 Duotone 的 secondary 颜色暂时由主色调推导（R3/2.3）
- [ ] Vulkan / Metal 后端是否要本轮一起验
- [ ] 两个既有失败测试：改实现补回，还是删过时断言
- [ ] 提交分组是否按第七节的 A/B/C/D 拆

---

## 九、覆盖缺口复核（2026-09-16 晚，回应实机反馈）

用户在实机看到两类问题：名牌中文回退 FreeType、玩家名两侧装饰符号显示为方框。本节是复核结论。

### 9.1 名牌中文回退 FreeType 的真正原因：CJK 图集被截断

**回退机制本身是通的。** `qm_nameplate_msdf_renderer.cpp:196` 定义了固定回退链：

```
用户选中的 profile  →  dejavu  →  noto_glow_cjk  →  FreeType
```

所以「选了拉丁字体、中文自动用 CJK 的 MTSDF」这条路径**存在且生效**。实机日志
`path '淤泥波波球': MSDF -> FreeType (U+7403 not in atlas)` 恰好证明它生效了——
`淤`(U+6DE4) `泥`(U+6CE5) `波`(U+6CE2) 都命中了 `noto_glow_cjk`，只有 `球`(U+7403) 没命中，
所以报告的首个缺失码点是 `U+7403` 而不是第一个汉字。

**真正的问题是图集覆盖只有 40%。** `noto_glow_cjk` 的五个 page 合计 8937 个字形，
其中 CJK 只覆盖 **U+4E00 – U+6F3F 共 8512 字**（该区间连续无空洞，说明是**按数量截断**，不是字体缺字）：

| 指标 | 数值 |
| --- | --- |
| CJK Unified Ideographs 覆盖 | 8512 / 20976 = **40.6%** |
| 覆盖区间 | U+4E00 – U+6F3F（到此为止） |
| 项目自身简中语料（`data/languages/simplified_chinese.txt`）汉字覆盖率 | 669 / 1126 = **59.4%** |
| 图集外的常用字举例 | 的、球、茶、空、王、爱、生、白、百、火、电、画、目、直、特、物、片、版、牌、玩、现、理 |
| 谚文（韩语）覆盖 | **0 / 11184** |

名牌是**整条回退**（不做 MSDF/FreeType 混排），所以只要名字里有一个字不在图集内，
整条名牌就退回 FreeType。实测：

```
「奶茶」      -> 缺 茶 -> 整条回退 FreeType
「天空之城」  -> 缺 空 -> 整条回退 FreeType
「爱丽丝」    -> 缺 爱 -> 整条回退 FreeType
「哈妮」      -> 全部命中，走 MTSDF
「小明」      -> 全部命中，走 MTSDF
```

**为什么只有 4 页。** `qmclient_scripts/qm_nameplate_msdf_batch_official.py` 的 `CJK_RANGES`
请求的是 `(0x4E00, 0x9FFF)` 全量，`NotoSansSC-VF.ttf` 本体也确实有全部 20976 字
（已用 fontTools 核实：`的`/`茶`/`球`/`語` 全在字体里）。按 `--chunk` 分页应产出约 10 页，
但仓库里只有 `cn_00..03` 四页。**HEAD 就是这个状态，不是本次工作引入。**
判断是为控制仓库体积而人为截断——现有四页 PNG 已 48 MB，全量约 **113 MB**。

### 9.2 韩语在基础 UI 里也是方框（独立缺陷）

`data/fonts/index.json` 把 `korean` 映射到 `Noto Sans SC`，但实测：

| 字体 | 谚文覆盖 |
| --- | --- |
| `NotoSansSC-VF.ttf` | **0 / 11184** |
| `GlowSansJ-Compressed-Book.otf` | **0 / 11184** |
| `DejaVuSans.ttf` | 0 / 11184 |
| `NotoEmoji-Regular.ttf` | 0 / 11184 |

即韩语在**基础文本链（聊天/HUD/计分板）里同样全是方框**，与 MTSDF 无关。
`language variants` 的键是语言文件名（`text.cpp:2034` 拼 `languages/%s.txt`），
所以 `korean` → `Noto Sans SC` 这条映射是直接生效的错误配置。

**现成修复材料**：`data/fonts/SourceHanSans.ttc`（19.4 MB，**git 已跟踪**）含 10 个字面，
谚文 11172 + CJK 20976 + 假名 189 全覆盖，但**没有登记进 `index.json` 的 `font files`**，
因此从未被加载——目前是 19.4 MB 的死资源。

### 9.3 装饰符号方框：随包字体一个都没有

方框来源是 `text.cpp:1005 / 1029` 的兜底——字形在整条链里都找不到时返回
`REPLACEMENT_CHARACTER`(U+FFFD)。基础链为：

```
DejaVu Sans → Noto Sans SC → Noto Sans Thai → Noto Emoji → Phosphor
```

已排除「服务端净化把符号吃掉」：`str_sanitize` / `str_sanitize_cc` 只把 <32 的字节换成空格，
不会产生 □；且该聊天行来自 `src/game/server/teams.cpp:1098` 的 `"%s 与 %s 已完成交换。"`，
`%s` 是玩家名原文。

按「翅膀/装饰」候选逐字核对内置字体链：

- **全链无覆盖（必然方框）**：`꧁꧂`(U+A9C1/A9C2)、`༺༻`(U+0F3A/0F3B)、`𓆩𓆪`(U+131A9/131AA)、
  `ᥬ᭄`、`𖤐𖤍`、`꒰꒱`、`୨୧`、`ꕤ`、`ᯓ`、`⟡`
- **有覆盖（能正常显示）**：`🪽🕊🌙⭐✨🖤🦋🍀🌸⚔🎵♪♥❤✿❀✦✧` 等

所以要给准确结论，需要用户给出那对符号的确切字符（或直接粘贴玩家名）。

### 9.4 各语言覆盖现状（回答「是否全覆盖」）

| 语言 | 基础文本链 | MTSDF 名牌 |
| --- | --- | --- |
| 英文 / 德语 | ✅ DejaVu（ASCII 95/95、拉丁扩展 464） | ⚠️ 看所选 profile，拉丁字体齐 |
| 俄语 | ✅ DejaVu（西里尔 256/256） | ⚠️ 拉丁 profile 含西里尔（如 Nunito 241 字） |
| 中文 | ✅ Noto Sans SC（CJK 20976） | ❌ 图集只到 U+6F3F（40.6%） |
| 日语 | ✅ Noto Sans SC（假名 189/192 + 汉字） | ❌ 同上 |
| 韩语 | ❌ 谚文 0 覆盖 | ❌ 图集内谚文 0 |

### 9.5 建议的修复顺序（需你决策，本轮未改）

1. **P0｜补全 CJK 图集**：用 `tmp/atlas-tool-build/qm-nameplate-msdf-atlas.exe` 重新生成
   `noto_glow_cjk` 全量页（约 10 页）。代价：`data/` 增长约 **+67 MB**（46 → 113 MB）。
   若不能接受，退而求其次按「常用字表（如通用规范汉字表一级 3500 字）+ 全部二级」定向生成，
   体积可控且能覆盖绝大多数真实 ID。
2. **P0｜韩语**：把 `SourceHanSans.ttc` 登记进 `index.json` 的 `font files`，并把
   `language variants.korean` 改为 `Source Han Sans K`；顺带解决这 19.4 MB 死资源。
3. **P1｜装饰符号**：在 `fallbacks` 里补一个覆盖面广的符号字体（如 Noto Sans Symbols 2），
   并按实测需要追加 Javanese / Tibetan 等块。是否值得为一个符号引入字体，取决于用户取舍。
4. **P2｜混排**：考虑把「整条回退」改成「逐字回退」（缺失字单独走 FreeType），
   这样图集缺口不会一次性拖垮整条名牌。改动面较大，需要单独设计。

---

## 十、按三条设计要求落地（2026-09-16 晚，第二轮）

用户给出的三条要求：① 随包字体尽量覆盖、自定义字体缺字要回退；② MTSDF 覆盖与体积成本
平衡（一种语言一种字体、符号至少覆盖一种）；③ MTSDF 只有全部回退都覆盖不到时才回退 FreeType。

### 10.1 关键前提修正：`em_pixels` 不能降，体积账要重算

先纠正两个上一节里的估算错误：

- **图集每页 PNG 不是 2.5 MB 而是约 11 MB**。实测 `nameplate_noto_glow_cn_00.png` = 10.46 MB
  （4096²），`cn_01` = 12.4 MB，`cn_02/03` 各 12.9 MB。四页合计 **48.6 MB**，不是 10 MB。
  所以「全量 CJK 约 +67 MB」这个数是偏乐观的。
- **每页容量不是 2200 而是约 1950 字形**（Han）。实测自建工具 shelf 打包在 4096² 上放
  1952 个汉字框（平均 72.7×72.7 px，+2 padding，填充率约 65%）就满了；旧产物能放 2199 是因为
  它由官方 `msdf-atlas-gen` 打包（更优的 skyline/guillotine），本仓库没有该工具的可执行文件。

`em_pixels` 是成本的主杠杆（页数 ∝ em²），但**不能降**：名牌字号
`18 + 20 × cl_nameplates_size/100` 上限 38 px，再乘 `CanvasToScreenScale()`
（= `ScreenHeight/CanvasHeight`，高 DPI / UI 缩放下可 >1）。em=64 时约 1.7× 过采样，
降到 40 会在最大字号下变成欠采样。**结论：em 保持 64，成本只能靠「覆盖哪些字」控制。**

顺带否掉两个「便宜办法」：把未被使用的 alpha 通道置常量只能省 15–20%（实测 10.46 → 8.89 MB），
换 DDS/BC 反而更大。**唯一有效杠杆是「按常用度取舍字形」。**

### 10.2 根本问题：旧管线按**码位升序**取字，最常用的字反而缺席

旧图集覆盖 U+3001–U+6F3F。而「**的**」(U+7684)、茶(0x8336)、空(0x7A7A)、爱(0x7231)、
球(0x7403)、王(0x738B)、生(0x751F)、白(0x767D)、火(0x706B)、电(0x7535) 全在截断线之上 ——
**汉字里出现频率最高的「的」都没进图集**。这解释了用户报的「奶茶 / 天空之城 / 爱丽丝」全部整条回退。

实测覆盖率（项目自身简中语料 `data/languages/simplified_chinese.txt`，1126 个不同汉字）：

| 取字方式 | 页数 | 语料覆盖 |
| --- | --- | --- |
| 旧：码位升序截到 U+6F3F | 4 | **669/1126 = 59.4%** |
| 新：GB2312 一级+二级优先 | 4 | **1125/1126 = 99.9%** |
| 新：同上，6 页预算 | 6 | 1125/1126 = 99.9%（余量给玩家昵称的中频字） |

**4 页就够，且体积与现状持平。** 唯一缺的「長」是繁体异体，不在 GB2312 内。

### 10.3 已落地的改动

**① 脚本泛化（`qmclient_scripts/qm_nameplate_msdf_build.py`）**

- 新增 `--fallback-scripts-only` / `--fallback-only-script TAG` / `--fallback-chunk` /
  `--jobs` / `--skip-profile-write` / `--profile-only`。
- 新增 `gb2312_han_order()`（6763 字，一级 3755 常用 → 二级 3008 次常用）、
  `ksx1001_hangul_order()`（2350 谚文音节）、`prioritize_codepoints()`（分层优先 + 余量按码位序）。
- 新增 `SFallbackScript`（tag / 页前缀 / 字体 / face / 范围 / 优先序 / 字形预算 / 分片）
  与四个脚本的兜底页定义；`HAN_RANGES` 调整为「统一表意文字在前、扩展 A 在后」，
  使预算的余量优先给统一表意文字而不是罕用的扩展 A。
- 新增 `run_tool_soft()` + `bake_page_split()`：自建工具装不下时**静默截断并返回 0**，
  旧代码会因此整批 `SystemExit`；现在改为回报缺失码点并对半递归拆页。
- 新增 `collect_fallback_pages()` + `fallback_page_rank()`：profile 由**扫描已落盘页**生成，
  页序按 `_cn_ < _jp < _kr_ < _thai_ < _emoji_` 固定；支持分脚本增量烘焙而不丢其它脚本的页。
- 页名保持 `noto_glow_*` 前缀 —— 渲染器对路径含 `noto_glow` 的页禁用 Alpha 真 SDF
  （`qm_nameplate_msdf_renderer.cpp:454`），改名会连带改变渲染路径。

**② 需求 ③：MTSDF 全落空才回 FreeType（`src/game/client/components/nameplates.cpp`）**

删掉了 `NameplateMsdfMissingCodepointNeedsFallback()` 的**区段白名单**。旧实现只对
「拉丁扩展 / 西里尔 / 假名 / CJK / 谚文」返回真，ASCII 外的**装饰符号会被留在 MSDF 路径
画成 `'?'`** —— 既丢信息，又与需求 ③ 冲突（FreeType 侧的 Noto Emoji 明明能画）。
现在 `MsdfCoversText()` 只要 `FindUnsupportedCodepoint()` 非 0 就整条交回 FreeType。

**③ 需求 ①：随包字体覆盖（`data/fonts/`）**

按实测缺口补入 13 个 Noto 字体（OFL-1.1），**合计 +3.9 MB**：

`NotoSansSymbols` / `NotoSansSymbols2` / `NotoSansMath` / `NotoSansJavanese` /
`NotoSerifTibetan` / `NotoSansYi` / `NotoSansOriya` / `NotoSansBalinese` / `NotoSansTaiLe` /
`NotoSansBatak` / `NotoSansVai` / `NotoSansBamum` / `NotoSansEgyptianHieroglyphs`

全部追加在 `index.json` 的 `fallbacks` **末尾**，保证既有字形的观感不变（`m_Glyphs`/回退链
都是先到先得）。用真实 `libfreetype.dll` + ctypes 逐字符验证（`FT_Get_Char_Index` 走完整回退链）：

| 区块 | 补字体前 | 补字体后 |
| --- | --- | --- |
| 数学字母 U+1D400-1D7FF（𝕬𝖇𝖈 花体昵称） | 11.4% | **97.3%** |
| 补箭头/符号 U+2B00-2BFF | 15.6% | **98.8%** |
| 杂项技术 U+2300-23FF | 40.2% | **98.4%** |
| 藏文 U+0F00-0FFF | 0% | **82.4%** |
| 爪哇文 U+A9C0-A9DF（꧁꧂） | 0% | **84.4%** |
| 埃及圣书 U+13000-1342F（𓆩𓆪） | 0% | **100%** |
| 杂项符号 U+2600-26FF | 82.8% | **100%** |
| 装饰符 U+2700-27BF | 98.4% | **100%** |
| 表情图形 U+1F300-1FAFF | 58.2% | **90.8%** |

9.3 节列的 10 个「必然方框」字符（`꧁꧂༺༻𓆩𓆪ᥬ᭄𖤐𖤍꒰꒱୨୧ꕤᯓ⟡`）现在**全部命中**，
另有 `⛧⛥⛦⯑⯒` 一并解决。`QmFontIconsContract.FontIndexKeepsIconFacesInSyncWithCodepoints`
用现有 `testrunner.exe` 实跑**通过**（该用例运行时读 `data/fonts/index.json`）。

> ⚠️ **踩坑记录**：FreeType 的 `FT_FaceRec.family_name` 取自 **name ID 16（排版族名）**，
> 不是 ID 1。实测 `GlowSansJ` 报 `'Glow Sans J'`（ID1 是 `'Glow Sans J Compressed'`）、
> 埃及圣书体报 `'Noto Sans Egyptian Hieroglyphs'`（ID1 是 `'Noto Sans EgyptHiero'`）。
> 用错名字会让 `AddFallbackFaceByName` 返回 false → `LoadFonts()` 返回 false →
> **用户可见告警「Some fonts could not be loaded」**（`gameclient.cpp:1033`）。
> 另外 Windows x64 上 `FT_Long` 是 32 位，`family_name` 在偏移 **24**（不是 40）。

**④ 需求 ①后半：自定义字体缺字回退 —— 已确认本来就有，无需改动**

`CGameClient::GetCharGlyph`（`text.cpp:485`）的顺序是
`m_SelectedFace → m_DefaultFace → m_VariantFace → m_vFallbackFaces → REPLACEMENT_CHARACTER`；
`SetCustomFace` 走 `TrySetDefaultFaceByName`（只把自定义字体设为**默认面**，不动回退链），
且 `LoadFonts()` 里 `LoadCustomFonts()` 之后无条件
`AddFallbackFaceByName("DejaVu Sans")` 再追加 index.json 的 fallbacks。
**所以自定义字体缺字时本来就会回退到随包字体**；剩下的方框只可能来自「随包字体也没有」，
这正是 10.3 ③ 解决的问题。

### 10.4 仍需你决策的部分

1. **CN 页预算 4 还是 6**：4 页与现状体积持平且已达 99.9%；6 页多约 +17 MB，收益是把
   额外约 4600 个中频汉字也纳入（对玩家昵称更宽）。当前脚本默认 **6 页**。
2. **全量覆盖的代价**：若坚持「扩展 A + 统一表意文字 27558 字全覆盖」，需 **15 页 ≈ 165 MB**，
   是已批准预算（+67 MB）的 2.5 倍。**不建议**，扩展 A 在昵称里几乎不会出现。
3. **显存**：`ParseManifest` 逐页急切 `LoadTexture`，页数直接线性抬显存。加上符号页与谚文页后
   profile 约 10–11 页。若在意显存，后续可把渲染器改成按需加载（独立改动）。
4. **字体授权登记**：`data/fonts/` 目前没有 license 说明文件（既有 Noto 字体也没有）。
   新增 13 个 OFL 字体后，建议补一份来源与授权清单。
5. **埃及圣书体是否保留**：572 KB，只服务 `𓆩𓆪` 这类极少数昵称。可删。

---

## 十一、第三轮：烘焙落地 + 三个真实缺陷（2026-09-16 深夜）

第十节的改动已全部执行完毕（汉字页重烤、profile 汇总、测试实跑）。这一轮在验证过程中
又发现并修掉三个真实缺陷，其中**两个是我在第十节自己引入/暴露的**。

### 11.1 汉字页重烤结果

6 片 × 1800 = **10800 个码点**，零溢出、零对半拆残留：

| 页 | 字形数 | 磁盘 |
| --- | --- | --- |
| `cn_00` … `cn_05` | 各 1800 | 9.27 / 9.41 / 10.00 / 9.50 / 9.31 / 9.62 MB |

覆盖实测（`tmp/verify_cn_pages.py`）：

| 口径 | 结果 |
| --- | --- |
| GB2312 一级（常用 3755） | **3755/3755 = 100%** |
| GB2312 二级（次常用 3008） | **3008/3008 = 100%** |
| 项目简中语料（`data/languages/simplified_chinese.txt`） | **1240/1244 = 99.7%**（只缺繁体异体「長」） |
| 抽样高频字「的一是不了在人有我他这为之大来以个中上们」 | **全命中** |

优先序已按设计生效：charset 前 12 个字是「啊阿埃挨哎唉哀皑癌蔼矮艾」（GB2312 一级序），
第 6763 个位置正好切到二级末尾（鼬鼯鼹鼷鼽鼾齄）再进入中频区。

> ⚠️ 上一轮报告里写的「每页容量 1859~1952」是**低估**。实际 `--fallback-chunk 1800`
> 六片全部一次装下、零溢出，说明 4096² 图集对 72×72 汉字框的真实容量高于 1800。
> 但 1800 是安全值（旧 cn_00 曾装到 2199 是因为那页混了大量窄标点），不再上调。

### 11.2 缺陷 A：**空格不在图集里，却被判成缺字**（我引入的回归）

第十节把 `MsdfCoversText()` 改成「缺任何码点就整条回退」之后，含空格的昵称
（`John Doe`、`[TAG] Name` —— 也就是最常见的形态）会**整条回退 FreeType**。

- 根因：`QmNameplateMsdfFirstMissingCodepoint()` 跳过了 `\n \r \t`，但**没有跳过 `' '`**；
  而 `msdf-atlas-gen` 与自建工具都**不为空格写 quad**（全 36 页扫描确认：`U+0020` 一页都没有）。
- 渲染器其实一直是正确的：`Measure()` 与 `DrawText()` 都为空格单独推进 `0.25em` 笔位
  （源码里原本就有注释说明这件事）。**是门控没有对齐渲染器契约。**
- 修复：新增 `QmNameplateMsdfCodepointNeedsGlyph()`，把「不需要字形」的判定集中到一处，
  门控与渲染器共用同一实现。
- 顺带修掉 `Measure()` 与 `DrawText()` 对 `\t` 的**宽度不一致**（前者落到 `'?'` 宽度
  约 0.53em，后者 0.5em）→ 铭牌底板会错位。现在两边都是 0.5em。

### 11.3 缺陷 B：**零宽码点同样被判成缺字**（同类问题，覆盖更广）

昵称里最常见的 emoji 形态是「基础字符 + 变体选择符」：`⭐️` = `U+2B50 U+FE0F`。
基础字符 `⭐` **在图集里**，但 `U+FE0F` 不在（全 36 页都没有），于是整条铭牌又回退 FreeType。

实测 `U+FE0F` 出现在项目**自己的 12 个语言文件**里（另有 `U+200B` 在 danish、
`U+200C` 在 persian）。修复：`QmNameplateMsdfCodepointNeedsGlyph()` 一并跳过
Unicode **Default_Ignorable_Code_Point** 的实用子集（变体选择符 U+FE00-FE0F、ZWSP/ZWJ/ZWNJ、
双向控制符、软连字符、标签字符 U+E0000-E0FFF 等），渲染器同判定跳过。
`U+3164` / `U+FFA0` 这两个「填充符」在 CJK 字体里有实际宽度，**故意不算**不可见。

### 11.4 缺陷 C：**日文页从未真正加载**（先于我这次改动，长期存在）

`nameplate_noto_glow_jp.json` 的 `atlas.image` 是 **`tmp/glow_jp.png`** —— 临时构建路径残留。
运行时只取 basename 再与 manifest 同目录拼接（`ParseManifest`），于是去找
`data/qmclient/nameplate_msdf/glow_jp.png` → **不存在 → 整页 LoadPage 失败被静默跳过**。

结果：**425 个假名字形（平假名/片假名/CJK 标点/全角形式）虽然在磁盘上、也在 profile 里，
但从未生效**，日文昵称一直走 FreeType。

- 修复 ①：把该页 `image` 改成 `qmclient/nameplate_msdf/nameplate_noto_glow_jp.png`。
- 修复 ②：`qm_nameplate_msdf_import_official.py` 不再接受任意 `--image-name`，
  而是**由输出文件名推导**，并在传入值 basename 不匹配时直接报错 —— 这个坑不可能再犯。
- 全目录复查：36 个页 manifest 的 `image` 字段现在 **0 个不可解析**。

### 11.5 图集合同测试：从 5 条「跳过」变成 5 条「实跑」

`src/test/qm_nameplate_msdf_atlas_test.cpp` 原本是占位脚手架（`kBaseManifest` 指向
不存在的 `nameplate_base_msdf.json`，`kPublishedProfileCount = 0`），**5 条用例长期 GTEST_SKIP**。
把它们接到真实产物后立刻抓到 11.4 的缺陷 —— 这就是这类测试的价值。

改动：
- 基线页指向 `nameplate_dejavu_00`；`aProfiles = {"dejavu", "noto_glow_cjk"}`（渲染器回退链的固定组合）。
- `px_range` / `em_pixels` 改用数字读取：生成器对整数值会写 `8` 而不是 `8.0`，
  原实现假定 `json_integer`，对官方链产物一律解析失败。
- 空格不再要求存在（与 11.2 的运行时契约一致）；若存在则必须有正推进宽度。
- 扫描器用例扩到 `noto_glow_jp`（2048²）与 `noto_glow_cn_00`（1800 字形），覆盖非 4096 图集。

**结果：5/5 通过**（此前 5 条全部跳过）。

### 11.6 体积与显存实账（回应第 10.4 节第 3 条）

| 项 | 数值 |
| --- | --- |
| `noto_glow_cjk` profile | **12 页 / 16527 字形 / 80.0 MB** |
| 目录 `data/qmclient/nameplate_msdf/` 总计 | 137.9 MB |
| 本轮净增（相对上一轮 129 MB） | **+8.9 MB** |
| 相对本轮开始前的基线 | **+28.4 MB**（CN 4 页 50.3 MB → 12 页 80.0 MB） |

显存（纹理为 **RGBA8**，无压缩、无 mipmap：`glTexImage2D(..., GL_RGBA8, ...)` /
`VK_FORMAT_R8G8B8A8_UNORM`，4096² = 64 MiB/页）：

| 组成 | 显存 |
| --- | --- |
| `noto_glow_cjk`（11 × 64 + jp 2048² 的 16） | **720 MiB** |
| `dejavu` 回退（2 页） | 128 MiB |
| 选中的随包字体 profile（1 页） | 64 MiB |
| **合计** | **约 912 MiB** |

> ✅ 缓解事实：`qm_nameplate_msdf` **默认 0（关闭）**，不开启就一分显存都不占。
> 若要在意开启后的占用，两条可选路线：
> ① CJK 页改 2048²（页数约翻倍到 24、显存降到约 384 MiB，磁盘基本不变）；
> ② 渲染器改按需加载（独立改动，收益最大）。
> **这两条都需要你先拍板，本轮没动。**

### 11.7 随包字体覆盖终检

**① 方框终判（`tmp/ft_language_gap.py`，真实 `libfreetype.dll` 走 28 个 face）**

对 38 个语言文件里出现的每个码点逐个判定「整条随包字体链是否都没有字形」：

- **37 / 38 个语言文件：0 个方框**
- 唯一残留：`persian` 的 `U+FBC2`（阿拉伯连字 UIGHUR KIRGHIZ YEH… ISOLATED FORM），
  一个字符。既有字体链都不含阿拉伯文，属可接受的已知边界。

**② `index.json` 名字解析（`tmp/check_font_index_resolves.py`）**

25 个名字（default / 4 个 language variants / 18 个 fallbacks / 2 个 icon）
**全部解析成功，0 个失败** —— 不会触发「Some fonts could not be loaded」告警。
（Phosphor 在 `data/qmclient/fonts/Phosphor/`，不在 `data/fonts/`。）

**③ 修掉一处发布漏项（`CMakeLists.txt`）**

`EXPECTED_DATA` 里的 `fonts/*` 是**手写枚举**（不像 `qmclient/fonts/*` 与
`qmclient/nameplate_msdf/*` 那样用 `file(GLOB_RECURSE ...)`），所以新增的 13 个字体
**根本不在发布拷贝列表里**。顺带发现 `fonts/SourceHanSans.ttc`（韩语变体 + 韩文图集烘焙
都依赖它）**此前就漏了**。已补齐 14 条并按 ASCII 排序整理该区块。

### 11.8 测试结果

`testrunner.exe --gtest_filter='*Font*:*Icon*:*Msdf*:*MSDF*:*Nameplate*'`

| | 本轮开始前 | 现在 |
| --- | --- | --- |
| 用例数 | 73 | **74** |
| 通过 | 64 | **70** |
| 跳过 | **5** | **0** |
| 失败 | 4 | 4（同一批，与本工作无关） |

新增/解锁：`QmNameplateMsdfAtlas` 5 条全部实跑通过 +
`QmNameplateMsdfGate.InvisibleCodepointsNeedNoGlyph` 新增。

**4 条失败与本工作无关**：`QmNewUiMenuBranches` / `QmNewUiMenuSettingsNameplateContract`
的 `NameplateOthersModeSuppressesLocalIdentityRows`、`NameplateGameUsesFullScopeReferenceFrame`。
它们断言的是 `Data.m_ShowName = pPlayerInfo->m_Local ? g_Config.m_ClNamePlatesOwn : …`
与 `(!g_Config.m_ClNamePlates || !g_Config.m_ClNamePlatesOwn)` 这两段字面量，而工作区里
**另一处「录像机 / 禅模式」重构**已把它们改成 `NameplateRenderValue(ConfigManager(), …)`
与 `NameplatePartiallyHidden`。属于**陈旧源码合同**，需要那个改动的作者同步更新断言，
不在 MTSDF 范围内，本轮**未改**。

### 11.9 本轮改动文件清单

| 文件 | 改动 |
| --- | --- |
| `qm_nameplate_msdf_gate.h` | 新增 `QmNameplateMsdfCodepointNeedsGlyph()`；门控改用它 |
| `qm_nameplate_msdf_renderer.cpp` | `Measure()` / `DrawText()` 共用该判定；补 `\t` 的 0.5em 一致宽度 |
| `qm_nameplate_msdf_import_official.py` | `image` 由输出名推导 + basename 校验 |
| `qm_nameplate_msdf_atlas_test.cpp` | 接入真实产物；数字字段按数字读；空格契约；扩到 jp/cn 页 |
| `qm_nameplate_msdf_gate_test.cpp` | 空格 / 零宽码点断言 + 新用例 |
| `CMakeLists.txt` | `EXPECTED_DATA` 补 14 个字体（含此前漏掉的 `SourceHanSans.ttc`） |
| `data/qmclient/nameplate_msdf/nameplate_noto_glow_jp.json` | `image` 路径修正 |
| `data/qmclient/nameplate_msdf/nameplate_noto_glow_cn_00..05.*` | 重烤（6 页 / 10800 字） |
| `data/qmclient/nameplate_msdf/profiles/nameplate_noto_glow_cjk.json` | 12 页汇总 |

**仍需你决策**（第 10.4 节各项仍在，另加两条）：

6. **显存路线**：912 MiB（默认关闭）是否可接受？要不要改 2048² 或按需加载。
7. **日文汉字缺口**：日文页只有 425 个假名，日文汉字（`楽検極` 等新字体）不在 GB2312
   → 走 FreeType。若要覆盖 JIS X 0208 一级+二级（6355 字）需再约 4 页 / +44 MB。
8. **字体授权登记**：`data/fonts/` 至今没有 license 清单文件（既有 5 个字体也没有）。
   新增 13 个 OFL 字体后建议补一份来源+授权清单，本轮**未创建**（等你确认放哪、叫什么）。

## 十二、第四轮：显存与页数优化（2026-09-16 深夜，回应第 11.6 / 10.4 节）

第 11.6 节给出的显存实账是 912 MiB。本轮把其中**纯浪费**的部分全部拿掉，覆盖范围一点没减。

### 12.1 先验证上一轮的合同断言同步

```
cd cmake-build-release && ./testrunner.exe --gtest_filter='*Font*:*Icon*:*Msdf*:*MSDF*:*Nameplate*'
```

`74 tests from 31 test suites ran. [  PASSED  ] 74 tests.` —— 从「74 / 70 通过 / 5 跳过 / 4 失败」
变成 **74 / 74 通过 / 0 跳过 / 0 失败**。4 处陈旧合同断言的同步生效，`QmNameplateMsdfAtlas`
的 5 条也从 SKIP 变成实跑。

### 12.2 三个关键机制（本轮所有优化的依据）

1. **图集边长 = 显存成本**。页是 RGBA8、无 mipmap、`LoadProfile()` 里逐页
   `LoadTexture()` 整页上传：**4096² = 64 MiB、2048² = 16 MiB、1024² = 4 MiB**。
2. **官方 `msdf-atlas-gen` 装不下时不会静默截断**：打印
   `Could not fit N out of M glyphs` 并以非 0 退出（只有「无轮廓字形」如空格会被静默跳过）。
   → 所以「先整批试一页，失败再拆」是可靠的，也是唯一能同时压住页数与显存的做法。
3. **两个生成工具都需要 DLL 在 PATH**（否则 `0xC0000135`）：
   - 自建 `qm-nameplate-msdf-atlas.exe` → `libfreetype.dll`
   - 官方 `msdf-atlas-gen.exe` → `libfreetype.dll` + `libpng16-16.dll` + `zlib1.dll`
     （都在 `ddnet-libs/*/windows/lib64/`）

### 12.3 兜底页（noto_glow_cjk）：12 页 → 11 页，720 → 608 MiB

| 页 | 改前 | 改后 | 依据 |
| --- | --- | --- | --- |
| `noto_glow_thai_00` | 4096² / 87 字形 / 填充 **1.2%** | **2048²** / 87 字形 / 填充 4.8% | 实测占用只有 2627×645 |
| `noto_glow_emoji_00..01` | 2×4096² / 800+615 字形 / 填充 29.4%+23.3% | **1×4096²** / 1415 字形 / 填充 52.7% | 两页各行高 2057+1688=3745 ≤ 4096 |

`SFallbackScript` 新增 `page_size` 字段（0 = 用 `--cjk-size`）；emoji 的 `chunk` 从 800
改为 0（用 `--fallback-chunk 1900`）后 1415 个字形整块进一页，零溢出。
**码点集合逐字节一致：thai 87/87、emoji 1415/1415，零丢失零新增。**

### 12.4 profile 页：整批合并 + 自动降尺寸，1472 → 948 MiB

`qm_nameplate_msdf_batch_official.py` 原来对每个 profile 固定 `--chunk 1800` + 硬编码 4096²，
这会把「本可一页装下」的字形切开：

| profile | 改前 | 改后 | 省 |
| --- | --- | --- | --- |
| `dejavu` | 2×4096² / 1730+564 字形 / 填充 46.8%+16.7% | **1×4096²** / 2294 字形 / 填充 **63.5%** | **-64 MiB（常驻）** |
| `phosphor_duotone` | 2×4096² / 1800+1222 | **1×4096²** / 3022 字形 / 填充 89.7% | -64 MiB |
| `minecraft` | 4096² / 填充 2.9% | **1024²** / 填充 46.7% | -60 MiB |
| `cabin` / `google_sans` / `poppins_regular` / `poppins_medium` / `poppins_bold` / `rubik` / `times_new_roman` / `glow_sans_j` | 4096²（填充 4.9%~19.9%） | **2048²** | 各 -48 MiB |
| `freesans` / `inter_regular` / `inter_semibold` / `maple_mono_regular` / `maple_mono_bold` / `montserrat` / `nunito` / `phosphor_regular` / `phosphor_bold` / `phosphor_fill` / `phosphor_light` | 4096² | 4096²（不变） | 0（字形量确实需要） |

实现：`bake_profile_pages()` 先拿**整批码点**试一页（`bake_best_fit()` 从估算尺寸起逐档放大），
失败才按 `--chunk` 拆；`prune_stale_pages()` 删掉本次没产出的旧页（`dejavu_01`、
`phosphor_duotone_01` 因此被清掉）。**全部 21 个 profile 覆盖零丢失**（逐码点集合比对）。

### 12.5 顺带修掉的脚本缺陷：`glow_sans_j` 的范围是错的

`batch_official.py` 给 `glow_sans_j` 配的是 `CJK_RANGES`，实测**请求 28366 个码点**，
会烤成约 16 页（**约 1 GB 显存**）。而已发布产物只有 1 页 425 字形，内容是
CJK 标点 63 + 假名 189 + 全角 173 —— **一个汉字都没有**，与
`qm_nameplate_msdf_build.py` 的注释（「假名、CJK 标点与全角形式由 Glow Sans J 提供，
汉字由 noto_glow_cn 兜底」）一致。改为新增的 `KANA_RANGES` 后：**441 字形 / 1 页 2048² /
16 MiB**（比原来多 16 个假名扩展 U+31F0–U+31FF，同字体同渲染，纯覆盖改进）。

另记：`FONTS` 里的 `noto_sans_sc` / `noto_emoji` / `noto_thai` 三个条目
**既没有已发布页、也没有 profile 引用**，直接跑会产出孤儿页（`noto_sans_sc` 同样会按
`CJK_RANGES` 爆成十几页）。已在脚本里加注释说明，未删条目。

### 12.6 显存账（最终）

| 场景 | 改前 | 改后 |
| --- | --- | --- |
| **常驻**（dejavu 兜底 + noto_glow_cjk 兜底） | 848 MiB | **672 MiB** |
| 选 dejavu | 848 MiB | 672 MiB |
| 选 poppins（已降尺寸的 profile） | 912 MiB | **688 MiB** |
| 选 nunito（未降尺寸的 profile） | 912 MiB | 736 MiB |
| 选 minecraft（最小） | 912 MiB | 676 MiB |
| 全部 21 个 profile 逐个加载之和 | 1472 MiB | **948 MiB** |

运行时页数 **14 → 12**（dejavu 2→1、noto_glow 12→11），字形总数不变（18721）。
磁盘：`data/qmclient/nameplate_msdf/` 由 137.9 MB 降到约 118 MB。

### 12.7 验证证据

- 合同测试：`74 / 74 通过 / 0 跳过 / 0 失败`（数据镜像到 `cmake-build-release/data/` 后复跑）。
- 覆盖比对：`tmp/compare_profile_coverage.py` 逐 profile 比对码点集合，
  **21 个 profile 全部零丢失**（glow_sans_j 为 +16 覆盖改进）。
- 引用完整性：所有 profile manifest 引用的 `.json` / `.png` 均存在，缺失 0。
- 运行时：`Nameplate MSDF ready: 12 page(s), 18721 glyphs`（e2e 场景
  `vector_font_and_icon_resources`；该场景仍会因第 6.4 节的 NVIDIA Vulkan 退出期崩溃
  在 `_quit_client` 阶段超时，但新断言在此之前已通过）。

### 12.8 本轮改动文件清单

| 文件 | 改动 |
| --- | --- |
| `qmclient_scripts/qm_nameplate_msdf_build.py` | `SFallbackScript.page_size`；thai 2048²；emoji 单片一页 |
| `qmclient_scripts/qm_nameplate_msdf_batch_official.py` | `KANA_RANGES`；`PAGE_SIZE_LADDER` / `bake_best_fit()` / `bake_profile_pages()` / `prune_stale_pages()` / `--no-merge`；`sys.executable` 替代硬编码 `python` |
| `data/qmclient/nameplate_msdf/nameplate_noto_glow_thai_00.*` | 2048² 重烤 |
| `data/qmclient/nameplate_msdf/nameplate_noto_glow_emoji_00.*` | 合并重烤（1415 字形） |
| `data/qmclient/nameplate_msdf/nameplate_noto_glow_emoji_01.*` | **删除**（并入 00） |
| `data/qmclient/nameplate_msdf/nameplate_dejavu_00.*` | 合并重烤（2294 字形） |
| `data/qmclient/nameplate_msdf/nameplate_dejavu_01.*` | **删除**（并入 00） |
| `data/qmclient/nameplate_msdf/nameplate_phosphor_duotone_00.*` | 合并重烤（3022 字形） |
| `data/qmclient/nameplate_msdf/nameplate_phosphor_duotone_01.*` | **删除**（并入 00） |
| `data/qmclient/nameplate_msdf/nameplate_{cabin,google_sans,poppins_*,rubik,times_new_roman,minecraft,glow_sans_j}_00.*` | 降尺寸重烤 |
| `data/qmclient/nameplate_msdf/profiles/*.json` | 21 个 profile 清单同步 |
| `tmp/atlas_extent_report.py` | 页占用范围 / 最小可行边长统计 |
| `tmp/probe_profile_page_size.py` | 单 profile 最小可行页尺寸探针 |
| `tmp/compare_profile_coverage.py` | 新旧 profile 覆盖比对（判据：不得丢失） |

### 12.9 仍未做 / 待决策（更新第 12.6 节之后）

1. **CJK 兜底页 608 MiB 仍是最大头**。按需加载（manifest 先解析出字形度量、
   纹理延迟到首次用到该页时上传）可让常见场景（纯拉丁昵称）只付 64~80 MiB，
   省下约 600 MiB。这是架构改动，**未做，需你拍板**。
2. 第 11.9 节的 6 条（日文汉字缺口、字体授权清单、埃及圣书体去留、汉字预算、
   全量页数取舍、显存路线）仍未答。
3. `noto_sans_sc` / `noto_emoji` / `noto_thai` 三个无产物条目：删还是补预算？
4. `glow_sans_j` 新增的 16 个假名扩展字形：保留（默认）还是严格对齐旧产物？
5. 提交分组仍按第 7 节：A（MTSDF/图标/Duotone）/ B（i18n）/ C（启动灰屏）/ D（其他）。
