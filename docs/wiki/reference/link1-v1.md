# Link1 protocol 1

`org.qindaqt.Link1` is the session-bus API of `qindaqt-link-service`, the
desktop half of [QindaLink](../architecture/qindalink.md). The phone-facing
wire format is the separate [QindaLink protocol 1](qindalink-protocol-v1.md).

- **Bus name:** `org.qindaqt.Link1` (session bus)
- **Object path:** `/org/qindaqt/Link1`
- **Interface:** `org.qindaqt.Link1`
- **Introspection:** `src/services/link_service/data/org.qindaqt.Link1.xml`
  (installed to `share/dbus-1/interfaces`)

## Methods

| Method | Signature | Behavior |
| --- | --- | --- |
| `Devices` | `() → s` | Compact JSON array of device snapshots (below). |
| `ConfirmPairing` | `(s id, b accept)` | The desktop user's decision for the pending pairing with device `id`. Unknown ids and ids with no pending code are ignored. |
| `Unpair` | `(s id)` | Deletes the stored pin, sends `bye{unpaired}` on a live connection and closes it. |
| `Ring` | `(s id)` | Asks a READY phone that lists the `ring` capability to ring. Fire-and-forget; failures are logged without the device id beyond a prefix. |
| `ListenPort` | `() → q` | The TCP port of the loopback listener (47160 in production). Diagnostics and tests only. |

## Signals

| Signal | Signature | Meaning |
| --- | --- | --- |
| `DevicesChanged` | `(s json)` | Any snapshot-visible change; `json` is what `Devices()` would return now. |
| `PairingRequested` | `(s id, s name, s model, s code)` | The phone and desktop have exchanged nonces. `code` is the raw six digits (`"007123"`); UIs show it as `007 123`. The desktop also posts a notification with Accept and Reject. |

## Notifications interface

`org.qindaqt.Link1.Notifications` is exported on the same object by a separate
adaptor, only when the service has a phone-notification presenter (so not under
`--no-notifications` alone). Frame semantics are in the
[protocol reference](qindalink-protocol-v1.md#notifications-notify).

| Member | Signature | Behavior |
| --- | --- | --- |
| `ListApps` | `() → s` | JSON array of `{pkg,label,muted}` for apps seen so far or muted. |
| `SetAppMuted` | `(s pkg, b muted)` | Persists the choice; muting closes that app's shown notifications. |
| `Reply` | `(s device, s key, s action, s text) → s` | Sends `notify.action` with `text`. Returns `""` once sent, or `not-found`, `bad-request` or `not-connected`. |
| `Current` | `() → s` | JSON array of the tracked notifications: `device,key,pkg,app,title,text,when,hasReply,actions`. Personal content, session bus only. |
| `Revision` | `() → u` | Number of `CurrentChanged` signals sent since the service started. |
| `CurrentChanged` | signal `(u revision, s item)` | One changed entry per signal: its `Current()` object, or `{device,key,removed:true}`. A client that sees a gap in `revision` re-reads `Current()`. |
| `ReplyFinished` | signal `(s device, s key, s action, s error)` | The phone's answer to `Reply`; `error` is empty on success, or a protocol code; `unverified` means the action launches an activity and Android may have suppressed it. |

`qindaqt-link-service --recording-notifier FILE` replaces the freedesktop
presenter with a file recorder for private-bus tests; `--config-dir` and
`--cache-dir` move the mute file and icon cache.

## Setup interface

`org.qindaqt.Link1.Setup` is exported on the same object by a separate adaptor. The service owns
ADB, so the QindaLink app asks here instead of running `adb`. All three methods reply after the
adb steps finish (a `SetupPhone` call can take tens of seconds; clients use a long timeout).
Without ADB (`--no-adb`) they fail with `NotConnected`. A serial that is not
`[A-Za-z0-9._:-]{1,64}` fails with `BadRequest` before anything runs.

| Method | Signature | Behavior |
| --- | --- | --- |
| `AdbDevices` | `() → s` | JSON array `{serial,appInstalled,tunneled}` of phones in ADB state `device`. |
| `SetupPhone` | `(s serial) → s` | Runs, through the `link_adb` runner, `pm grant org.qindaqt.link android.permission.{READ_SMS,SEND_SMS,RECEIVE_SMS,READ_CONTACTS,POST_NOTIFICATIONS,READ_PHONE_STATE}`, `appops set … ACCESS_RESTRICTED_SETTINGS allow`, `cmd notification allow_listener org.qindaqt.link/org.qindaqt.link.android.NotificationListener` and `cmd deviceidle whitelist +org.qindaqt.link`. A failed step does not stop the rest. |
| `SetupStatus` | `(s serial) → s` | Reads the same state without changing it (`dumpsys package`, `appops get`, `settings get secure enabled_notification_listeners`, `cmd deviceidle whitelist`). |

Both report `{"serial","ok":bool,"steps":[{"id","ok","code"}]}`. Step ids are
`grant:<PERMISSION>`, `restricted-settings`, `notification-listener` and `battery-whitelist`. For
`SetupPhone`, `ok` means the adb command exited 0 and `code` is its exit code; for `SetupStatus`,
`ok` means "already granted". Command output is inspected and dropped, never returned or logged.
Only the QindaLink package is touched.

<!-- AGENT-GUARD: SetupPhone changes permissions on the phone. It is not yet restricted by the
caller-policy helper; until the PM wires it (see setup_adaptor.h), any same-user process on the
session bus can call it. -->

## Messages

`org.qindaqt.Link1.Messages` at `/org/qindaqt/Link1/Messages` on the same bus
name exposes the phone's SMS/MMS provider over the `sms` capability
([wire format](qindalink-protocol-v1.md#messages-sms)). `device` is the device
id from `Devices`. All string results are compact JSON; the JSON bodies are the
validated `sms.*` reply bodies, restricted to the documented keys. Calls reply
asynchronously, after the phone answers (10 s limit).

| Method | Signature | Result JSON |
| --- | --- | --- |
| `ListThreads` | `(s device, i limit, x before, x beforeId) → s` | `{"threads":[…],"more":b,"unread":n}`. `limit` is clamped to 1–100; `before`/`beforeId` < 0 mean "newest". |
| `ListMessages` | `(s device, x thread, i limit, x before, s beforeKind, x beforeId) → s` | `{"thread":n,"messages":[…],"more":b}`; `limit` clamped to 1–200. The cursor is the last row's `date`, `kind` and `id`; `beforeKind` is `sms`, `mms` or empty (`sms`). |
| `GetPart` | `(s device, x part, i maxEdge) → s` | `{"part":n,"mime":"…","size":n,"scaled":b,"data":"<base64>"}` |
| `Send` | `(s device, as addrs, s text, i subscription) → s` | `{"ref":n}`; `subscription` < 0 means the default. Progress arrives as `SendStatus`. |
| `MarkRead` | `(s device, x thread) → s` | `{"thread":n,"unread":n}` |

| Signal | Signature | Meaning |
| --- | --- | --- |
| `ThreadsChanged` | `(s device, s json)` | `{"threads":[ids],"all":b,"unread":n}`. Also sent once when a phone becomes READY. |
| `SendStatus` | `(s device, x ref, s state, s code, s detail)` | `state` is `sent`, `delivered` or `failed`; `code` is non-empty only for `failed`. `detail` is empty, or `{"unsent":[i…]}` listing recipient indexes never sent after a partial failure. |

| Property | Type | Meaning |
| --- | --- | --- |
| `UnreadCount` | `i` | Unread messages summed over connected phones; changes are announced with `PropertiesChanged`. It is 0 for a disconnected phone. |

Errors are D-Bus errors `org.qindaqt.Link1.Error.<Code>`: `NotConnected`
(no READY session), `Unsupported` (the phone lacks `sms`, cannot mark read, or
the part is too large), `Denied` (SMS permission not granted on the phone),
`BadRequest` (arguments out of bounds), `BadReply` (the phone's data failed
validation), `Timeout`, `Internal`. A phone error code outside the protocol's list is reported as `Internal`.

Privacy: the service keeps only unread counts in memory. Message content
lives in the reply and in the client; nothing is written to disk or logged.

## Device snapshot

```json
{"id":"<64 hex SPKI digest>","name":"…","model":"…","paired":true,
 "connected":true,"ready":true,"transport":"usb",
 "battery":{"level":87,"charging":true,"plugged":"usb"},
 "network":{"type":"wifi","signal":3},"lastSeen":1767225600000}
```

| Field | Meaning |
| --- | --- |
| `paired` | The key is pinned in `devices.json`. |
| `connected` | A TLS connection exists and the phone's `hello` was received. |
| `ready` | Both sides are pinned and the connection is in READY state. A connected but not ready device is waiting for pairing, or is a stale-pin "pair again" case. |
| `transport` | `usb` for the loopback listener, `lan` for a LAN listener, `none` when not connected. |
| `battery`, `network` | Latest `status` frame; `null` until one arrives or after disconnect. |
| `lastSeen` | Milliseconds since the epoch; the current time while connected, the last disconnect otherwise, `0` if never. |

Devices that are paired are always listed. Unpaired devices are listed only
while connected.

## Service contract

<!-- AGENT-CONTRACT: the applet and app consume this page through
src/services/link_client; changing a method, signal or JSON key means changing
the client, the introspection XML and this page together. -->

- **Ownership.** The service owns the identity (`identity-key.pem`,
  `identity-cert.pem`, key `0600` in a `0700` directory), `devices.json`
  (written atomically) and the listener. The state directory is
  `$XDG_DATA_HOME/qindalink`.
- **Start-up.** An unreadable `devices.json` stops the service rather than
  starting with an empty trust store. Another owner of the bus name makes the
  service exit non-zero.
- **Listener.** `127.0.0.1:47160` only; LAN mode is not implemented. ADB
  tracking keeps `adb reverse tcp:47160 tcp:47160` in place for each
  `device`-state phone whose `pm path org.qindaqt.link` succeeds.
- **Activation.** `org.qindaqt.Link1.service` starts the binary on demand
  and names the `qindaqt-link-service.service` user unit. The unit is
  `WantedBy=default.target`, never `graphical-session.target`.
- **Hardening.** The process sets `PR_SET_DUMPABLE` to 0 at start-up.
- **Test switches.** `--data-dir DIR`, `--port N` (0 = ephemeral),
  `--no-adb` and `--no-notifications` exist so tests never touch the real
  state, port 47160, the adb server or the notification server.

## Known gaps

- `ConfirmPairing` accepts any caller on the session bus. Restricting it to
  the QindaLink app and the notification path is a later change; until then
  any same-user process can approve a pending pairing it has seen the code
  for via `PairingRequested`.
- The adb server started for tracking is shared with the user's adb
  sessions; the unit uses `KillMode=process` so a restart does not kill it.

## Bounds on unauthenticated connections

Implemented as the protocol page specifies: 10 s hello deadline, 120 s
UNPAIRED lifetime (`bye{pair-timeout}`, not extended by `ping`), at most 8
UNPAIRED connections, one pairing at a time across keys, and no READY when
the pin cannot be written (`bye{error}`).

## Client

`src/services/link_client` provides `LinkClient` over a `LinkTransport`
interface, with `QtLinkTransport` as the QtDBus implementation. The client
ignores replies to requests made before the latest availability transition
and clears its device list when the service name disappears.

## Verification

| Row | Covers |
| --- | --- |
| `qindaqt.link-vectors` | Every case of `frames-v1.json` and `pairing-v1.json` |
| `qindaqt.link-identity` | libcrypto identity, pin equals the `openssl` pipeline, 0700/0600 files, trust store round trip |
| `qindaqt.link-pairing` | TLS 1.3 pairing end to end with a scripted phone: success, tampered reveal, rejects, timeout, UNPAIRED gate, stale pin, busy, LAN window |
| `qindaqt.link-session` | Replacement, LAN rule, keepalive with injected timings, unpair, error replies, vault log redaction |
| `qindaqt.link-adb` | Tunnel manager against a fake adb |
| `qindaqt.link-client` | Parsing and availability gating |
| `qindaqt.link-messages` | `MessagesFeature` with a scripted phone: limit clamping and cursors, rejected oversized/mistyped replies, error mapping, change events and unread totals, send status mapping, no content in logs |
| `qindaqt.link-service-smoke` | The real binary on a private `dbus-run-session` bus |
| `qindaqt.link-setup` | `PhoneSetup` against a fake adb: every grant, failures reported, unsafe serials refused, status reads only |
