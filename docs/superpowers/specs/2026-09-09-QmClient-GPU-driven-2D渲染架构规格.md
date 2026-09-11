---
title: QmClient GPU-driven 2D 渲染架构规格
date: 2026-09-09
status: draft
---

# 1. 文档目的

本文定义 QmClient 利用现代 GPU 能力的长期渲染架构。目标是在不改动协议、预测、物理、地图格式、demo 格式和皮肤格式的前提下，逐步把现有 2D 渲染路径演化为 GPU-driven 2D，并保持 Windows/Linux 的 Vulkan、macOS 的 Metal、iOS 的 Metal/OpenGLES、Android 的 Vulkan/OpenGLES、旧 OpenGL 以及 headless/null 路径可用。当前 iOS 尚未完成 QmClient 本身的整体适配，iOS Metal 后端接入也未完成，因此这部分仍是目标状态，不是当前可交付能力。

本文是架构规格，不代表功能已经实现。实现应按阶段门推进；在前一阶段的正确性和验证证据未完成前，不得开启后一阶段的默认能力。

# 2. 当前代码证据

当前渲染系统已经具备可复用的基础，不需要从游戏层重写 renderer：

- `src/engine/client/graphics_threaded.h` 定义了独立命令缓冲、数据缓冲、渲染线程和 `CMD_*` 命令。
- `CGraphics_Threaded` 已有 `RenderTileLayer`、`RenderQuadContainer`、`RenderQuadContainerAsSpriteMultiple`、文本、render target、Gaussian blur 和 readback 命令。
- Vulkan 后端已经有 frame buffer、延迟释放、buffer object/container、批处理和执行缓冲路径。
- Metal 后端的现有实现和合同主要覆盖已接入的平台路径；不能据此推断 iOS 的 QmClient 窗口、生命周期、输入和资源适配已经完成。
- `IGraphicsBackend` 已通过 `HasTileBuffering()`、`HasQuadBuffering()`、`HasTextBuffering()` 等 capability 向前端发布后端能力。
- `src/engine/client/backend/graphics_backend_contract.*` 已提供 backend identity、编译能力和安全回退的公共合同。
- `qm_graphics_mode` 已提供面向用户的“兼容模式/性能模式”选择；模式只表达平台偏好，实际后端仍由编译能力、驱动和初始化结果决定。
- 新安装默认使用性能模式；`-1` 仍保留为旧配置的兼容值，不作为新配置默认值。已有用户的保存配置不应被静默改写。iOS 尚未上线，Metal/iOS 适配完成后再进行专项默认策略评估。
- 旧版本升级通过 `cl_config_version < 5` 迁移：根据原有 `gfx_backend` 推导性能/兼容模式，再保存新配置；不会把旧用户手动选择的 OpenGL/GLES 静默切换到 Vulkan/Metal。
- Vulkan 版本选择已从用户可见的 API 下拉框改为内部自动协商：默认按 Vulkan 1.4、1.3、1.1 依次尝试，初始化失败时继续降级；1.3 配置按 1.3、1.1 尝试，1.1 配置只请求 1.1。
- GPU 信息页显示实际运行中的 backend 和 API 版本；配置的首选后端或首选版本不能被当作实际运行结果。
- `data/shader/vulkan/` 已包含 tile、textured MSDF、rounded-rect SDF、Gaussian blur 等现代 shader；`data/shader/metal/qmclient.metal` 已作为 Metal shader 源。

当前审查记录还发现 Vulkan/Metal 的 render pass 兼容性、命令重录后的状态缓存、Metal 离屏目标跨批次恢复、buffer 原地更新和 readback 生命周期问题。这些问题属于本架构的前置约束，不能被 GPU-driven 改造掩盖。

# 3. 目标与非目标

## 3.1 目标

1. 减少每帧 CPU 命令提交、状态切换和资源同步。
2. 让 tile、sprite、quad、text 和 UI SDF 使用统一的中立 batch/packet 描述。
3. 在支持的 GPU 上使用 persistent upload ring、instancing、indirect draw、compute culling 和异步 readback。
4. 让 Vulkan 1.4 成为现代能力的优先目标，并按 1.4 -> 1.3 -> 1.1 自动回退；Metal 使用平台原生能力。
5. 保留旧 draw 路径作为 capability fallback，低端 GPU、旧驱动、OpenGL/GLES 和 SDL GPU 不完整能力不应导致功能缺失。
6. 允许未来接入官方 DDNet 的 SDL3 平台迁移，以及可选的 SDL GPU 后端，而不让上层渲染依赖 SDL/Vulkan/Metal 类型。

## 3.2 非目标

- 不以 ray tracing、mesh shader、神经网络超分辨率作为 DDNet 2D 渲染的首要目标。
- 不在本规格中重写游戏组件、地图数据、网络协议、预测、物理、demo、skin 或存档格式。
- 不把 `SDL_GPUDevice` 直接提升为 QmClient 的公共渲染合同。
- 不在没有 profiling 证据时保证 FPS 提升；每个优化阶段必须提供 CPU submit、GPU duration、同步等待和 draw/dispatch 数量证据。

# 4. 目标分层架构

```text
game / map / QmUi
        |
        v
IGraphics 兼容接口
        |
        v
CGraphics_Threaded
  - command buffer
  - GPU 2D packet builder
  - frame lifetime
  - capability fallback
        |
        v
GPU 2D backend contract
  - resources
  - passes
  - barriers
  - draw/dispatch
        |
  +-----+---------+----------+
  |               |          |
Vulkan 1.4/1.3/1.1  Metal     OpenGL/GLES
  |               |          |
  +--------- optional SDL_GPU adapter
```

上层只能依赖中立句柄和 packet：

```cpp
struct SGpu2DTextureHandle;
struct SGpu2DBufferHandle;
struct SGpu2DRenderTargetHandle;

struct SGpu2DDrawItem
{
	SGpu2DTextureHandle m_Texture;
	SGpu2DBufferHandle m_Geometry;
	uint32_t m_First;
	uint32_t m_Count;
	uint32_t m_SortKey;
};
```

实际类型名称可以按当前 DDNet 风格调整，但公共层不得暴露 `Vk*`、`MTL*` 或 `SDL_GPU*` 类型。后端负责把中立句柄映射到 API 资源。

# 5. 命令模型

现有 `CCommandBuffer::SCommand_*` 不应一次删除。新增一个可选命令，例如 `CMD_GPU2D_SUBMIT_BATCH`，携带 command/data arena 中的 packet offset、resource handle、pass id、clip/scissor、material/pipeline key 和 instance range。

旧命令继续作为 fallback：

```text
现有 IGraphics 调用
  -> 兼容命令
  -> GPU 2D packet（能力允许时）
  -> legacy backend draw（能力不允许时）
```

packet 的排序必须遵守 DDNet 的视觉语义：layer/group 顺序、透明绘制顺序、clip 区域、文本顺序和 render target 边界不能因批处理而改变。只有标记为可重排的对象才能交给 compute culling 或 GPU 排序。

# 6. 首批 GPU-driven 路径

## 6.1 Tile layer

现有 `RenderTileLayer` 已经带有 buffer container、index offsets 和 draw counts，适合转为 tile instance buffer 加 indirect draw。透明层和需要严格顺序的 tile 必须保留 CPU/固定顺序 fallback；不得为了 GPU culling 破坏前景层和透明层语义。

## 6.2 Sprite multiple

`RenderQuadContainerAsSpriteMultiple` 已经是批量接口，应优先转为 instance buffer。实例数据至少包括位置、缩放、旋转、颜色和纹理区域。该路径适用于 Tee、粒子和其他可批量对象。

## 6.3 Text 与 SDF UI

保留 CPU 侧文字布局、换行、字形选择和 atlas 管理；GPU 侧统一 MSDF、rounded-rect SDF、media-island SDF 的 material/pipeline key，减少 shader、纹理和 descriptor 切换。不得在第一阶段把文字布局搬到 compute。

## 6.3.1 文本 CPU 成本的实测边界（2026-09-12）

实测（`docs/superpowers/plans/2026-09-09-Windows图形掉帧撕裂与连接中断调查.md` §13–§18）表明，文本路径的可感知卡顿全部来自 CPU 侧：FreeType 字形光栅化（单帧 30–44 ms / 108–210 字形）与 plan 收集布局（单帧 1697 容器创建 / 12.7 ms）；GPU 上传占比不足 1%。这些成本已通过“时机优化”在打开场景消除——字形缺失记录 + 空闲帧预热 + 跨会话持久化（`qmclient/glyph_prewarm.txt`）、ESC 文本 plan 收集提前到菜单关闭时的空闲帧——不依赖本规格的 GPU-driven 改造。

对本规格的约束：

- GPU-driven 批处理不减少 FreeType 光栅化与布局；文本 CPU 成本需按“光栅化 / 布局 / 上传 / 帧时间”四个预算域独立治理，FreeType 工作线程化是独立立项（需先设计 FT_Face 生命周期与字形发布协议，不能直接把现有 FT_Face 调用塞进后台线程）。
- 字形图集扩容（4096→8192）会触发全量重传，属极端场景（4096 图集约容纳 1.6 万字形）；若未来字形规模显著增长，多页图集应先于 GPU-driven 改造落地。
- §5 的文本绘制顺序与 clip 语义在文本批处理时同样适用。

# 7. Render graph

现有 `BeginRenderTarget`、`EndRenderTarget`、`CaptureBackbufferToRenderTarget`、`GaussianBlurRenderTarget` 和 readback API 已经构成 render graph 的输入来源。新增的内部 render graph 只负责：

- pass 顺序和 render target 读写关系；
- load/store、layout 和 barrier；
- 主画面、离屏小窗、背景捕获、blur 和 readback 的生命周期；
- 命令批次切分后的活动目标恢复；
- 资源延迟释放和跨帧引用。

公共 `IGraphics` API 可以保持兼容，由 `CGraphics_Threaded` 把调用转换为 graph node。不得先强制所有游戏组件直接使用 render graph。

# 8. Capability 合同

现有 `m_GLTileBufferingEnabled` 等命名应逐步迁移为中立 capability。建议新增：

```text
HasGpu2DBatching
HasPersistentUploadRing
HasIndirectDraw
HasComputeCulling
HasDescriptorIndexing
HasAsyncReadback
HasRenderGraph
```

每项能力必须同时具备：初始化探测、失败回退、运行时日志、对应 backend 实现和 focused test。Vulkan 1.4 的能力不可假定存在；应按版本、扩展、格式、队列和实际资源限制逐项探测。Vulkan 1.3 和 1.1 设备继续走兼容分支；版本协商本身也必须遵守 `1.4 -> 1.3 -> 1.1` 的降级顺序。

当前用户配置边界：普通设置只暴露“兼容模式”和“性能模式”，不要求用户选择 Vulkan 1.1/1.3/1.4 或 OpenGL 版本。`qm_vulkan_api_version` 仅作为内部/兼容配置保留，用于表达最高优先级和测试场景；运行时必须以实际创建成功的 Vulkan instance/device 版本为准。

# 9. SDL3 兼容边界

官方 DDNet 的 SDL3 工作目前属于 SDL2 到 SDL3 的平台层迁移，涉及 SDL 符号、窗口、输入、音频、CMake 和图形初始化；它不等于 QmClient 必须使用 SDL GPU API。参考：[DDNet SDL3 PR #12558](https://github.com/ddnet/ddnet/pull/12558)。

因此：

- SDL2/SDL3 差异集中在 `backend_sdl.*`、窗口、事件、surface/context、resize、HiDPI 和 fullscreen 生命周期。
- Vulkan 和 Metal backend 可以继续直接使用原生 API。
- SDL GPU 只能作为未来的可选 backend adapter。
- `src/game`、`QmUi`、`IGraphics` 公共合同不得包含 SDL/Vulkan/Metal 类型。
- 不能用 SDL GPU 的共同最低能力限制 Vulkan/Metal 的 capability 集合。

未来允许以下并存关系：

```text
SDL3 platform + native Vulkan
SDL3 platform + native Metal
SDL3 platform + SDL_GPU
```

# 10. 实施阶段与阶段门

## P0：后端正确性

修复并测试已有 Vulkan/Metal/OpenGL/GLES render target、状态恢复、buffer 生命周期和 readback 问题；另需完成 iOS 的窗口、SDL/UIKit 生命周期、输入、资源和 Metal/OpenGLES 选择适配。阶段门：validation layer、Metal validation/GPU capture、iOS 真机启动与离屏读回、现有合同测试和离屏读回场景均通过。

## P1：中立 packet 与 capability

新增 packet 合同和 API-neutral capability；旧命令保持可用。阶段门：OpenGL、Vulkan、Metal、null/headless 均能编译，未支持能力回退到旧 draw。

## P2：persistent upload 与批次

先实现 frame-local instance/upload ring、tile/sprite/quad/text batch 合并和 GPU timestamp。阶段门：同场景对比 CPU submit、GPU 时间、draw count、同步等待和视觉结果。

## P3：indirect draw

优先接入 sprite multiple 和可安全合并的 tile 路径。阶段门：排序、透明层、clip、render target 和 fallback 场景通过。

## P4：render graph 与异步资源

把离屏、blur、backbuffer capture 和 readback 接入 graph；禁止隐式 GPU idle 作为常规路径。阶段门：跨批次、resize、MSAA、readback 和设备丢失/初始化失败路径通过。

## P5：compute culling 与可选 SDL_GPU adapter

仅对可重排对象启用 compute culling；根据官方 SDL3 迁移状态和实测能力决定是否实现 SDL_GPU adapter。阶段门：多 GPU、旧驱动、低端设备、Vulkan 1.3 fallback、Metal 和 SDL3 组合验证。

# 11. 必须保留的 fallback

- OpenGL/GLES：保持现有 draw 路径，不要求 GPU-driven 能力。
- Vulkan 1.3/1.1：使用不依赖 Vulkan 1.4 的 packet/indirect 子集；初始化按配置的最高优先级自动回退到可用版本。
- Metal：按 Apple GPU family 和资源限制发布 capability，不因原生 Metal 存在就默认开启所有能力。
- null/headless：保持命令合同和测试可用，不创建 GPU 资源。
- 任意 backend 初始化失败：清空本次 capability，回退到安全 backend 或失败路径，不复用上一次初始化残留。
- 性能模式只表示首选后端：Windows/Linux 首选 Vulkan、macOS/iOS 首选 Metal、Android 首选 Vulkan；iOS 的 Metal 选择必须等 QmClient/iOS 适配完成后再启用。兼容模式首选 OpenGL（移动端为 OpenGLES）。首选后端初始化失败时，必须回退到该平台的安全兼容后端，并在诊断信息中区分“配置首选”与“实际运行”。

# 12. 验证与性能证据

代码实现按共享接口、生命周期和线程风险选择验证：

- `GraphicsBackendContract.*`、render target、Metal 状态合同测试；
- Vulkan validation layer 和 RenderDoc capture；
- Metal validation、GPU capture 和实际 Apple 设备；
- Windows/Linux/macOS 的同场景截图与读回；
- CPU submit、GPU duration、draw/dispatch、descriptor、upload、readback 和 wait 统计；
- Vulkan 1.1/1.3/1.4、集成显卡/独显、AMD/NVIDIA/Intel、OpenGL/GLES fallback；
- SDL2/SDL3 平台层与 native Vulkan/Metal 的组合测试。

未执行实机、GPU capture 或跨厂商矩阵验证时，只能称为静态设计或编译验证，不能宣称性能提升或跨平台完成。

# 13. 决策结论

QmClient 应采用“中立 GPU 2D 前端 + 原生 Vulkan/Metal backend + 可选 SDL_GPU adapter”的路线。现代能力的主要落点是 persistent upload、批次合并、indirect draw、render graph、异步 readback 和有限范围的 compute culling，而不是 ray tracing 或强制迁移到 SDL GPU API。

该方案可以与官方 DDNet SDL3 迁移并存：SDL3 负责平台层，QmClient 的 packet、render graph 和 capability 合同保持独立。只有在官方 SDL3 路径稳定、SDL GPU 的实际能力和调试成本经过验证后，才决定是否把 SDL_GPU adapter 作为默认 backend。

# 14. 当前 Windows 图形回归阻断项

2026-09-09 在 NVIDIA 620.02 和 Windows 11 Insider build 29648.1000 上观察到 Vulkan 严重撕裂和卡顿、OpenGL 也存在掉帧，以及偶发画面停滞后连接中断。当前 Vulkan 关闭 VSync 时优先使用 `IMMEDIATE`，本机还存在 GamePP、OBS、Steam、ReShade 和 NVIDIA 等图形注入/implicit layer；历史 OpenGL 崩溃实际落在 `nvoglv64.dll`。

该问题必须先完成无注入层 A/B、Vulkan 1.1/1.3/1.4、FIFO/MAILBOX/IMMEDIATE、graphics-thread backpressure、acquire/present/fence 和 network pump 时间线调查。详细入口见 [Windows 图形掉帧撕裂与连接中断调查](../plans/2026-09-09-Windows图形掉帧撕裂与连接中断调查.md)。在诊断闭环前，P2 之后的 GPU-driven 运行时能力不得默认启用。

用户进一步确认官方 DDNet、BC 客户端和 QmClient 都能在同一台机器复现，其他机器暂无同样反馈。因此 present mode 调整只能作为显示策略实验，不能作为主修复；P0 必须优先建立跨客户端的机器层、present 层和网络层统一证据。
