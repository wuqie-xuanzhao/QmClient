#!/usr/bin/env bash
set -euo pipefail

DMG_PATH="${1:?usage: verify_macos_dmg.sh <path-to-dmg>}"
MOUNT_POINT="$(mktemp -d "${TMPDIR:-/tmp}/qmclient-dmg.XXXXXX")"

cleanup()
{
	hdiutil detach "$MOUNT_POINT" -quiet >/dev/null 2>&1 || true
	rmdir "$MOUNT_POINT" 2>/dev/null || true
}
trap cleanup EXIT

hdiutil attach "$DMG_PATH" -readonly -nobrowse -mountpoint "$MOUNT_POINT" >/dev/null

APP_PATH="$MOUNT_POINT/DDNet.app"
MAIN_EXECUTABLE="$APP_PATH/Contents/MacOS/DDNet"

test -d "$APP_PATH"
test -x "$MAIN_EXECUTABLE"
test -x "$APP_PATH/Contents/Frameworks/SDL2.framework/Versions/A/SDL2"

# SDL2.framework contains a versioned framework symlink that makes Apple's
# recursive bundle verifier report an ambiguous bundle. Verify the signed
# code objects directly instead of using --deep on the outer app bundle.
codesign --display --verbose=2 "$APP_PATH"
codesign --display --verbose=2 "$MAIN_EXECUTABLE"
codesign --verify --strict --verbose=2 "$APP_PATH/Contents/Frameworks/SDL2.framework/Versions/A/SDL2"
for dylib in "$APP_PATH"/Contents/Frameworks/*.dylib; do
	codesign --verify --strict --verbose=2 "$dylib"
done

if otool -L "$MAIN_EXECUTABLE" | grep -E '/Users/|/opt/homebrew/|/private/tmp/'; then
	echo "DMG contains an absolute build-machine dependency path" >&2
	exit 1
fi

echo "Verified macOS DMG: $DMG_PATH"
