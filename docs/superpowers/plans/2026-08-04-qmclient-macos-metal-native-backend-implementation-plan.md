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
- 2026-08-25 真机补充：已安装可用的 Xcode Metal toolchain，并以独立 `cmake-build-metal-release` 完成 `METAL=ON` 配置、MSL/metallib、`game-client` 与 `testrunner` 构建。Apple M2 上显式选择 `gfx_backend Metal` 后创建 `Metal 4.1` context，无 OpenGL fallback；修复了 plain quad pipeline 的缺失 texcoord attribute、frame resource 索引 buffer 初始化、跨 `RunCommand` 的 encoder/drawable 所有权，以及 frame slot 轮转误释放长期 vertex buffer。
- Metal API validation 已通过 `MTL_DEBUG_LAYER=1` 启用。启动参数 `gfx_fsaa_samples 4` 的性能日志记录 `backend=Metal native`、`renderer=Apple M2`、`fsaa=4`，并在多个 120 帧窗口记录 `frame_serialization_wait_count=0`。受控启动稳定运行至终止前，无新增 DDNet crash report、pipeline error、validation error 或 fallback。
- 新增 `metal_backend_runtime_test.cpp` 真机集成测试：在隐藏 `SDL_WINDOW_METAL` 窗口中实际加载 `qmclient.metallib`、初始化 Metal command processor，并验证 render target 双向 Gaussian blur/offscreen RGBA readback，以及 backbuffer capture/readback 后下一批次的 screenshot/read-pixel/video 读回复用同一绿色已呈现帧。测试还以 `4 -> 0 -> 4 -> 0` 反复切换 FSAA；4x 在不支持设备上显式 skip、Apple M2 上实际为 4，每次切换后均绑定对应 sample-count pipeline、呈现并读回颜色帧；并通过 `CMD_UPDATE_VIEWPORT` 验证 resize 后 Metal drawable 与 screenshot 尺寸一致。`MTL_DEBUG_LAYER=1 ./testrunner '--gtest_filter=MetalBackendRuntime.*'` 为 3/3，输出确认 `Metal API Validation Enabled`。同一命令从 `cmake-build-metal-release` 由 `xcrun xctrace record --instrument 'Metal Application'` 采集，trace 的 `testrunner` 退出码为 0。
- 验证：Metal 配置 `GraphicsBackendContract.*:MetalBackendContract.*` 为 23/23；Metal 配置 `run_cxx_tests` 为 2835/2835；`cmake --build cmake-build-metal-release --target game-client -j 8` 通过；`python3 qmclient_scripts/gate/check_gate.py --mode quick` 通过。当前仍未完成可交互的 screenshot/read-pixel/video/capture、render-target blur 固定场景、fullscreen/HiDPI 与反复 resize 矩阵；自动 resize 被 macOS 辅助访问权限拒绝，未修改系统权限或用户配置。P3 不能标记为阶段验收完成，P4 不得启动。
- 2026-08-25：隔离 storage 的真实客户端 smoke 暴露 `qmclient.metallib` 查找错误：资源在 data root 的 `shader/metal/` 下，后端错误地再加了 `data/` 前缀，导致 Metal 初始化后回退 OpenGL。改为 data-root 相对路径并让 runtime 测试显式检查该资源可见；资源读取失败日志会记录逻辑路径、`TYPE_ALL` 和读取结果。隔离目录运行 `MetalBackendRuntime.*` 为 3/3，完整 Metal `run_cxx_tests` 为 2835/2835，修复后 `default gate` 为 13/0/0；Apple M2 GUI smoke 使用 `MTL_DEBUG_LAYER=1` 显示 `Metal native`、FSAA 4，连续 120 帧样本的 `frame_serialization_wait_count=0`。屏幕截取因 macOS 屏幕采集权限拒绝，未修改系统权限；交互 capture/blur、fullscreen、HiDPI 和 resize 矩阵仍是 P3 阶段门缺口。
- 2026-08-25 补充：DMG 打包实际启动暴露 `qmclient_quad_container_ex_textured_fragment` 缺少 fragment-stage `SMetalQuadContainerUniforms` 的 buffer(1) 绑定，Metal API Validation 会在首帧终止进程。`DrawQuadContainerEx` 现绑定与顶点阶段相同的 stream uniform，并新增 `QuadContainerExBindsFragmentUniforms` 真机回归测试，验证 container buffer、fragment tint 与 screenshot readback。`MTL_DEBUG_LAYER=1` 下 `MetalBackendRuntime.*` 为 4/4，Metal 配置 `run_cxx_tests` 为 2836/2836，default gate 为 13/0/0；`package_default` 重新生成 DMG，`verify_macos_dmg.sh` 通过签名与绝对依赖检查，包内 metallib 与 build 产物 SHA-256 一致。以隔离 HOME、`/tmp` 工作目录启动 bundle 进入服务器列表，日志显示 bundle `Contents/Resources` 为 data root 与 `Metal native`，未出现 fallback 或 validation assertion。测试和客户端日志时间戳显示为 2026-08-26，较当前会话日期 2026-08-25 快一天，验证日期以会话日期为准。`screencapture` 仍返回 `could not create image from display`，所以 P3 的交互 screenshot/read-pixel/video/capture、固定 UI blur、fullscreen/borderless/display switch、HiDPI 与长稳矩阵仍未完成；P3 保持未验收，P4 不得启动。
- 2026-08-25 P3 补充：不依赖系统 Screen Recording 或 Accessibility，使用现有 `cl_input_fifo` 在 Metal 图形初始化后向前台隔离客户端发送 `screenshot`。`cmake-build-metal-release/DDNet -s -f tmp/metal-p3-client/metal-fifo-screenshot.cfg` 正常退出，日志确认 `Created Metal 4.1 context`、`GPU version: Metal native` 和 `Saved screenshot`；产物 `tmp/metal-p3-client/save/screenshots/screenshot_2026-08-26_06-58-21.png` 是 1920x1018 RGBA PNG（SHA-256 `df76a3b7a1da526d9b41d2b60d94a40b5285221aac9be2226782316f690bf30b`），人工核对为完整主菜单，非空白或 fallback 帧。系统时间戳仍快一天，故文件名日期不作为验证日期。真实客户端 screenshot/readback 已覆盖；P3 仍缺 demo video/capture、固定 UI blur、fullscreen/borderless/display switch、HiDPI 与长稳矩阵，继续保持未验收，P4 不得启动。
- 2026-08-25 P3 视频补充：以工作区 `DDNet-Server` 在 `127.0.0.1:8311` 承载 `Sunny Side Up`，同一隔离 Metal 客户端通过 FIFO 完成连接、手工录制 demo、回放和 `start_video`。日志确认 `Metal native`、`demo_player: Loading demo`、录像器 H.264 初始化和 `Recording to 'videos/metal_p3_video_070051.mp4'`；产物为 1,048,383-byte demo 及 1,577,018-byte MP4。`ffprobe` 显示 MP4 含 1920x1018 H.264（433 帧，2165/36 fps）和 AAC 音频，时长 7.253 秒；`ffmpeg -f null -` 完整解码通过，抽帧为完整游戏场景。demo 到达末尾时录像器自动收尾，随后 FIFO 的 `stop_video` 正确返回 `Not recording`，不是录像失败。真实 video/readback 已覆盖；P3 仍缺固定 UI blur、fullscreen/borderless/display switch、HiDPI 与长稳矩阵，继续保持未验收，P4 不得启动。
- 2026-08-25 P3 UI blur 补充：隔离配置启用 `qm_gaussian_blur 1` 后，在本地回环游戏中通过 FIFO 保持 `+scoreboard` 并截图。`tmp/metal-p3-client/save/screenshots/screenshot_2026-08-26_07-02-57.png` 为 1920x1018 RGBA、331,010-byte PNG（SHA-256 `fadec12a76fe52bda96aba89a3c08c3245e24197ee89e33115cbb6fceffb270c`）；人工核对为完整游戏 UI，记分板半透明面板后为模糊场景。源码路径为 `CScoreboard` 的 `CUiScopedGaussianBlur` 和 `CUi::PrepareGaussianBlur`，后者执行 backbuffer capture 与双 pass Gaussian blur；本次 Metal 日志无 error、assertion、fallback 或 validation 输出。固定 UI blur 已覆盖；P3 仍缺 fullscreen/borderless/display switch、HiDPI 与长稳矩阵，继续保持未验收，P4 不得启动。
- 2026-08-25 P3 窗口模式补充：前台隔离客户端分别以 `gfx_fullscreen 1` 和 `gfx_borderless 1` 启动，并经 FIFO 请求 screenshot 后有序退出。两个实例均记录 `Created Metal 4.1 context`、`Metal native` 和 `Saved screenshot`；对应产物均为完整 1920x1080 RGBA PNG，fullscreen SHA-256 为 `454802611d50c97413d2ea210b4ecebbe237a85172aa5ee2adb2ad9d16d1bc69`，borderless SHA-256 为 `83903df6d204a3be7876326a86b10d5a74a64dd4cf59b726bad7c1470eee13d8`。当前真机 `system_profiler` 只报告一台主显示器（1920x1080、100 Hz），不具备多显示器/display switch 或 HiDPI 覆盖条件；这两项是硬件矩阵 gap，不以单显示器替代。P3 仍缺长稳矩阵及上述硬件场景，继续保持未验收，P4 不得启动。
- 2026-08-25 P3 长稳补充：前台隔离客户端以 `MTL_DEBUG_LAYER=1`、Metal、FSAA 4 连续运行约 120 秒，结束时经 FIFO 成功保存 1920x1018 RGBA screenshot（SHA-256 `bb12e236609e5b331b3ef0a1f14a2b369b4f6915f98142bfe9c1f72ac4de2dfb`）并有序退出。启动器确认 `Metal API Validation Enabled`；自动诊断从 102 个 120-frame `frame_submit` 样本累计约 12,240 帧，全部为 `backend=Metal native`、`renderer=Apple M2`、`fsaa=4`、`hidpi_scale=1.000`，且 `frame_serialization_wait_count=0`、等待时间为零。客户端和启动器无 error、assertion、fallback 或 validation failure。P3 的 screenshot、video、固定 UI blur、fullscreen、borderless、FSAA、resize/readback runtime 及长稳均已完成本机覆盖；仅余 HiDPI 与多显示器/display switch 的真实硬件矩阵 gap。当前设备是单一非 HiDPI 1920x1080 显示器，不能把该 gap 伪标为通过；因此 P3 仍不作全矩阵阶段验收，P4 不得启动。
- 2026-08-25 验证刷新：当前工作树运行 `python3 qmclient_scripts/gate/check_gate.py --mode default` 为 13/0/0；默认 `cmake-build-release` 的 C++ 全量测试为 2832/2832，Rust 全量通过。该 gate 复核通用源码卫生与当前工作树测试状态；Metal 专用构建、runtime test、打包与真机证据仍以独立 `cmake-build-metal-release` 记录为准，不能用此 1x/单显示器 gate 替代 HiDPI 或多显示器矩阵。
- 2026-08-26：用户明确授权在 P3 硬件矩阵缺口仍存在时进入 P4。该授权不改变 P3 的未验收状态：发布前仍必须在真实 HiDPI 显示器及多显示器/显示器切换场景复核；在完成 P5 Task 32 前，不得以当前单一非 HiDPI 显示器的结果作为 macOS 默认后端晋级依据。

# 7. P4：Qm 专用 shader

## Task 27：Media Island 与 rounded-rect SDF

- 对照 OpenGL/Vulkan 参数布局和 alpha/clip 语义编写 MSL。
- CPU/MSL offset 测试、普通/HiDPI/缩放视觉对比。
- shader/pipeline 失败保持现有非 SDF fallback。

### 实施记录（2026-08-26）

- Metal 新增 Media Island 与 rounded-rect SDF fragment entry point，复用 `GL_SVertex`、`SState` 的 MVP/clip/alpha contract；Media Island 保留 45-vec4 参数块、可选 backdrop texture、blob/capsule/ring/阴影合成语义。
- 普通与 MSAA 各自建立 SDF pipeline；仅当两种 SDF 的全部 blend variant 创建成功时发布 `m_MediaIslandSdf` / `m_RoundedRectSdf` capability。pipeline 创建失败时保持 capability false，让上层继续既有几何 fallback；FSAA 切换同步重建或撤销 SDF pipeline。
- 新增 `MetalBackendContract.QmSdfCommandsUseMatchedPipelinesAndLayout` 与 `MetalBackendRuntime.QmSdfPipelinesRenderBackdropAlphaAndClip`，runtime 在 `MTL_DEBUG_LAYER=1` 下验证 backdrop alpha 合成、rounded-rect clip 和 Metal native command path。
- 验证：`xcrun --sdk macosx metal -c data/shader/metal/qmclient.metal ...` 通过；`cmake --build cmake-build-metal-release --target game-client -j 8` 通过；`cmake --build cmake-build-metal-release --target testrunner -j 8` 通过；`MTL_DEBUG_LAYER=1 ./cmake-build-metal-release/testrunner --gtest_filter='MetalBackendContract.QmSdfCommandsUseMatchedPipelinesAndLayout:MetalBackendRuntime.QmSdfPipelinesRenderBackdropAlphaAndClip'` 为 2/2，Metal API Validation enabled 且无 validation error。

## Task 28：Textured MSDF

- 实现 texture、transform、rotation、alpha 和 fallback 合同。
- capability 使用 atomic release/acquire 发布。
- MSDF probe/reload 失败不得产生每帧重试或 command-buffer flush 回归。

### 实施记录（2026-08-26）

- Metal 新增 qmclient_textured_msdf_fragment，沿用 STexturedMsdfParams 的 pxRange/atlasWidth/atlasHeight 参数布局、median RGB 与 fwidth(TexCoord) 抗锯齿、顶点 tint/alpha 和 clamp sampler 语义；CPU 旋转与 UV 打包保持在 CGraphics_Threaded::RenderTexturedMsdf。
- 普通与 MSAA 各自建立 Textured MSDF pipeline，命令路径校验纹理槽、纹理资源、参数范围、primitive 数量和 buffer 边界；pipeline 或 shader 不完整时 capability 保持 false，上层继续几何 fallback。FSAA 切换同步重建或撤销 Textured MSDF pipeline，生命周期使用 atomic release 清零/发布，读取由既有 acquire 路径完成。
- 新增 MetalBackendContract.TexturedMsdfUsesDedicatedPipelinesAndAtomicCapability 与 MetalBackendRuntime.TexturedMsdfPipelineRendersTintAndOpacity；runtime 使用 1x1 白色 MSDF texel 验证纹理绑定、tint、alpha/opacity 和 native command path。
- 验证：cmake --build cmake-build-metal-release --target game-client -j 8 通过；cmake --build cmake-build-metal-release --target testrunner -j 8 通过；MTL_DEBUG_LAYER=1 ./cmake-build-metal-release/testrunner --gtest_filter='MetalBackendContract.*:MetalBackendRuntime.TexturedMsdfPipelineRendersTintAndOpacity:MetalBackendRuntime.QmSdfPipelinesRenderBackdropAlphaAndClip' 为 16/16，Metal API Validation enabled 且无 validation error。按用户要求未运行 default gate。

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

### 实施记录（2026-08-26）

- Metal 新增默认关闭的 `qm_macos_graphics_diagnostics` 采样：每 120 帧记录 command buffer/render call/encoder、frame-slot wait、drawable 获取、GPU completion wait、upload/readback bytes 及纹理/缓冲创建数量与字节数，用于区分设置页响应、首帧初始化和游戏场景 GPU 压力；未以单一 FPS 数字判断 Metal 与 Vulkan 优劣。
- 修复呈现读回生命周期：普通异步 `Swap()` 保留的 shared readback buffer 在 screenshot、read-pixel 和视频读取前统一等待对应 command buffer 完成并检查状态，避免三帧 in-flight 下读到旧数据或未完成数据。
- 修复纹理、文本纹理 create/update 失败静默成功：资源分配或上传失败现在传播为 render command error；文本纹理成对创建失败时回滚已成功的一侧，避免半初始化槽位。
- 验证：`cmake --build cmake-build-metal-release --target run_cxx_tests -j 4` 为 2843/2843；`cmake --build cmake-build-metal-release --target game-client -j 4` 通过；release C++ 为 2837/2837；release Rust 全量（含 doctest）通过；`python3 qmclient_scripts/gate/check_gate.py --mode default` 为 13/0/0。真实 UI 点击延迟、进入游戏首帧卡顿及 Vulkan 对比仍需在 dev 客户端用同场景诊断日志与 GPU capture 归因。
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
