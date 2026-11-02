# QindaLink phone app (`org.qindaqt.link`)

Qt for Android UI plus a Java engine. Architecture and protocol:
`docs/wiki/architecture/qindalink.md`, `docs/wiki/reference/qindalink-protocol-v1.md`.

## Layout

| Path | Contents |
| --- | --- |
| `app/` | CMake project: `main.cpp`, `LinkBridge` (the only JNI seam), `qml/` QindaTK screens (one self-contained file each; `qml-files.cmake` is the shared file list) |
| `preview/` | Desktop preview host `qindalink-preview` (same QML, fake bridge, scripted engine states, `--grab`/`--smoke`) |
| `app/android/` | `AndroidManifest.xml` and the Java engine (`proto` pure codec, `engine` android-free session logic, `android` Keystore/TLS/service/bridge) |
| `engine-tests/` | JVM runner for the golden vectors and a loopback session test |
| `tools/build-apk.sh` | Builds QindaTK for Android (if `build/phone/qindatk-stage` is missing), then builds and signs `build/phone/QindaLink-debug.apk` |
| `tools/fake-desktop.py` | Dev-only desktop peer on port 47199 |

## Build

Everything comes from Portage (JDK 21 `/opt/openjdk-bin-21`, Android SDK/NDK 29.0.14206865,
Qt for Android 6.11.1 under `/opt/qt6-android`, host Qt 6.11.1, Gradle 9.3.1).
Parallelism is left to the tools' defaults.

    phone/engine-tests/run.sh      # JVM vectors + loopback session, about 5 s
    phone/tools/build-apk.sh       # about 3 min cold; APK in build/phone/ (git-ignored)

If `phone/mirror_server/src/main/java` exists its sources are staged into the same APK.

## UI on QindaTK

The screens use `import QindaTK as Tk` (touch density, `sloom-dark`, theme tokens only, Flex/Grid
layout). `build-apk.sh` clones QindaTK main from `~/git/QindaTK.git` (pinned `QINDATK_REV`, default
`27da0a0d`) into `build/phone/qindatk-src`, builds it with `qt.toolchain.cmake` into
`build/phone/qindatk-build`, installs the stage to `build/phone/qindatk-stage`, then configures the app
with that stage as `CMAKE_PREFIX_PATH` and `CMAKE_FIND_ROOT_PATH` and bundles the QML plugin through
`QT_QML_IMPORT_PATH` (QindaTK `docs/android.md`). The first run adds about 10 minutes for QindaTK.
Screens: Home, Pairing (large `123 456` code, equal-weight Accept/Reject, stale-pin "Pair again"),
Permissions (states from `DeviceFacts.java`; features not built yet show "Coming soon"), Tools and
the placeholders Touchpad, Keyboard, Macros, Vault (one file each; replace the file to build the feature).

## Desktop preview

    phone/preview/run-smoke.sh                 # builds, loads every scenario x screen, QML warnings are fatal
    build/phone/preview/qindalink-preview --grab build/phone/shots --size 412x915 --scale 2.625

Use `QT_QPA_PLATFORM=offscreen` (the default for `--grab`/`--smoke`) with `QT_QUICK_BACKEND=software`
or `rhi`. It uses the installed QindaTK; set `QML_IMPORT_PATH` to a directory containing a newer
`QindaTK` module to test against a QindaTK build. Scenarios: offline, found, confirm, stale, ready,
perms-partial (synthetic data only).

## Install and run on a phone

    S=<adb serial>
    adb -s $S install --user 0 -r build/phone/QindaLink-debug.apk
    adb -s $S shell pm grant --user 0 org.qindaqt.link android.permission.POST_NOTIFICATIONS
    adb -s $S shell am start --user 0 -n org.qindaqt.link/org.qtproject.qt.android.bindings.QtActivity

The phone must be unlocked: the activity (which starts the foreground service) does not
run behind the keyguard. The service keeps the engine alive after the UI closes.

## Live check with the fake desktop

    adb -s $S reverse tcp:47160 tcp:47199
    phone/tools/fake-desktop.py --serial $S --auto-accept
    # on the phone: tap "Pair with this desktop", compare the six digits, tap Accept
    adb -s $S reverse --remove tcp:47160

The fake desktop prints frame types, the pairing code, battery level and network type only. It
fetches the phone's public certificate with `adb run-as` (debug build) to trust it. Port 47160
on the host belongs to the real link service; do not bind it from tests.

## Known limits (M1c)

- Only `status` is advertised; `ring` replies `error{unsupported}`.
- Signal strength is `null`; the LAN listener, notifications, SMS and input are out of scope.
- Pairing starts from a button, not automatically, so a stray local listener cannot raise a code.

## Cross-implementation test (real desktop service)

    phone/engine-tests/run-cross-impl.sh <path to qindaqt-link-service>

Drives the real Java `Session` over JSSE TLS 1.3 against the real desktop binary on a private
`dbus-run-session` bus (`--data-dir <tmp> --port 0 --no-adb --no-notifications`), accepts through
Link1 `ConfirmPairing`, checks both codes match and that `Devices()` shows the phone ready with its
status. It touches no real state, port 47160, adb or notifications.
