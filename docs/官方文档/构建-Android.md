# 构建 Android 版

> 对应上游：`docs/BUILDING-android.md`

## 在 Linux 上构建 Android 版的前置条件

- 至少 10–15 GiB 可用磁盘空间。
- 先按 https://github.com/ddnet/ddnet 的通用说明在 Linux 上做好构建准备。
- 在与 Android Studio 解包位置相同的地方安装 Android NDK、Android SDK build tools 和 Android 命令行工具。可以运行以下脚本自动下载正确版本：
	```shell
	scripts/android/download_android_sdk.sh
	```
	这会把 Android SDK 的相关组件下载到 `~/Android/Sdk`。
	它也会通过 SDK manager 接受 Android SDK 许可，否则 Gradle 构建会失败。
	注意：如果你此前下载过其他版本的 Android SDK 组件，请先删除，以确保使用正确版本。
- 使用 stable 版 Rust。使用 nightly 版会导致链接错误。
	为保证构建可复现，请使用与 CI 完全相同的 Rust 版本：
	```shell
	rustup default 1.92.0
	```
- 安装 cargo-ndk，并在**设置好 Rust 版本之后**为 rustup 添加 Android target，以便用 Android NDK 构建 Rust：
	```shell
	cargo install cargo-ndk
	rustup target add armv7-linux-androideabi
	rustup target add i686-linux-android
	rustup target add aarch64-linux-android
	rustup target add x86_64-linux-android
	```
- 安装 OpenJDK 21：
	```shell
	sudo apt install openjdk-21-jdk
	```
- 安装 ninja：
	```shell
	sudo apt install ninja-build
	```
- *（仅 macOS）* 安装 coreutils 以获得 `nproc`：
	```shell
	brew install coreutils
	```
- 需要 Android 用的 `ddnet-libs`。
	最简便的方式是克隆 `ddnet-libs` submodule，使用 https://github.com/ddnet/ddnet-libs/ 提供的预编译库。
	也可以自行构建 Android 用的 `ddnet-libs`，本地编译方法见下文。
	也可以通过手动运行 GitHub workflow [`build-libraries-android`](https://github.com/ddnet/ddnet/blob/master/.github/workflows/build-libraries-android.yml) 来构建这些库。
- 关于可复现构建的重要提示：为保证构建可复现，必须使用完全相同的 Android SDK 组件和 Rust 版本。
	此外，配置时必须使用恰好 CMake 3.22.1 版本。

## 在 Windows 上使用 MSYS2 构建 Android 版的前置条件

- 从零开始至少需要 50 GiB 可用磁盘空间。
- 先安装 MSYS2（https://www.msys2.org/wiki/MSYS2-installation/）以及 Windows 上用 MSYS2 构建 DDNet 所需的全部软件包。
	（目前没有更详细的指南。）
- 注意：本说明中的所有命令都必须在 `bash` 终端（例如 MSYS2）中执行，而不能在 `cmd.exe` 或 PowerShell 中执行。
- 安装 cargo-ndk，并为 rustup 添加 Android target，以便用 Android NDK 构建 Rust：
	```shell
	cargo install cargo-ndk
	rustup target add armv7-linux-androideabi
	rustup target add i686-linux-android
	rustup target add aarch64-linux-android
	rustup target add x86_64-linux-android
	```
- 安装 JDK 21，例如从 https://adoptium.net/temurin/releases/?package=jdk&os=windows&version=21 获取。
- 安装 ninja：
	```shell
	pacman -S mingw-w64-x86_64-ninja
	```
- 安装 coreutils 以获得 `nproc`：
	```shell
	pacman -S coreutils
	```
- 需要 Android 用的 `ddnet-libs`。
	最简便的方式是克隆 `ddnet-libs` submodule，使用 https://github.com/ddnet/ddnet-libs/ 提供的预编译库。
	也可以自行构建 Android 用的 `ddnet-libs`，本地编译方法见下文。
	也可以通过手动运行 GitHub workflow [`build-libraries-android`](https://github.com/ddnet/ddnet/blob/master/.github/workflows/build-libraries-android.yml) 来构建这些库。
- 设置 `ANDROID_HOME` 环境变量以覆盖 Android SDK 的安装位置，例如 `C:/Android/SDK`。路径中务必只用正斜杠且不含空格。
- 从 https://developer.android.com/studio 安装 Android Studio（内含 SDK manager 图形界面），或从 https://developer.android.com/studio/#command-line-tools-only 安装独立命令行工具（内含 `sdkmanager` 工具）。
- 使用命令行工具时：确保命令行工具安装在预期位置，即 `%ANDROID_HOME%/cmdline-tools/latest/bin` 中应包含 `sdkmanager.bat`。
	用 SDK manager 接受许可，否则 Gradle 构建会失败：
	```shell
	yes | $ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager.bat --licenses
	```
- 通过 Android Studio 的 SDK Manager（Tools 菜单）或 `sdkmanager` 命令行工具安装以下内容：
	- API Level 36 的 SDK Platform
	- NDK（Side by side）29 版本
	- Android SDK Build-Tools（最新版本）
- 也可以使用提供的脚本自动下载相关工具：
	```shell
	scripts/android/download_android_sdk.sh "<path to android home>"
	```

## 如何本地构建 Android 用的 `ddnet-libs`

- Windows 上的构建注意：本说明中的所有命令都必须在 `bash` 终端（例如 MSYS2）中执行，而不能在 `cmd.exe` 或 PowerShell 中执行。
- Linux 上安装以下依赖：
	```shell
	sudo apt install autoconf automake libtool m4
	```
- Windows 上使用 MSYS2 时安装以下依赖：
	```shell
	pacman -S autoconf-wrapper automake-wrapper libtool unzip
	```
- 注意：在把自建的 `ddnet-libs` 与它合并之前，必须先克隆 `ddnet-libs` submodule。
	否则 `ddnet-libs` 文件夹会缺少各平台共享的通用文件（例如头文件）。
- 有一个脚本可以自动下载并构建所有库。
	这需要联网，耗时约 10–20 分钟：
	```shell
	scripts/compile_libs/gen_libs.sh build-android-libs android
	```
	**警告**：不要选择 `src` 文件夹内部的目录！
- 如果最初几分钟就出现错误信息，检查输出并确认 NDK 及其他前置条件安装正确。
- 脚本执行完毕后，会在你指定的输出目录下创建 `ddnet-libs` 目录，其中包含目录结构正确的全部库，可合并进源码目录的 `ddnet-libs`：
	```shell
	find ddnet-libs -type d -name android -exec rm -r {} + -prune
	rm -rf ddnet-libs/sdl/java
	cp -r build-android-libs/ddnet-libs/. ddnet-libs/
	```
- 如需强制重新构建这些库，删除库文件夹内各自的 build 目录：
	```shell
	rm -rf build-android-libs/compile_libs/*/build_android_*
	```
- 如需在更换库版本后强制重新下载，删除整个库构建文件夹：
	```shell
	rm -rf build-android-libs
	```

## 如何构建 Android 版 DDNet 客户端

- 在 `ddnet` 项目根目录打开终端并运行：
	```shell
	scripts/android/cmake_android.sh <x86/x86_64/arm/arm64/all> <Game name> <Package name> <Debug/Release> <Build folder>
	```
	- 第一个参数表示架构。
	  使用 `all` 编译全部架构。
	- 第二个参数表示 APK 名称，必须与库名一致。
	  若要重命名 APK，请在构建之后进行。
	- 第三个参数表示 APK 的包名。
	- 第四个参数表示构建类型。
	- 第五个参数表示构建文件夹。
- 仅构建 `x86_64` 架构 debug 版的示例：
	```shell
	scripts/android/cmake_android.sh x86_64 DDNet org.ddnet.client Debug build-android-debug
	```
- 要构建已签名的 APK，在运行构建脚本前生成签名密钥并导出环境变量：
	```shell
	keytool -genkey -v -keystore my-release-key.jks -keyalg RSA -keysize 2048 -validity 10000 -alias my-alias
	export TW_KEY_NAME=<key name>
	export TW_KEY_PW=<key password>
	export TW_KEY_ALIAS=<key alias>
	```
- 默认情况下，APK 的版本号（version code）和版本名（version name）会根据 `src/game/version.h` 中的定义自动确定。
	也可以在运行构建脚本前手动指定版本号和版本名，例如：
	```shell
	export TW_VERSION_CODE=20210819
	export TW_VERSION_NAME="1.0"
	```
	新版本的版本号必须递增，用户才能自动更新到新版本。
	版本名是展示给用户的字符串，例如 `1.2.3-snapshot4`。
- 为所有架构构建已签名 release APK 的示例：
	```shell
	keytool -genkey -v -keystore Teeworlds.jks -keyalg RSA -keysize 2048 -validity 10000 -alias Teeworlds-Key
	# 会提示输入密码，例如输入 "mypassword"
	export TW_KEY_NAME=Teeworlds.jks
	export TW_KEY_PW=mypassword
	export TW_KEY_ALIAS=Teeworlds-Key
	# 版本号和版本名会自动确定
	scripts/android/cmake_android.sh all DDNet org.ddnet.client Release build-android-release
	```
- 注意：签名密钥只应生成一次（并做备份）。
	只有包名和签名密钥都相同时，用户才能自动更新应用；否则必须手动卸载旧应用。

## 如何在模拟器中运行 DDNet 客户端

- 安装 Android 系统映像：
	```shell
	~/Android/Sdk/cmdline-tools/latest/bin/sdkmanager "system-images;android-36;default;<x86/x86_64/arm/arm64>"
	```
	注意：为获得最佳性能，应与宿主 CPU 架构一致。
- 创建虚拟设备：
	```shell
	~/Android/Sdk/cmdline-tools/latest/bin/avdmanager create avd -n android36default -k "system-images;android-36;default;<x86/x86_64/arm/arm64>"
	```
- 启动虚拟设备：
	```shell
	~/Android/Sdk/emulator/emulator -avd android36default -skin 1280x800 &
	```
- 虚拟设备运行后，把 APK 装载进去：
	```shell
	~/Android/Sdk/platform-tools/adb install build-android-debug/DDNet.apk
	```

## 常见问题与解决

- 如果 Gradle 构建失败并报错指向 Gradle 缓存中的文件问题，尝试清除 Gradle 缓存，即删除 `~/.gradle/caches` 文件夹的内容（Windows 上是 `%USERPROFILE%/.gradle/caches`）。
	如果仍未解决，再删除**构建文件夹中**的 `.gradle` 文件夹，并重启系统以重启 Gradle 守护进程。
- Gradle 构建可能提示无法确定 JDK 版本，可以安全忽略。
- 如果使用的 JDK 版本与 `build.gradle` 中指定的不同，Gradle 构建会以「不支持的 class 文件版本」失败。
	提升所支持的 JDK 版本时，Gradle 版本也必须按 https://docs.gradle.org/current/userguide/compatibility.html 相应提升。
	如果本机安装了多个 JDK，可以在 Gradle 主目录的 `gradle.properties` 文件中用 `org.gradle.java.home` 属性为 Gradle 指定 JDK 版本。

## 维护者备注

- 要更新 Android SDK 的下载链接，参考 https://dl.google.com/android/repository/repository2-3.xml，其中列出了所有可用文件。
- 要更新 Gradle 和 Android Gradle Plugin，参考 https://developer.android.com/build/releases/gradle-plugin#updating-gradle，其中列出了兼容版本。
	更新 Gradle Wrapper 需要一个可用的构建文件夹，所以先正常构建一次 Android 客户端。
	在**构建文件夹中**的 `gradle/wrapper/gradle-wrapper.properties` 文件里更新 Gradle 版本和 SHA256。
	接着在**构建文件夹中**运行 `./gradlew wrapper` **两次**。
	最后，把构建文件夹中的 `gradlew`、`gradlew.bat` 文件和 `gradle` 文件夹复制到项目根目录的 `scripts/android/files` 文件夹，覆盖现有文件。
	把变更后的文件提交到版本控制。
	直接在 `scripts/android/files` 文件夹中运行 wrapper 不可行。
