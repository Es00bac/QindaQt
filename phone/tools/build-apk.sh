#!/bin/bash
# Builds the QindaLink phone APK (org.qindaqt.link) with Qt for Android 6.11.1, arm64-v8a.
#
# Output: build/phone/QindaLink-debug.apk (signed with build/phone/debug.keystore, never committed).
# Needs (all installed through Portage, see phone/README.md): JDK 21, Android SDK/NDK, Qt for
# Android under /opt/qt6-android, host Qt for QT_HOST_PATH, Gradle 9.3.1 (/usr/share/gradle-bin-9.3.1).
# Build parallelism is whatever the tools default to; MAKEOPTS and job counts are never overridden.
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
out="$root/build/phone"
qt=/opt/qt6-android/6.11.1/android_arm64_v8a
sdk=/opt/android-sdk
ndk=$sdk/ndk/29.0.14206865
mkdir -p "$out"

# QindaTK for Android (docs/android.md of QindaTK, D-354). Pinned commit; built into build/phone and
# staged there if the stage is missing. Nothing is installed system-wide.
qtk_rev=${QINDATK_REV:-27da0a0d}
qtk_src="${QINDATK_SRC:-$out/qindatk-src}"
qtk_stage="$out/qindatk-stage"
if [ ! -f "$qtk_stage/lib/cmake/QindaTK/QindaTKConfig.cmake" ]; then
    if [ ! -d "$qtk_src/.git" ]; then
        git clone -q "${QINDATK_REPO:-$HOME/git/QindaTK.git}" "$qtk_src"
    fi
    git -C "$qtk_src" checkout -q "$qtk_rev"
    export JAVA_HOME=/opt/openjdk-bin-21
    cmake -S "$qtk_src" -B "$out/qindatk-build" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$qt/lib/cmake/Qt6/qt.toolchain.cmake" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$qtk_stage" -DQINDATK_BUILD_TOOLS=OFF -DQINDATK_BUILD_EXAMPLES=OFF \
        -DBUILD_TESTING=OFF -DQINDATK_ENABLE_POPPLER_QT6=OFF -DQINDATK_ENABLE_ALSA=OFF \
        -DQINDATK_ENABLE_FFMPEG=OFF -DQINDATK_ENABLE_QUICK3D=OFF -DQINDATK_ENABLE_SHADER_TOOLS=OFF \
        -DQINDATK_ENABLE_DBUS=OFF -DQINDATK_ENABLE_WIDGETS=OFF -DQINDATK_ENABLE_NATIVE_PRINT=OFF \
        -DQINDATK_ENABLE_SQLITE=OFF -DQINDATK_ENABLE_LCMS2=OFF -DQINDATK_ENABLE_LIBVTERM=OFF \
        -DQINDATK_ENABLE_PLUGIN_HOST=OFF
    cmake --build "$out/qindatk-build"
    cmake --install "$out/qindatk-build"
fi

export JAVA_HOME=/opt/openjdk-bin-21
export PATH="$JAVA_HOME/bin:/usr/share/gradle-bin-9.3.1/bin:$PATH"
export ANDROID_SDK_ROOT=$sdk ANDROID_HOME=$sdk ANDROID_NDK_ROOT=$ndk

# Stage the Android package sources: the app's own Java plus the mirror server's, when that lane has landed.
pkg="$out/pkg-src"
rm -rf "$pkg"; mkdir -p "$pkg"
cp -a "$root/phone/app/android/." "$pkg/"
if [ -d "$root/phone/mirror_server/src/main/java" ]; then
    mkdir -p "$pkg/src"
    cp -a "$root/phone/mirror_server/src/main/java/." "$pkg/src/"
fi

"$qt/bin/qt-cmake" -S "$root/phone/app" -B "$out/cmake" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DQT_HOST_PATH=/usr \
    -DANDROID_SDK_ROOT="$sdk" -DANDROID_NDK_ROOT="$ndk" \
    -DQT_ANDROID_BUILD_ALL_ABIS=OFF -DQT_ANDROID_ABIS=arm64-v8a \
    -DQT_ANDROID_COMPILE_SDK_VERSION=36 \
    -DCMAKE_PREFIX_PATH="$qtk_stage" -DCMAKE_FIND_ROOT_PATH="$qtk_stage" \
    -DQINDALINK_ANDROID_PACKAGE_SOURCE="$pkg"
cmake --build "$out/cmake" --target apk

# Re-sign with a keystore that lives only under build/.
ks="$out/debug.keystore"
[ -f "$ks" ] || keytool -genkeypair -keystore "$ks" -storepass android -keypass android -alias qindalink-debug \
    -keyalg EC -groupname secp256r1 -validity 3650 -dname "CN=QindaLink debug"
apk=$(find "$out/cmake" -name '*-debug.apk' -o -name '*debug*.apk' | head -n1)
[ -n "$apk" ] || { echo "no APK produced" >&2; exit 1; }
cp "$apk" "$out/QindaLink-debug.apk"
"$sdk/build-tools/36.0.0/apksigner" sign --ks "$ks" --ks-pass pass:android --key-pass pass:android \
    --ks-key-alias qindalink-debug "$out/QindaLink-debug.apk"
echo "built $out/QindaLink-debug.apk"
