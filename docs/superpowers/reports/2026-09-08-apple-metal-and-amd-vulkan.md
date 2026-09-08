# Apple Metal 构建调整与 AMD Vulkan 多线程调查

日期：2026-09-08  
状态：Apple 构建与默认选择已修改；AMD 限制来源已查明，原始崩溃根因尚未实机确认。

## Apple 交付

版本通过 `qmclient_scripts/bump_version.py` 从 3.0.1 更新到 3.0.2。

- macOS/iOS 的 Vulkan 默认关闭。`cmake/QmAppleGraphics.cmake` 同时覆盖旧 cache 和显式 `VULKAN=ON`，清除旧 Vulkan 编译着色器清单，避免旧构建目录继续打包 SPIR-V。
- Apple 图形客户端强制启用 Metal；该策略不会为 server-only/headless 构建额外强制开启 Metal。
- Stable、Nightly 的 macOS 配置均改为 `VULKAN=OFF` / `METAL=ON`，移除 MoltenVK、vulkan-loader、glslang 的 Homebrew 安装请求。
- DMG 验证增加 Metal shader library 存在检查，以及旧 Vulkan dylib/ICD 不存在检查。
- 编译了 Metal 的 Apple 客户端，新的 `gfx_backend` 默认值为 Metal。旧配置若保存为 Vulkan，则启动时迁移为 Metal；明确选择 OpenGL/GLES 的配置及失败回退机制仍保留。
- Windows/Linux 的 Vulkan 策略没有改变。CMakeLists.txt 中其他任务新增的 `ui_popups.cpp` 条目原样保留。

## AMD 限制的证据

`git log -S`、`git blame` 和提交 diff 指向 `703413eb9b73da99f2bf8144b16e1ab12eb3e37a`，日期为 2026-01-16，标题“feat(ui): 添加分身小地图及页面切换过渡动画”。

这个提交同时引入分身小窗的动态 viewport、强制主线程调度，以及按 AMD 厂商字符串把渲染线程数设为 1 的逻辑。提交说明写的是“不解决 AMD 显卡动态视口崩溃问题”。没有附 GPU 型号、驱动版本、验证层信息或最小复现。

所以目前能确认的是“QmClient 为当时的小窗问题加入了整个 AMD 厂商级别的规避”，不能确认“AMD Vulkan 多线程本身不可用”或“所有 AMD 驱动均受影响”。

该提交中的 GetGraphicCommandBuffer 仍仅根据 m_ThreadCount 决定是否使用 secondary，强制主线程调度不等于切成 primary。它之前的实现也已将 viewport/scissor 存入每条执行命令，不能仅凭全局 viewport 更新推断 worker 读取竞争。

后来 `033cdebc2578ddea40ccb086efc6c47878eeb25f` 将 GetGraphicCommandBuffer 改为同时检查 m_ForceSingleThreadedRender。该提交作者日期为 2026-07-21，提交日期为 2026-07-31。这引入了下面可以确定的应用侧状态转换问题，但时间晚于 AMD 限制，不能倒推为 1 月原始崩溃的根因。

## 当前仍需修复的应用侧问题

位置：`src/engine/client/backend/vulkan/backend_vulkan.cpp:9330`、`:7620`、`:3040`。

多线程 PrepareFrame 开启 `VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS` 的 pass；自定义 viewport 命令只等待 worker，随后设置 m_ForceSingleThreadedRender。后续 GetGraphicCommandBuffer 因此返回 primary buffer，但当前 pass 仍要求 secondary 内容。

结果是直接在 primary 中录制不允许的绘制命令；该问题与厂商无关，违背 Vulkan 的 VkSubpassContents 规则。单纯等待线程不能更改 subpass 内容模式。

`f8a227787`（2026-08-18）已将 Vulkan 分身小窗迁到离屏目标，当前小窗路径通过 EndSwapRenderPassForExternalWork 正确结束原 pass 后再进入 INLINE 目标，不能把上述缺陷描述为当前小窗必现。直接 viewport API 和 SetForcedAspect 仍存在：高窗口切换强制宽高比、新旧 presented viewport 不一致且后续仍有绘制时，可以进入上述危险路径。

修复选择：

1. 自定义 viewport 只改变 CPU 调度，仍按当前 pass 模式录入 secondary。
2. 如需切为 primary，完整执行已有 secondary，结束原 pass，以兼容的 load pass 恢复 INLINE，并失效相关录制缓存。

这里还依赖前一份 graphics-backend-bug-scan 报告中的 renderpass 兼容性、缓存状态问题。不能只删除 AMD 初始化限制并视为安全。

本轮保留 AMD 强制单线程，未改 Vulkan 渲染代码。下一步需要在 AMD 设备上修复上述应用侧问题后，用 1 与 3 线程对照验证 viewport/强制宽高比、小窗、背景捕获、读回和跨批次绘制。

## 已执行验证

- `python -m unittest qmclient_scripts.tests.test_cmake_platform_defaults`：6 项通过，包含 macOS/iOS 旧参数覆盖、非 Apple 参数保留、server/headless 策略，以及实际 Clang 预处理配置默认值。
- `python -m unittest qmclient_scripts.tests.test_release_workflow_contract`：4 项通过；本轮与平台测试合并执行时共 9 项，之后新增预处理测试并单独重跑平台测试。
- `qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client testrunner -j 14`：成功。
- `testrunner.exe --gtest_filter=GraphicsBackendContract.*:GraphicsRenderTarget*.*:Metal*.*`：119 项通过。Windows 下不能用这些合同/状态测试替代 Metal GPU 测试。
- `git diff --check`：本轮代码、构建及脚本改动通过。
- quick gate：10 项通过，1 项失败。失败为设置页迁移合同，检查脚本只读取 `ui.cpp`，但工作区其他任务已把包含 `Props.m_FontSize = ResolvedFontSize` 和 `State.m_SelectionPopupContext.m_FontSize = ResolvedFontSize` 的代码移至 `ui_popups.cpp`（当前分别在 528、555 等行）。本轮没有修改该 UI 拆分或检查规则。

当前机器的物理显卡为 NVIDIA GeForce RTX 4060 Ti，没有 AMD 设备。尚未执行 Apple 原生构建、DMG 验证、Metal GPU 测试或 AMD 驱动复现；没有启动客户端实例。
