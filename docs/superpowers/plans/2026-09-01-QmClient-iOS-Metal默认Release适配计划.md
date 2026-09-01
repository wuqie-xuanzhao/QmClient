---
title: QmClient iOS Metal 默认 Release 适配计划
date: 2026-09-01
status: active
---

# QmClient iOS Metal 默认 Release 适配计划

## 目标

将原生 Metal 作为 macOS 和 iOS 的默认图形后端构建内容：

- macOS 和 iOS 的标准 `Release` 配置必须编译、链接并打包 Metal backend 和 metallib。
- 不新增 `cmake-build-metal` 或 `tmp/cmake-ios-metal` 这类专用构建目录；继续使用正常的 `cmake-build-release`、iOS Xcode build folder 和对应的 `Release` configuration。
- `-DMETAL=OFF` 只保留为诊断/回退开关，不能作为默认脚本参数。
- OpenGL/OpenGL ES 保留为显式回退路径，不能因为配置文件保存了旧 backend 名称而启动崩溃。
- 不修改 DDNet 协议、预测、物理、地图格式、demo 格式或 QmClient 私有玩法语义。

## 当前缺口

1. `CMakeLists.txt` 当前把 `METAL` 默认设为 `OFF`，并在 iOS 分支强制设为 `OFF`；Metal 源文件也只在 macOS 加入 target。
2. `backend_sdl.cpp` 和 `graphics_backend_contract.h` 的 Metal 编译条件只覆盖 `CONF_PLATFORM_MACOS`。
3. `cmake/BuildMetalShaders.cmake` 把 shader 编译和 metallib 链接硬编码为 `macosx` SDK，无法生成 iOS device/simulator 产物。
4. `scripts/ios/cmake_ios.sh` 显式传入 `-DMETAL=OFF`，与默认 Release 目标冲突。
5. iOS 目前只有 bundle 编译证据，没有 Metal simulator/device 启动、触控、联网、前后台恢复、截图和长时间运行证据。

## 执行阶段

### P0：构建开关和 target 接线

文件边界：`CMakeLists.txt`、`cmake/BuildMetalShaders.cmake`、`scripts/ios/cmake_ios.sh`、`src/engine/client/backend_sdl.cpp`、`src/engine/client/backend/graphics_backend_contract.h`。

1. 将 `METAL` 的默认值改为 Apple client target 默认开启；显式 `-DMETAL=OFF` 仍可覆盖。默认 `Release`、`RelWithDebInfo` 和 Xcode 的 `Release` configuration 必须走同一套 Metal target，不依赖目录名。
2. 将 iOS Metal backend 源文件加入 `ENGINE_CLIENT`，并把 `CONF_BACKEND_METAL`、`CONF_BACKEND_METAL_READY`、factory、selectability 和测试条件统一扩展到 `CONF_PLATFORM_IOS`。
3. iOS 脚本删除 `-DMETAL=OFF`；保留环境变量或命令行显式关闭能力用于 GLES 诊断构建。
4. shader 构建使用当前 `CMAKE_OSX_SYSROOT`、架构和 deployment target，分别产出 device 与 simulator 可用的 metallib；不能复用 macOS metallib。
5. bundle/CPack/Xcode 资源路径统一使用 `data/shader/metal/qmclient.metallib`，并增加构建时文件存在检查。

停止条件：任一 Apple target 在默认 Release 下没有 `CONF_BACKEND_METAL_READY`、shader 产物或链接 framework；或显式 `METAL=OFF` 仍无法生成 GLES 回退。

### P1：iOS backend 身份和生命周期

文件边界：`src/engine/client/backend/metal/backend_metal.mm`、`backend_metal.h`、`backend_sdl.cpp`、`src/ios/ios_main.cpp`。

1. 先保证 SDL Metal view、`CAMetalLayer`、drawable 获取、present、窗口尺寸和旋转在 device/simulator 都有明确状态。
2. `drawable` 暂时不可用、应用切后台/回前台、layer 重绑、resize 和设备错误必须进入可恢复状态，不得把 `nextDrawable` 失败当作永久 GPU 崩溃。
3. iOS 使用 `drawableSize` 和 UIKit/SDL 实际像素尺寸，禁止复用 macOS 窗口尺寸假设。
4. 设备失败时先传播 Metal 初始化错误，再按现有图形后端回退约定尝试 OpenGL ES；回退不能重复初始化旧 context 或继续提交已失效 command buffer。

### P2：资源和 shader 兼容

1. 明确 device、`arm64` simulator 和必要的 x86_64 simulator shader 编译产物及命名。
2. 检查 metallib 在 app bundle 的实际资源路径、签名/归档后路径和开发运行路径一致。
3. 逐项核对纹理格式、二维纹理数组、sampler、uniform/buffer layout、MSAA 和 render target 能力；不以 macOS feature set 推断 iOS 支持。
4. 对不支持的能力返回现有 `SBackendCapabilities` 失败原因，保留 GLES 回退。

### P3：绘制能力和行为对照

按风险顺序完成并逐阶段验收：

1. 普通 tile、quad、text 和 text container；
2. buffer/container 与纹理数组；
3. render target、MSAA、Gaussian blur 和 backbuffer capture；
4. screenshot/readback、失败传播、窗口旋转和后台恢复。

每个阶段都要同时通过 C++ contract、simulator 编译、simulator 视觉检查和至少一台 arm64 真机视觉检查，并与现有 GLES 截图做对照。

### P4：默认 Release 验收与发布接线

1. macOS：在现有 `cmake-build-release` 中重新 configure/build，确认默认 `METAL=ON`、metallib 生成、app/DMG 资源复制和启动日志为 Metal。
2. iOS：使用现有 `scripts/ios/cmake_ios.sh device ... Release <正常构建目录>` 和 `sim ... Release <正常构建目录>`，确认不需要专用 Metal 目录。
3. 验证旧配置 `gfx_backend=Metal/OpenGL/GLES`、显式 `DDNET_DRIVER`、Metal 初始化失败和 GLES 回退。
4. 只有在 simulator 和真机的启动、触控、联网、前后台恢复、截图/Files export、音频和长时间运行均有证据后，才把 iOS Metal 标记为可发布。

## 验收矩阵

| 项目 | macOS Release | iOS simulator Release | iOS device Release |
|---|---|---|---|
| 默认 configure/build 启用 Metal | 必须 | 必须 | 必须 |
| metallib 生成并进入 bundle | 必须 | 必须 | 必须 |
| SDL Metal view/layer/drawable/present | 必须 | 必须 | 必须 |
| tile/quad/text/render target/readback | 必须 | 必须 | 必须 |
| 旋转、后台恢复、失败回退 | 必须 | 必须 | 必须 |
| 触控、联网、截图、音频、长时间运行 | 回归 | 必须 smoke | 必须 |

## 验证命令和证据

构建前必须检查是否有后台 `cmake`、`ninja`、`xcodebuild`、`cargo` 或 `run_cxx_tests` 任务；同一 build 目录内的 build、C++ tests、Rust tests 和 package 必须串行。

```sh
cmake -G Ninja -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release --target game-client
cmake --build cmake-build-release --target run_cxx_tests
cmake --build cmake-build-release --target run_rust_tests
python3 qmclient_scripts/gate/check_gate.py --mode default
scripts/ios/cmake_ios.sh sim QmClient org.qmclient.client Release tmp/cmake-ios-sim
scripts/ios/cmake_ios.sh device QmClient org.qmclient.client Release tmp/cmake-ios-device
```

每条证据记录 command、结果、证明范围和未覆盖 gap。没有 simulator runtime、签名设备或真实运行记录时，只能标记为构建/合同完成，不能标记为 iOS Metal 完成。

## 回滚

回滚单位为 Apple build wiring、Metal backend 平台分支、shader 产物和 iOS bundle 接线；不得回滚或覆盖无关的 128 人支持、网络、菜单、统计、翻译或其他并行改动。若 iOS Metal 真机失败，默认构建保留代码但发布配置可显式 `-DMETAL=OFF` 回到 GLES，待修复后再恢复默认。
