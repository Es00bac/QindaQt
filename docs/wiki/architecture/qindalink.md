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
(transport, identity and pairing). Macros follow
[ADR-0436](../adr/0436-qindalink-macros-reuse-the-input-actions-library.md)
(ADR-0427's library, one portal session). The wire contract is the
[QindaLink protocol 1](../reference/qindalink-protocol-v1.md).

Current maturity: **M1 link core, panel applet, phone apps on the desktop, notifications, SMS and the QindaTK app (pairing, Messages, Phone apps, mute list, USB setup, reply window) integrated; on-device pairing, notification mirroring and SMS paging verified on the user's phone (2026-11-02). Input and macros are integrated with fake-portal evidence only (the first live portal run needs the user's approval). The vault is not integrated yet.**

- **Desktop.** The codec, TLS 1.3 transport, code-comparison pairing,
  unpaired-connection bounds, ADB tunnels, `qindaqt-link-service` with
  `org.qindaqt.Link1` and the typed client are integrated. See
  [Link1 protocol 1](../reference/link1-v1.md).
- **Phone.** The Java engine and the QindaTK UI are integrated. The UI uses
  QindaTK `9ff55fbd` (D-355), and the APK is signed with the shared per-user
  debug key.
- **Panel applet** (`qindaqt.applets.link`, [ADR-0437](../adr/0437-qindalink-panel-applet-and-phone-capability-tokens.md)).
  Integrated with fake-transport and offscreen evidence. It has not been
  checked in a live session yet.
- **Phone apps on the desktop** ([ADR-0435](../adr/0435-decode-qindalink-streams-with-ffmpeg.md)).
  On the phone, these were proven by the calculator streaming, input
  (0 → 74), resize and app exit. Mirroring pauses when the phone screen turns
  off, by the user's decision.
- **Notifications and SMS.** Both are integrated, after independent review
  and a repair round. Caller authority for the sensitive Link1 methods is the
  next slice.
- **Evidence on the user's phone (SM-S948U, Android 17, 2026-11-02):**
  - Pairing over USB with the Keystore identity: both sides showed the same
    code, and both trust stores were written.
  - Reconnection after a service restart without pairing again.
  - QindaTK UI rendering.
  - A 45-notification snapshot plus a test notification mirrored (recorded
    as lengths and hashes only).
  - 429 SMS threads paged with no duplicates, including one thread of 27,665
    messages (19,539 SMS, 8,126 MMS), with no content printed.
- **Shared evidence.**
  - Both codecs pass every golden vector.
  - `phone/engine-tests/run-cross-impl.sh` pairs the real Java session with
    the real desktop service on a private bus.
- **Input and macros** ([ADR-0436](../adr/0436-qindalink-macros-reuse-the-input-actions-library.md)).
  Integrated after review and a repair round; tested against a fake portal and
  a fake `InputActions1`. See [Input and macros](#input-and-macros). The first
  live run against the real portal needs the user's approval and is **not yet
  recorded**.
- **LAN and Tailscale** ([ADR-0441](../adr/0441-qindalink-lan-listener-and-discovery.md)).
  Implemented behind the protected `SetLanEnabled` setting and tested against a
  `127.0.0.2` stand-in LAN, a fake Avahi daemon and a fake NSD. No live Wi-Fi run is
  recorded yet; it needs the user's approval and an open firewall port.
- **Not implemented yet:** the vault (in
  progress) and the QindaTK desktop app. Sending an SMS has never
  been exercised live; it needs the user's approval.

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
| Trust store | inside `link_transport` (`trust_store.h`; implemented) | Paired devices in `$XDG_DATA_HOME/qindalink/devices.json`, written atomically |
| ADB tunnels | `src/services/link_adb` | Track devices with `adb track-devices`, check that the app is installed, keep `adb reverse tcp:47160`, run setup grants |
| Notifications | `src/services/link_notifications` | `notify` capability: presenter interface, icon cache, mute list, `org.qindaqt.Link1.Notifications` adaptor |
| Service | `src/services/link_service` | `qindaqt-link-service`: owns the above, routes features, exports `org.qindaqt.Link1` |
| Client | `src/services/link_client` | Typed `Link1` client for the applet and the app |
| Applet | `src/shell/link_applet` | `qindaqt.applets.link`: phone chip, battery, unread badge slot, pairing UI, quick actions (built; see [applet page](../shell/link-applet.md)) |
| App | `src/apps/link` | QindaTK `qindaqt-link`: pairing, Messages, Phone Apps, notification filters, Settings link for macros, phone setup (built; see [app page](../apps/qindalink.md)). There is no macro editor in the app (ADR-0436) |
| Mirror | `src/apps/link_mirror` | `qindaqt-link-mirror`: one window per streamed phone app. Its pure core `qindaqt_link_mirror_core` holds the mirror framing, input mapping and decoder policy |
| Input | `src/services/link_input` | `input` capability: the single RemoteDesktop portal session (`PortalInputSession`, an `InputSink`), frame parsing and bounds, text batching, held-key ledger, the `input.*` handler |
| Macros | `src/services/link_macros` | `macro` capability: `MacroCatalog` (InputActions1 client), `PortalMacroOutput` (the shared `MacroRunner`'s output over the sink), the `macro.*` handler |
| Phone app | `phone/app` | Qt for Android app: QML UI built only from QindaTK controls (Home, Pairing, Permissions, Tools, Touchpad, Keyboard, Macros and the Vault placeholder, bottom tab bar; `DeviceFacts.java` supplies battery, network and permission states), Java engine under `phone/app/android/` |
| Phone UI preview | `phone/preview` | `qindalink-preview`: the same QML on the desktop with a fake bridge and scripted states; `--grab` writes one PNG per screen, `--smoke` fails on any QML warning |
| Phone engine (Java) | `phone/app/android/src/org/qindaqt/link/{proto,engine,android}` | `proto`: pure codec, JSON, pairing math (no `android.*`); `engine`: session state machine, reconnect loop, pin store; `android`: Keystore identity, pinned TLS 1.3 connector, `connectedDevice` foreground service, status source, the single JNI bridge class |
| Phone tests and tools | `phone/engine-tests`, `phone/tools` | JVM vector and loopback-session runner (`run.sh`), `build-apk.sh`, dev-only `fake-desktop.py` (see `phone/README.md`) |
| Mirror server | `phone/mirror_server` | Java `app_process` entry point `org.qindaqt.link.mirror.Server`, compiled into the APK, with no dependency on the phone app's code. `build-dev-jar.sh` builds a standalone development jar |
| Shared vectors | `tests/link/vectors` | Golden frame and pairing vectors used by both codecs |

Feature modules (`link_notifications`, `link_messages`, `link_input`,
`link_macros`) are added under `src/services/` as each feature lands. Each
one depends only on `link_protocol` and the narrow routing interface of
`link_service`.

**Adding a feature.** A feature owns one capability string and its frame
namespaces, and plugs in without editing the session state machine. Desktop:
subclass `FeatureHandler` (`src/services/link_transport/.../feature.h`) and
`add()` it to the `FeatureRouter`; the router supplies the hello caps and the
service calls `handle()` for READY sessions. Phone: implement
`engine/FeatureHandler` (`capability()`, `namespaces()`, `onReady`, `onTick`,
`onFrame`, `onClosed`) and add it to the `FeatureRegistry` that `LinkService`
(or a test) passes to `LinkEngine`; `Session` advertises the registered
capabilities, routes only READY frames by namespace, answers unhandled
requests with `error{unknown-type}`, and `FeatureContext.send` refuses a frame
whose namespace the handler does not own or whose capability the peer did not
advertise. `status` is the first such feature (`StatusFeature`). Core frames
(`hello`, `bye`, `ping`, `pong`, `ok`, `error`, `pair.*`) stay in `Session`.

**Phone app maturity (M1c, 2026-11).** Built and installed as a debug APK;
the Java codec and pairing math pass all golden vectors, and a JVM loopback
test drives the whole phone session (pairing, pin, status, `ring` refused).
The UI is plain QtQuick (to be moved onto QindaTK) and talks to the engine
through one polled JNI bridge. The `status`, `notify`, `sms`, `input`, `macro`
and `ring` capabilities are advertised. `ring` (`ring/RingFeature` over a `RingSink`; Android
implementation plays the alarm stream at full volume, posts a Stop/full-screen notification and
restores the volume) is proven end to end against the real service with a fake sink; a live ring
on hardware needs the user's approval and is not yet recorded. `hello.name` is the user-visible
device name (`Settings.Global.device_name`, then the model) and `status.network.signal` carries a
0-4 level for Wi-Fi and cellular, `null` when unknown. Pairing with the real desktop service is proven by the cross-implementation
test; on-device pairing over USB is not yet recorded.

## LAN and Tailscale

Off by default. `SetLanEnabled(true)` (a trusted caller, see the
[Link1 reference](../reference/link1-v1.md)) persists `lanEnabled` and starts
`LanController` ([ADR-0441](../adr/0441-qindalink-lan-listener-and-discovery.md)):

- **Listeners.** One TLS listener per up, non-loopback interface address on
  port 47160 (IPv4 and IPv6 global/ULA; no link-local or container bridges),
  re-synced every 15 s. Tailscale's `tailscale0` address is covered the same way.
  A failed bind is reported in `LanStatus` and retried.
- **Advert.** `_qindalink._tcp` through Avahi with TXT `proto=1`,
  `id=<first 16 hex of the desktop SPKI digest>`, `name=<hostname>`, only while a
  listener is bound. Without Avahi the listeners still work and the phone can use
  its manual address.
- **Pairing.** Only inside the window opened by `OpenPairingWindow` (120 s).
  A paired phone reconnects over LAN without a window.
- **Phone.** Setup has a Connection section: phone network, the desktops
  found, a manual address (for example the desktop's Tailscale IP or MagicDNS
  name, optionally `:port`), and Pair over Wi-Fi, which tells the user to open
  the pairing window on the desktop. The Home screen reports the transport
  truthfully (`USB`, or `Wi-Fi or Tailscale`).
- **Firewall.** The service never edits firewall rules. TCP 47160 must be
  reachable on the interface in use. On the development desktop, read-only checks
  on 2026-11-02 showed `nftables`, `firewalld` and `ufw` inactive and
  `qindafirewall.service` disabled; the kernel ruleset itself needs root to read
  and was not inspected. If a firewall blocks the port, the user allows inbound
  TCP 47160 from the home network or the Tailscale interface, for example in the
  QindaFirewall app ("let the home network in") or with an `nft` rule such as
  `tcp dport 47160 accept` on the right interface; the service does not do this.
- **Android 17.** The app declares `ACCESS_LOCAL_NETWORK` (local-network access
  is gated for apps that target API 37; this build targets 36) and needs no
  multicast-lock or `NEARBY_WIFI_DEVICES` permission because `NsdManager` uses
  the system mDNS daemon. Whether Android 17 blocks a target-36 app from
  private-range connections or mDNS is a live-test item.

Evidence: desktop rows `qindaqt.link-lan`, `qindaqt.link-lan-service`,
`qindaqt.link-avahi` and the introspection coverage sweep; phone `LanTest`
(candidate ordering, TXT filtering, manual address parsing, backoff, transport
reporting, USB takeover, all with a fake NSD and opener).

## Notifications

Capability `notify`; the frames are in the
[protocol reference](../reference/qindalink-protocol-v1.md#notifications-notify).

- **Phone.** `NotificationListener` (a `NotificationListenerService`) feeds
  `NotificationAndroidSource`, which turns each notification into a
  `PhoneNotification`: ongoing, foreground-service, group-summary and
  QindaLink's own notifications are dropped, and the app icon is rendered to a
  64 px PNG and hashed. The pure-Java `NotificationFeature` coalesces events,
  sends a snapshot (at most 50) on READY and executes `notify.dismiss` and
  `notify.action` (`cancelNotification`, `PendingIntent.send`, replies through
  `RemoteInput.addResultsToIntent`). Without notification access the engine
  state key `notifyAccess` is `missing` so the UI can say "notification access
  needed". Text that Android redacts (Android 15+, sensitive content such as
  one-time codes) is forwarded as received.
- **Desktop.** `NotificationFeature` shows each notification through an
  injected `NotificationPresenter`. Production is
  `FdoPhoneNotificationPresenter` on `org.freedesktop.Notifications`: the app
  name is `<label> (phone)`, the cached PNG
  (`$XDG_CACHE_HOME/qindalink/icons/<sha256>.png`, verified against its hash)
  is passed as both `app_icon` and the `image-path` hint, and actions are
  `(key, label)` pairs. When the server advertises `body-markup` in
  `GetCapabilities` (and until that answer arrives) title and text are escaped.
  The icon cache keeps at most 256 files, evicting the least recently used. Only a `NotificationClosed` with reason 2 (dismissed by
  the user) sends `notify.dismiss`; closing because the phone removed the
  notification, or because the server closed it after an action button, never
  does. Only the two seconds after an action button count as that case.
- **Reply.** QindaQt's notification server has no inline reply, so a reply
  action appears as a "Reply…" button, and only when `org.qindaqt.Link.desktop`
  is installed. It runs `qindaqt-link --reply -- <device> <key> <action>` (the
  `--` keeps a phone-controlled key from being read as an option); the
  reply window itself belongs to the QindaLink app. Until then the same
  request is available as `Link1.Notifications.Reply`. Inline reply in QindaQt's
  own notification server is a follow-up.
- **Mute.** `$XDG_CONFIG_HOME/qindalink/notifications.json`, written
  atomically, holds muted packages and the apps seen. Muting closes what that
  app currently shows. Muted apps are not tracked, so unmuting does not bring
  old notifications back.
- **Privacy.** Titles, texts and reply texts are never logged on either side
  (frame type and size only). The recording presenter used by tests stores
  lengths and a hash of the title, not text. Because the desktop keeps phone
  notifications across a reconnect and reconciles them with the next snapshot,
  restarting the link service shows the current set once more.
- **Live check recipe** (needs the phone plugged in and unlocked, and an APK signed
  with the same key as the installed one, because `adb install -r` refuses a
  signature change). Install the APK, then:
  `adb -s <serial> shell cmd notification allow_listener org.qindaqt.link/org.qindaqt.link.android.NotificationListener`;
  start `qindaqt-link-service --recording-notifier /tmp/rec.jsonl --no-adb` on a
  private `dbus-run-session` bus with `adb reverse tcp:47160 tcp:47160` pointing at
  its `--port 47160`, pair, then
  `adb -s <serial> shell cmd notification post -t "QindaLink test" qltest hello`
  and look for a `present` line in `/tmp/rec.jsonl`.

## Messages (`sms`)

The `sms` capability is the data path for the later QindaTK Messages UI; it
has no UI of its own.

- **Phone.** `org.qindaqt.link.sms` holds the pure part (`SmsModel` for
  paging and bounds, `SmsFeature`, `SendTracker`) behind the `SmsProvider`,
  `ContactResolver` and `SmsSender` interfaces, so it runs in the JVM tests
  over fakes. `org.qindaqt.link.android` supplies the real ones:
  `SmsProviderAndroid` (content://mms-sms, sms, mms with a debounced
  `ContentObserver`), `ContactsResolver` and `SmsSenderAndroid`
  (`SmsManager`, multipart). Permissions (READ_SMS, READ_CONTACTS, SEND_SMS)
  are runtime grants; engine state carries only a `read/send` summary.
- **Desktop.** `src/services/link_messages`: `MessagesFeature` (a
  `FeatureHandler` that issues requests and validates every phone reply
  against the protocol bounds), `UnreadTotals` (the only retained state: counts)
  and `MessagesAdaptor`, which exports `org.qindaqt.Link1.Messages` at
  `/org/qindaqt/Link1/Messages` with delayed replies. Link1Service only
  registers them. Message content is never persisted or logged.
- **Vendor providers.** Thread queries use capability probing (`FallbackChain`):
  `?simple=true` with the full projection first, then the plain conversations URI
  enriched from each thread's newest message. This matters on the Samsung
  SM-S948U (Android 17), where the plain URI lacks `message_count`,
  `recipient_ids` and `snippet`. MMS rows are fetched as headers and hydrated
  (parts, address) only when they make the page.
- **Limits.** RCS is invisible (not in the provider). QindaLink is not the
  default SMS app, so marking a thread read is attempted and answered
  `unsupported` when Android refuses, and messages it sends are not recorded
  in the provider by the platform.

## Who may call Link1

The session bus is shared by every process of the user, so `qindaqt-link-service`
authorizes callers itself ([ADR-0438](../adr/0438-qindalink-link1-caller-authority.md)).
Device metadata (`Devices`, `PendingPairings`, change signals) is open.
Reading messages or notifications, sending, replying, muting and
pairing decisions need a caller whose executable is `qindaqt-link`,
`qindaqt-shell` or given with `--trusted-caller`; others get
`org.qindaqt.Link1.Error.Denied`. Broadcast signals never carry message or
notification text, and every accepted text send raises a desktop notice and
counts against a per-device rate limit.

## Connection and pairing

- The phone engine always tries `127.0.0.1:47160` first. That address works
  whenever the desktop has an ADB reverse tunnel to the phone.
- When LAN mode is on, the phone then tries the last LAN address that worked,
  the desktops it has pinned (addresses from `_qindalink._tcp` discovery, kept
  only when the TXT `id` is a prefix of the pinned SPKI id), and finally a
  manually entered address (which also covers Tailscale). It backs off with
  jitter, searches only while not connected, and moves from LAN to USB when the
  cable comes back. See [LAN and Tailscale](#lan-and-tailscale).
- Pairing compares a six-digit code on both screens; see the
  [protocol reference](../reference/qindalink-protocol-v1.md#pairing).
- The desktop presents an incoming pairing request as a notification with
  Accept and Reject actions, and also in the QindaLink app.

## Phone setup over ADB

A sideloaded app on Android 17 cannot get notification access, SMS
permissions or an exemption from battery optimisation without several manual
Settings visits, some of them behind "restricted settings". With ADB
connected, the QindaLink app's **Set up phone** action asks the service
(`org.qindaqt.Link1.Setup`) to run the equivalent grants and shows each result:

```
pm grant org.qindaqt.link android.permission.{READ_SMS,SEND_SMS,RECEIVE_SMS,READ_CONTACTS,POST_NOTIFICATIONS,READ_PHONE_STATE}
appops set org.qindaqt.link ACCESS_RESTRICTED_SETTINGS allow
cmd notification allow_listener org.qindaqt.link/<listener class>
cmd deviceidle whitelist +org.qindaqt.link
```

The service uses Portage's `/usr/bin/adb` (`dev-util/android-tools`). A user
`adb` elsewhere on `PATH` speaks the same server protocol and can share the
running ADB server.

## Input and macros

The phone is a touchpad, a keyboard and a macro pad
([protocol](../reference/qindalink-protocol-v1.md#remote-input-input),
[ADR-0436](../adr/0436-qindalink-macros-reuse-the-input-actions-library.md)).

```
 phone: Touchpad / Keyboard / Macros screens
   │ input.* / macro.*                       desktop: qindaqt-link-service
   ▼                                          ┌───────────────────────────────┐
 InputFeature ── HeldInput ── PortalInputSession ──► RemoteDesktop portal
 MacroFeature ── MacroRunner ── PortalMacroOutput ─┘  (one session, approved once)
        │             ▲
        │      MacroCatalog ◄── org.qindaqt.InputActions1 (GetSnapshot, Changed)
        └── InputGate (native compositor Unlocked receipt)
```

- **One approval.** The portal session is created only when the controlling
  phone sends `input.begin`. It requests keyboard and pointer with
  `persist_mode` 2; the restore token lives in
  `$XDG_STATE_HOME/qindalink/restore-token` (0600) and is discarded when the
  portal revokes the session. The session closes when its controller ends,
  disconnects, locks the desktop or loses READY.
- **Reusable sink.** `InputSink` is the only way QindaLink moves desktop input.
  Features wrap it in a `HeldInput` ledger so that a disconnect, lock, cancel or
  revoke releases every key and button. Secret typing reuses the sink and
  `TextTyper`.
- **Gate.** Input flows only while the portal is active and the gate is open.
  The production gate (`NativeInputGate`) opens only on an authenticated native
  Unlocked receipt, with the compositor PID taken from the Wayland socket's
  kernel peer and checked against the bus owner. Unknown, Locking and Locked
  are closed.
- **Macros.** The catalog is read from `InputActions1`; the runner is the shared
  `MacroRunner`. Compositor-only actions fail the run. See the ADR.
- **Phone.** `TouchGestures` (pure Java) turns touches into frames: one-finger
  move, tap = left click, two-finger tap = right click, two-finger scroll,
  press-hold-drag; `InputFeature` and `MacroFeature` hold the sender state.

### First live run (needs the user's approval, not yet done)

The prompt is a desktop remote-control approval. Tell the user first, then:

1. Start `qindaqt-link-service` normally and pair the phone.
2. Open the phone's Tools → Touchpad. The desktop shows the portal approval once.
3. Approve, move the pointer, tap, type a short text, and run one macro.
4. Revoke the access in the desktop's remote-control settings and confirm the
   phone shows the approval state again.

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

## Phone vault (part 1)

Decision record: [ADR-0439](../adr/0439-qindalink-phone-password-vault.md).

- Code: pure-Java format and crypto in `phone/app/android/src/org/qindaqt/link/vault/`
  (JVM tests `phone/engine-tests/VaultTests.java`); Android glue in
  `android/Vault*.java`; UI in `phone/app/qml/Vault*.qml`; Qt seam
  `phone/app/VaultBridge.*` (`vault` context property, preview stand-in
  `FakeVault`).
- Data key (DEK) wrapped by a per-use-authenticated Keystore key (two alias
  slots so a restore never deletes the old key first); entries AES-256-GCM;
  header generation counter against rollback; `FLAG_SECURE` for the whole
  unlocked session; locks on 5 min idle, screen off, 30 s without UI, explicit.
  Only an AEAD tag failure counts as tampered; a failed authentication is a
  retry, never a recovery prompt (`VaultLifecycle`).
- Backups are passphrase-encrypted (PBKDF2-HMAC-SHA256, 600 000+ iterations) and
  are the only way content leaves the phone. A backup is the recovery path when
  the Keystore key is invalidated.
- Part 2 consumes `VaultTypingSource` (steps text/key/delay, only while
  unlocked) and defines the `vault.*`/`desktop.*` frames.
- Word list: EFF large list, CC BY 3.0 US, (c) Electronic Frontier Foundation.

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
  QindaLink APK, or a development jar passed with `--server-jar`. A
  per-session secret reaches the server only on its stdin and in `hello`. The
  socket name and the ADB forward carry a non-secret random suffix (see the
  [threat model](../reference/qindalink-mirror-v1.md#threat-model)).
- **Display.** The server creates a trusted virtual display that owns its
  content, accepts touch, has its own display group and destroys its content
  when removed. It is sized to the window in device pixels at
  160 dpi × the window's device pixel ratio. The server launches the app's
  launcher activity on it and encodes it with MediaCodec (H.264 High, or HEVC
  with `--codec hevc`).
- **Power and keyboard.** The phone's own screen and keyboard stay out of the
  way.
  - A wake lock is held only when the mirror display has its own display
    group. On the SM-S948U (SDK 37) it stays in group 0, so mirroring
    follows the phone's own power state and never wakes or holds its screen.
  - **User decision (2026-11-02): pause on screen-off.** QindaLink never holds
    the phone's main screen awake and never turns the panel off. When the
    phone's screen goes off, the server sends `screen-state off`. The window
    dims the last frame under "Phone screen is off — wake your phone to
    continue" and drops input. On `screen-state on`, or the next keyframe,
    the picture resumes.
  - The display's IME policy hides the phone keyboard, because the desktop
    types instead.
- **Decode and present.** The desktop decodes with libavcodec
  ([ADR-0435](../adr/0435-decode-qindalink-streams-with-ffmpeg.md)), VA-API
  first (radeonsi on the RX 5700 XT), falling back to software
  automatically. It
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
- **App exit.** When the app leaves its display on the phone, the window shows
  "App closed on phone" and closes after 2 s.
- **Server lifetime.** The shell-uid server never outlives its desktop. It
  exits without a completed `hello` in 20 s, after 3 bad peers, and after
  30 s without a frame (the desktop pings every 5 s).
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
