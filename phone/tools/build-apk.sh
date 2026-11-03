#!/bin/bash
# Builds the QindaLink phone APK (org.qindaqt.link) with Qt for Android 6.11.1, arm64-v8a.
#
# Output: build/phone/QindaLink-debug.apk, signed with the shared per-user debug key
# ($QINDALINK_KEYSTORE, default ~/.local/share/qindalink-dev/debug.keystore; never committed).
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
qtk_rev=${QINDATK_REV:-9ff55fbd}
# AGENT-NOTE: ONE QindaTK-for-Android stage per QindaTK revision, shared by every
# worktree (user requirement 2026-11-02: no repeated huge builds). It used to live
# in each worktree's build/phone and every phone lane rebuilt it (~10 min, 4.5 GB).
qtk_cache="${QINDATK_ANDROID_CACHE:-$HOME/.cache/qindalink/qindatk-android}"
qtk_stage="$qtk_cache/$qtk_rev"
qtk_src="${QINDATK_SRC:-$qtk_cache/src}"
qtk_build="$qtk_cache/build-$qtk_rev"
launcher=()
command -v ccache >/dev/null && launcher=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
# AGENT-GUARD: the stage records the QindaTK revision it was built from; a pin
# change must rebuild it, or an old stage (e.g. one missing the D-355 Android
# qmldir fix) is silently reused and `import QindaTK` fails on the phone.
if [ ! -f "$qtk_stage/lib/cmake/QindaTK/QindaTKConfig.cmake" ] \
        || [ "$(cat "$qtk_stage/.qindatk-rev" 2>/dev/null)" != "$qtk_rev" ]; then
    rm -rf "$qtk_stage" "$qtk_build"
    mkdir -p "$qtk_cache"
    if [ ! -d "$qtk_src/.git" ]; then
        git clone -q "${QINDATK_REPO:-$HOME/git/QindaTK.git}" "$qtk_src"
    fi
    git -C "$qtk_src" cat-file -e "$qtk_rev^{commit}" 2>/dev/null || git -C "$qtk_src" fetch -q origin
    git -C "$qtk_src" checkout -q "$qtk_rev"
    export JAVA_HOME=/opt/openjdk-bin-21
    cmake -S "$qtk_src" -B "$qtk_build" -G Ninja "${launcher[@]}" \
        -DCMAKE_TOOLCHAIN_FILE="$qt/lib/cmake/Qt6/qt.toolchain.cmake" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$qtk_stage" -DQINDATK_BUILD_TOOLS=OFF -DQINDATK_BUILD_EXAMPLES=OFF \
        -DBUILD_TESTING=OFF -DQINDATK_ENABLE_POPPLER_QT6=OFF -DQINDATK_ENABLE_ALSA=OFF \
        -DQINDATK_ENABLE_FFMPEG=OFF -DQINDATK_ENABLE_QUICK3D=OFF -DQINDATK_ENABLE_SHADER_TOOLS=OFF \
        -DQINDATK_ENABLE_DBUS=OFF -DQINDATK_ENABLE_WIDGETS=OFF -DQINDATK_ENABLE_NATIVE_PRINT=OFF \
        -DQINDATK_ENABLE_SQLITE=OFF -DQINDATK_ENABLE_LCMS2=OFF -DQINDATK_ENABLE_LIBVTERM=OFF \
        -DQINDATK_ENABLE_PLUGIN_HOST=OFF
    cmake --build "$qtk_build"
    cmake --install "$qtk_build"
    echo "$qtk_rev" > "$qtk_stage/.qindatk-rev"
    rm -rf "$qtk_build"  # the stage is all the app needs; keep the cache small
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

"$qt/bin/qt-cmake" -S "$root/phone/app" -B "$out/cmake" -G Ninja "${launcher[@]}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DQT_HOST_PATH=/usr \
    -DANDROID_SDK_ROOT="$sdk" -DANDROID_NDK_ROOT="$ndk" \
    -DQT_ANDROID_BUILD_ALL_ABIS=OFF -DQT_ANDROID_ABIS=arm64-v8a \
    -DQT_ANDROID_COMPILE_SDK_VERSION=36 \
    -DCMAKE_PREFIX_PATH="$qtk_stage" -DCMAKE_FIND_ROOT_PATH="$qtk_stage" \
    -DQINDALINK_ANDROID_PACKAGE_SOURCE="$pkg"
cmake --build "$out/cmake" --target apk

# Re-sign with ONE per-user development key shared by every worktree.
# AGENT-NOTE: Android refuses to update an installed app signed with a different
# key (INSTALL_FAILED_UPDATE_INCOMPATIBLE), and uninstalling wipes the phone's
# Keystore identity and desktop pin. A per-worktree key therefore forced
# re-pairing on every lane's build. The key is a debug key, never committed;
# release signing is a packaging decision for later.
ks="${QINDALINK_KEYSTORE:-$HOME/.local/share/qindalink-dev/debug.keystore}"
if [ ! -f "$ks" ]; then
    mkdir -p "$(dirname "$ks")"
    keytool -genkeypair -keystore "$ks" -storepass android -keypass android -alias qindalink-debug \
        -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=QindaLink Debug"
    chmod 600 "$ks"
fi
apk=$(find "$out/cmake" -name '*-debug.apk' -o -name '*debug*.apk' | head -n1)
[ -n "$apk" ] || { echo "no APK produced" >&2; exit 1; }
cp "$apk" "$out/QindaLink-debug.apk"
"$sdk/build-tools/36.0.0/apksigner" sign --ks "$ks" --ks-pass pass:android --key-pass pass:android \
    --ks-key-alias qindalink-debug "$out/QindaLink-debug.apk"
echo "built $out/QindaLink-debug.apk"
