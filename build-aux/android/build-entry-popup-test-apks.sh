#!/bin/bash
#
# Build before/after APKs of the Entry Popup Test demo for PR video recording.
#
# Usage:
#   ./build-aux/android/build-entry-popup-test-apks.sh [aarch64|x86_64]
#
# Output:
#   apks/org.gtk.EntryPopupTest/entry-popup-test-before-unsigned.apk
#   apks/org.gtk.EntryPopupTest/entry-popup-test-after-unsigned.apk
#
# SPDX-License-Identifier: LGPL-2.1-or-later

set -euo pipefail

ARCH="${1:-aarch64}"
APPID="org.gtk.EntryPopupTest"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PATCH_FILES=(
  "gdk/android/gdkandroidpopup.c"
  "gtk/gtktext.c"
)
PRE_PATCH_REF="3ffe53adf82"

export ANDROID_HOME="${ANDROID_HOME:-$HOME/Android/Sdk}"
export ANDROID_SDKVER="${ANDROID_SDKVER:-35.0.0}"
export PATH="${HOME}/.local/bin:$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH"
MESON="${MESON:-meson}"

cd "$ROOT"

if [ ! -d "$ANDROID_HOME" ]; then
  echo "ANDROID_HOME ($ANDROID_HOME) not found." >&2
  exit 1
fi

if [ ! -f "$ANDROID_HOME/toolchain.cross" ]; then
  echo "Setting up Android toolchain.cross..."
  NDK_DIR="$(ls -d "$ANDROID_HOME"/ndk/27.* 2>/dev/null | head -1)"
  if [ -z "$NDK_DIR" ]; then
    echo "No NDK found. Installing ndk;27.2.12479018..."
    sdkmanager --sdk_root="$ANDROID_HOME" "ndk;27.2.12479018"
    NDK_DIR="$ANDROID_HOME/ndk/27.2.12479018"
  fi
  tee "$ANDROID_HOME/toolchain.cross" <<EOF
[constants]
toolchain='${NDK_DIR}/toolchains/llvm/prebuilt/linux-x86_64/'
EOF
fi

if [ ! -d pixiewood ]; then
  git clone https://github.com/sp1ritCS/gtk-android-builder.git --branch android36 pixiewood
fi

if [ ! -d mini-studio ]; then
  git clone --depth 1 https://github.com/sp1ritCS/mini-studio.git
fi

# Ensure GTK subprojects are present before pixiewood rewrites wrap files.
"$MESON" subprojects download
for d in subprojects/*/; do
  [ -f "${d}meson.build" ] || continue
  (cd "$d" && "$MESON" subprojects download) || true
done

# Pixiewood's glib hack patch must be applied for Android (exports g_set_user_dirs).
if [ -f subprojects/glib/glib/gutilsprivate.h ] && \
   ! grep -q 'GLIB_AVAILABLE_IN_ALL' subprojects/glib/glib/gutilsprivate.h; then
  echo "Applying glib Android hack patch..."
  (cd subprojects/glib && patch -p1 < ../../pixiewood/prepare/wraps/glib/hack.patch)
fi

build_apk () {
  local label="$1"

  echo "=== Building $label APK ==="

  rm -rf .pixiewood
  ./pixiewood/pixiewood prepare --release -s "$ANDROID_HOME" -a ./mini-studio \
    --meson "$MESON" "build-aux/android/${APPID}.xml"
  ./pixiewood/pixiewood generate
  ./pixiewood/pixiewood build

  mkdir -p "apks/${APPID}"
  local src
  src="$(find .pixiewood/android/app/build/outputs/apk/release -name '*.apk' | head -1)"
  cp "$src" "apks/${APPID}/entry-popup-test-${label}-unsigned.apk"
  echo "Wrote apks/${APPID}/entry-popup-test-${label}-unsigned.apk"
}

restore_patch_files () {
  git checkout HEAD -- "${PATCH_FILES[@]}"
}

trap restore_patch_files EXIT

echo "Building AFTER (with long-press + nested popup fixes)..."
build_apk after

echo "Reverting to pre-patch GTK for BEFORE build..."
git checkout "${PRE_PATCH_REF}" -- "${PATCH_FILES[@]}"

echo "Building BEFORE (broken long-press + nested popup positioning)..."
build_apk before

restore_patch_files
trap - EXIT

if [ -f "$ANDROID_HOME/build-tools/$ANDROID_SDKVER/apksigner" ]; then
  KEYSTORE=".gitlab-ci/debug.keystore"
  if [ -f "$KEYSTORE" ]; then
    for label in before after; do
      unsigned="apks/${APPID}/entry-popup-test-${label}-unsigned.apk"
      signed="apks/${APPID}/entry-popup-test-${label}.apk"
      "$ANDROID_HOME/build-tools/$ANDROID_SDKVER/apksigner" sign \
        --ks "$KEYSTORE" --ks-pass pass:android \
        --in "$unsigned" --out "$signed"
      echo "Signed: $signed"
    done
  fi
fi

echo ""
echo "Done. Install on device with:"
echo "  adb install apks/${APPID}/entry-popup-test-before.apk"
echo "  adb install apks/${APPID}/entry-popup-test-after.apk"
echo ""
echo "Demo steps for video:"
echo "  1. Open app, long-press main window entry (hold, release)"
echo "  2. Tap 'Open Nested Dialog', long-press dialog entry"
echo "  3. Compare bubble timing and position before vs after"
