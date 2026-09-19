# 构建 iOS 版

> 对应上游：`docs/BUILDING-ios.md`

> **QmClient 差异提醒**：本文是上游通用说明。QmClient 的 iOS 目标**默认启用 Metal 后端**并使用 `org.qmclient.client` 包名，构建命令见 [参考/构建指南.md](../参考/构建指南.md)。两处的依赖要求一致，差异主要在默认开关、包名和验收要求。

## 在 macOS 上构建 iOS 版的前置条件

- 已安装 Xcode（含 iOS SDK 和命令行工具）。
- CMake 3.20 或更新版本。
- Rust（stable）。为保证构建可复现，请使用与 CI 相同的版本。
- 安装 iOS 的 Rust target：
  ```shell
  rustup target add aarch64-apple-ios
  rustup target add aarch64-apple-ios-sim
  rustup target add x86_64-apple-ios
  ```
- 为 iOS 构建 `ddnet-libs`（见下），或使用 https://github.com/ddnet/ddnet-libs/ 提供的预编译版本。

## 如何本地构建 iOS 用的 `ddnet-libs`

- 安装依赖：
  ```shell
  brew install autoconf automake cmake libtool m4 ninja pkg-config
  ```
- 设置为使用 GNU m4，否则 opusfile 首次构建会失败：
  ```shell
  export M4="$(brew --prefix m4)/bin/m4"
  ```
- 运行 iOS 库构建脚本：
  ```shell
  scripts/compile_libs/gen_libs.sh build-ios-libs ios
  ```
  **警告**：不要选择 `src` 文件夹内部的目录！
- 脚本执行完毕后，会在你指定的输出目录下创建 `ddnet-libs` 目录，其中包含目录结构正确的全部库，可合并进源码目录的 `ddnet-libs`：
  ```shell
  find ddnet-libs -type d -name ios -exec rm -r {} + -prune
  cp -r build-ios-libs/ddnet-libs/. ddnet-libs/
  ```

## 如何构建 iOS 版 DDNet 客户端

- 在项目根目录打开终端并运行：
  ```shell
  scripts/ios/cmake_ios.sh <device/sim-arm64/sim-x86_64/sim> <App name> <Bundle id> <Debug/Release> <Build folder>
  ```
  - `device` 构建 arm64 iPhoneOS 应用。
  - `sim` 使用宿主架构构建 iOS 模拟器应用。
- 在 Apple Silicon 上构建模拟器应用的示例：
  ```shell
  scripts/ios/cmake_ios.sh sim DDNet org.ddnet.client Debug build-ios-sim
  ```
- 除非把 `IOS_DEVELOPMENT_TEAM` 设为你的 team ID，否则设备构建是未签名的——这足以验证应用能编译，但这样的应用无法安装到设备上。设置该变量以启用自动签名：
  ```shell
  IOS_DEVELOPMENT_TEAM=XXXXXXXXXX scripts/ios/cmake_ios.sh device DDNet org.ddnet.client Debug build-ios-device
  ```
  设备必须先注册到你的 team。Xcode 只会为它已准备好用于开发的设备做这件事，所以如果构建失败并提示 "Your team has no devices"，先用 Xcode 运行一次该应用。
