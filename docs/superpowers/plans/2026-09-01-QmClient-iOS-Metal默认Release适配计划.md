---
title: QmClient iOS Metal 默认 Release 适配计划
date: 2026-09-01
status: active
---

# QmClient iOS Metal 默认 Release 适配计划

## 1. 审验结论

本计划方向正确，但原版本把“编译接线”“shader 资源”“backend 生命周期”和“真机发布验收”混在同一层，且部分“当前缺口”已经被 2026-09-02 工作树中的未提交改动覆盖。该工作树当前包含以下临时改动：

- `CMakeLists.txt`：Apple target 默认打开 `METAL`，iOS 不再强制关闭，并按 `iphoneos` / `iphonesimulator` 选择 `xcrun` SDK。
- `cmake/BuildMetalShaders.cmake`：shader 编译和 `metallib` 链接改为使用选定 SDK。
- `scripts/ios/cmake_ios.sh`：移除显式 `-DMETAL=OFF`。
- `backend_sdl.cpp`、`graphics_backend_contract.h`、`backend_metal.mm` 和 C++ contract 测试：增加 iOS 编译保护或 API 可用性分支。

以上均属于**未提交、未完成验证的工作树状态**，不能作为计划已完成项。审验后必须保留两个明确结论：

1. P0 的“默认开关和平台接线”可以在现有改动上收口，但必须补充 CMake/Xcode 配置隔离、架构、deployment target、资源路径和 `METAL=OFF` 回退验证。
2. 即使 macOS 和 simulator 编译通过，也不能把 iOS Metal 标记为可发布；必须有 simulator 运行证据、arm64 真机证据以及失败回退证据。

## 2. 目标与非目标

### 目标

- macOS 和 iOS 的正常 `Release` 配置默认启用原生 Metal；显式 `-DMETAL=OFF` 仍可生成 GLES/OpenGL 诊断构建。
- Metal backend、Metal shader、`Metal.framework` / `QuartzCore.framework` 和运行时资源在默认 Release 中形成可追踪的构建链。
- iOS device、arm64 simulator（必要时 x86_64 simulator）使用与自身 SDK/架构匹配的 `qmclient.metallib`，不能复用 macOS 产物。
- `gfx_backend=Metal/OpenGL/GLES`、`DDNET_DRIVER`、旧配置文件和 Metal 初始化失败都不会导致启动崩溃；不可用 backend 必须按现有 fallback 约定回退。
- 记录可复核的 configure、build、测试、bundle、simulator、真机和回退证据。

### 非目标

- 不修改 DDNet 协议、snapshot/input、预测、物理、碰撞、地图/demo/skin 格式或 QmClient 私有玩法语义。
- 不在本计划中改动 MoltenVK、OpenGL ES 绘制语义、iOS 网络/后台恢复逻辑或第三方依赖内容；这些属于 `2026-08-30-DDNet20-iOS运行时适配计划.md` 的边界。
- 不创建 `cmake-build-metal`、`tmp/cmake-ios-metal` 等 Metal 专用目录。允许使用按 SDK/架构隔离的普通构建目录，例如 `cmake-build-release`、`tmp/cmake-ios-sim`、`tmp/cmake-ios-device`；同一目录不得交替 configure 不同 SDK/架构。

## 3. 当前基线与前置检查

执行实现前先记录：

```sh
git status --short
git diff -- CMakeLists.txt cmake/BuildMetalShaders.cmake scripts/ios/cmake_ios.sh \
  src/engine/client/backend_sdl.cpp src/engine/client/backend/graphics_backend_contract.h \
  src/engine/client/backend/metal src/test/graphics_backend_contract_test.cpp
pgrep -af 'cmake|ninja|xcodebuild|cargo|run_cxx_tests|run_rust_tests' || true
xcode-select -p
xcrun --sdk macosx --show-sdk-path
xcrun --sdk iphoneos --show-sdk-path
xcrun --sdk iphonesimulator --show-sdk-path
xcrun simctl list runtimes
```

判定规则：

- 若发现后台已有同一 checkout 的 build/test/package 任务，先等待或协调，禁止重复占用同一 build 目录。
- 先把未提交 Apple/Metal 改动归类为“并行工作树变更”；本计划只补计划文件，不覆盖或回滚这些改动。
- iOS deployment target 以当前 iOS 计划确定的 `15.0` 为基线；configure、`Info.plist` 和最终 bundle 必须一致，不能沿用历史 `12.0` 产物。
- 没有 simulator runtime 时，只能完成静态/CMake/bundle 诊断，不能声称 simulator 启动或视觉验收完成。
- 没有签名 team、可用 arm64 设备和安装权限时，只能记录 device 编译/链接，不能声称真机完成。

## 4. 分阶段执行

### P0：默认开关、平台接线与构建合同

文件边界：`CMakeLists.txt`、`cmake/BuildMetalShaders.cmake`、`scripts/ios/cmake_ios.sh`、`src/engine/client/backend_sdl.cpp`、`src/engine/client/backend/graphics_backend_contract.h`、对应 contract 测试。

1. **默认值**：将默认值限定为 Apple client target；非 Apple 平台继续保持关闭。显式 `-DMETAL=OFF` 必须有最高覆盖优先级，旧 cache 不能悄悄覆盖本次明确参数。
2. **平台宏**：`CONF_BACKEND_METAL`、`CONF_BACKEND_METAL_READY`、factory、selectability、driver name 和测试条件在 macOS/iOS 使用同一套合同；非 Apple 编译不能引入 Objective-C++ 或 Metal header。
3. **GLES 共存**：iOS 默认仍编译 GLES fallback。Metal 初始化失败时，只能走一次 backend 重选和 context 重建，不能复用失效的 SDL window/context 或继续提交旧 command buffer。
4. **Xcode 配置**：验证 `Release`、`RelWithDebInfo` 和 `CMAKE_BUILD_TYPE` 不会互相覆盖；脚本传入的 SDK、架构、deployment target 必须在 `CMakeCache.txt`/Xcode build settings 中可见。
5. **资源目标**：`game-client` 必须依赖 `build_metal_shaders`；资源的逻辑路径固定为 `data/shader/metal/qmclient.metallib`，并在构建时检查源文件、生成文件和 bundle 文件均存在。

RED/GREEN：

- RED：关闭 Metal 宏或在非 Apple 条件编译 contract，确认 Metal 不可选且无链接依赖。
- GREEN：macOS/iOS Apple 配置中 contract 测试确认 Metal 可选，`METAL=OFF` 配置确认 GLES 可构建。

停止条件：任一 Apple Release configure/build 没有明确的 Metal 宏、shader 目标或 framework 链接；或者 `METAL=OFF` 不能生成可启动的 GLES 回退目标。

### P1：shader 编译、架构和 bundle 资源

文件边界：`cmake/BuildMetalShaders.cmake`、相关 CMake 资源复制段、`backend_metal.mm` 的 shader 加载路径。

1. `metal`/`metallib` 必须通过当前 `CMAKE_OSX_SYSROOT` 选择 `macosx`、`iphoneos` 或 `iphonesimulator`，不能硬编码 macOS SDK。
2. shader 编译命令必须显式继承当前架构和 deployment target；至少覆盖 macOS arm64、iOS arm64 device、iOS arm64 simulator，若仍支持 Intel simulator 则增加 x86_64 slice。
3. 不同 SDK/架构的中间文件不能在同一普通 build folder 互相覆盖。优先使用按 SDK/架构隔离的普通目录；若允许同一 Xcode 工程多 destination 构建，则输出文件名/目录必须带 SDK/架构键，并在 bundle 阶段映射回固定逻辑路径。
4. `qmclient.metallib` 进入开发运行 bundle、安装 bundle、归档/导出 bundle；`backend_metal.mm` 的加载失败必须输出绝对/相对路径、SDK/架构和错误原因。
5. shader 产物不能只用 `file` 判断可用性；至少记录 `xcrun --sdk <sdk> metallib` 成功、产物非空、bundle 路径一致和运行时 `newLibraryWithData` 成功。

停止条件：device/simulator 产物混用、bundle 缺文件、shader 只在 macOS 可加载，或同一 build folder 的增量构建可能复用错误 SDK 产物。

### P2：iOS Metal backend 生命周期与窗口合同

文件边界：`src/engine/client/backend/metal/backend_metal.mm/.h`、`src/engine/client/backend_sdl.cpp`、必要的 iOS entry/SDL 适配点。

按状态机验证 `UNINITIALIZED -> PRE_INITIALIZED -> INITIALIZED -> SHUTDOWN`：

1. SDL Metal view、`CAMetalLayer`、device、command queue、drawable 和 shader library 的所有权/释放顺序明确，重复 init/shutdown 不泄漏、不 double release。
2. `SDL_Metal_GetDrawableSize` 与实际像素尺寸驱动 `drawableSize`、viewport 和 readback；禁止使用 macOS 窗口尺寸或点数尺寸替代像素尺寸。
3. `nextDrawable` 返回 nil、drawable 超时、窗口 resize、旋转、layer 重绑、前后台切换和 GPU command buffer error 都进入可恢复路径；单次失败不能直接变成永久 fatal state。
4. iOS 不访问仅 macOS 可用的 `CAMetalLayer` 属性或多 GPU API；所有 API availability 分支必须由 deployment target 保护。
5. 回退顺序固定为：传播 Metal 初始化错误 -> 销毁 Metal 私有资源/窗口状态 -> 按现有 backend 选择逻辑尝试 GLES -> 记录最终 backend。不得静默把 Metal 名称写成 OpenGL 而丢失诊断。

自动化：为状态转换、nil drawable、初始化错误粘滞、旧配置解析和 fallback 增加源码 contract/GoogleTest；真实 layer、触控、后台恢复留给 simulator/真机。

停止条件：生命周期错误需要修改上层 command protocol，或无法证明 fallback 不会重复初始化旧 context。

### P3：绘制能力和行为对照

按以下顺序逐段验收，每段都要有 C++ contract、simulator 编译和视觉证据；进入下一段前收口失败传播：

1. 普通 tile、quad、sprite、text、text container、blend mode。
2. buffer/container、纹理数组、sampler、uniform/buffer layout、纹理上传和 mip 行为。
3. render target、MSAA、backbuffer load/store、Gaussian blur、readback/screenshot。
4. HiDPI、横竖屏/旋转、窗口尺寸变化、前后台恢复、drawable 暂不可用和 GPU error 后恢复。

每段与 GLES 对照同一输入/地图/分辨率，记录截图、日志和已知差异；不能用“能启动”代替绘制正确性。

### P4：默认 Release、打包和回退验收

1. macOS：在现有 `cmake-build-release` 重新 configure/build，确认默认 `METAL=ON`、Metal framework、metallib、app Resources 和 DMG 资源复制。
2. iOS simulator：使用 `scripts/ios/cmake_ios.sh sim ... Release <普通 simulator build folder>`；检查 arm64/x86_64 选择、bundle、安装、启动和日志。
3. iOS device：使用 `device ... Release <普通 device build folder>`；先完成无签名编译，再在具备签名 team/设备时执行安装启动。脚本默认关闭签名不等于真机发布包已验证。
4. 运行矩阵至少覆盖：默认 Metal、旧 `gfx_backend=Metal`、旧 `gfx_backend=OpenGL/GLES`、`DDNET_DRIVER=Metal/OpenGL/GLES`、显式 `METAL=OFF` 构建、Metal shader 缺失、Metal 初始化失败、GLES fallback。
5. 只有 simulator 和 arm64 真机都完成启动、触控、联网 smoke、前后台恢复、截图/Files export、音频和长时间运行后，才允许把 iOS Metal 标为可发布。

## 5. 验收矩阵

| 项目 | macOS Release | iOS simulator Release | iOS device Release |
| --- | --- | --- | --- |
| 默认 configure/build 启用 Metal | 必须 | 必须 | 必须 |
| `METAL=OFF` GLES 回退构建 | 必须 | 必须 | 必须 |
| SDK/架构/deployment target 与产物匹配 | 必须 | 必须 | 必须 |
| Metal framework 与 backend factory | 必须 | 必须 | 必须 |
| metallib 生成、加载并进入 bundle | 必须 | 必须 | 必须 |
| tile/quad/text/array/render target/readback | 必须 | 必须 | 必须 |
| resize/旋转/后台恢复/drawable 失败 | 回归 | 必须 smoke | 必须 |
| 触控/联网/截图/音频/长时间运行 | 回归 | 必须 smoke | 必须 |
| Metal 失败后的 GLES 回退 | 必须 | 必须 | 必须 |

## 6. 验证命令与证据格式

构建前检查后台任务；同一 build 目录内 `game-client`、`run_cxx_tests`、`run_rust_tests`、`package_default` 串行执行。普通文档修改本身不跑代码 gate，但本计划中的命令必须人工核对路径、参数和前置条件。

```sh
# macOS Release
cmake -G Ninja -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release --target game-client
cmake --build cmake-build-release --target run_cxx_tests
cmake --build cmake-build-release --target run_rust_tests
python3 qmclient_scripts/gate/check_gate.py --mode default

# iOS simulator/device；两个目录必须分别对应 SDK/架构，不是 Metal 专用目录
scripts/ios/cmake_ios.sh sim QmClient org.qmclient.client Release tmp/cmake-ios-sim
scripts/ios/cmake_ios.sh device QmClient org.qmclient.client Release tmp/cmake-ios-device
```

每条证据使用以下格式：

```text
Command: <exact command>
Result: <pass/fail and key output>
Scope: <configure/build/test/bundle/runtime/recovery/packaging>
Artifacts: <app path, metallib path, log or screenshot path>
Gaps: <runtime, signing, device, feature or integration not verified>
```

没有 simulator runtime、签名设备或真实运行记录时，只能标记为 configure/build/contract/bundle 完成，不能标记为 iOS Metal 完成。

## 7. 版本、提交与回滚

- 本计划只更新执行记录，不因计划文本自动 bump 版本；功能实现收口后按仓库 MMP 规则另行处理。
- 提交前必须确认工作树中并行的 Metal、菜单、统计、翻译和其他改动未被混入；按文件/区块选择性暂存。
- 回滚单位为 Apple build wiring、Metal backend 平台保护、shader 产物和 iOS bundle 接线；不得覆盖无关的 iOS 网络/后台恢复、玩法、统计、翻译或第三方依赖改动。
- 若真机 Metal 失败，允许发布配置显式 `-DMETAL=OFF` 回到 GLES，但必须保留失败日志、触发条件和待修复项；未重新通过 P2-P4 前不得恢复“默认可发布”结论。
