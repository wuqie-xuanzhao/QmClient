---
type: design-specification
date: 2026-08-04
status: draft
scope:
  - macOS 原生 Metal 图形后端
  - SDL2 Metal view 与 CAMetalLayer 接入
  - CGraphics_Threaded 命令协议到 Metal 的完整映射
  - Metal shader、pipeline、资源、同步、呈现、截图与诊断
  - macOS 构建、打包、回退、验证与默认后端晋级条件
authority:
  - 当前 QmClient 生产源码与 CodeGraph 调用关系
  - Apple Metal、QuartzCore 与 Xcode 官方文档
  - SDL2 官方 API 文档及仓库内 SDL 2.0.20 头文件
relationship:
  - 本文定义 Metal 后端的目标行为、架构边界和分阶段实现路径
  - 本文不证明 Metal 已实现、可用或性能优于 OpenGL/Vulkan
  - 实现计划可以拆分本文阶段，但不得改变本文的行为与验收约束
---

# QmClient macOS Metal 原生渲染后端规格

## 1. 文档地位

本文是 QmClient 在 macOS 增加原生 Metal 渲染后端的设计规格。它同时规定：

1. 用户可见行为和回退方式。
2. 与现有 `CGraphics_Threaded`、SDL 窗口层和图形命令协议的集成边界。
3. Metal 的窗口、帧、资源、shader、pipeline、同步、readback 和错误处理方法。
4. 从最小兼容后端到完整现代后端的实施顺序。
5. 自动验证、真机视觉验收、GPU capture 和默认后端晋级条件。

文中结论分为三类：

| 标记 | 含义 |
|---|---|
| **当前事实** | 已由当前仓库源码、CodeGraph 或仓库内依赖头文件确认。 |
| **平台约束** | 由 Apple 或 SDL 官方文档直接支持。 |
| **规格决定** | QmClient 为满足现有行为和风险边界作出的设计选择；实现前不得擅自替换。 |
| **待验证** | 静态设计不能证明，必须通过 macOS 真机构建、运行、视觉对比或 GPU capture 决定。 |

本文处于 `draft`。进入生产实现前必须完成 P0 的合同测试、shader 构建探针和 macOS 设备基线；未完成这些前置项时，不得把 Metal 设为默认后端。

## 2. 问题与目标

### 2.1 当前问题

**当前事实：** macOS 默认使用 OpenGL，Vulkan 路径依赖 Vulkan loader、MoltenVK 和离线 shader 工具，并且 CMake 当前在 macOS 上将 Vulkan 保持为显式 opt-in。当前仓库没有 Metal command processor、MSL shader 或 Metal 构建目标。

新增 Metal 后端需要解决的不是“创建一个 Metal window”，而是完整实现上层图形命令的语义，包括纹理、流式顶点、buffer container、tile/quad/text、render target、MSAA、SDF、截图、read pixel、VSync、HiDPI 和错误恢复。

### 2.2 用户目标

- macOS 上可以通过 `gfx_backend Metal` 选择原生 Metal。
- Metal 后端与 OpenGL/Vulkan 共用同一个 `CGraphics_Threaded` 上层 API，不复制游戏或 UI 渲染逻辑。
- 菜单、游戏地图、文本、HUD、Qm UI、截图和视频相关路径保持可用。
- Metal 初始化或运行失败时，客户端可以回退到 macOS OpenGL 4.1 安全配置。
- Metal 后端具备可诊断、可捕获、可量化的运行时证据。
- 只有在正确性、稳定性和性能基线完成后，才允许讨论将 Metal 改为 macOS 默认后端。

### 2.3 非目标

- 不迁移到 SDL3 或 SDL GPU API。
- 不改变协议、物理、预测、快照、输入、Demo、地图或皮肤格式。
- 不为了 Metal 重写 `CGraphics_Threaded` 或游戏侧渲染调用。
- 不在第一阶段引入 Metal 4 专用 API、mesh shader、ray tracing、MetalFX 或 argument buffer 架构。
- 不承诺 Metal 必然比 OpenGL 或 MoltenVK 更快；性能结论必须来自固定场景的真机测量。
- 不在首轮删除 OpenGL 或 Vulkan 后端。

## 3. 权威来源与证据边界

### 3.1 来源优先级

出现冲突时按以下顺序判断：

1. 当前 QmClient 命令协议、用户行为和兼容性要求。
2. 当前 SDK 中的 Apple API 声明及 Apple Developer Documentation。
3. SDL2 官方文档及当前仓库实际携带的 SDL 头文件。
4. 当前 OpenGL/Vulkan 实现体现的行为语义。
5. 历史 Apple Best Practices 文档，仅用于仍与当前 API 一致的基础原则。

博客、论坛、示例仓库和第三方 Metal 封装不得作为规格依据。

### 3.2 当前源码基线

| 基线 | 当前事实 | 影响 |
|---|---|---|
| 后端枚举 | `src/engine/graphics.h` 只有 OpenGL、GLES、Vulkan、Auto。 | 必须新增 `BACKEND_TYPE_METAL`，并更新所有穷举分支。 |
| 后端选择 | `CGraphicsBackend_SDL_GL::DetectBackend()` 解析 `DDNET_DRIVER` 和 `gfx_backend`。 | Metal 必须同时支持环境变量和配置选择。 |
| 窗口创建 | 当前在 `SDL_WINDOW_OPENGL` 和 `SDL_WINDOW_VULKAN` 之间二选一。 | 必须增加 `SDL_WINDOW_METAL` 第三路。 |
| 命令处理 | `CCommandProcessor_SDL_GL` 先调用 API fragment，再调用 SDL/general fragment。 | Metal fragment 可以直接消费 core render、swap、vsync 和 multisampling 命令。 |
| 初始化生命周期 | API fragment 通过 `CMD_PRE_INIT`、`CMD_INIT`、`CMD_SHUTDOWN`、`CMD_POST_SHUTDOWN` 管理。 | Metal view/layer 和 GPU 对象必须按线程要求分配到对应阶段。 |
| 能力协商 | `SBackendCapabilities` 控制 buffering、texture array、render target 和 SDF 路径。 | 可用能力必须在实现完成后逐项开启，禁止虚报。 |
| 设置页 | Graphics 设置遍历 `BACKEND_TYPE_COUNT` 和 `GetDriverVersion()`。 | 新枚举会自然进入列表，但显示名和版本语义要显式处理。 |
| 安全恢复 | 当前 graphics crash recovery 固定回退 OpenGL，macOS 使用 4.1、无 FSAA、窗口模式。 | Metal 初始化/提交错误必须接入相同安全回退。 |
| SDL 版本 | 仓库内 macOS SDL framework 头文件为 SDL 2.0.20。 | 已满足 SDL Metal API 的最低版本要求。 |
| macOS 源码 | 仓库已有 `.mm` 文件，CMake 已编译 Objective-C++ 平台源。 | Metal fragment 使用 `.mm` 不需要引入新的语言体系。 |

当前源码锚点：

- 后端枚举：`src/engine/graphics.h:147-157`。
- core graphics command 清单：`src/engine/client/graphics_threaded.h:121-187`。
- backend interface 与主线程边界：`src/engine/client/graphics_threaded.h:794-876`。
- backend capability：`src/engine/client/backend_sdl.h:111-139`。
- command processor 分派顺序：`src/engine/client/backend_sdl.cpp:380-410`。
- OpenGL/GLES/Vulkan fragment 创建：`src/engine/client/backend_sdl.cpp:413-460`。
- 配置/环境变量后端选择：`src/engine/client/backend_sdl.cpp:752-784`。
- SDL window/context/drawable 初始化：`src/engine/client/backend_sdl.cpp:1142-1451`。
- API fragment 初始化参数和 capability 发布：`src/engine/client/backend_sdl.cpp:1453-1507`。
- shutdown 与窗口销毁顺序：`src/engine/client/backend_sdl.cpp:1575-1610`。
- drawable size 分支：`src/engine/client/backend_sdl.cpp:1805-1811`。
- Graphics 设置的后端枚举：`src/game/client/components/menus_settings.cpp:3618-3663`。
- macOS crash recovery：`src/engine/client/client.cpp:260-349`。
- macOS 默认 backend 配置：`src/engine/shared/config_variables.h:821-831`。
- macOS Vulkan opt-in 与 client source 注册：`CMakeLists.txt:122-130,2643-2667`。
- 当前 macOS deployment target：`CMakeLists.txt:1-10`。
- 仓库内 SDL Metal API 声明：`ddnet-libs/sdl/mac/libarm64/SDL2.framework/Versions/A/Headers/SDL_metal.h:45-102`。

### 3.3 SDL Metal 约束

SDL2 官方文档明确说明：

- `SDL_Metal_CreateView()` 创建并挂载由 `CAMetalLayer` 支持的 view。
- macOS 上 SDL 不会自动给 `CAMetalLayer` 关联 `MTLDevice`，必须由应用设置。
- `SDL_Metal_GetLayer()` 返回底层 `CAMetalLayer`。
- `SDL_Metal_DestroyView()` 必须在 `SDL_DestroyWindow()` 之前调用。
- `SDL_Metal_GetDrawableSize()` 返回用于 viewport、scissor 等操作的像素尺寸。

因此，SDL 在本规格中只负责窗口/view 桥接，不负责 Metal device、command queue、frame lifecycle 或资源管理。

### 3.4 官方依据矩阵

| 本规格采用的约束 | 官方依据 | 规格中的落点 |
|---|---|---|
| SDL 创建 Metal view，但 macOS 应用负责给 layer 关联 device。 | SDL `SDL_Metal_CreateView` 文档和当前 SDL 头文件。 | 第 6.1 节由 Metal fragment 设置 `layer.device`。 |
| drawable 使用像素尺寸而非逻辑窗口尺寸。 | SDL `SDL_Metal_GetDrawableSize`、Apple `CAMetalLayer.drawableSize`。 | 第 6.3 节将 logical/window 与 drawable pixel 分离。 |
| drawable 来自有限池，应晚获取、早释放。 | Apple `CAMetalLayer.nextDrawable` 与 Best Practices: Drawables。 | 第 8.3 节延迟 `nextDrawable()` 并记录等待。 |
| drawable count 可以配置为 2 或 3。 | Apple `CAMetalLayer.maximumDrawableCount`。 | 第 6.2、8.2 节采用 3 个 frame slot。 |
| 不应在 present 前等待 command buffer 完成。 | Apple Best Practices: Drawables。 | 第 8.4 节禁止 wait-before-present。 |
| 每帧应提交较少 command buffer。 | Apple Best Practices: Command Buffers。 | 第 8.1 节规定正常帧一个主 command buffer。 |
| 动态资源需要限制 in-flight 帧覆盖。 | Apple Synchronizing CPU/GPU Work 与 Triple Buffering。 | 第 8.2、12.2 节使用 semaphore 和三帧 arena。 |
| resource storage mode 必须按访问模式和 GPU 类型选择。 | Apple resource storage mode 系列文档。 | 第 12 节先 correctness-first，再按 profile 分设备优化。 |
| managed resource 在 macOS 需要显式同步。 | Apple Synchronizing a Managed Resource in macOS。 | 第 12.2 节不在首轮盲目启用 managed。 |
| 小数据可以直接绑定，大数据应使用预分配 buffer。 | Apple Best Practices: Buffer Bindings。 | 第 12.2 节限制 `set*Bytes` 的用途。 |
| pipeline 创建应移出关键渲染路径。 | Apple Pipeline State Creation 与 Best Practices: Pipelines。 | 第 13.3 节要求初始化期建立必需 pipeline。 |
| render pass 的 load/store action 影响正确性和带宽。 | Apple Render Passes 与 Best Practices: Load and Store Actions。 | 第 10 节建立显式 encoder/pass 状态机。 |
| MSAA pipeline sample count 与 attachment 必须一致并显式 resolve。 | Apple MSAA、`supportsTextureSampleCount`、`MTLStoreAction` 文档。 | 第 14 节逐项验证并重建依赖对象。 |
| drawable readback 使用 texture-to-buffer copy 并在完成后读 CPU buffer。 | Apple Reading Pixel Data 与 `MTLBlitCommandEncoder`。 | 第 16 节限定同步 readback 边界。 |
| shader 可以预编译为 library 并从文件加载。 | Apple precompiled shader library 与 `makeLibrary(filepath:)`。 | 第 13.2、18.1 节要求 release 离线构建和打包 metallib。 |
| macOS 可以枚举全部 Metal device，系统也提供默认 device。 | Apple `MTLCopyAllDevices` 与 `MTLCreateSystemDefaultDevice`。 | 第 6.1.1 节对接现有 `gfx_gpu_name` 与 `GetGpus()`。 |
| Xcode/Metal 提供帧捕获和 GPU counters。 | Apple `MTLCaptureManager` 与 GPU counters 文档。 | 第 17.4、20.5 节定义诊断和性能证据。 |

## 4. 总体架构

### 4.1 目标数据流

```text
Game / UI / Text
        |
IGraphics / CGraphics_Threaded
        |
CCommandBuffer
        |
CCommandProcessor_SDL_GL
        |-- CCommandProcessorFragment_General
        |-- CCommandProcessorFragment_SDL
        `-- CCommandProcessorFragment_Metal
                |
                |-- CAMetalLayer / CAMetalDrawable
                |-- MTLDevice / MTLCommandQueue / MTLCommandBuffer
                |-- MTLRenderCommandEncoder / MTLBlitCommandEncoder
                |-- MTLTexture / MTLBuffer / MTLSamplerState
                `-- MTLRenderPipelineState
```

### 4.2 复用与隔离

**规格决定：** 保留现有 `CCommandBuffer` 和 `CCommandProcessorFragment_GLBase` 生命周期协议。第一轮允许 Metal 工厂返回 `CCommandProcessorFragment_GLBase *`，因为该基类实际已经承担跨 API 命令协议。

以下重命名不属于 Metal 首轮必要范围：

- `CCommandProcessorFragment_GLBase` 改为通用名字。
- `CCommandProcessor_SDL_GL` 改为通用名字。
- `CGraphicsBackend_SDL_GL` 改为通用名字。

只有当独立重构能以机械修改、无行为变化和完整测试完成时，才允许在后续提交处理。不得把大范围命名重构与第一个可运行 Metal 后端混在一起。

### 4.3 新文件边界

目标文件：

```text
src/engine/client/backend/metal/backend_metal.h
src/engine/client/backend/metal/backend_metal.mm
src/engine/client/backend/metal/metal_types.h
data/shader/metal/qmclient.metal
```

约束：

- Objective-C 和 Metal 对象只存在于 `.mm` 实现或私有实现对象中。
- 公共 C++ 头不得暴露 `id<MTL...>`、`CAMetalLayer *` 或 Objective-C import。
- `metal_types.h` 只允许包含 CPU/MSL 两端共享、布局明确的 POD 常量结构。
- Metal 后端只能在 `CONF_PLATFORM_MACOS && CONF_BACKEND_METAL` 下编译。

## 5. 后端发现、设置与回退

### 5.1 枚举和编译开关

新增：

```cpp
BACKEND_TYPE_METAL
CONF_BACKEND_METAL
```

所有 `switch(EBackendType)` 必须显式处理 Metal。禁止依赖 `default` 将 Metal 当作 OpenGL 或 Vulkan。

### 5.2 配置行为

- `gfx_backend Metal` 选择 Metal。
- `DDNET_DRIVER=Metal` 临时覆盖配置。
- 非 macOS 构建不报告 Metal 可用，配置中遗留 `Metal` 时回退当前平台默认后端。
- 首轮实现保持 macOS 默认 `gfx_backend OpenGL`。
- `gfx_gl_major/minor/patch` 对 Metal 不具有 API 版本含义；Metal 后端不得根据这些字段改变 feature set。

设置页中的 Metal 显示名必须为 `Metal`，不能伪装为 `Metal 1.0`。如果现有 `GetDriverVersion()` 接口强制提供数字，数字只作为兼容占位且不得显示；长期应把“后端选择”与“OpenGL 版本选择”拆开。

### 5.3 安全回退

以下情况必须回退 macOS OpenGL 4.1、FSAA 0、窗口模式，并记录原因：

- `MTLCreateSystemDefaultDevice()` 返回空。
- SDL Metal view 或 layer 创建失败。
- shader library 或必需 pipeline 创建失败。
- 首帧无法取得 drawable 且错误可重复。
- command buffer 进入 error 状态并被判定为不可恢复。
- 上一会话 crash marker 指向 Metal/graphics driver 路径。

回退必须落回配置，避免每次启动重复进入已知失败路径。一次偶发 drawable timeout 不得立即永久改写配置，应先记录并允许有限重试。

## 6. 窗口、CAMetalLayer 与 HiDPI

### 6.1 创建顺序

1. SDL video subsystem 初始化。
2. 使用 `SDL_WINDOW_METAL`、现有 resize/fullscreen/borderless/HiDPI flags 创建 `SDL_Window`。
3. 在主线程执行的 `CMD_PRE_INIT` 中调用 `SDL_Metal_CreateView()`。
4. 通过 `SDL_Metal_GetLayer()` 获得 `CAMetalLayer`。
5. 创建或选择 `MTLDevice`，设置 `layer.device`。
6. 设置 pixel format、drawable size、framebuffer-only 和 presentation properties。
7. 创建 command queue、shader library、sampler、pipeline 和 frame resources。

销毁顺序与之相反；`SDL_Metal_DestroyView()` 必须发生在 `SDL_DestroyWindow()` 之前。

#### 6.1.1 Device 枚举与选择

QmClient 已有 `gfx_gpu_name` 和 `GetGpus()` 合同，Metal 不得永久忽略它：

- `gfx_gpu_name=auto` 使用 `MTLCreateSystemDefaultDevice()`。
- 显式 GPU 名称使用 `MTLCopyAllDevices()` 枚举并按稳定的设备名称匹配。
- 配置设备不存在时记录 warning，回退系统默认 device，不把整个 Metal 后端判为不可用。
- `GetGpus()` 至少填充设备名称、集成/独立类别和当前选择状态；能取得 registry ID 时用它辅助诊断，不把名称当作跨机器稳定 ID。
- 选择完成后必须把同一个 device 同时用于 `CAMetalLayer.device`、command queue 和全部资源。
- 多 GPU Mac 上的 display/device 匹配和跨 GPU 呈现成本属于真机验证项；在没有设备证据前，不声称手工选择独立 GPU 一定更快。

### 6.2 Layer 配置

首轮固定：

| 属性 | 规格决定 | 原因 |
|---|---|---|
| `device` | 当前 Metal device | SDL 官方明确不会在 macOS 自动设置。 |
| `pixelFormat` | `MTLPixelFormatBGRA8Unorm` | 与默认窗口格式和截图转换路径匹配；sRGB 迁移需单独视觉评审。 |
| `drawableSize` | `SDL_Metal_GetDrawableSize()` 的像素尺寸 | 与现有 HiDPI viewport 语义一致。 |
| `maximumDrawableCount` | 3 | 与三帧资源模型一致；Apple 允许 2 或 3。 |
| `displaySyncEnabled` | 跟随 `CMD_VSYNC` | Apple 将其定义为是否同步显示刷新。 |
| `framebufferOnly` | 首轮设为 `false` | 现有 screenshot/read-pixel 合同要求读取或复制呈现结果。 |
| `presentsWithTransaction` | `false` | 游戏渲染不依赖 Core Animation transaction 呈现。 |

`framebufferOnly=false` 可能减少 Core Animation 优化空间。P5 可以评估“常态 true、截图帧使用中间 present texture”的方案，但只有 GPU capture 证明收益且截图行为无回归时才允许切换。

### 6.3 尺寸与 resize

- 逻辑窗口尺寸继续来自 SDL window API。
- drawable 像素尺寸只使用 `SDL_Metal_GetDrawableSize()`。
- `GetViewportSize()` 的 Metal 分支不得调用 `SDL_GL_GetDrawableSize()`。
- resize、屏幕切换、全屏切换或 DPI 变化后，主线程更新 layer drawable size，并向图形线程提交 `CMD_UPDATE_VIEWPORT`。
- drawable size 为零时视为窗口最小化/不可呈现状态，不创建零尺寸 texture，不阻塞等待 drawable。
- 所有尺寸相关 render target 和 MSAA texture 延迟到下一次有效帧重建。

## 7. 线程与生命周期

### 7.1 线程所有权

| 对象/操作 | 所在线程 |
|---|---|
| SDL window 创建、Metal view 创建/销毁、窗口属性 | 主线程 / `CMD_PRE_INIT`、`CMD_POST_SHUTDOWN` 单线程阶段 |
| command queue、command buffer、encoder、pipeline/资源命令 | 现有 graphics processor 线程 |
| 游戏/UI 命令生成 | 现有主线程 |
| command buffer completed handler | Metal 回调线程，只允许更新线程安全状态或 signal semaphore |

Apple 文档说明 command queue 是线程安全的，但这不构成首轮并行编码的理由。首轮 Metal fragment 必须保持单 graphics thread 编码，与当前 OpenGL 路径的可理解性一致。

### 7.2 Objective-C 生命周期

- graphics processor 每次处理一批 Metal 命令时必须存在 `@autoreleasepool`。
- drawable 必须尽可能晚获取，并在 `present`、`commit` 后尽快释放引用。
- 禁止使用 `commandBufferWithUnretainedReferences`。
- completion handler 不得捕获会在 shutdown 前销毁的裸 C++ 指针，除非 shutdown 明确等待所有 in-flight command buffer。
- `CMD_SHUTDOWN` 必须停止接受新帧、等待 in-flight frames、销毁 GPU 资源。

## 8. 帧模型与呈现

### 8.1 一帧一个主 command buffer

Apple 建议每帧提交尽可能少的 command buffer，并通常使用一到两个；本规格首轮要求正常帧使用一个主 command buffer。

```text
AcquireFrameSlot
  -> create MTLCommandBuffer
  -> encode uploads/offscreen passes
  -> acquire drawable as late as possible
  -> encode onscreen render pass
  -> endEncoding
  -> present(drawable)
  -> addCompletedHandler
  -> commit
  -> release frame-local Objective-C references
```

仅在 screenshot/readback 同步、初始化上传或设备限制有明确证据时，才允许额外 command buffer。

### 8.2 三帧并行

建立 3 个 frame slot，每个 slot 至少持有：

- 动态 vertex/index/uniform upload arena。
- screenshot/readback staging 状态。
- command buffer 完成状态。
- 本帧临时资源强引用。

CPU 在复用 slot 前等待 semaphore；GPU completion handler 释放 slot。禁止每帧创建新的动态 `MTLBuffer`。

### 8.3 Drawable 获取

Apple 明确指出 drawable 来自有限资源池，获取不到时调用线程可能等待；应尽可能晚获取并尽快释放。因此：

- offscreen render target 和 upload 可以在获取 drawable 前编码。
- 首个需要 backbuffer 的 `CMD_CLEAR`/draw 或最终 onscreen pass 才调用 `nextDrawable()`。
- 获取 drawable 的等待时间必须单独计数，不能混入“GPU render time”。
- `nextDrawable()` 返回空时跳过呈现并记录连续失败次数；不得解引用空 drawable。
- `allowsNextDrawableTimeout` 保持系统默认，除非真实设备证明默认 timeout 造成可恢复的长期错误；不得为掩盖资源泄漏而无限等待。

### 8.4 Swap 和 VSync

`CMD_SWAP` 必须：

1. 结束活动 render/blit encoder。
2. 在 command buffer 上注册 drawable presentation。
3. 注册 completion handler。
4. commit command buffer。
5. 推进 frame slot。

禁止先等待 command buffer 完成再 present，Apple 文档明确指出这会造成明显 CPU stall。

`CMD_VSYNC` 更新 `CAMetalLayer.displaySyncEnabled`。VSync 关闭后是否真正达到非同步呈现受系统 compositor 和设备影响，必须通过真机测量，不得只凭属性写入声称“已解除帧率限制”。

## 9. 命令协议映射

### 9.1 完整性原则

Metal fragment 对每个 core graphics command 必须满足以下之一：

1. 正确实现并返回 handled。
2. 在明确关闭对应 `SBackendCapabilities` 后，由上层不再生成该命令。
3. 将命令转交 General/SDL fragment。

禁止把未实现的绘制命令静默返回成功。debug 构建必须对意外命令给出命令编号和后端状态；release 构建必须 latch 可诊断错误并进入安全路径。

### 9.2 P1 必需命令

P1 使用非 buffering 兼容路径，必须实现：

- texture create/update/destroy。
- text texture create/update/destroy。
- clear。
- immediate render。
- viewport、clip/scissor、blend、wrap/sampler。
- swap、vsync。
- screenshot、read pixel。
- signal 和必要的 init/shutdown 命令。

P1 必须把以下能力报告为 false：tile buffering、quad buffering、text buffering、quad container buffering、2D texture array、render target、backbuffer capture、Gaussian blur、Media Island SDF、rounded-rect SDF、textured MSDF。

### 9.3 P2 必需命令

P2 实现并开启：

- buffer object create/recreate/update/copy/delete。
- buffer container create/update/delete。
- index capacity notification。
- tile layer、border tile、quad layer/grouped。
- text buffering。
- quad container、extended quad、sprite multiple。
- 2D texture array 或等价的完整语义路径。

每项 capability 只能在对应命令、资源生命周期、视觉测试和异常路径全部完成后开启。

### 9.4 P3/P4 命令

P3：render target create/destroy/begin/end/draw、capture backbuffer、readback、MSAA、Gaussian blur。

P4：Media Island SDF、rounded-rectangle SDF、textured MSDF，并与 OpenGL/Vulkan 的参数、alpha、clip、rotation 和 fallback 行为保持一致。

## 10. Render pass 与状态机

### 10.1 Encoder 边界

Metal render encoder 与 render target、load/store action 绑定。后端必须维护显式状态：

```text
NoFrame
FrameOpenNoEncoder
BackbufferEncoderOpen
RenderTargetEncoderOpen
BlitEncoderOpen
FrameCommitted
```

切换 render target、执行 texture/buffer copy、readback 或 Gaussian blur 时，必须先结束不兼容 encoder。连续绘制到同一 attachment 时应复用 encoder，避免每个 draw 创建独立 render pass。

### 10.2 Clear 语义

- 帧或 target 的第一次 clear 优先映射到 attachment `loadAction=Clear`。
- encoder 已开始后的强制 clear 可以结束并重开 pass，或使用正确的全屏绘制路径。
- 非强制 clear 与当前 OpenGL/Vulkan 行为必须通过合同测试确认，不能仅按 Metal 习惯重新解释。

### 10.3 Load/store

- 后续 pass 需要读取前一 pass 内容时，前一 pass 必须 store，后一 pass 必须 load。
- 内容不再使用时才允许 `DontCare`。
- store action 在 encoder 结束前必须是明确有效值。
- 优化 load/store action 必须以 GPU capture 的 attachment 依赖为依据。

## 11. 坐标、裁剪、颜色与混合

### 11.1 坐标合同

Metal viewport 和 scissor 使用左上角 window coordinate。QmClient 上层 screen mapping、纹理坐标和 render target 方向必须形成唯一转换层。

要求：

- clip rect 使用 drawable 像素坐标并 clamp 到 attachment 范围。
- 空 clip 不发出 draw。
- 负尺寸、越界和 resize 中间状态不得产生无效 scissor。
- backbuffer 和 offscreen target 的图像方向必须一致。
- screenshot 输出保持当前 `CImageInfo` 约定，不得出现上下翻转或 BGRA/RGBA 通道交换。

### 11.2 Blend

建立 `EBlendMode -> MTLRenderPipelineColorAttachmentDescriptor` 的纯函数映射，并对每种模式做单元测试。Pipeline cache key 必须包含 blend mode，因为 Metal blend state 属于 pipeline state。

### 11.3 Sampler/wrap

`EWrapMode` 映射为持久化 `MTLSamplerState`。至少缓存 normal/repeat 与 clamp 两种状态。禁止每次 draw 创建 sampler。

### 11.4 颜色空间

首轮保持 `BGRA8Unorm` 非 sRGB，避免在没有全链视觉基线时改变 gamma 行为。引入 `BGRA8Unorm_sRGB`、HDR、EDR 或 wide color 必须另开规格，包含纹理源颜色空间、shader 输出和截图编码迁移。

## 12. 资源模型

### 12.1 总体原则

Apple 建议预先分配并复用 `MTLBuffer`、`MTLTexture`，为资源选择明确 storage mode 和 texture usage。Metal 后端不得在每个 draw 创建资源。

### 12.2 Buffer 分类

| 类型 | P1/P2 存储策略 | 更新方式 |
|---|---|---|
| frame stream arena | `StorageModeShared`，三帧环 | CPU 直接写入，按 alignment 分配 offset |
| 小于 4 KB 的极小常量 | 可用 `setVertexBytes`/`setFragmentBytes` | 仅用于固定小数据，不替代大 upload arena |
| 静态 buffer object | `StorageModePrivate` | shared staging + blit copy |
| 高频更新 buffer object | P1/P2 先用 shared 或 staging copy，由 profile 决定 | 必须记录上传字节与 copy 次数 |
| readback buffer | 首轮 `StorageModeShared` | GPU copy 完成后 CPU 读取 |

Intel/AMD Mac 可以使用 managed resource，但需要显式 CPU/GPU 同步。首轮优先采用 correctness-first 的 shared staging/private target 模型；P5 再基于设备类型和 profile 引入 managed 优化。

### 12.3 Texture 分类

- 常规纹理使用 private texture，usage 至少包含实际需要的 `ShaderRead`、`RenderTarget`、`PixelFormatView` 等标志。
- 上传使用 shared staging buffer 和 blit encoder，不在 render loop 临时创建中间纹理。
- texture update 必须正确处理 bytes-per-row、子区域和 mip level。
- mipmap 只在现有 texture flags 要求时生成。
- text atlas 的单通道/双纹理语义保持与当前后端一致。
- texture slot 销毁后必须从所有缓存、bind state 和 memory counters 中移除。

### 12.4 内存统计

继续填写：

- texture memory。
- buffer memory。
- streamed memory。
- staging memory。

统计必须来自后端实际分配大小，并区分持久化资源与 frame ring。Objective-C retain 数量不能替代 GPU 内存统计。

## 13. Shader 与 pipeline

### 13.1 Shader 来源

**规格决定：** Metal 使用显式维护的 Metal Shading Language，不在运行时转换 GLSL/SPIR-V，也不把 MoltenVK 或 SPIRV-Cross 作为 Metal 后端依赖。

理由：

- 当前特殊 shader 数量有限。
- 显式 MSL 可以直接审核 binding、结构体布局、导数、颜色、坐标和 sample count。
- 避免增加运行时编译、转换失败和额外打包依赖。

MSL 与 GLSL 语义必须通过渲染金图和参数合同保持同步；不得只凭函数名认为两端等价。

### 13.2 离线编译

Apple 官方支持使用 Metal command-line tools 将 MSL 预编译为 shader library，并通过文件路径创建 `MTLLibrary`。Release 构建必须：

1. CMake 查找当前 SDK 的 Metal 编译工具。
2. 编译 `.metal` 为中间产物。
3. 链接为 `.metallib`。
4. 将 `.metallib` 放入客户端 bundle/resource 路径。
5. 启动时从确定路径加载，失败则给出完整错误并回退。

禁止 release 版本依赖运行时 `newLibraryWithSource`。开发构建也不应悄悄运行时编译并掩盖缺失打包资源。

### 13.3 Pipeline cache key

至少包含：

```text
shader variant
vertex layout
color pixel format
sample count
blend mode
render target class
textured/untextured
texture dimensionality
special SDF/MSDF variant
```

Apple 建议在非关键路径创建 pipeline，并尽量预先建立已知 pipeline。P1/P2 必需 pipeline 在初始化完成前创建；渲染热路径不得同步编译 pipeline。

Binary archive 属于 P5 可选优化，不是首轮正确性依赖。

### 13.4 CPU/MSL 共享结构

- 所有共享 uniform 结构使用固定宽度类型和显式 alignment。
- 使用 `static_assert(sizeof/offsetof)` 验证 CPU 布局。
- MSL 侧避免依赖 C++ ABI packing。
- shader buffer/texture/sampler index 集中定义，禁止散落 magic number。

## 14. MSAA

- 配置 0 表示单采样。
- 其他请求按 `MTLDevice::supportsTextureSampleCount` 验证，选择不高于请求值的受支持样本数。
- pipeline raster sample count 必须与 attachment sample count 一致。
- 多采样 backbuffer 使用独立 multisample texture，resolve 到 drawable。
- 多采样 render target 使用 multisample attachment 和单采样 resolve texture。
- resolve 使用明确的 resolve texture 和 `MultisampleResolve` 或必要的 store-and-resolve action。
- `CMD_MULTISAMPLING` 只在帧边界切换，并重建依赖 sample count 的 attachment/pipeline。

P1 可以只支持 sample count 1，但必须返回真实能力；不得接受 FSAA 请求后静默保持单采样。

## 15. Render target、backbuffer capture 与 blur

### 15.1 Render target

P3 的 render target texture 至少包含 `RenderTarget | ShaderRead` usage；需要 copy/readback 时增加对应 usage。Begin/End 必须保持现有状态、clip 和 screen mapping 语义。

### 15.2 Backbuffer capture

当前上层支持捕获 backbuffer 并在后续 pass 使用。Metal 实现必须保留一份可采样的单采样结果；多采样时捕获 resolve 后的图像。

### 15.3 Gaussian blur

- 使用两个同尺寸的 ping-pong texture。
- 水平和垂直 pass 必须显式结束/创建 encoder。
- source/destination 不能是同一 texture。
- radius 和 weights 与当前命令合同一致。
- 没有完整实现前保持 capability false，让上层使用现有 fallback。

## 16. Screenshot、read pixel 与视频路径

Apple 官方 readback 示例以“texture copy 到 CPU 可访问 buffer”为基础。本规格要求：

1. 结束当前 render encoder。
2. 将 drawable/resolve texture 的目标区域 copy 到 readback buffer。
3. commit command buffer。
4. 仅在调用合同需要同步结果时等待该 command buffer 完成。
5. 按 row pitch、BGRA/RGBA、alpha 和图像方向转换到 `CImageInfo`。

整帧常规呈现不得调用 `waitUntilCompleted`。同步等待只允许出现在 screenshot、read pixel、shutdown 或资源销毁需要的明确边界。

如果 `framebufferOnly=false` 的性能成本不可接受，P5 可以改为每帧最终颜色先写内部 present texture，再 blit/draw 到 framebuffer-only drawable；是否采用必须由 GPU capture 证明，不能凭直觉增加永久额外 pass。

## 17. 错误处理与可观测性

### 17.1 初始化错误

必须包含阶段和 Apple/SDL error：

```text
metal_device
metal_view
metal_layer
metal_command_queue
metal_shader_library
metal_pipeline:<variant>
metal_frame_resources
```

设置页/启动错误文本必须包含 Metal，并将现有“切换到 OpenGL 或 Vulkan”更新为准确后端列表。

### 17.2 异步 GPU 错误

每个 command buffer completion handler 检查 status/error，并把错误复制到线程安全 latch。下一次 processor 边界统一交给现有 graphics error 容器处理。

分类：

- drawable unavailable：可恢复 warning，有限重试。
- pipeline/resource 创建失败：初始化失败或功能禁用。
- command buffer error/device lost 类错误：fatal，停止提交并进入安全恢复。
- screenshot/readback 失败：当前操作失败，不伪造成功图片。

### 17.3 标签与计数器

所有主要 Metal 对象设置 label。至少暴露：

- command buffers/frame。
- render/blit encoders/frame。
- draw calls、pipeline switches、texture/sampler binds。
- uploaded bytes、staging bytes、readback bytes。
- drawable wait count/time、empty drawable count。
- CPU encode time、commit count、GPU start/end time（API 可用时）。
- in-flight frame high-water mark。
- pipeline cache hit/miss 和初始化耗时。

这些数据接入现有 `qm_macos_graphics_diagnostics` 或同一 graphics diagnostics 通道，不新增永久高频日志。

### 17.4 GPU capture

使用 Xcode GPU Frame Capture 作为人工验收工具。可以在 debug/diagnostics 模式下通过 `MTLCaptureManager` 提供“一次性捕获下一帧”，但不得默认开启或写入普通用户配置。

## 18. 构建与打包

### 18.1 CMake

macOS Metal 构建必须：

- 增加 `METAL` CMake option，macOS 默认可编译但不改变运行时默认后端。
- 定义 `CONF_BACKEND_METAL`。
- 注册 `backend_metal.mm/.h` 和共享类型文件。
- 链接 `Metal.framework`、`QuartzCore.framework`。
- 使用当前 `CMAKE_OSX_DEPLOYMENT_TARGET`，不得由 Metal 后端另行修改部署目标。
- 找不到 Metal shader compiler 或 metallib 工具时给出清晰 configure error；允许显式 `-DMETAL=OFF` 构建 OpenGL-only 客户端。
- 将 metallib 加入 app bundle、DMG 和非 bundle 本地运行资源布局。

### 18.2 架构

- Apple Silicon 是必测目标。
- 如果发布产物仍包含 `x86_64`，Intel Mac 也属于发布阻断矩阵。
- metallib 和 bundle 资源必须对实际发布架构/SDK 可加载；不能只验证裸可执行文件旁的开发路径。

### 18.3 不引入的依赖

原生 Metal 后端不得依赖：

- MoltenVK。
- Vulkan loader。
- SPIRV-Cross runtime。
- SDL3。
- 第三方 Metal wrapper。

## 19. 分阶段实施路径

### P0：合同与构建探针

目标：在不提供用户可选后端前，锁定语义和工具链。

实施：

- 建立 `EBackendType`、backend display、fallback、drawable size 的纯函数/源码合同测试。
- 建立 blend、wrap、primitive、clip、sample-count、pipeline-key 映射测试。
- 增加最小 MSL shader 和 CMake metallib 构建测试。
- 验证 bundle 与裸 executable 两种资源查找。
- 记录当前 OpenGL 固定场景基线和设备信息。

完成条件：shader 可离线编译和加载；测试先失败后通过；不向普通设置页暴露 Metal。

### P1：兼容渲染后端

目标：使用 immediate/non-buffering 路径启动客户端并完成基础菜单与游戏渲染。

实施：

- SDL Metal window/view/layer/device/queue 生命周期。
- 三帧 shared stream arena。
- texture/text texture、clear、immediate render、viewport/scissor/blend/sampler。
- swap/vsync、screenshot/read pixel。
- 所有高级 capability 保持 false。
- 初始化失败和 crash recovery 回退 OpenGL 4.1。

完成条件：菜单、进服、地图、普通文本和截图可用；无 Metal validation error；resize/HiDPI/fullscreen/vsync 通过真机矩阵。

### P2：现代 buffering 路径

目标：达到现有 OpenGL 3/Vulkan 的常规 tile、quad、text 和 container 渲染能力。

实施：

- private static buffers + staging upload。
- buffer object/container 全生命周期。
- tile、border tile、quad、grouped quad、text、sprite multiple。
- texture array 路径。
- 对应 capability 逐项开启。

完成条件：固定地图和 UI 的 draw 内容与 OpenGL 金图一致；资源创建/更新/销毁压力测试无错误；热路径无每 draw 资源分配。

### P3：Render target、MSAA 和 readback 完整化

目标：完成现代 UI effect、截图和 offscreen 工作流。

实施：render target、capture、readback、MSAA resolve、Gaussian blur、动态 sample count。

完成条件：render-target 单测、UI blur 场景、截图、resize 后重建和 FSAA 切换全部通过；多采样 pipeline/attachment 无 validation mismatch。

### P4：Qm 专用 shader

目标：对齐 OpenGL/Vulkan 的 Media Island SDF、rounded rect SDF 和 textured MSDF。

实施：显式 MSL shader、参数布局测试、alpha/clip/rotation/fallback 对比、后端 capability 接线。

完成条件：普通/HiDPI/缩放/旋转场景视觉一致；MSDF 失败回退 alpha atlas；不能将 FPS 差异静态归因于 shader。

### P5：性能、稳定性与默认后端评估

目标：在正确性完成后决定资源策略和默认后端。

候选优化：

- framebuffer-only present strategy。
- Apple GPU 与 Intel/AMD 的 storage mode 分支。
- pipeline binary archive。
- upload 合并、draw batching、encoder 合并。
- GPU counter/capture 指导的 load/store 优化。

Metal 默认后端晋级条件：

- 发布硬件矩阵无阻断错误。
- 固定场景视觉/功能验收全部通过。
- crash recovery 已实测。
- 无持续 Metal validation error。
- CPU frame、GPU frame、drawable wait、内存和功耗数据已记录。
- 与 OpenGL 对比没有超出预先登记预算的回归。
- OpenGL fallback 仍保留并可由用户选择。

## 20. 测试与验收矩阵

### 20.1 自动测试

- backend string/enumeration/compile-guard。
- Metal unavailable/factory failure/fallback。
- blend、sampler、primitive、clip、viewport、sample count 映射。
- CPU/MSL shared struct size/offset。
- pipeline cache key equality/hash。
- texture row pitch 和 BGRA/RGBA 转换。
- screenshot 上下方向。
- frame slot acquire/release 和 shutdown drain。
- capability false 时上层不生成对应高级命令。
- render target 生命周期和非法 ID 防护。

### 20.2 构建验证

代码阶段按仓库 gate 执行，至少串行：

```bash
cmake --build cmake-build-release --target game-client -j 14
cmake --build cmake-build-release --target run_cxx_tests -j 14
cmake --build cmake-build-release --target run_rust_tests -j 14
python3 qmclient_scripts/gate/check_gate.py --mode quick
```

提交前优先 `--mode default`。Metal shader 自定义 target 和 bundle/package target 必须单独验证。

### 20.3 真机功能矩阵

| 场景 | 必验内容 |
|---|---|
| 启动/退出 | 正常启动、初始化错误、重复启动、shutdown 无 hang。 |
| 菜单 | 文本、图标、卡片、clip、dropdown、滚动、颜色。 |
| 游戏 | tile、quad、skin、particle、hook/laser、zoom。 |
| HUD | Media Island、scoreboard、chat、nameplate。 |
| 窗口 | resize、最小化、恢复、全屏、borderless、屏幕切换。 |
| HiDPI | 1x/2x 或实际可用 scale，drawable 与逻辑尺寸一致。 |
| 同步 | VSync on/off、帧率上限、焦点切换。 |
| 资源 | 换图、换皮肤、语言/字体变化、长时间游戏。 |
| 输出 | screenshot、read pixel、录制入口。 |
| 特效 | render target、blur、SDF、MSDF、FSAA。 |
| 恢复 | Metal 初始化失败、shader 缺失、模拟 command buffer error 后回退。 |

### 20.4 视觉对比

- 同一二进制、同一配置、同一地图、同一相机位置比较 OpenGL 与 Metal。
- 保存普通 DPI 和 HiDPI 截图。
- 检查像素方向、clip 边缘、alpha、字体、texture filtering 和 SDF 边缘。
- 允许抗锯齿造成的微小差异，但必须定义差异 mask/阈值，不能只凭肉眼说“一样”。

### 20.5 性能基线

每个固定场景至少记录：

- CPU frame time 分布。
- graphics command encode time。
- GPU frame time。
- drawable wait time。
- draw/encoder/command-buffer 数量。
- upload/staging/readback bytes。
- texture/buffer/stream/staging memory。
- VSync on/off 和 HiDPI 差异。

不能仅用“450 FPS 对 60 FPS”判断根因或后端优劣。显示同步、frame limiter、drawable wait、CPU encode 和 GPU workload 必须分开。

## 21. 风险与开放问题

| 编号 | 问题 | 当前处理 |
|---|---|---|
| R1 | 现有基类和类名带 GL，Metal 接入后命名失真。 | 首轮复用协议，后续机械重命名，不阻塞实现。 |
| R2 | `gfx_gl_*` 同时承担后端版本选择，不能自然表达 Metal。 | Metal 忽略该字段；设置页后续拆分后端和 GL 版本。 |
| R3 | `framebufferOnly=false` 可能影响呈现性能。 | 首轮保证 readback 正确；P5 用 capture 决定是否引入内部 present texture。 |
| R4 | Intel/AMD 与 Apple GPU memory model 不同。 | 首轮 correctness-first；发布包含 x86_64 时必须真机验证并在 P5 调整 storage mode。 |
| R5 | OpenGL/Vulkan shader 与显式 MSL 可能漂移。 | 共享参数布局、shader 清单和视觉金图；新增 shader 必须同时更新全部启用后端。 |
| R6 | Metal VSync off 仍可能受 compositor 限制。 | 报告属性设置成功与实测呈现行为，禁止等同描述。 |
| R7 | 异步 command buffer error 到达现有 error container 有线程边界。 | completion handler 只写线程安全 latch，processor 边界消费。 |
| R8 | 当前 Vulkan 支持多 render thread，Metal 首轮单线程。 | 不继承并发复杂度；只有 profile 证明 CPU encode 瓶颈才评估并行编码。 |
| R9 | macOS 当前部署目标和发布架构可能继续变化。 | 使用仓库当前 CMake 值，不在 Metal 模块硬编码 OS/架构。 |

## 22. 文件影响面

预计涉及：

```text
CMakeLists.txt
src/engine/graphics.h
src/engine/client/backend_sdl.h
src/engine/client/backend_sdl.cpp
src/engine/client/backend/metal/*
src/engine/client/graphics_threaded.h
src/engine/client/graphics_threaded.cpp
src/engine/shared/config_variables.h
src/engine/client/client.cpp
src/game/client/components/menus_settings.cpp
data/shader/metal/*
src/test/*metal*
docs/superpowers/plans/*
```

`graphics_threaded.*` 只允许为通用 capability、diagnostics 或现有命令合同补充必要接口，禁止把 Metal API 调用放入上层。

## 23. 官方参考文档

### 23.1 SDL2

- [SDL_Metal_CreateView](https://wiki.libsdl.org/SDL2/SDL_Metal_CreateView)：创建 CAMetalLayer-backed view；macOS 上应用负责设置 device。
- [SDL_Metal_GetLayer](https://wiki.libsdl.org/SDL2/SDL_Metal_GetLayer)：取得底层 CAMetalLayer。
- [SDL_Metal_DestroyView](https://wiki.libsdl.org/SDL2/SDL_Metal_DestroyView)：Metal view 的销毁顺序。
- [SDL_Metal_GetDrawableSize](https://wiki.libsdl.org/SDL2/SDL_Metal_GetDrawableSize)：取得 drawable 像素尺寸。

### 23.2 Apple 窗口与呈现

- [CAMetalLayer](https://developer.apple.com/documentation/quartzcore/cametallayer)
- [CAMetalLayer.nextDrawable](https://developer.apple.com/documentation/quartzcore/cametallayer/nextdrawable%28%29)
- [CAMetalLayer.device](https://developer.apple.com/documentation/quartzcore/cametallayer/device)
- [CAMetalLayer.drawableSize](https://developer.apple.com/documentation/quartzcore/cametallayer/drawablesize)
- [CAMetalLayer.maximumDrawableCount](https://developer.apple.com/documentation/quartzcore/cametallayer/maximumdrawablecount)
- [CAMetalLayer.displaySyncEnabled](https://developer.apple.com/documentation/quartzcore/cametallayer/displaysyncenabled)
- [CAMetalLayer.framebufferOnly](https://developer.apple.com/documentation/quartzcore/cametallayer/framebufferonly)
- [CAMetalLayer.allowsNextDrawableTimeout](https://developer.apple.com/documentation/quartzcore/cametallayer/allowsnextdrawabletimeout)
- [MTLCommandBuffer.present](https://developer.apple.com/documentation/metal/mtlcommandbuffer/present%28_%3A%29)
- [Metal Best Practices: Drawables](https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/Drawables.html)

### 23.3 命令、线程与同步

- [MTLCreateSystemDefaultDevice](https://developer.apple.com/documentation/metal/1433401-mtlcreatesystemdefaultdevice)
- [MTLCopyAllDevices](https://developer.apple.com/documentation/metal/1433367-mtlcopyalldevices)
- [Setting up a command structure](https://developer.apple.com/documentation/metal/setting-up-a-command-structure)
- [MTLCommandQueue](https://developer.apple.com/documentation/metal/mtlcommandqueue)
- [Synchronizing CPU and GPU work](https://developer.apple.com/documentation/metal/synchronizing-cpu-and-gpu-work)
- [Metal Best Practices: Command Buffers](https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/CommandBuffers.html)
- [Metal Best Practices: Triple Buffering](https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/TripleBuffering.html)
- [Command Organization and Execution Model](https://developer.apple.com/library/archive/documentation/Miscellaneous/Conceptual/MetalProgrammingGuide/Cmd-Submiss/Cmd-Submiss.html)

### 23.4 资源与 readback

- [Setting resource storage modes](https://developer.apple.com/documentation/metal/setting-resource-storage-modes)
- [Choosing a resource storage mode for Apple GPUs](https://developer.apple.com/documentation/metal/choosing-a-resource-storage-mode-for-apple-gpus)
- [Choosing a resource storage mode for Intel and AMD GPUs](https://developer.apple.com/documentation/metal/choosing-a-resource-storage-mode-for-intel-and-amd-gpus)
- [Synchronizing a managed resource in macOS](https://developer.apple.com/documentation/metal/synchronizing-a-managed-resource-in-macos)
- [Metal Best Practices: Resource Options](https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/ResourceOptions.html)
- [Metal Best Practices: Persistent Objects](https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/PersistentObjects.html)
- [Metal Best Practices: Buffer Bindings](https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/BufferBindings.html)
- [Reading pixel data from a drawable texture](https://developer.apple.com/documentation/metal/reading-pixel-data-from-a-drawable-texture)
- [MTLBlitCommandEncoder texture-to-buffer copy](https://developer.apple.com/documentation/metal/mtlblitcommandencoder/copy%28from%3Asourceslice%3Asourcelevel%3Asourceorigin%3Asourcesize%3Ato%3Adestinationoffset%3Adestinationbytesperrow%3Adestinationbytesperimage%3Aoptions%3A%29)

### 23.5 Pipeline、render pass 与 MSAA

- [Pipeline state creation](https://developer.apple.com/documentation/metal/pipeline-state-creation)
- [MTLRenderPipelineDescriptor](https://developer.apple.com/documentation/metal/mtlrenderpipelinedescriptor)
- [Render passes](https://developer.apple.com/documentation/metal/render-passes)
- [Customizing render pass setup](https://developer.apple.com/documentation/metal/customizing-render-pass-setup)
- [Metal Best Practices: Pipelines](https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/Pipelines.html)
- [Metal Best Practices: Load and Store Actions](https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/LoadandStoreActions.html)
- [Improving edge-rendering quality with MSAA](https://developer.apple.com/documentation/metal/improving-edge-rendering-quality-with-multisample-antialiasing-msaa)
- [MTLStoreAction.multisampleResolve](https://developer.apple.com/documentation/metal/mtlstoreaction/multisampleresolve)
- [MTLDevice.supportsTextureSampleCount](https://developer.apple.com/documentation/metal/mtldevice/supportstexturesamplecount%28_%3A%29)

### 23.6 Shader 构建与诊断

- [Building a shader library by precompiling source files](https://developer.apple.com/documentation/metal/building-a-shader-library-by-precompiling-source-files)
- [MTLDevice.makeLibrary(filepath:)](https://developer.apple.com/documentation/metal/mtldevice/makelibrary%28filepath%3A%29)
- [Capturing Metal commands programmatically](https://developer.apple.com/documentation/metal/capturing-metal-commands-programmatically)
- [MTLCaptureManager](https://developer.apple.com/documentation/metal/mtlcapturemanager)
- [GPU counters and counter sample buffers](https://developer.apple.com/documentation/metal/gpu-counters-and-counter-sample-buffers)
- [Improving your game's graphics performance and settings](https://developer.apple.com/documentation/metal/improving-your-games-graphics-performance-and-settings)

### 23.7 CMake

- [enable_language](https://cmake.org/cmake/help/latest/command/enable_language.html)：CMake 对 `OBJCXX` 的正式语言支持。
- [CMAKE_OSX_DEPLOYMENT_TARGET](https://cmake.org/cmake/help/latest/variable/CMAKE_OSX_DEPLOYMENT_TARGET.html)：保持项目统一 macOS 部署目标。

## 24. 完成定义

本文只有在以下条件全部满足后才能从 `draft` 转为 `active`：

1. P0 探针证明当前 macOS SDK、CMake 和 bundle 路径可以稳定构建/加载 metallib。
2. 当前源码影响面经独立只读 review，无遗漏的后端枚举、fallback 或设置入口。
3. P1 任务计划列出失败测试、最小实现切片、每阶段 capability 和回滚点。
4. Apple Silicon 基线设备、系统、GPU、OpenGL 固定场景数据已写入计划。
5. 如果发布包含 Intel，Intel 设备验证责任和资源策略已明确。

Metal 实现只有完成 P1-P4 的对应验收后才能称为“完整 Metal 后端”；仅能创建窗口、清屏或显示菜单的原型不得作为完成版本发布。
