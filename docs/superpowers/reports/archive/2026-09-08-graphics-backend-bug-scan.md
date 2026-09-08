# 图形后端 bug 扫描

日期：2026-09-08  
状态：静态审查结论，尚未修复、尚未进行 GPU 运行时复现。

范围：重点审查 Vulkan、Metal 的帧状态、资源生命周期、离屏目标、背景捕获、读回及直接调用链。后续补充检查 OpenGL/GLES 的目标切换、状态恢复、采样器、纹理更新和读回。交叉检查公共 graphics_threaded/backend_sdl 接口和现有相关测试；不代表所有图形后端的穷尽审计。行号以本次工作区版本为准。

## 1. 严重：公共异步读回请求扩容使后台输出地址悬空

位置：

- `src/engine/client/graphics_threaded.h:1009`：请求存储为 `std::vector<SRenderTargetReadbackRequest>`。
- `src/engine/client/graphics_threaded.cpp:869`：新请求通过 `emplace_back()` 增长容器。
- `src/engine/client/graphics_threaded.cpp:1212`：命令保存元素内部的 `&pRequest->m_Image`。
- `src/engine/client/backend_sdl.cpp:138`：提交等待前一批命令完成，随后异步执行本批命令。

触发：第一个 `BeginRenderTargetReadback()` 返回后，其后台命令尚未写完输出；再次申请读回，且请求 vector 需要扩容。第二次申请发生在第二次 KickCommandBuffer 的等待之前，不能由提交时的等待保护。

影响：第一个请求的 `m_pImage` 指向已经释放的 vector 存储，后端写入时产生 use-after-free；也可能与元素搬移并发。影响所有使用该公共接口的后端，包括 Vulkan、Metal。当前检索到的生产同步包装 `ReadRenderTarget()` 会等待完成；没有发现游戏组件连续调用异步接口，因此不将其描述为现有常规 UI 的必现崩溃。

最小修复：给请求使用稳定地址存储，例如 `std::vector<std::unique_ptr<SRenderTargetReadbackRequest>>`，保留现有索引与 generation 机制；不要仅 reserve 一个经验容量。

建议验证：使用可阻塞的测试后端挂起第一条读回，继续分配到触发容器增长，再释放后端；验证两个结果和完成信号，配合 ASan/线程检查。

## 2. 重要：Vulkan 恢复主画面的 load pass 与原 framebuffer/pipeline 不兼容

位置：

- `src/engine/client/backend/vulkan/backend_vulkan.cpp:5437`：仅当 `LoadAttachments` 为真时改变 subpass dependency 的 stage/access masks。
- 同文件 `:7391`、`:7393`：分别创建普通 pass 和 load pass。
- 同文件 `:5495`、`:5776`：交换链 framebuffer 和普通 pipeline 使用普通 pass 创建。
- 同文件 `:2654`、`:8658`、`:8893`：离屏/背景捕获后使用 load pass 恢复原 framebuffer。

触发：执行背景捕获、结束离屏目标，或帧中途提交后恢复主画面。

影响：两种 pass 不仅 loadOp/initialLayout 不同，dependency 也不同，不满足 Vulkan render pass compatibility 的要求。恢复时 framebuffer 不兼容，随后的普通管线绘制也不兼容；会触发验证层错误，实际显示结果属于未定义行为。

最小修复：让需要共享 framebuffer/pipeline 的普通 pass 与 load pass 使用相同的、足够保守的依赖定义；只改变规范允许的 load/store/layout 项。若要保留不同依赖，则分别建立兼容的 framebuffer 和 pipeline。

建议验证：Vulkan validation layers 下执行一次背景捕获和一次 Begin/EndRenderTarget，检查 `vkCmdBeginRenderPass` 与绘制命令的 render pass compatibility 错误。

## 3. 重要：Vulkan 普通绘制管线直接用于不兼容的离屏 pass

位置：

- `src/engine/client/backend/vulkan/backend_vulkan.cpp:5381`：离屏格式固定为 `VK_FORMAT_R8G8B8A8_UNORM`。
- 同文件 `:7395`：离屏 pass 强制单采样，且具有不同的外部依赖。
- 同文件 `:5681`、`:5776`：普通管线沿用交换链采样数与交换链 pass。
- 同文件 `:8620`：BeginRenderTarget 切换到离屏 pass；普通绘制继续取原来的管线。
- `src/game/client/components/hud.cpp:2133`、`:2140`：Vulkan 分身小窗创建并开始离屏绘制。
- `src/engine/client/graphics_threaded.cpp:783`：普通 RenderTarget 支持检查没有按 MSAA 禁用。

触发：Vulkan 分身小窗等离屏目标里执行普通矩形、地图、人物、文字绘制。交换链为 BGRA 时颜色格式不同；启用 MSAA 时采样数也不同。即使交换链恰为 RGBA 单采样，两种 pass 的 dependency 定义仍不同。

影响：普通管线与当前离屏 render pass 不兼容；启用 MSAA 时还违反当前颜色附件的采样数约束。可能出现小窗缺失、错误画面或验证层错误。专门的 Gaussian blur pipeline 已绑定离屏 pass，但这不能保护其他绘制。

最小修复：普通绘制管线应按实际目标的 render pass/格式/采样数选择兼容变体，或统一兼容条件；仅修复 blur pipeline 或仅禁用 blur 的 MSAA 不够。

建议验证：BGRA 单采样、RGBA 单采样及 MSAA 三种配置下，在小窗中绘制普通图形、tile、人物和文字。

## 4. 重要：Vulkan 帧中途重录命令缓冲后未清空绑定缓存

位置：

- `src/engine/client/backend/vulkan/backend_vulkan.cpp:2740`：重置并重新 begin 主命令缓冲。
- 同文件 `:2746`：仅恢复 swap pass。
- 同文件 `:2642`：BeginSwapRenderPass 只清理 `m_vLastPipeline`。
- 同文件 `:7999`、`:8020`、`:8040`：vertex/index/descriptor 绑定依据 CPU 缓存跳过。
- 同文件 `:4092`：viewport/scissor 也依据缓存跳过。

触发：单线程渲染，或本帧已经切到主命令缓冲直接录制；先绘制，再因销毁非空 RenderTarget 或读回调用 `SubmitCurrentCommandsAndRestartSwapPass()`，之后继续绘制。前后相同的 index buffer、descriptor 或动态状态会命中旧缓存。

影响：Vulkan 命令缓冲重录后不保留旧绑定，CPU 却误认为其仍有效；后续 draw 缺少 index buffer/descriptor/动态状态设置，产生无效绘制。普通 BeginRenderPass 不负责恢复这些绑定。

最小修复：在重录开始时调用 `ResetDrawCommandState(0)`；检查其他主/次命令缓冲切换处是否也需要清空对应缓存。

建议验证：同一帧中绘制纹理四边形，读回或销毁一个已存在目标，再用相同纹理绘制；validation layers 检查未绑定资源和动态状态，截图确认第二次绘制。

## 5. 重要：Vulkan 静态管线切换后错误复用动态 viewport/scissor 缓存

位置：

- `src/engine/client/backend/vulkan/backend_vulkan.cpp:4077`：绑定新管线时未失效动态状态缓存。
- 同文件 `:4092`：只按数值相等判断是否省略动态命令。
- 同文件 `:5790`：只有动态裁剪变体声明 viewport/scissor 为动态状态。

触发：同一个命令缓冲内，先在裁剪矩形 A 中绘制，再关闭裁剪绘制，最后恢复相同矩形 A。中间静态 viewport/scissor 管线的绑定使原动态状态失效。

影响：最后一次绘制因 A 与缓存相同，跳过 `vkCmdSetViewport` / `vkCmdSetScissor`；违反动态状态生命周期要求，可能裁剪错误或产生验证错误。该问题不依赖离屏目标或帧中途提交。

最小修复：绑定包含静态 viewport/scissor 的管线时，清除 `m_HasDynamicViewportScissor`；或根据实际 pipeline 状态分别跟踪失效。

建议验证：固定单线程录制，执行“裁剪 A 绘制 → 无裁剪绘制 → 裁剪 A 绘制”，检查动态状态 VUID 和最终像素。

## 6. 重要：Metal 拆分命令批次时丢失活动离屏目标

位置：

- `src/engine/client/backend/metal/backend_metal.mm:4292`：每次 StartCommands 无条件重置 `m_RenderTargetState`。
- 同文件 `:4309`：EndCommands 提交当前 GPU 命令缓冲。
- 同文件 `:2556`、`:3553`：普通绘制和 Gaussian blur 依赖活动目标状态。
- `src/engine/client/graphics_threaded.h:1099`：命令容量不足自动 KickCommandBuffer，不补发 BeginRenderTarget。
- `src/engine/client/graphics_threaded.cpp:1169`：blur 的 Begin、绘制、End 分别加入命令缓冲。

触发：BeginRenderTarget 与对应绘制/End 之间因容量不足拆成两批。

影响：下一批普通绘制误写 backbuffer，EndRenderTarget 变成空操作；Gaussian blur 因没有活动目标返回失败，并在 Metal RunCommand 的 `:4618` 处转为渲染错误，可能进一步停止图形提交。现有 backbuffer continuation 仅保留主画面，不能恢复被清空的离屏目标。

最小修复：跨同一逻辑帧的命令分片保留目标状态，新的 encoder 以 Load 恢复目标；仅在显式 End 或真正的生命周期边界清理。

建议验证：强制在 Begin 与 GaussianBlurPass 之间、普通离屏绘制中间拆批，核对目标像素和主画面、错误状态。

## 7. 重要：Metal Shared buffer 原地更新改变前序绘制的数据

位置：

- `src/engine/client/backend/metal/backend_metal.mm:868`：判断在途引用时可排除当前尚未提交的录制。
- 同文件 `:896`：WaitForBufferIdle 同样排除当前录制。
- 同文件 `:1279`：相同尺寸的持久 buffer 可直接复用。
- 同文件 `:1136`：Shared buffer 更新直接 memcpy/memset 到 GPU 将读取的存储。
- `src/engine/client/graphics_threaded.cpp:3395`：重建 buffer 不强制结束当前命令批次。

触发：同批次执行 `Draw(B, old)`、`Update/Recreate(B, new)`、`Draw(B, new)`；B 使用 Shared 存储，且没有其他已提交帧引用阻止复用。小于等于 256 KiB 的持久 buffer 默认使用 Shared。

影响：第一次 draw 只记录 buffer 引用，没有快照旧数据；当前命令缓冲尚未提交时 CPU 已把内容改成 new，第一次 draw 也读取新数据。可能导致文字或其他复用几何内容错误。命令序列缺陷确定，但具体玩家场景的出现频率未验证。

最小修复：将当前录制引用纳入“禁止 CPU 原地覆盖”的判定；改用新 buffer 加延迟回收，或采用按 GPU 顺序执行的 staging blit。仅等待已提交帧不能修复同批次读旧值的语义。

建议验证：同一命令批次用一个小 Shared buffer 绘制两份不同几何，中间等尺寸重建或部分更新；读回确认第一次绘制仍为旧内容。

## 8. 重要：Metal 忽略自定义 viewport，分身小窗按全屏绘制

位置：

- `src/engine/client/backend/metal/backend_metal.mm:4416`：CMD_UPDATE_VIEWPORT 只调用 UpdateDrawableSize，不读取命令矩形。
- 同文件 `:2610`：唯一的 setViewport 使用整个附件尺寸。
- `src/game/client/components/hud.cpp:2051`：只有 Vulkan 小窗使用离屏目标。
- 同文件 `:2155`：Metal 小窗通过自定义 viewport 绘制，且此前禁用了裁剪。

触发：Metal 下开启有信号的分身小窗。

影响：小窗背景、地图与人物绘制被映射到整个 backbuffer，覆盖主画面，不受小窗矩形限制。恢复 viewport 的命令也被忽略。

最小修复：保存并应用命令中的 X/Y/W/H，既更新当前 encoder，也在 encoder 重建时恢复；离屏切换需保存和恢复主画面的 viewport。

建议验证：主画面保持静止，开启小窗，移动和缩放小窗，确认窗口外像素不被改写；增加非全屏 viewport 的 GPU 读回测试。

## 9. 重要：小窗调用方仍按底部原点传入 viewport Y

位置：

- `src/game/client/components/hud.cpp:2115`：`(m_Height - (InnerY + InnerH)) * YScale`。
- `src/engine/client/graphics_threaded.h:623`：明确规定 viewport 以 drawable 左上角为原点。
- `src/engine/client/backend/opengl/backend_opengl.cpp:44`：后端再次执行 OpenGL Y 转换。

触发：直接 viewport 小窗位于非垂直居中位置；当前 OpenGL/GLES 路径可达。Vulkan 使用离屏路径不受这个调用点影响；Metal 当前忽略 viewport，修复第 8 项之后也需要修复此处。

影响：小窗实际内容与 UI 边框在垂直位置上错开。例如顶部 y=100、高度=200、屏高=1080 时，命令传入 y=780，后端却将其当作顶部坐标。

最小修复：使用 `InnerY * YScale` 生成顶部原点坐标，保持后端统一转换。

建议验证：将小窗分别放在顶部和底部，使用两个位置不对称的场景核对内容与边框；只检查居中位置会漏掉该错误。

## 10. 重要：OpenGL 3.3+ / GLES3 离屏清屏污染主画面的 clear color 缓存

位置：

- `src/engine/client/backend/opengl/backend_opengl.cpp:1139`：BeginRenderTarget 直接调用 glClearColor。
- `src/engine/client/backend/opengl/backend_opengl3.cpp:898`：普通 Cmd_Clear 只在请求颜色不同于 CPU 缓存时更新 glClearColor。
- `src/game/client/gameclient.cpp:1791`：每帧使用配置中的背景色清屏。
- `src/engine/client/graphics_threaded.cpp:1169`：Gaussian blur 使用透明黑清空中间目标。

触发：主画面以非黑色 C 清屏，随后执行透明黑清屏的离屏 blur；下一帧继续请求同样的 C。

影响：CPU 的 m_ClearColor 仍是 C，实际 GL_COLOR_CLEAR_VALUE 已是透明黑。下一次 Cmd_Clear 误以为无须更新颜色，将主画面清成黑色，导致未被场景覆盖的背景颜色错误。目标 End 只恢复 framebuffer、viewport 和绘制状态，不恢复 clear color。

最小修复：离屏清屏保存/恢复 clear color，或用不修改该全局状态的 glClearBufferfv；也可统一所有清屏入口的缓存管理。旧固定管线 Cmd_Clear 每次都设置颜色，不受同一缓存问题影响。

建议验证：用非黑背景连续渲染两帧，第一帧中插入透明黑离屏清屏；第二帧读取未被几何覆盖的背景像素。

## 11. 重要：OpenGL 3.3+ / GLES3 绘制离屏纹理时误用普通纹理 sampler

位置：

- `src/engine/client/backend/opengl/backend_opengl3.cpp:1087`：DrawRenderTarget 先按前端 m_State 绑定普通纹理和 sampler，随后只替换 texture。
- `src/engine/client/backend/opengl/backend_opengl.cpp:1472` 附近：现代 SetState 为有效的普通纹理绑定其 sampler。
- `src/engine/client/backend/opengl/backend_opengl3.cpp:769`：普通纹理 sampler 可使用 GL_LINEAR_MIPMAP_LINEAR。
- `src/engine/client/backend/opengl/backend_opengl.cpp:1078`：RenderTarget 只分配 level 0。
- `src/engine/client/graphics_threaded.cpp:1014`：DrawRenderTarget 复制 m_State，没有清除其普通纹理槽位。

触发：当前前端纹理状态指向一个使用 mipmap 的普通纹理，随后调用 DrawRenderTarget。该 API 并未要求调用方先 TextureClear。

影响：离屏纹理自己的 GL_LINEAR 过滤设置被遗留 sampler 覆盖；只有 level 0 的目标不能满足 mipmap 采样要求，结果可能显示黑色。即便调用方已清空纹理状态，也应防止继承之前绘制绑定的 sampler。GaussianBlurPass 自己解绑 sampler，但无法保护其后所有 DrawRenderTarget 的状态组合。

最小修复：DrawRenderTarget 在设置普通绘制状态后显式绑定专用的线性 clamp sampler，或 glBindSampler(0, 0) 使用目标自身的参数。目标采样不能依赖调用方当前的普通纹理。

建议验证：先选中带 mipmap 的普通纹理，再绘制一个已清成已知颜色的离屏目标；分别测试不调用和调用 TextureClear 的命令序列。

## 12. 重要：GLES3 离屏清屏与 Gaussian blur 沿用旧 scissor

位置：

- `src/engine/client/backend/opengl/backend_opengl.cpp:71`：最底层 SetState 整个函数体被 BACKEND_GL_MODERN_API 条件排除。
- `src/engine/client/backend/opengl/backend_opengl.cpp:1138`：继承的 BeginRenderTarget 调用这个非虚的底层 SetState。
- `src/engine/client/backend/opengles/backend_opengles.cpp:9`：GLES 编译复用该实现，GLES3 定义 BACKEND_GL_MODERN_API。
- `src/engine/client/backend/opengl/backend_opengl3.cpp:1224`：GaussianBlurPass 设置程序和 blend，但没有关闭 scissor。

触发：GLES3 前序普通绘制启用了裁剪，接着在该 UI 裁剪范围内准备 Gaussian blur。前端 Begin 命令虽然将 m_ClipEnable 置 false，但后端实际调用的 SetState 是空函数。

影响：旧的屏幕 scissor 原样作用于尺寸更小的离屏目标，glClear 只清除其中一块，blur 也仅绘制其中一块；裁剪框落在目标外时整次清屏和 blur 都可能没有输出。桌面 OpenGL 的底层 SetState 会执行裁剪更新，不能将这一项笼统算到桌面 GL。

最小修复：离屏入口独立设置现代 API 通用的 scissor 状态，清屏和全目标 blur 前明确关闭裁剪并维护 m_LastClipEnable；退出时按后续绘制状态恢复。不要依赖 GLES3 下被编译为空的固定管线 SetState。

建议验证：GLES3 中先绘制一个带屏幕裁剪的元素，再执行小尺寸离屏 blur，读回目标四角和中心；比较关闭裁剪的对照结果。

OpenGL 补充审查新增第 10–12 项。第 1 项公共异步读回悬垂指针、第 9 项小窗 Y 原点错误也影响 OpenGL，保留原编号，不重复计数。

## 证据与验证边界

已执行源码与调用点交叉审查，读取现有 render_target、graphics_backend_contract 和 Metal 测试。Metal 由独立只读审查提供候选，主审查再次检查实现与调用链。Vulkan API 语义依据 Khronos Vulkan Specification 的 Render Pass Compatibility、Command Buffer Lifecycle，以及 Vulkan Guide 的 Dynamic State Lifetime 复核；Shared 存储访问原则参照 Apple 的 Synchronizing CPU and GPU Work。

这次没有构建、运行客户端、运行 GPU validation layers 或执行测试，因此以上结论是代码级缺陷判定，不是设备实测结果。当前环境为 Windows，Metal 的 GPU 行为需要 Apple 设备验证。已有源码字符串合同测试不能证明命令序列符合 Vulkan/Metal 的运行时要求。

OpenGL/GLES 补充结论核对了 Khronos OpenGL Reference Pages 的 glClear 与 glBindSampler 语义；同样没有执行 GPU 运行时测试。

`backend_metal.mm:1144` 对 Shared buffer 使用 `didModifyRange:` 的行为尚未充分核实，不计入以上十二项缺陷。
