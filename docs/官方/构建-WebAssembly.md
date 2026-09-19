# 构建 WebAssembly 版（Emscripten）

> 对应上游：`docs/BUILDING-emscripten.md`

## 在 Linux 上通过 Emscripten 构建的前置条件

- 至少 5–10 GiB 可用磁盘空间。
- 先按 https://github.com/ddnet/ddnet 的通用说明在 Linux 上做好构建准备。
- 安装并激活 4.x 系列最新版的 Emscripten SDK（emsdk）。在希望安装 emsdk 的文件夹中运行：
	```sh
	git clone https://github.com/emscripten-core/emsdk.git
	cd emsdk
	./emsdk install 4.0.22
	./emsdk activate 4.0.22
	```
	Emscripten SDK 的使用与更新细节见 [Emscripten 文档](https://emscripten.org/docs/getting_started/downloads.html)。
	注意：从包管理器安装 Emscripten（例如 `sudo apt install emscripten`）可能不可用，因为构建部分库所需的 Emscripten 版本与包管理器提供的不一致。
- 使用 stable 版 Rust。使用 nightly 版会导致链接错误。
	注意特别地，Rust 1.90.0 – 1.93.0 版本无法使用。
	为保证构建可复现，请使用与 CI 完全相同的 Rust 版本：
	```sh
	rustup default 1.89.0
	```
- 在**设置好 Rust 版本之后**，为 rustup 添加 WASM target，以便用 Emscripten 构建 Rust：
	```sh
	rustup target add wasm32-unknown-emscripten
	```
- 需要 Emscripten 用的 `ddnet-libs`。
	最简便的方式是克隆 `ddnet-libs` submodule，使用 https://github.com/ddnet/ddnet-libs/ 提供的预编译库。
	也可以自行构建 Emscripten 用的 `ddnet-libs`，本地编译方法见下文。
	也可以通过手动运行 GitHub workflow [`build-libraries-emscripten`](https://github.com/ddnet/ddnet/blob/master/.github/workflows/build-libraries-emscripten.yml) 来构建这些库。
- 关于可复现构建的重要提示：为保证构建可复现，必须使用完全相同的 emsdk 和 Rust 版本。
	此外，配置时必须使用恰好 CMake 3.22.1 版本。
- 若要在后续步骤中于终端内使用 emsdk，先在终端执行一次 `source .../emsdk/emsdk_env.sh`（路径需正确指向 emsdk）以设置环境变量。

## 在 Windows 上通过 Emscripten 构建的前置条件

- 目前无法直接在 Windows 上通过 Emscripten 构建 DDNet 客户端，原因见 [CMake issue 25049](https://gitlab.kitware.com/cmake/cmake/-/issues/25049)。
	在 Windows 上，CMake 的 Ninja 和 Makefile 生成器会破坏链接选项中必需的 `$` 符号，而 `DEFAULT_LIBRARY_FUNCS_TO_INCLUDE` 标志依赖这些符号。
	这会导致链接步骤因参数转义错误而失败，或者客户端无法启动，因为 SDL 找不到本应通过 `DEFAULT_LIBRARY_FUNCS_TO_INCLUDE` 标志暴露的相关函数。
- 如果你有动力，可以手动修正并以正确参数运行链接命令，从而在 Windows 上成功构建并运行客户端。

## 通过 Emscripten 构建 `ddnet-libs`

- Windows 上的构建注意：本说明中的所有命令都必须在 `bash` 终端（例如 MSYS2）中执行，而不能在 `cmd.exe` 或 PowerShell 中执行。
- Linux 上安装以下依赖：
	```sh
	sudo apt install autoconf automake libtool m4
	```
- Windows 上使用 MSYS2 时安装以下依赖：
	```sh
	pacman -S autoconf-wrapper automake-wrapper libtool unzip
	```
- 注意：在把自建的 `ddnet-libs` 与它合并之前，必须先克隆 `ddnet-libs` submodule。
	否则 `ddnet-libs` 文件夹会缺少各平台共享的通用文件（例如头文件）。
- 有一个脚本可以自动下载并构建所有库。
	这需要联网，耗时约 10–20 分钟：
	```sh
	scripts/compile_libs/gen_libs.sh build-webasm-libs webasm
	```
	**警告**：不要选择 `src` 文件夹内部的目录！
- 如果最初几分钟就出现错误信息，检查输出并确认 emsdk 及其他前置条件安装正确。
- 脚本执行完毕后，会在你指定的输出目录下创建 `ddnet-libs` 目录，其中包含目录结构正确的全部库，可合并进源码目录的 `ddnet-libs`：
	```sh
	find ddnet-libs -type d -name webasm -exec rm -r {} + -prune
	cp -r build-webasm-libs/ddnet-libs/. ddnet-libs/
	```
- 如需强制重新构建这些库，删除库文件夹内各自的 build 目录：
	```sh
	rm -rf build-webasm-libs/compile_libs/*/build_webasm_*
	```
- 如需在更换库版本后强制重新下载，删除整个库构建文件夹：
	```sh
	rm -rf build-webasm-libs
	```

## 通过 Emscripten 构建 DDNet 客户端

- 新建一个用于构建客户端的目录。
- 然后在构建目录中运行 `emcmake cmake .. -G "Unix Makefiles" -DVIDEORECORDER=OFF -DVULKAN=OFF -DSERVER=OFF -DTOOLS=OFF -DPREFER_BUNDLED_LIBS=ON` 完成配置，随后运行 `cmake --build . -j8` 构建。
- 测试时强烈建议通过额外传入 `-DCMAKE_BUILD_TYPE=Debug` 以 debug 模式构建，因为这样能加快构建过程，并附带调试信息和额外检查。
- 注意：由于 [CMake issue 16395](https://gitlab.kitware.com/cmake/cmake/-/issues/16395)，目前无法在 Emscripten 下使用 Ninja 构建系统。

## 通过 Emscripten 运行客户端

- 要在本地测试编译产物，在构建目录中运行 `emrun --browser firefox index.html`。
- 要托管编译好的 Emscripten 客户端，把构建目录中的 `DDNet.data`、`DDNet.js`、`DDNet.wasm` 和 `index.html` 复制到 web 服务器。
	构建文件夹中的 `index.html` 是从 `other/emscripten/index.html` 复制而来的。
- 也可以运行 `other/emscripten/server.py`，用 Python 托管一个最小测试服务器，无需安装 Emscripten。
- 使用正规 web 服务器时，需启用跨源策略，以允许客户端下载其组件。
	例如在基于 Debian 的发行版上使用 apache2：
	```sh
	sudo a2enmod header
	```
	编辑 apache2 配置以允许 `.htaccess` 文件：
	```sh
	sudo nano /etc/apache2/apache2.conf
	```
	在编辑器中把你所在目录的 `AllowOverride` 设为 `All`。
	然后在 web 服务器上（`index.html` 所在处）创建 `.htaccess` 文件，加入以下两行：
	```htaccess
	Header add Cross-Origin-Embedder-Policy "require-corp"
	Header add Cross-Origin-Opener-Policy "same-origin"
	```
	现在重启 apache2：
	```sh
	sudo service apache2 restart
	```

## 常见问题与解决

- 确保 shell 环境中没有设置 `CC` 和 `CXX` 变量，否则会干扰 emsdk 的编译器选择。
