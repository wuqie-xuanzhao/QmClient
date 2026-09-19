# DDNet 官方文档（中文）

本目录收录 DDNet 上游官方文档的中文翻译，用于本地查阅构建、调试、贡献流程。

## 来源与对照

- **上游仓库**：https://github.com/ddnet/ddnet
- **对照路径**：上游 `docs/` 目录
- **同步基线**：`ddnet-upstream/master`（本地最后刷新 2026-09-08）
- **英文原件**：保留在 `英文原文/` 子目录，便于与上游逐句对照

这些文档描述的是 **DDNet 上游**的行为。QmClient 在其上做了定制，凡涉及本仓库的构建与发布，以 `docs/参考/` 下 QmClient 自己的文档为准。

## 目录

| 中文文档 | 上游文件 | 内容 |
| --- | --- | --- |
| [构建选项.md](构建选项.md) | `docs/BUILDING.md` | CMake 构建开关全集、交叉编译入口 |
| [构建-Android.md](构建-Android.md) | `docs/BUILDING-android.md` | Android 依赖与 APK 构建、模拟器运行 |
| [构建-WebAssembly.md](构建-WebAssembly.md) | `docs/BUILDING-emscripten.md` | Emscripten/WASM 构建与部署 |
| [构建-iOS.md](构建-iOS.md) | `docs/BUILDING-ios.md` | iOS 依赖与客户端构建 |
| [贡献指南.md](贡献指南.md) | `docs/CONTRIBUTING.md` | 提交流程、代码风格、命名规范 |
| [数据库导入.md](数据库导入.md) | `docs/DATABASE.md` | 导入官方 DDNet 数据库 |
| [调试与崩溃分析.md](调试与崩溃分析.md) | `docs/DEBUGGING.md` | Sanitizer、Valgrind、崩溃转储符号化 |
| [性能基准测试.md](性能基准测试.md) | `docs/BENCHMARKING.md` | 通过 Phoronix Test Suite 跑基准 |

## QmClient 相关的差异提醒

- **iOS 构建**：上游 `BUILDING-ios.md` 是通用 DDNet 说明；QmClient 的 iOS 目标默认启用 Metal 后端、使用 `org.qmclient.client` 包名，见 [参考/构建指南.md](../参考/构建指南.md)。
- **构建入口**：Windows 下本仓库必须用 `qmclient_scripts/cmake-windows.cmd`，不要直接调用 `cmake --build`。
- **代码风格**：上游要求大驼峰、`m_`/`g_`/`s_` 前缀等，QmClient 沿用；细节见 `.agents/skills/qmclient-cpp-conventions/SKILL.md`。
