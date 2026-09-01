# Building QmClient for iOS

This document covers the iOS dependency build and the current SDL/OpenGL ES
client target. The P3A/P3B implementation is committed in `1eafa9ed3` and
`e5325e77b`.
Native Metal currently remains a macOS-only backend; iOS does not
enable `METAL` or reuse the macOS metallib.

The iOS runtime acceptance record is maintained in
`docs/superpowers/plans/2026-08-30-DDNet20-iOS运行时适配计划.md`. A successful
bundle build is not simulator launch or device validation.

## Requirements

- macOS with Xcode, the iOS SDK, and command-line tools installed.
- CMake 3.20 or newer, Ninja, Git, `curl` (or `wget`), `jq`, `autoconf`,
  `automake`, `libtool`, and GNU m4.
- Rust targets for device and both simulator architectures:

  ```sh
  rustup target add aarch64-apple-ios
  rustup target add aarch64-apple-ios-sim
  rustup target add x86_64-apple-ios
  ```

On Homebrew, install the dependency build tools with:

```sh
brew install autoconf automake cmake jq libtool m4 ninja pkg-config
export M4="$(brew --prefix m4)/bin/m4"
```

## Generate Dependencies

Run from the repository root and use an empty directory outside `src/`:

```sh
scripts/compile_libs/gen_libs.sh build-ios-libs ios
```

The script builds device arm64 plus simulator arm64/x86_64 slices and writes
the required `.xcframework` libraries below the selected directory. Review the
generated tree before merging it into `ddnet-libs`; do not overwrite unrelated
platform libraries.

## Configure And Build

The wrapper selects the SDK, architecture, Rust target, app name, bundle ID and
build configuration:

```sh
scripts/ios/cmake_ios.sh sim QmClient org.qmclient.client Release tmp/cmake-ios-sim
```

Supported first arguments are `device`, `sim-arm64`, `sim-x86_64`, and `sim`
(which selects the host simulator architecture). The default build compiles the
AppIcon asset catalog. The wrapper intentionally disables code signing, so it
can build an unsigned simulator bundle. Device installation requires an Apple
development team and signing configuration in the generated Xcode project or
an equivalent signed `xcodebuild` invocation.

On this checkout, `actool` cannot compile the catalog while no simulator
runtime is installed. Use the following diagnostic-only override to validate
the iOS C++/Rust compile, link and resource copy paths:

```sh
IOS_USE_ASSET_CATALOG=OFF scripts/ios/cmake_ios.sh sim QmClient org.qmclient.client Release tmp/cmake-ios-sim
```

`IOS_USE_ASSET_CATALOG=OFF` is not a release build and does not satisfy the
AppIcon acceptance requirement. After a simulator runtime is installed, rerun
the default command with the asset catalog enabled and then install and launch
the app.

For an Apple Silicon simulator bundle, the output is:

```text
tmp/cmake-ios-sim/Release-iphonesimulator/QmClient.app
```

The bundle must contain the executable, `Info.plist`, `storage.cfg`, and the
`data/` directory. Current compile/link/bundle evidence does not cover GLES
startup, touch input, networking, background recovery, Files export, exit, or
any device behavior.
