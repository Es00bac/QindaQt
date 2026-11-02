# ADR-0433: QindaLink phone companion architecture

- **Status:** Accepted
- **Date:** 2026-11-02
- **Owners:** QindaLink (Platform workgroup)
- **Supersedes:** None
- **Superseded by:** None

## Context

The user wants a phone companion written for QindaQt instead of KDE Connect.
It must provide SMS, notification mirroring, phone apps running as desktop
windows, phone-as-touchpad/keyboard with custom macros, and a password vault
held on the phone that types into the desktop. It also needs a panel applet
and a QindaTK configuration utility. The user chose Qt/QML with QindaTK for the
phone UI, and the name QindaLink.

The facts that shape the design:

- Almost every phone capability involved is reachable only from Android's
  Java framework: `NotificationListenerService`, the SMS/MMS content
  providers and `SmsManager`, `BiometricPrompt`, Android Keystore/StrongBox,
  `NsdManager`, foreground services, and the hidden `DisplayManager`,
  `InputManager` and `IActivityManager` APIs needed to stream apps.
- Notification and SMS forwarding must keep working while no UI is open. A
  Qt activity's process lifetime is tied to its activity, which makes it a
  poor host for an always-on engine.
- No ordinary Android app may create a trusted virtual display, start another
  app's activity on it, or inject input into it. A process running as the
  `shell` user, started over ADB, may do all three. The phone in use
  (SM-S948U, Android 17/SDK 37) is reachable over USB ADB.
- QindaQt already delivers synthetic keyboard and pointer input through the
  RemoteDesktop portal ([ADR-0087](0087-agent-input-via-remotedesktop-portal.md)),
  including Unicode keysyms. That gives remote input and password typing one
  consented and revocable path, with no `uinput` or root access.
- RCS conversations are not exposed to third-party apps on Android. Only
  SMS/MMS history is readable.

## Decision

QindaLink consists of six components with these process boundaries:

| Component | Process | Owns |
| --- | --- | --- |
| Phone engine | Java foreground service in the APK (`org.qindaqt.link`) | Link connection and protocol, pairing keys (Keystore), notifications, SMS/MMS/contacts, battery and network status, vault storage and crypto, outgoing input/macro/vault requests |
| Phone UI | Qt 6.11 QML + QindaTK activity in the same APK | Every screen; holds no secrets beyond what the visible screen shows; talks to the engine through one JNI bridge |
| Phone mirror server | `app_process` as the `shell` user, classes from the installed APK, started by the desktop over ADB | Virtual displays, launching apps on them, H.264/HEVC encoding, input injection, app inventory |
| Link service | `qindaqt-link-service`, user session D-Bus service `org.qindaqt.Link1` | TLS listener, trust store, pairing, ADB tunnel management, protocol routing, notification and SMS bridging, input/macro/secret-typing sink via the RemoteDesktop portal |
| Mirror windows | `qindaqt-link-mirror`, one process per streamed phone app | ADB forward to the mirror server, VA-API decode, one ordinary QindaQt toplevel per phone app, desktop input forwarded to the phone |
| Desktop UI | Panel applet `qindaqt.applets.link` and the QindaTK app `qindaqt-link` | Presentation and user intent only, through the public `Link1` client |

Rules that follow:

1. **Two protocol implementations, one contract.** The phone engine (Java) and
   the link service (C++) each implement the
   [QindaLink protocol](../reference/qindalink-protocol-v1.md). Golden frame
   vectors under `tests/link/vectors/` are the shared contract, and both
   implementations must pass them. A protocol change updates the reference
   page, the vectors and both codecs in one commit.
2. **Secrets do not leave the phone except to be typed.** The vault is phone
   only. The desktop never stores, logs, caches or displays a vault secret.
   It types one through the portal and then zeroes its buffers. The secret
   typing contract is in the [architecture page](../architecture/qindalink.md#secret-typing).
3. **App streaming requires ADB.** The mirror server exists only while ADB is
   up (USB, or Android wireless debugging). Everything else works over the
   TLS link without ADB once the phone is paired.
4. **Desktop input goes through the RemoteDesktop portal only.** The link
   service holds one portal session, approved by the user, with a persisted
   restore token where the backend supports it. It never uses `uinput`, the
   KWin development injector or the test-only `InjectTestInput`.
5. **Source layout.** Desktop modules live in container-wm under
   `src/services/link_*`, `src/shell/link_applet`, `src/apps/link` and
   `src/apps/link_mirror`. The phone app lives in container-wm under `phone/`,
   which the desktop CMake tree never includes. Keeping both sides in one
   repository makes a protocol change atomic. QindaTK gains an Android
   (arm64-v8a, Qt 6.11.1) build in its own repository.
6. **Packaging.** Desktop parts ship in the QindaQt desktop package through
   Portage. The APK is built with Qt for Android (`dev-qt/qt-for-android`),
   the Portage Android SDK and Gradle, and installed on the phone over ADB. It
   installs nothing on the desktop.

## Consequences

- One JNI bridge (phone UI to engine) and one Java/C++ protocol pair have to
  be kept in agreement. The golden vectors and a cross-implementation pairing
  smoke over USB are required evidence for any protocol change.
- Because the phone UI and engine share a process but not a lifetime, the UI
  must render from engine state on every start and must never be the
  connection owner.
- SMS history in the desktop Messages view excludes RCS chats. Those surface
  as mirrored notifications whose reply action works.
- App streaming stops working when ADB drops (cable unplugged, or wireless
  debugging disabled). The UI must say so rather than fail silently.
- The first live use of desktop input asks for one portal approval. Agents
  must announce any action that can raise that prompt before taking it.
- QindaTK's Android build adds a target platform to QindaTK's support matrix.

## Revisit when

- Android exposes a public API for creating trusted virtual displays or for
  cross-app projection without ADB.
- RCS history becomes readable by third-party apps.
- The RemoteDesktop portal path cannot keep a session across logins, which
  would force a QindaQt-native input authority.
