# QindaLink phone companion

QindaLink connects an Android phone to the QindaQt desktop. It provides SMS,
phone notifications on the desktop, phone apps running as desktop windows,
the phone as a touchpad and keyboard with custom macros, and a password vault
on the phone that types into the focused desktop window. It replaces KDE
Connect for QindaQt and is written from scratch for it.

The durable decisions are
[ADR-0433](../adr/0433-qindalink-phone-companion-architecture.md)
(components and process boundaries) and
[ADR-0434](../adr/0434-qindalink-pinned-tls-transport-and-sas-pairing.md)
(transport, identity and pairing). The wire contract is the
[QindaLink protocol 1](../reference/qindalink-protocol-v1.md).

Current maturity: **design accepted; no implementation integrated.**

## Components

```
 phone (org.qindaqt.link APK)                 desktop (QindaQt session)
┌──────────────────────────────┐            ┌──────────────────────────────────┐
│ QML + QindaTK UI (activity)  │            │ panel applet   QindaLink app     │
│        │ JNI bridge          │            │ (link_applet)  (src/apps/link)   │
│ Java engine (fg service) ◄───┼── TLS 1.3 ─┼─► qindaqt-link-service           │
│  notifications, SMS, vault,  │  pinned    │   org.qindaqt.Link1 (D-Bus)      │
│  status, input/macro sender  │  keys      │   ├─ freedesktop Notifications   │
│                              │            │   └─ RemoteDesktop portal input  │
│ mirror server (shell uid) ◄──┼── adb ─────┼── qindaqt-link-mirror (1/app)    │
└──────────────────────────────┘            └──────────────────────────────────┘
```

| Part | Path | Responsibility |
| --- | --- | --- |
| Protocol codec | `src/services/link_protocol` | Pure framing, envelope and frame value types; pairing math. No sockets. |
| Transport | `src/services/link_transport` | Desktop identity (libcrypto key/cert), `QSslServer` listeners, peer pin checks, connection state machine, keepalive |
| Trust store | inside `link_transport` until it grows | Paired devices in `$XDG_DATA_HOME/qindalink/devices.json`, written atomically |
| ADB tunnels | `src/services/link_adb` | Track devices with `adb track-devices`, check that the app is installed, keep `adb reverse tcp:47160`, run setup grants |
| Service | `src/services/link_service` | `qindaqt-link-service`: owns the above, routes features, exports `org.qindaqt.Link1` |
| Client | `src/services/link_client` | Typed `Link1` client for the applet and the app |
| Applet | `src/shell/link_applet` | `qindaqt.applets.link`: phone chip, battery, unread badge, quick actions |
| App | `src/apps/link` | QindaTK `qindaqt-link`: pairing, Messages, Phone Apps, notification filters, macro editor, phone setup |
| Mirror | `src/apps/link_mirror` | `qindaqt-link-mirror`: one window per streamed phone app. Its pure core `qindaqt_link_mirror_core` holds the mirror framing, input mapping and decoder policy |
| Phone app | `phone/app` | Qt for Android app: QML/QindaTK UI, Java engine under `phone/app/android/` |
| Mirror server | `phone/mirror_server` | Java `app_process` entry point `org.qindaqt.link.mirror.Server`, compiled into the APK, with no dependency on the phone app's code. `build-dev-jar.sh` builds a standalone development jar |
| Shared vectors | `tests/link/vectors` | Golden frame and pairing vectors used by both codecs |

Feature modules (`link_notifications`, `link_messages`, `link_input`,
`link_macros`) are added under `src/services/` as each feature lands. Each
one depends only on `link_protocol` and the narrow routing interface of
`link_service`.

## Connection and pairing

- The phone engine always tries `127.0.0.1:47160` first. That address works
  whenever the desktop has an ADB reverse tunnel to the phone.
- When LAN mode is on, the phone then tries the desktops it has pinned, using
  addresses from `_qindalink._tcp` discovery or from a manually entered
  address (which also covers Tailscale).
- Pairing compares a six-digit code on both screens; see the
  [protocol reference](../reference/qindalink-protocol-v1.md#pairing).
- The desktop presents an incoming pairing request as a notification with
  Accept and Reject actions, and also in the QindaLink app.

## Phone setup over ADB

A sideloaded app on Android 17 cannot get notification access, SMS
permissions or an exemption from battery optimisation without several manual
Settings visits, some of them behind "restricted settings". With ADB
connected, the QindaLink app's **Set up phone** action runs the equivalent
grants and shows each result:

```
pm grant org.qindaqt.link android.permission.{READ_SMS,SEND_SMS,RECEIVE_SMS,READ_CONTACTS,POST_NOTIFICATIONS,READ_PHONE_STATE}
appops set org.qindaqt.link ACCESS_RESTRICTED_SETTINGS allow
cmd notification allow_listener org.qindaqt.link/<listener class>
cmd deviceidle whitelist +org.qindaqt.link
```

The service uses Portage's `/usr/bin/adb` (`dev-util/android-tools`). A user
`adb` elsewhere on `PATH` speaks the same server protocol and can share the
running ADB server.

## Secret typing

The vault exists only on the phone.
[ADR-0433](../adr/0433-qindalink-phone-companion-architecture.md) rule 2 is
enforced by this contract:

1. **Phone storage.**
   - Each entry is encrypted with AES-256-GCM.
   - The vault key is wrapped by an Android Keystore key, backed by StrongBox
     when available. That key requires user authentication, either
     class-3 biometric or the device credential.
   - The vault activity sets `FLAG_SECURE`.
   - Secret fields use sensitive input hints so the keyboard neither learns
     nor suggests them.
2. **Choosing a target.** Before typing, the phone requests the focused
   desktop window (`desktop.focus`) and shows its application and title. The
   user confirms that target.
3. **Sending.** The phone sends one `vault.type` frame. It carries the steps
   (text, named keys, delays) and the target token from step 2.
4. **Desktop checks.** The link service types only if the focused window still
   matches the token. It checks focus again between steps and aborts with
   `target-changed` on any change.
5. **Delivery.** Typing goes through the RemoteDesktop portal session as
   keysyms. A secret is never placed on the clipboard.
6. **Desktop handling of the secret.** The service never logs a `vault.*`
   body, never persists it and never shows it. It zeroes its own buffers
   after typing and sets itself non-dumpable (`PR_SET_DUMPABLE 0`). JSON
   parsing may leave transient copies inside Qt containers. Zeroing is best
   effort, and the guarantee is no persistence and no logging.

Typing into the QindaQt lock screen is out of scope. The portal does not
deliver input while the session is locked, and this design does not try to
work around that.

## Phone apps on the desktop

`qindaqt-link-mirror --serial <serial> --package <package>` shows one phone app
in one ordinary desktop window. The wire contract, the server command line and
the verified hidden-API shapes are in
[QindaLink mirror protocol 1](../reference/qindalink-mirror-v1.md).

```
qindaqt-link-mirror                                phone (shell uid, app_process)
┌──────────────────────────────────────┐          ┌──────────────────────────────────┐
│ AdbSession: push jar / pm path,      │── adb ──►│ Server: probe hidden API, listen │
│   start server, forward tcp:0        │          │   localabstract:qindalink-mirror-│
│ MirrorConnection: hello, demux ◄─────┼─ frames ─┤   <token>                        │
│ DecodeWorker: libavcodec VA-API      │          │ VirtualDisplayHost ─ MediaCodec  │
│   (software fallback), newest frame  │          │ AppLauncher (launch on display)  │
│ MirrorWindow: letterboxed presenter, │─ input ─►│ InputInjector (display id)       │
│   InputTranslator / ViewportMapper   │          │ DisplayPower (per-display lock)  │
│ MirrorController: resize debounce,   │          │ AppInventory (labels, PNG icons) │
│   cleanup                            │          └──────────────────────────────────┘
└──────────────────────────────────────┘
```

- **Start.** The desktop starts the mirror server on the phone with
  `app_process` as the `shell` user. The class path is the installed
  QindaLink APK, or a development jar passed with `--server-jar`. The socket
  name carries a random per-session token.
- **Display.** The server creates a trusted virtual display that owns its
  content, accepts touch, has its own display group and destroys its content
  when removed. It is sized to the window in device pixels at
  160 dpi × the window's device pixel ratio. The server launches the app's
  launcher activity on it and encodes it with MediaCodec (H.264 High, or HEVC
  with `--codec hevc`).
- **Power and keyboard.** The phone's own screen and keyboard stay out of the
  way. A per-display wake lock keeps only the mirror display awake, and the
  display's IME policy hides the phone keyboard because the desktop types
  instead.
- **Decode and present.** The desktop decodes with libavcodec, VA-API first
  (radeonsi on the RX 5700 XT), falling back to software automatically. It
  presents the newest frame letterboxed in a window titled with the app's
  label and icon, with the app id `org.qindaqt.link.mirror.<package>`.
- **Input.** The primary button is a finger. The secondary button is Back.
  The wheel scrolls, typing is sent as text, and chords and special keys are
  sent as Android key events. All of it is injected with the mirror display's
  id, so it never reaches the phone's own screen.
- **Resize and close.** Resizing the window resizes the virtual display after
  a 250 ms debounce, and the encoder restarts as a new stream generation.
  Closing the window, or Ctrl+C, ends the session. The server then releases
  the display, which ends the app's task on it. The desktop removes its ADB
  forward.
- **ADB loss.** Streaming needs ADB and stops when ADB drops. A visible window
  then shows the reason until it is closed.
- **Evidence modes.** `--grab`, `--probe` and `--list` run on the offscreen
  Qt platform and never map a window. They are the evidence path for agents.

## Known limits

- RCS chats are not readable by third-party Android apps. Messages shows
  SMS/MMS history, and RCS conversations appear as mirrored notifications
  whose reply action still works.
- Streaming requires USB ADB or Android wireless debugging.
- Phone-to-desktop input and secret typing need one RemoteDesktop portal
  approval. Agents announce any action that can raise that prompt before
  taking it.

## Delivery plan

| Milestone | Outcome | Evidence |
| --- | --- | --- |
| M1a | QindaTK builds for Android arm64 (QindaTK repo) | QindaTK controls render in an APK on the phone |
| M1b | Desktop link core: codec, TLS, pairing, trust, ADB tunnel, `Link1` | Golden vectors pass; a scripted TLS peer pairs and reports status over `busctl` |
| M1c | Phone engine and app skeleton: foreground service, Keystore identity, pairing and status | The Java codec passes the same vectors; the phone pairs with the desktop over USB |
| M1d | Phone apps as desktop windows over ADB | One phone app runs in a desktop window with working touch, keys and resize |
| M2 | Notifications, SMS, input and macros, vault typing, panel applet, QindaTK app | Per-feature focused tests and one USB end-to-end smoke each |
| M3 | LAN discovery, wireless ADB streaming, audio forwarding, desktop package revision | A signed Portage build and a LAN smoke |
