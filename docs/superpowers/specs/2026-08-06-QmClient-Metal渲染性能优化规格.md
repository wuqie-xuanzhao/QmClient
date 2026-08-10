---
type: performance-specification
date: 2026-08-06
status: draft
parent_spec: /docs/superpowers/specs/2026-08-04-QmClient-macOS-Metal原生渲染后端规格.md
scope:
  - Vulkan 近期渲染性能优化
  - macOS Metal 后端批处理、资源绑定和提交策略
  - Minecraft Multi-Draw Indirect 思路在 DDNet/QmClient 的适用边界
  - DDNet 官方上游性能提交筛选
---

# QmClient Vulkan 与 Metal 渲染性能优化规格

## 1. 结论

近期性能工作以 Vulkan 为主，Metal 在基础后端正确可用后复用同一套测量和批处理原则。OpenGL 只修正后端身份与实际 context 版本显示，不作为主要性能投入方向。

Minecraft 的 Multi-Draw Indirect（MDI）说明了一个通用原则：当 CPU 每帧需要提交大量相似 draw 时，应减少逐条提交次数。但 DDNet 是二维渲染器，不能因为 Minecraft 使用 MDI 就直接给所有命令增加 indirect buffer。

QmClient 的执行顺序固定为：

1. 不画屏幕外内容。
2. 合并连续索引范围。
3. 减少文字重建、descriptor 更新、临时分配和强制等待。
4. 只有 profile 证明 CPU draw encoding 是瓶颈时，才启用 Vulkan indirect draw 或 Metal indirect command buffer。

## 2. 当前事实

### 2.1 OpenGL 版本显示

截至 2026-08-07，此问题已在本地工作树改为运行时探测：`gfx_gl_major/minor=0.0` 表示自动模式，启动时从后端和平台支持上限开始尝试创建 context，失败后逐级降低；桌面 OpenGL 的探测上限为 Windows/Linux `4.6`、macOS `4.1`，Android GLES 为 `3.0`，这些值只限制第一次尝试，不作为设置项显示结果。成功后按 `glGetString(GL_VERSION)`（或 GLES 对应版本字符串）解析，并把真实版本保存在 `SBackendCapabilities::m_DetectedContextMajor/m_DetectedContextMinor/m_DetectedContextPatch`。能力检查使用的 `m_ContextMajor/m_ContextMinor/m_ContextPatch` 允许因 blocklist 或兼容性降级而变化，但设置项文本只使用不可变的 detected 字段。

这解决了 macOS 设置页把 modern OpenGL 固定显示成 `3.3` 的问题，并让不同系统使用各自实际可创建的 context。当前后端选项文本的含义是：活动的 OpenGL/GLES 后端显示实际探测到的版本；尚未初始化或非活动后端显示“自动探测”，而不是伪造一个平台版本。后续若增加独立图形诊断页，仍应分别显示：

- 配置项：用户请求的 OpenGL profile/version。
- 运行信息：驱动实际返回的 context version、vendor 和 renderer。

这属于后端身份与诊断修复，不应和 Vulkan/Metal 性能优化混成一个补丁。

### 2.2 Tile layer 已有合并基础

当前 `CRenderLayerTile::RenderTileLayer()` 会复用 draw 数组，并在相邻行的 index range 连续时合并 draw count。Vulkan 后端也会再次合并连续区间。

这已经覆盖 DDNet 上游“完整可见宽度时把多行 tile 合成一次 draw”的核心收益。不得直接 cherry-pick 上游实现覆盖 QmClient 当前更通用的合并逻辑，只需增加统计和回归测试确认实际 draw 数。

### 2.3 Vulkan 是近期主路径

Vulkan 已有 command buffer、stream buffer、descriptor、pipeline 和帧统计基础，适合先验证：

- draw-call 合并是否减少 CPU render time；
- descriptor allocation/update 是否为热项；
- MoltenVK 是否仍被每帧 `WaitForIdle()` 串行化；
- 非连续 tile ranges 是否多到值得使用 `vkCmdDrawIndexedIndirect`。

### 2.4 Metal 的阶段边界

Metal P1/P2 先完成正确的 direct draw、三帧资源 arena、pipeline cache、present/readback 和错误恢复。Indirect command buffer 不是最小后端的依赖，必须留到性能阶段。

## 3. 真正值得优先做的优化

### P0：先建立证据

固定记录：

- CPU frame time 与 render-thread time；
- GPU frame time；
- 每帧 render command、实际 draw、pipeline bind、descriptor bind/update 数；
- tile command 的 range 数、连续合并前后 draw 数；
- stream buffer 上传字节和临时 allocation 数；
- `WaitForIdle`、drawable wait、GPU completion wait 的次数和耗时。

固定场景至少包括：普通地图、缩放很远的大地图、大量 projectile/laser/pickup、64 人 scoreboard、设置页滚动、开启 blur/MSAA 的 Qm UI。

### P1：减少根本不需要的工作

1. 移植上游屏外 entity、player、hook、ghost 和 spectator culling。
2. 保留并测试现有 tile 连续 range 合并。
3. 延迟 ellipsis 测量，避免短文本重复 shaping/measurement。
4. 对 scoreboard 等跨帧稳定文本使用可失效的 text container cache。
5. 把 MoltenVK frame-serialization workaround 改为明确 capability，原生 Metal 不得进入该等待路径。

这组优化通常比 MDI 更先见效，因为它直接减少 draw、文字构建和等待，而不是只改变 draw 的提交方式。

### P2：Vulkan 提交与资源优化

1. 优先合并相同 pipeline、texture、blend、clip 和 render target 下的命令。
2. 减少每帧 descriptor set allocation/update；优先复用 frame-local pool 和连续 uniform/instance 数据。
3. 对 tile range 采用三级策略：
   - 连续：合成一个 `vkCmdDrawIndexed`；
   - 少量不连续：保留多个 direct draw；
   - 大量不连续且 CPU encoding 已被证明为瓶颈：写入复用的 indirect arena，调用 `vkCmdDrawIndexedIndirect`。
4. indirect command 数据必须使用现有 frame-in-flight 生命周期，禁止每个 tile command 单独创建 buffer 或等待上传完成。
5. MoltenVK 必须单独 A/B；原生 Vulkan 的收益不能直接推定到 macOS。

### P3：Metal 性能路径

Metal direct draw 基线必须先满足：

- 正常帧一个主 `MTLCommandBuffer`；
- 同一 render target/pass 尽量保持一个 `MTLRenderCommandEncoder`；
- 三帧 ring/arena 管理动态 vertex、index、uniform 和 readback buffer；
- pipeline、depth-stencil 和 sampler 对象初始化或缓存，不在 draw 热路径创建；
- drawable 晚获取、早 present，不在 present 前等待 GPU；
- 相同 texture/pipeline/scissor/blend 的命令连续编码。

只有同时满足下列条件时才评估 `MTLIndirectCommandBuffer`：

1. tile/entity draw 数在目标场景持续较高；
2. CPU command encoding 明显限制帧率；
3. draw 参数可以跨帧复用，或批量写入 frame-local arena；
4. direct draw 与 indirect draw 有同场景 A/B 数据；
5. 支持设备和系统版本有明确 capability/fallback。

首轮不做 GPU culling、argument buffer 全面改造、parallel encoder、mesh shader 或 Metal 4 专用路径。

## 4. Vulkan 与 Metal 的共同批处理合同

上层仍提交当前 `CCommandBuffer`，不为了 indirect draw 重写游戏/UI 渲染 API。后端内部可以把兼容命令整理为批次：

```text
相同 render target + pipeline + texture + sampler + blend + clip
        |
        |-- 连续 index range -> 单次 direct draw
        |-- 少量离散 range -> 多次 direct draw
        `-- 大量离散 range且CPU受限 -> backend-specific indirect batch
```

对应实现：

| 后端 | 批处理方式 |
|---|---|
| Vulkan | `vkCmdDrawIndexed` / `vkCmdDrawIndexedIndirect` |
| Metal | direct indexed draw / `MTLIndirectCommandBuffer` |
| OpenGL | 仅保留兼容与身份修复，不作为主要新优化路径 |

阈值不得硬编码为拍脑袋常量。先记录 range-count 分布，再按目标设备测量选择；低于阈值必须保留 direct draw，避免 indirect buffer 上传成本反而更高。

## 5. DDNet 官方上游提交结论

基线：2026-08-06 `ddnet-official/master`，最新检查提交 `82cb1cc30`。

执行状态：按当前范围，上游候选仅记录结论，全部暂缓；本轮不 cherry-pick、不人工移植任何上游提交。

| 上游提交 | 内容 | QmClient 结论 |
|---|---|---|
| `96deb8fa5` / PR `#12272` | 完整可见宽度的 tile rows 合成一次 draw | **不直接 pick。** 当前 QmClient 已有更通用的连续 range 合并；补统计和测试即可。 |
| `14fc1e9d1`、`c4cb37a35`、`ada53c8cb`、`fabf09a3a` | 屏外 projectile/laser/pickup/player/hook/ghost/spec char 不渲染，并修正 projectile 与 hookline 边界 | **高优先级人工移植，必须作为一组。** 单独取早期提交会带回已修复的裁剪错误。 |
| `6ab07501c` | 文本接近行宽时才测量 ellipsis | **高优先级候选。** 当前补丁可干净应用，但 `text.cpp` 有并行改动，实施时仍需独立任务和文字回归测试。 |
| `5b10b49e1` | scoreboard 文本容器跨帧缓存 | **中高优先级人工移植。** QmClient UI/text 生命周期已有扩展，不能盲目 pick。 |
| `86fed1482` / PR `#12441` | macOS named semaphore 创建后立即 unlink，避免泄漏 | **应人工移植。** 主要是正确性与长期稳定性，对 Vulkan/Metal worker 同样重要。 |
| `ede14dd9e` / PR `#12382` | 修复 Vulkan shader install | **已存在等价实现，无需 pick。** |
| `4299da8fb` / PR `#11853` | 修复 Vulkan heap 搜索未定义行为 | **已存在等价实现，无需 pick。** |
| `88c1f6580` | 修正 VSync 命令结果处理 | **已存在等价实现，无需 pick。** |
| `1882978bf` / PR `#12282` | OpenGL version string 解析越界修复 | **已存在等价实现。** 它不解决设置页固定显示 3.3 的问题。 |
| `e234b5e23` / PR `#12519` | 加速 Huffman 压缩 | **可选的非渲染性能任务。** 与 Vulkan/Metal 无关，单独评估和验证网络数据兼容。 |

上游跨度超过 1600 个提交，QmClient 又有大量 renderer/UI 定制，因此不建议按大范围 merge 或连续 cherry-pick 处理这些优化。应按上表拆成功能级人工移植任务。

## 6. 验收与停止条件

每项性能改动必须满足：

1. 固定场景前后数据可比，并报告 CPU/GPU 分开结果。
2. draw 数下降不等于性能已经提升；耗时没有脱离噪声就不保留复杂实现。
3. 无画面缺失、裁剪跳变、文字截断、截图/readback 或 VSync 回归。
4. Vulkan、MoltenVK 和 Metal 分开给结论，不互相代替。
5. indirect 路径若只在极端测试场景有效，保持 opt-in 或删除，不增加默认维护成本。

## 7. 推荐执行顺序

1. 修正 OpenGL 设置项版本与实际 context version 的显示语义。
2. 移植完整的上游屏外渲染裁剪提交组。
3. 移植 ellipsis 延迟测量。
4. 评估 scoreboard text container cache 与 QmClient 现有文本缓存的重叠。
5. 完成 Vulkan/MoltenVK 等待、draw、descriptor 和 upload 指标。
6. 只对测量确认的 Vulkan 热点实现 indirect batch 原型。
7. Metal P1/P2 正确性完成后复用相同指标，最后评估 ICB。

## 8. 外部依据

- Minecraft 26.3 Snapshot 6：<https://www.minecraft.net/en-us/article/minecraft-26-3-snapshot-6>
- Apple Metal indirect command buffers：<https://developer.apple.com/documentation/metal/mtlindirectcommandbuffer>
- Vulkan indexed indirect draw：<https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdDrawIndexedIndirect.html>
- DDNet tile draw 优化：<https://github.com/ddnet/ddnet/pull/12272>
- DDNet 屏外 entity 裁剪：<https://github.com/ddnet/ddnet/pull/12275>
- DDNet projectile 裁剪修正：<https://github.com/ddnet/ddnet/pull/12524>
- DDNet player/hook/ghost 裁剪：<https://github.com/ddnet/ddnet/pull/12527>
- DDNet hookline 裁剪：<https://github.com/ddnet/ddnet/pull/12529>
