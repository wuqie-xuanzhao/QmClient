---
title: QmClient macOS Metal 原生图形后端完整实施计划
date: 2026-08-04
status: active
spec: /docs/superpowers/specs/2026-08-04-QmClient-macOS-Metal原生渲染后端规格.md
scope: 从后端公共合同、构建探针和最小兼容渲染开始，分阶段完成 buffering、render target、Qm shader、性能验证与默认后端评估；不改协议、物理、预测、Demo、地图或皮肤格式
---

# 1. 执行合同

本计划是上述 Metal 规格的可执行拆分。规格定义行为和约束，本计划定义任务顺序、文件边界、测试、验证和阶段门。两者冲突时以规格为准，先修正文档再实现，禁止模型自行选择折中语义。

执行规则：

1. 严格按 P0 -> P1 -> P2 -> P3 -> P4 -> P5 推进；前一阶段门未通过，不开始后一阶段。
2. 每次只执行一个 Task 或一个明确的相邻 Task 组。不得把整个计划一次性交给模型实现。
3. 每个代码任务遵循 RED -> 最小实现 -> focused GREEN -> 范围验证 -> 只读 review。
4. 修改前重新读取当前代码和 `.codegraph/`；本文的符号和行号只用于定位，不替代当前源码。
5. 保持 OpenGL、GLES、Vulkan、null/headless 行为；Metal 未编译时不得产生 Apple framework、Objective-C 或 metallib 依赖。
6. 不进行 `CCommandProcessorFragment_GLBase`、`CCommandProcessor_SDL_GL`、`CGraphicsBackend_SDL_GL` 的大范围重命名。
7. Metal 在 P5 完成前保持非默认；P0 完成前不进入普通设置页，P1 阶段仅作为显式 opt-in。
8. capability 只在对应命令、生命周期、失败路径和测试均完成后开启。
9. 不用 source-string 断言替代可测试的纯函数/状态机；源码合同只用于 compile guard、资源清单和穷举分支等静态约束。
10. 真机 Metal validation、GPU capture、视觉、VSync、多 GPU 和硬件矩阵不得以静态测试代替。

# 2. 目标架构和依赖图

```text
P0A backend identity/capability/error contracts
  -> P0B frame-finalization/readback/fallback state machines
  -> P0C CMake + MSL + metallib packaging probe
  -> P1A SDL Metal view/layer/device/queue lifecycle
  -> P1B texture + immediate render + pipeline/state
  -> P1C present/readback/diagnostics/recovery
  -> P2 buffer objects/containers/tile/quad/text/texture arrays
  -> P3 render targets/backbuffer capture/MSAA/blur
  -> P4 Qm SDF/MSDF shaders
  -> P5 profiling/stability/default-backend decision
```

预期新增文件：

```text
src/engine/client/backend/graphics_backend_contract.h
src/engine/client/backend/graphics_backend_contract.cpp
src/engine/client/backend/metal/backend_metal.h
src/engine/client/backend/metal/backend_metal.mm
src/engine/client/backend/metal/metal_types.h
src/engine/client/backend/metal/metal_frame_state.h
src/engine/client/backend/metal/metal_frame_state.cpp
data/shader/metal/qmclient.metal
cmake/BuildMetalShaders.cmake
src/test/graphics_backend_contract_test.cpp
src/test/metal_frame_state_test.cpp
src/test/metal_types_test.cpp
```

文件名可在实现前根据当前仓库模式做小幅调整，但职责不能重新混入菜单或游戏渲染层。

# 3. P0：公共合同与构建探针

## Task 1：锁定 backend 枚举、名称与 compile guard

**Files**

- Modify: `src/engine/graphics.h`
- Create: `src/engine/client/backend/graphics_backend_contract.{h,cpp}`
- Modify: `src/engine/client/backend_sdl.{h,cpp}`
- Modify: `CMakeLists.txt`
- Create: `src/test/graphics_backend_contract_test.cpp`

**RED**

- 增加测试：`Metal` 名称大小写不敏感解析为 `BACKEND_TYPE_METAL`。
- 增加测试：未编译/非 macOS 时 Metal 不可枚举，遗留配置解析为平台安全默认值。
- 增加测试：`BACKEND_TYPE_COUNT` 遍历包含 Metal，`AUTO` 不作为用户选项。
- 增加静态合同：公共头不包含 Objective-C/Metal/QuartzCore 类型。

**实现**

- 新增 `BACKEND_TYPE_METAL`，更新所有 `switch(EBackendType)`。
- 将名称解析、是否可枚举、是否使用 GL version tuple 等判断集中到纯 C++ helper。
- `CONF_BACKEND_METAL` 仅在 `CONF_PLATFORM_MACOS` 下可定义。
- headless/null、GLES-only 和非 macOS 构建保持原行为。

**验证**

- focused：`GraphicsBackendContract.*`
- 编译：macOS `METAL=OFF` 与一个非 macOS CI 配置。

**完成条件**

- 不存在依赖 `default` 吞掉 Metal 的 backend switch。
- `METAL=OFF` 不引用 Metal factory 或 Apple symbol。

## Task 2：建立 API-neutral 设置身份

**Files**

- Modify: `src/engine/client/backend/graphics_backend_contract.{h,cpp}`
- Modify: `src/engine/client/backend_sdl.cpp`
- Modify: `src/game/client/components/menus_settings.cpp`
- Modify: `src/test/graphics_backend_contract_test.cpp`
- Modify or add focused settings test under `src/test/`

**RED**

- Metal 只枚举一个选项，显示名精确为 `Metal`。
- Metal 选中判断忽略 `gfx_gl_major/minor/patch`。
- 选择 Metal 不改写保留的 OpenGL version tuple。
- 从 Metal 切回 OpenGL 后，OpenGL 版本仍为用户选择值。
- 非 macOS 遗留 `gfx_backend Metal` 不产生 custom Metal 项。

**实现**

- `GetDriverVersion()` 对 Metal 只在一个 driver-age 槽返回 `Metal, 0.0.0`。
- 设置页通过 backend contract 判断是否显示/比较/写回版本。
- 保持现有 OpenGL 多版本与 Vulkan 单项行为。

**完成条件**

- 配置往返、设置页重开和重启后的选中项稳定。
- 不通过把 GL tuple 强制写成 `0.0.0` 实现 Metal identity。

## Task 3：完整初始化 `SBackendCapabilities`

**Files**

- Modify: `src/engine/client/backend_sdl.h`
- Modify: `src/engine/client/backend/{null,opengl,vulkan}/**`
- Modify: `src/test/graphics_backend_contract_test.cpp`

**RED**

- 默认构造后所有 capability 都有确定值。
- `ResetToUnsupported()` 覆盖 bool、pointer reason、context tuple 和 atomic MSDF。
- backend 初始化失败再重试时不保留上一次 capability。
- null/OpenGL/Vulkan 当前期望矩阵保持不变。

**实现**

- 给每个字段明确默认值，并增加统一 reset 方法。
- 每次 backend `CMD_INIT` 前 reset，再由具体 backend 开启能力。
- 不使用会复制 `std::atomic` 的整体赋值实现 reset。

**完成条件**

- P1 Metal 矩阵能够在一个函数中完整发布。
- 上层读取不到未初始化或失败初始化残留值。

## Task 4：隔离 MoltenVK frame-serialization workaround

**Files**

- Modify: `src/engine/client/backend/graphics_backend_contract.{h,cpp}` 或 backend capability
- Modify: `src/engine/client/backend_sdl.{h,cpp}`
- Modify: `src/engine/client/backend/vulkan/backend_vulkan.cpp`
- Modify: `src/engine/client/graphics_threaded.cpp`
- Modify: `src/test/graphics_backend_contract_test.cpp`

**RED**

- version/vendor/renderer 任一字符串包含 `Metal` 都不能独立触发 `WaitForIdle()`。
- 原生 Metal capability 固定不要求 frame serialization。
- 受影响 Vulkan/MoltenVK 路径仍可显式开启 workaround。

**实现**

- 增加 `RequiresFrameSerializationWorkaround()` 或等价明确 capability。
- 删除 `str_find(GetVersionString(), "Metal")` 同步决策。
- diagnostics 分开记录 workaround wait、drawable wait 和 GPU completion。

**完成条件**

- native Metal 三帧并行不会被公共 `Swap()` 隐式串行化。
- Vulkan 兼容 workaround 未被无证据删除。

## Task 5：定义 frame-finalization 与 last-presented-frame 状态机

**Files**

- Create: `src/engine/client/backend/metal/metal_frame_state.{h,cpp}`
- Create: `src/test/metal_frame_state_test.cpp`
- Modify if required: `src/engine/client/graphics_threaded.h`

**RED**

- 普通 swap 每帧只 finalize/present 一次。
- screenshot 单独出现时负责唯一 present。
- read-pixel 单独出现时负责唯一 present。
- screenshot + read-pixel 同帧只 present 一次，读取同一 frame id。
- drawable unavailable 不设置 swapped，不泄漏 slot，不生成成功结果。
- 下一帧成功 finalize 后才允许回收上一帧 capture。

**实现**

- 建立不依赖 Objective-C 的纯状态机：frame id、slot、finalized、presented、capture lifetime、failure。
- 把 `FinalizeFrameForPresent()` 与 `ReadLastPresentedFrame()` 作为不同操作。
- 保持现有 `m_pSwapped` 外部协议，不在 P0 改写整个 command buffer API。

**完成条件**

- 状态机测试覆盖所有命令排列和失败转移。
- P1 Metal handler 只能调用该状态机，不能各自复制 swap 逻辑。

## Task 6：错误分类与 safe-backend 恢复状态机

**Files**

- Modify: `src/engine/client/graphics_threaded.h`
- Modify: `src/engine/client/backend/backend_base.h`
- Modify: `src/engine/client/backend_sdl.{h,cpp}`
- Modify: `src/engine/client/graphics_threaded.cpp`
- Modify: `src/engine/client/client.cpp`
- Modify: `src/test/graphics_backend_contract_test.cpp`

**RED**

- Metal init 错误不会进入 GL context/version 递减循环。
- partial init cleanup 顺序固定为 API shutdown、SDL shutdown、post-shutdown、processor/window 清理。
- 同次启动最多执行一次 Metal -> OpenGL 4.1 完整 fallback。
- runtime fatal 标记停止提交并请求下一次启动使用安全配置，不宣称热切换。
- safe config 包含 OpenGL 4.1、FSAA 0、windowed、non-borderless。

**实现**

- 新增 API-neutral/Metal init error code。
- 为 init retry 增加明确 fallback-attempt guard。
- 扩展 graphics error container 的 recovery 分类和诊断字段。
- `ProcessError()` 在 assertion 前完成停止提交和 safe-config 标记；保留当前错误展示路径。
- 配置持久化必须在主线程安全边界完成。

**完成条件**

- init failure 和 runtime fatal 具有不同且可测试的状态转移。
- release/禁用 assertion 构建仍能停止提交并传播错误。

## Task 7：建立 MSL/metallib 构建与打包探针

**Files**

- Modify: `CMakeLists.txt`
- Create: `cmake/BuildMetalShaders.cmake`
- Create: `data/shader/metal/qmclient.metal`
- Create: `src/engine/client/backend/metal/metal_types.h`
- Create: `src/test/metal_types_test.cpp`

**RED/探针**

- `METAL=ON` 但找不到 `xcrun metal`/`metallib` 时 configure 给出明确错误。
- `METAL=OFF` 不运行 shader 工具。
- 故意缺失 metallib 时 runtime loader 返回带完整搜索路径的初始化错误。
- CPU/MSL 共享结构 size、alignment、offset 测试先建立。

**实现**

- macOS 增加 `METAL` option、`CONF_BACKEND_METAL`、OBJCXX、Metal/QuartzCore 链接。
- `.metal -> .air -> .metallib` 使用显式 custom command/target，输入输出和依赖完整。
- 将 metallib 安装到 app bundle、DMG/package 和裸 executable 的确定资源位置。
- shader loader 只查文档规定的位置，不依赖当前工作目录。

**P0 阶段门**

- P0 focused tests 全绿。
- macOS arm64 `METAL=ON` shader probe 可编译、加载最小 library。
- macOS `METAL=OFF` 与非 macOS compile guard 通过。
- OpenGL/Vulkan/null 后端合同测试无回归。
- 只读 review 无重要/严重 finding。

# 4. P1：最小兼容 Metal 后端

## Task 8：创建 Metal fragment factory 与命令分派骨架

**Files**

- Create: `src/engine/client/backend/metal/backend_metal.{h,mm}`
- Modify: `src/engine/client/backend_sdl.cpp`
- Modify: `CMakeLists.txt`

**实现顺序**

- 声明 `CreateMetalCommandProcessorFragment()`，返回现有 fragment 基类。
- 在 processor factory 显式创建 Metal fragment。
- 未实现 command 返回 `UNHANDLED`；属于 Metal 且不允许转交的 draw command返回可诊断 error，禁止静默 handled。
- `@autoreleasepool` 包围每批 Metal command 处理。

**验证**

- factory/compile-guard 测试。
- debug 未知 command 包含 command id、backend state 和 frame id。

## Task 9：SDL Metal view、layer 和 device 生命周期

**Files**

- Modify: `src/engine/client/backend_sdl.{h,cpp}`
- Modify: `src/engine/client/backend/metal/backend_metal.mm`

**RED/测试 seam**

- fake lifecycle 验证 pre-init/init/shutdown/post-shutdown 次序和 partial failure cleanup。
- view 创建后 window 销毁前必须 destroy view。

**实现**

- `SDL_WINDOW_METAL` 第三路窗口 flag。
- `CMD_PRE_INIT` 主线程创建 view、取得 layer；失败返回 Metal init error。
- 选择 `MTLDevice`，同一 device 用于 layer、queue 和资源。
- `CMD_POST_SHUTDOWN` 主线程销毁 view。
- 所有 Objective-C 对象隐藏在 `.mm` 私有实现。

## Task 10：GPU 枚举、选择和 backend identity

**Files**

- Modify: `src/engine/client/backend/metal/backend_metal.mm`
- Modify: `src/engine/client/backend_sdl.cpp`

**实现**

- `auto` 使用系统默认 device；显式名称枚举全部 device。
- 不存在的名称 warning 后回退默认 device。
- 填充 GPU 名称、集成/独立类型和选择状态。
- vendor/version/renderer 字符串明确区分 native Metal 与 Vulkan/MoltenVK。

**验证**

- fake device list 纯函数测试。
- Apple Silicon 真机；有条件时补多 GPU/Intel Mac。

## Task 11：Layer、drawable size、resize 和 VSync

**Files**

- Modify: `src/engine/client/backend_sdl.cpp`
- Modify: `src/engine/client/backend/metal/backend_metal.mm`

**实现**

- 配置 BGRA8Unorm、drawable count 3、framebufferOnly false、transaction false。
- Metal viewport size 只使用 `SDL_Metal_GetDrawableSize()`。
- resize/DPI/fullscreen/screen switch 更新 layer drawable size，零尺寸不分配、不阻塞。
- `CMD_VSYNC` 设置 `displaySyncEnabled`，保留平台可用性检查。

**验证**

- drawable-size 纯合同测试。
- 真机 1x/2x、最小化/恢复、全屏、borderless、屏幕切换。

## Task 12：三帧资源与 command-buffer 生命周期

**Files**

- Modify: `src/engine/client/backend/metal/metal_frame_state.{h,cpp}`
- Modify: `src/engine/client/backend/metal/backend_metal.mm`

**实现**

- 三个 frame slot、semaphore、shared stream arena、临时对象强引用和 readback 状态。
- slot 复用前等待；completion handler 只 signal 和写线程安全状态。
- drawable 晚获取、present 后尽快释放；正常帧不 `waitUntilCompleted`。
- shutdown 停止新提交并 drain 全部 in-flight slot。

**验证**

- slot wraparound、completion error、drawable nil、shutdown drain 测试。
- diagnostics 记录 in-flight high-water mark。

## Task 13：纹理、文本纹理、上传和内存统计

**Files**

- Modify: `src/engine/client/backend/metal/backend_metal.mm`
- Modify: `src/engine/client/backend/metal/metal_types.h`
- Add focused pure conversion tests under `src/test/`

**实现**

- texture/text texture create/update/destroy，slot generation/非法 ID 防护。
- NPOT、subregion、row pitch、格式映射、mipmap、wrap/filter。
- shared staging -> private texture upload；禁止每 draw 创建 texture/buffer。
- texture/staging memory 使用实际分配字节并在销毁时扣除。

**验证**

- 格式、size overflow、row pitch、NPOT、mipmap、update-after-destroy 测试。
- 资源压力 smoke 与 Xcode validation。

## Task 14：P1 immediate pipeline、状态和绘制

**Files**

- Modify: `data/shader/metal/qmclient.metal`
- Modify: `src/engine/client/backend/metal/backend_metal.mm`
- Modify: `src/engine/client/backend/metal/metal_types.h`
- Modify: `src/test/metal_types_test.cpp`

**实现**

- vertex/fragment 基础 shader、text 双纹理 shader。
- clear、immediate render、viewport、scissor、blend、sampler/wrap。
- `QUADS` 用持久化索引或等价 triangles；支持显式 triangles/lines。
- pipeline key 包含 pixel format、sample count、blend、textured/format variant。
- 连续相同 attachment 复用 encoder；状态变化不遗漏 clip/texture/sampler。

**验证**

- blend、primitive、clip clamp、pipeline key 和共享布局单测。
- 菜单、普通文本、地图 2D fallback、HUD 固定截图。

## Task 15：P1 capability 发布与上层 fallback

**Files**

- Modify: `src/engine/client/backend/metal/backend_metal.mm`
- Modify if required: `src/game/map/render_map.cpp`
- Modify: `src/test/graphics_backend_contract_test.cpp`

**实现**

- 精确发布规格第 9.2 节矩阵。
- 验证 texture array、buffering、render target、SDF/MSDF 命令不会由上层生成。
- 若发现现有 fallback 依赖额外 capability，先补合同和测试，不虚报支持。

## Task 16：present、screenshot、read-pixel 和视频取帧

**Files**

- Modify: `src/engine/client/backend/metal/backend_metal.mm`
- Modify: `src/engine/client/backend/metal/metal_frame_state.{h,cpp}`
- Modify if required: `src/engine/client/video.cpp`
- Modify: `src/test/metal_frame_state_test.cpp`

**实现**

- 按 P0 状态机实现唯一 present 和 last-presented-frame 保留。
- texture-to-buffer readback，处理 BGRA/RGBA、alpha、方向、checked size/row pitch。
- screenshot/read-pixel 失败返回明确结果和 warning。
- `TGLBackendReadPresentedImageData` 对 Metal 返回同一 presented image 语义，确保录像路径不读下一帧或释放资源。

**验证**

- screenshot/read-pixel 四种组合测试。
- 真机截图、编辑器 pipette、自动截图和实际启用的视频录制入口。

## Task 17：P1 初始化回退、runtime fatal 和 diagnostics

**Files**

- Modify: `src/engine/client/backend/metal/backend_metal.mm`
- Modify: `src/engine/client/backend_sdl.cpp`
- Modify: `src/engine/client/graphics_threaded.cpp`
- Modify: `src/engine/client/client.cpp`

**实现**

- 所有 init stage 填充 Metal error code、stage 和 Apple/SDL error。
- command-buffer completion error 写 latch；processor 边界停止提交。
- 初始化错误同进程回退 OpenGL 4.1；runtime fatal 保存安全配置并有序退出。
- 接入现有 macOS diagnostics：encode、commit、drawable wait、GPU time、upload/readback、pipeline cache、frame slots。

**P1 阶段门**

- 设置页显式选择 Metal 可完成启动、菜单、进服、普通地图/文本/HUD、截图、read-pixel、退出。
- 初始化失败同次启动回退通过；runtime fatal 下一次启动恢复通过。
- 无持续 Metal validation error、无正常帧 `waitUntilCompleted`、无每 draw GPU 资源分配。
- focused + full C++ + Rust + quick/default gate 通过，或明确记录与本轮无关的既有阻断。
- Apple Silicon 真机矩阵完成；未具备 Intel/多 GPU 设备时明确标为发布 gap。

# 5. P2：现代 buffering 路径

## Task 18：buffer object 生命周期

- 先测试 create/recreate/update/copy/delete、非法 ID、generation、size overflow、one-time-use。
- 实现 private static buffer、shared staging、高频 shared/分段更新策略。
- buffer/staging/stream memory 统计必须成对。

## Task 19：buffer container 与 index capacity

- 测试 attribute layout、stride/offset、container 引用 buffer 销毁、index 扩容。
- 实现 container create/update/delete 和 `CMD_INDICES_REQUIRED_NUM_NOTIFY`。
- CPU/MSL vertex layout 使用静态断言和测试锁定。

## Task 20：tile、border tile、quad 和 grouped draw

- 按现有命令逐项建立参数映射测试。
- 实现 tile layer、border tile、quad layer/grouped；不改变地图或纹理格式。
- 固定地图对比普通/缩放/HiDPI/clip 边界。

## Task 21：text buffering、quad container 和 sprite multiple

- 实现 text buffering、quad container、extended quad、sprite multiple 全生命周期。
- 测试 rotation、color、offset、draw ranges、删除后引用和空 draw。
- 与 immediate fallback 做视觉对比。

## Task 22：2D texture array

- 实现完整 2D array upload/sample/update/destroy，不使用 GL extension 语义。
- 通过地图 texture-array 固定场景和数组边界测试后才开启 capability。

**P2 阶段门**

- P2 capability 逐项开启，未实现项仍为 false。
- 固定地图/UI 与 OpenGL 金图在登记阈值内一致。
- 热路径无每 draw allocation，buffer/container 压力测试无 validation error。

# 6. P3：Render target、MSAA、capture 与 blur

## Task 23：Render target 状态机

- 扩展 `render_target_test.cpp` 或新增 Metal contract 测试。
- 实现 create/destroy/begin/end/draw、attachment size/format、resize 重建。
- 切换 target 必须结束 encoder并恢复 viewport、clip、screen mapping。

## Task 24：Backbuffer capture

- 实现可采样的单采样 presented result；多采样读取 resolve 结果。
- 与 screenshot/read-pixel capture 区分 capability 和生命周期。
- 测试 capture 后 resize、连续 capture 和非法顺序。

## Task 25：MSAA

- `supportsTextureSampleCount` 校验实际 sample count。
- pipeline、MSAA attachment、resolve attachment 同步重建。
- `CMD_MULTISAMPLING` 只在帧边界应用；拒绝请求时返回真实值和 warning。

## Task 26：Gaussian blur

- 两张 ping-pong texture，水平/垂直 pass 明确 encoder 边界。
- 测试 source/destination 不同、radius/weights、resize 和单采样外部 pass。
- 完整实现后才开启 blur capability。

**P3 阶段门**

- render target、capture、readback、MSAA、blur 固定场景和 resize/FSAA 切换通过。
- pipeline/attachment sample count 无 mismatch。
- 截图、视频和正常 present 不因 offscreen path 回归。

### 实施记录（代码级）

- Task 23-26 已分别实现 render target、backbuffer capture、MSAA 与 Gaussian blur；`CMD_RENDER_TARGET_READBACK` 补齐后，P3 的公开 command 路径与 capability 已对齐。
- 读回在 offscreen 场景完成无 present 的 command buffer；已有 drawable 时在同一 command buffer 中复制 drawable、唯一 present，并让随后的 screenshot/read-pixel/swap 消费同一帧，避免空白二次 present。BGRA 到 RGBA 转换保留 alpha。
- 已呈现读回的单次消费标记由 `CMetalFrameState` 持有，并以 mutex 保护；新 backbuffer encoder 清除旧标记，slot drain 不会误消费它。纯 C++ 测试覆盖单次消费、清除和 drain 语义。
- 验证：`MetalBackendContract.*:MetalFrameState.*:MetalRenderTargetState.*:MetalTypes.*` 42/42；`xcrun clang++ -fsyntax-only -x objective-c++ ... backend_metal.mm` 通过；`python3 qmclient_scripts/gate/check_gate.py --mode default` 为 13/0/0、C++ 全量与 Rust 全量通过。
- 阶段门仍保留真机缺口：当前环境没有 `xcrun metal` / `metallib`，无法完成 MSL 编译、Metal API validation、固定场景、resize/FSAA 切换和连续 screenshot/read-pixel 的真实 GPU 验证。因此 P3 不能标记为阶段验收完成，P4 不得启动。

# 7. P4：Qm 专用 shader

## Task 27：Media Island 与 rounded-rect SDF

- 对照 OpenGL/Vulkan 参数布局和 alpha/clip 语义编写 MSL。
- CPU/MSL offset 测试、普通/HiDPI/缩放视觉对比。
- shader/pipeline 失败保持现有非 SDF fallback。

## Task 28：Textured MSDF

- 实现 texture、transform、rotation、alpha 和 fallback 合同。
- capability 使用 atomic release/acquire 发布。
- MSDF probe/reload 失败不得产生每帧重试或 command-buffer flush 回归。

## Task 29：跨后端 shader 清单与漂移防护

- 建立启用后端 shader variant 清单和 shared struct/layout 测试。
- 新增特殊 shader 时要求 OpenGL/Vulkan/Metal 或显式 capability fallback 同步更新。

**P4 阶段门**

- Qm HUD/UI 在普通 DPI、HiDPI、缩放、旋转下达到登记视觉阈值。
- SDF/MSDF 失败路径可用且无 validation error。

# 8. P5：性能、稳定性与默认后端评估

## Task 30：固定基线与 GPU capture

- 同一二进制、配置、地图、相机位置分别采集 OpenGL 与 Metal。
- 记录 CPU frame p50/p95/p99/max、GPU frame、drawable wait、encode、command buffers、encoders、draw、upload/readback bytes、内存和功耗。
- VSync on/off、普通/HiDPI 分开，不用单个 FPS 数字判断后端优劣。

## Task 31：证据驱动优化

只允许在 capture/profile 证明后执行：

- framebuffer-only + internal present texture。
- Apple GPU 与 Intel/AMD storage mode 分支。
- pipeline binary archive。
- upload/draw/encoder 合并。

每项优化必须有前后同场景数据；无收益且无正确性理由则不保留。

## Task 32：发布硬件矩阵与默认后端决策

- Apple Silicon 为阻断矩阵。
- 发布仍含 x86_64 时 Intel Mac 为阻断矩阵。
- 多 GPU、外接屏、不同刷新率、睡眠/唤醒、长时间运行、反复 resize/fullscreen。
- crash recovery、shader 缺失、drawable failure、command buffer fatal 实测。
- 只有规格的默认晋级条件全部满足，才另开决策修改 macOS 默认 backend。

# 9. 每阶段统一验证

代码任务完成后串行执行：

```bash
cmake --build cmake-build-release --target game-client -j 14
cmake --build cmake-build-release --target run_cxx_tests -j 14
cmake --build cmake-build-release --target run_rust_tests -j 14
python3 qmclient_scripts/gate/check_gate.py --mode quick
```

阶段合并前优先补：

```bash
python3 qmclient_scripts/gate/check_gate.py --mode default
```

Metal 特有验证不得被上述命令替代：

- `METAL=ON` 和 `METAL=OFF` configure/build。
- metallib target、bundle/package、裸 executable 资源加载。
- Xcode Metal API validation。
- 真机视觉矩阵和 GPU capture。
- 非 macOS compile guard。

每次汇报必须记录 command、exit status、测试数量、设备/renderer、覆盖范围和 gap。构建成功不能代替测试、视觉、validation 或 capture。

# 10. 建议提交边界

建议保持以下提交序列，每个提交可独立构建或至少有明确 RED/GREEN 证据：

1. `test(graphics): 锁定 Metal 后端公共合同`
2. `refactor(graphics): 建立 API-neutral 后端身份与 capability reset`
3. `fix(macos): 隔离 MoltenVK 帧同步 workaround`
4. `build(metal): 增加 MSL 与 metallib 构建探针`
5. `feat(metal): 接入 SDL Metal 生命周期与基础设备`
6. `feat(metal): 实现纹理和 immediate 渲染`
7. `feat(metal): 实现呈现、readback 与错误恢复`
8. `feat(metal): 实现 buffering 与 texture array`
9. `feat(metal): 实现 render target、MSAA 与 blur`
10. `feat(metal): 实现 Qm SDF 与 MSDF shader`
11. `perf(metal): 完成真机基线与证据驱动优化`

不得为了维持提交数量，把尚未可用的 stub backend 暴露给用户。涉及共享 backend 合同的提交必须同时验证 OpenGL/Vulkan/null。

# 11. 模型执行停止条件

遇到以下任一情况必须停止当前阶段并报告，不得猜测继续：

- 当前 SDK/SDL 声明与规格引用不一致。
- 需要改变协议、地图、Demo、skin、物理、预测或文件格式。
- 必须热切换后端才能满足需求。
- capability false 仍无法阻止上层生成未实现命令。
- 需要大范围重命名/重写图形基类才能继续。
- Metal shader 工具或真机验证环境不可用，却准备宣称阶段完成。
- OpenGL/Vulkan/null 共享测试发生无法解释的回归。
- 工作树并行修改与目标文件产生语义冲突。

# 12. 全部完成定义

只有满足以下条件才能称“Metal 原生后端完成”：

- P0-P4 所有阶段门通过，P5 已记录性能和稳定性证据。
- 所有 core graphics command 已实现、正确 fallback 或明确转交，无静默成功。
- capability 与真实行为一致，重复 init/shutdown/失败重试无残留状态。
- screenshot、read-pixel、视频、resize、HiDPI、fullscreen、VSync 和 crash recovery 可用。
- OpenGL/Vulkan/null/headless 与非 macOS 构建无回归。
- bundle/package 中 metallib 可加载，缺失时错误可诊断。
- Apple Silicon 发布矩阵通过；若发布 x86_64，Intel 矩阵也通过。
- Metal validation 无持续错误，固定视觉场景通过，性能数据已归档。
- OpenGL fallback 仍可由用户选择；Metal 是否成为默认由独立决策处理。
- 按仓库 MMP 规则更新版本、完成最终只读 review、default/full gate 和发布说明。
