# Link1 protocol 1

`org.qindaqt.Link1` is the session-bus API of `qindaqt-link-service`, the
desktop half of [QindaLink](../architecture/qindalink.md). The phone-facing
wire format is the separate [QindaLink protocol 1](qindalink-protocol-v1.md).

- **Bus name:** `org.qindaqt.Link1` (session bus)
- **Object path:** `/org/qindaqt/Link1`
- **Interface:** `org.qindaqt.Link1`
- **Introspection:** `src/services/link_service/data/org.qindaqt.Link1.xml`
  (installed to `share/dbus-1/interfaces`)

## Access

Every member is either **open** (public metadata, any bus client) or **trusted**
(content or an action as the user). A trusted member called by anything else
fails with `org.qindaqt.Link1.Error.Denied`. The trusted callers are the installed
`qindaqt-link` and `qindaqt-shell` executables plus any `--trusted-caller <exe>`
given to the service (test/development only; logged as a warning) ([ADR-0438](../adr/0438-qindalink-link1-caller-authority.md)).
The **Access** column below uses these words.

## Methods

| Method | Access | Signature | Behavior |
| --- | --- | --- | --- |
| `Devices` | open | `() → s` | Compact JSON array of device snapshots (below). |
| `PendingPairings` | open | `() → s` | JSON array `[{id,name,model,code}]` of pairing requests waiting for the desktop user's decision, so a shell started after the request still shows it. |
| `ConfirmPairing` | trusted | `(s id, b accept)` | The desktop user's decision for the pending pairing with device `id`. Unknown ids and ids with no pending code are ignored. |
| `Unpair` | trusted | `(s id)` | Deletes the stored pin, sends `bye{unpaired}` on a live connection and closes it. |
| `Ring` | trusted | `(s id)` | Asks a READY phone that lists the `ring` capability to ring. Fire-and-forget; failures are logged without the device id beyond a prefix. |
| `ListenPort` | open | `() → q` | The TCP port of the loopback listener (47160 in production). Diagnostics and tests only. |
| `SetLanEnabled` | trusted | `(b enabled)` | Persists `lanEnabled` in `$XDG_CONFIG_HOME/qindalink/service.json` and applies it: starts or stops the per-address LAN listeners and the `_qindalink._tcp` advert. Turning it off ends LAN sessions with `bye{shutdown}` and closes the pairing window. Off by default ([ADR-0441](../adr/0441-qindalink-lan-listener-and-discovery.md)). |
| `OpenPairingWindow` | trusted | `() → u` | Opens (or restarts) the 120 s window in which a LAN connection may start pairing; returns its length in seconds, or `0` when LAN is off. Loopback (USB) pairing never needs it. |

## Properties

| Property | Access | Type | Meaning |
| --- | --- | --- | --- |
| `LanEnabled` | open, read-only | `b` | The persisted LAN setting. |
| `LanStatus` | open, read-only | `s` | Compact JSON `{enabled, state, listeners, failed[], advertised, publisher, pairingWindow, persisted}`. `state` is `off`, `waiting` (enabled, no usable address), `listening` or `error` (every bind failed); `failed` holds socket error texts; `publisher` is `off`, `published`, `renamed` (after a name collision), `unavailable` (no Avahi), `failed` or `none`; `pairingWindow` is the seconds left. It carries no addresses. |

## Signals

| Signal | Access | Signature | Meaning |
| --- | --- | --- | --- |
| `DevicesChanged` | open | `(s json)` | Any snapshot-visible change; `json` is what `Devices()` would return now. |
| `PairingWindowChanged` | open | `(b open, u seconds)` | The pairing window opened (with its length) or closed. |
| `LanStatusChanged` | open | `(s json)` | Any change of what `LanStatus` would return. |
| `PairingRequested` | open | `(s id, s name, s model, s code)` | The phone and desktop have exchanged nonces. `code` is the raw six digits (`"007123"`); UIs show it as `007 123`. The desktop also posts a notification with Accept and Reject. |

## Notifications interface

`org.qindaqt.Link1.Notifications` is exported on the same object by a separate
adaptor, only when the service has a phone-notification presenter (so not under
`--no-notifications` alone). Frame semantics are in the
[protocol reference](qindalink-protocol-v1.md#notifications-notify).

| Member | Access | Signature | Behavior |
| --- | --- | --- | --- |
| `ListApps` | trusted | `() → s` | JSON array of `{pkg,label,muted}` for apps seen so far or muted. |
| `SetAppMuted` | trusted | `(s pkg, b muted)` | Persists the choice; muting closes that app's shown notifications. |
| `Reply` | trusted | `(s device, s key, s action, s text) → s` | Sends `notify.action` with `text`. Returns `""` once sent, or `not-found`, `bad-request` or `not-connected`. |
| `Current` | trusted | `() → s` | JSON array of the tracked notifications: `device,key,pkg,app,title,text,when,hasReply,actions`. Personal content, session bus only. |
| `Revision` | open | `() → u` | Number of `CurrentChanged` signals sent since the service started. |
| `CurrentChanged` | open signal | `(u revision, s item)` | One changed entry per signal, identifiers only: `{device,key}` plus `removed:true` when it went away; never text. Trusted clients read the content with `Current()`. A client that sees a gap in `revision` re-reads `Current()`. |
| `ReplyFinished` | open signal `(s device, s key, s action, s error)` | The phone's answer to `Reply`; `error` is empty on success, or a protocol code; `unverified` means the action launches an activity and Android may have suppressed it. |

`qindaqt-link-service --recording-notifier FILE` replaces the freedesktop
presenter with a file recorder for private-bus tests; `--config-dir` and
`--cache-dir` move the mute file and icon cache.

## Messages

`org.qindaqt.Link1.Messages` at `/org/qindaqt/Link1/Messages` on the same bus
name exposes the phone's SMS/MMS provider over the `sms` capability
([wire format](qindalink-protocol-v1.md#messages-sms)). `device` is the device
id from `Devices`. All string results are compact JSON; the JSON bodies are the
validated `sms.*` reply bodies, restricted to the documented keys. Calls reply
asynchronously, after the phone answers (10 s limit).

| Method | Access | Signature | Result JSON |
| --- | --- | --- | --- |
| `ListThreads` | trusted | `(s device, i limit, x before, x beforeId) → s` | `{"threads":[…],"more":b,"unread":n}`. `limit` is clamped to 1–100; `before`/`beforeId` < 0 mean "newest". |
| `ListMessages` | trusted | `(s device, x thread, i limit, x before, s beforeKind, x beforeId) → s` | `{"thread":n,"messages":[…],"more":b}`; `limit` clamped to 1–200. The cursor is the last row's `date`, `kind` and `id`; `beforeKind` is `sms`, `mms` or empty (`sms`). |
| `GetPart` | trusted | `(s device, x part, i maxEdge) → s` | `{"part":n,"mime":"…","size":n,"scaled":b,"data":"<base64>"}` |
| `Send` | trusted | `(s device, as addrs, s text, i subscription) → s` | `{"ref":n}`; `subscription` < 0 means the default. Progress arrives as `SendStatus`. Limited to 5 sends per minute and 30 per hour per device (`Error.RateLimited`); every accepted send posts a desktop notice "QindaLink sent a text to N recipient(s)" naming the requesting executable. |
| `MarkRead` | trusted | `(s device, x thread) → s` | `{"thread":n,"unread":n}` |

| Signal | Access | Signature | Meaning |
| --- | --- | --- |
| `ThreadsChanged` | open | `(s device, s json)` | `{"threads":[ids],"all":b,"unread":n}`. Also sent once when a phone becomes READY. |
| `SendStatus` | open | `(s device, x ref, s state, s code, s detail)` | `state` is `sent`, `delivered` or `failed`; `code` is non-empty only for `failed`. `detail` is empty, or `{"unsent":[i…]}` listing recipient indexes never sent after a partial failure. |

| Property | Access | Type | Meaning |
| --- | --- | --- | --- |
| `UnreadCount` | open | `i` | Unread messages summed over connected phones; changes are announced with `PropertiesChanged`. It is 0 for a disconnected phone. |

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
| `caps` | The capability strings from the phone's `hello` (empty while not connected). |
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
  state, port 47160, the adb server or the notification server. LAN mode adds
  `--no-avahi` (never touch the system bus), `--lan-port N` (default: `--port`),
  `--lan-test-address ADDR` (repeatable; bind the LAN listener there instead of the
  machine's interfaces, for example `127.0.0.2`) and `--pairing-window-ms N`
  (shorten the window). They only wire machinery: LAN still starts disabled
  and needs `SetLanEnabled` from a trusted caller.

## Known gaps

- The pairing notification's Accept/Reject buttons reach `Hub::confirmPairing` through the notification
  server rather than through `ConfirmPairing`, so they are not subject to the caller policy.
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
| `qindaqt.link-caller-policy` | Caller decisions with fake process facts (trusted, untrusted, deleted exe, unknown pid, PID reuse), Send rate limit and notice |
| `qindaqt.link-caller-coverage` | Introspects the live service and requires `Denied` for every method outside the open allowlist, so new `Link1.*` members are covered automatically |
| `qindaqt.link-caller-dbus` | The real service on a private bus: untrusted `gdbus` denied for every protected member, open members work, trusted test binary succeeds, `caps`, `PendingPairings`, rate limit and notices |
| `qindaqt.link-service-smoke` | The real binary on a private `dbus-run-session` bus |
