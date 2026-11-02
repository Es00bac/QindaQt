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
