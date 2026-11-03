# QindaLink protocol 1

This page is the normative wire contract between the QindaLink phone engine
(Java, `phone/`) and the link service (C++, `src/services/link_*`). The
decisions behind it are [ADR-0433](../adr/0433-qindalink-phone-companion-architecture.md)
and [ADR-0434](../adr/0434-qindalink-pinned-tls-transport-and-sas-pairing.md).
The [architecture page](../architecture/qindalink.md) describes the
components.

<!-- AGENT-CONTRACT: both codecs implement exactly this page. Changing a frame
shape means changing this page, tests/link/vectors/*.json, the C++ codec and
the Java codec in one commit. -->

## Transport

- TCP, TLS 1.3 only. Both sides present a certificate whose key is ECDSA
  P-256.
- **Identity** is `SHA-256(DER SubjectPublicKeyInfo)` of the peer certificate
  as seen in the TLS session. It is written as 64 lowercase hex characters
  and called `deviceId` on the desktop. A side never trusts a key that the
  peer merely claims in a frame.
- **Ports.**
  - The desktop listens on `127.0.0.1:47160` (the USB path).
  - When LAN mode is on, it also listens on port 47160 on other interfaces and
    advertises the Avahi service `_qindalink._tcp`.
  - Over USB the desktop runs `adb -s <serial> reverse tcp:47160 tcp:47160`.
    The phone always tries `127.0.0.1:47160` first.
- **Connections per device.** The desktop keeps at most one connection per
  device. A new connection replaces the old one with `bye{reason:"replaced"}`,
  except that a LAN connection never replaces a live loopback one; that LAN
  connection is refused with the same `bye` instead.

## Framing

Each frame is a 4-byte unsigned big-endian payload length followed by that
many bytes of payload.

- **Length.** It must be between 1 and 4 194 304 (4 MiB). A length of 0 or
  anything larger is a protocol error: the receiver closes the connection
  without replying.
- **Payload.** It is one UTF-8 JSON object with no byte-order mark, nested
  at most 64 levels deep. The top-level object is level 1, and each nested
  object or array adds one level. Anything else (invalid UTF-8, invalid JSON,
  a non-object, deeper nesting, or an object that breaks the envelope rules)
  is a protocol error and closes the connection.
- **Decoder outcomes.** The golden vectors name the outcomes `bad-length`,
  `bad-payload` (UTF-8, BOM, JSON or non-object) and `bad-envelope`. A frame
  whose bytes have not all arrived yet is `needsMore`, which is not an error.
  These names are internal to the decoders and never sent on the wire.

### Envelope

| Key | Type | Rule |
| --- | --- | --- |
| `t` | string | Required. Frame type: 1–64 characters. The **whole** string must match `[a-z][a-z0-9]*(\.[a-z][a-z0-9-]*)*`, so a trailing newline is invalid. |
| `id` | integer | Optional. Request id, 1 ≤ id ≤ 2⁵³−1, chosen by the sender, written as a plain JSON integer token with no sign, fraction or exponent (`1.0`, `1e0` and `-0` are invalid). A frame with an `id` expects exactly one reply frame. |
| `re` | integer | Optional. The `id` of the request this frame answers. Same range. |
| `b` | object | Optional body. Absent means `{}`. |

Rules:

- Receivers ignore unknown top-level keys and unknown body keys.
- Receivers check field types strictly. A wrong type is `bad-request`.
- A request (has `id`) with an unknown `t` is answered with
  `error{code:"unknown-type"}`. An unknown `t` without `id` is ignored.
- Encoders write compact JSON. Key order carries no meaning, so tests compare
  parsed values, not bytes.
- Binary values (keys, nonces, digests) are lowercase hex strings of the
  exact length the field defines: 64 characters for every 32-byte value in
  this page. Any other length or character is invalid for that field. Images
  are base64 in a `png` string.

## Connection lifecycle

```
TLS 1.3 handshake (both certificates) ──► both send hello
   │
   ├─ both hello.trusted == true and both pins match ──► READY
   └─ otherwise ──► UNPAIRED (only hello, pair.*, ping, pong, error, bye)
                        └─ pairing completes ──► READY (same connection)
```

- Each side sends `hello` immediately after the handshake, without waiting
  for the peer's.
- If `hello.proto` is not `1`, the receiver sends `bye{reason:"protocol-version"}`
  and closes.
- In UNPAIRED state, any frame type outside the allowed set closes the
  connection.
- **Bounds on unpaired connections.**
  - A peer that has not sent `hello` within 10 seconds of the handshake is
    closed without a reply.
  - An UNPAIRED connection is closed with `bye{reason:"pair-timeout"}` 120
    seconds after the handshake, whether or not pairing started. `ping`
    does not extend this.
  - The desktop holds at most 8 UNPAIRED connections. It refuses more by
    closing them right after the handshake.
  - The desktop runs at most one pairing at a time. A `pair.request` from any
    key while another pairing is in progress gets `error{code:"busy"}`.
- **Keepalive.** A side that has sent nothing for 30 seconds sends `ping`
  (with an `id`), and the peer answers `pong` (with `re`). A side that has
  received nothing for 90 seconds closes the connection.
- **Sending rules.** Before READY, a side never sends a feature frame.
  After READY, it sends a namespace's frames only if the peer listed that
  capability in `hello.caps`.

## Core frames

| Type | Direction | Body | Reply |
| --- | --- | --- | --- |
| `hello` | both | `proto` (int, `1`), `role` (`"desktop"`/`"phone"`), `name` (string ≤ 128), `model` (string ≤ 128), `os` (string ≤ 128), `app` (version string ≤ 64), `caps` (array of capability strings), `trusted` (bool: the sender has the receiver's key pinned) | none |
| `bye` | both | `reason`: `shutdown`, `unpaired`, `protocol-version`, `pair-rejected`, `pair-timeout`, `replaced` or `error` | none; the sender closes after flushing |
| `ping` | both | `{}` | `pong` |
| `pong` | both | `{}` | — |
| `ok` | both | `{}`; generic success reply | — |
| `error` | both | `code` (see below), `message` (string ≤ 512, for logs only, never shown verbatim as a secret-bearing string) | — |
| `status` | phone→desktop | See [status](#status) | none |
| `ring` | desktop→phone | `{}`. The phone rings at full volume, overriding silent mode, until the user stops it on the phone. | `ok` or `error` |

Error codes: `unknown-type`, `bad-request`, `not-paired`, `unsupported`,
`busy`, `denied`, `target-changed`, `timeout`, `internal`.

Capability strings in protocol 1: `status`, `ring`, `notify`, `sms`,
`input`, `macro`, `vault`, `apps`. A side lists only the capabilities it
implements. The feature sections below are added by the change that
implements each one.

### Status

`status` is sent once on entering READY, and then whenever a value changes.
At most one frame goes out per 2 seconds, except that a change in the
`charging` or `plugged` state is sent immediately.

```json
{"t":"status","b":{
  "battery":{"level":87,"charging":true,"plugged":"usb"},
  "network":{"type":"wifi","signal":3}
}}
```

| Field | Values |
| --- | --- |
| `battery.level` | integer 0–100 |
| `battery.charging` | bool |
| `battery.plugged` | `none`, `usb`, `ac`, `wireless`, `dock` |
| `network.type` | `none`, `wifi`, `cellular`, `ethernet`, `other` |
| `network.signal` | integer 0–4, or `null` when unknown |

## Pairing

Pairing runs inside an UNPAIRED connection. The phone always initiates.
Below, `spkiD` and `spkiP` are the raw 32-byte SPKI digests of the desktop's
and the phone's certificates, as each side observed them in this TLS session.
`nd` and `np` are 32 bytes each from a cryptographically secure random
generator. `‖` means byte concatenation, and the labels are their ASCII
bytes with no terminator.

| Step | Frame | Body |
| --- | --- | --- |
| 1 | phone → `pair.request` (with `id`) | `{}` |
| 2 | desktop → `pair.commit` (`re` = step 1 id) | `c` = hex `SHA-256("QLINK-COMMIT-1" ‖ nd ‖ spkiD ‖ spkiP)` |
| 3 | phone → `pair.nonce` | `np` = hex `np` |
| 4 | desktop → `pair.reveal` | `nd` = hex `nd` |
| 5 | phone | Recomputes `c` from `nd`. On a mismatch it sends `bye{reason:"pair-rejected"}` and closes. |
| 6 | both | Compute and show the code (see below) |
| 7 | both → `pair.confirm` | `accept` (bool), sent when the local user decides |

Rules:

- **The code.** Let `h = SHA-256("QLINK-SAS-1" ‖ spkiD ‖ spkiP ‖ nd ‖ np)`.
  The code is `uint32_be(h[0..3]) mod 1000000`, formatted as six digits with
  leading zeros. Both screens show it as `123 456`.
- **Completion.** Pairing succeeds when a side has its own user's acceptance
  and has received `pair.confirm{accept:true}`. Each side then pins the
  peer's key and the connection enters READY. No further frame is needed.
- **Persisting the pin.** A side that cannot store the pin durably must not
  enter READY. It sends `bye{reason:"error"}` and closes, so the two trust
  stores never disagree after a restart.
- **Malformed pairing fields.** A pairing frame whose `c`, `np` or `nd` is not
  exactly 64 lowercase hex characters ends pairing with
  `bye{reason:"pair-rejected"}`.
- **Locale.** The six digits are always ASCII `0`–`9`, whatever the locale,
  for example `String.format(Locale.ROOT, ...)` in Java.
- **Rejection.** A `pair.confirm{accept:false}` from either side ends pairing:
  both send `bye{reason:"pair-rejected"}` and close.
- **Timeout.** Pairing that is not complete 120 seconds after `pair.request`
  ends with `bye{reason:"pair-timeout"}`.
- **Refusing a request.** The desktop answers `pair.request` with
  `error{code:"denied"}` when the request arrived on the LAN listener while
  no pairing window is open
  ([ADR-0434](../adr/0434-qindalink-pinned-tls-transport-and-sas-pairing.md)).
  It answers `error{code:"busy"}` while any other pairing is in progress
  (see the bounds in [Connection lifecycle](#connection-lifecycle)).
- **Stale pins.** A peer whose `hello.trusted` is false while the local side
  has it pinned has forgotten this side. The local side keeps the pin, stays
  UNPAIRED, and its UI offers "pair again", which deletes the stale pin
  before sending `pair.request`.

Worked values are in `tests/link/vectors/pairing-v1.json`, and both
implementations must reproduce them.

## Golden vectors

| File | Contents |
| --- | --- |
| `tests/link/vectors/frames-v1.json` | `decode` cases: hex frame bytes with the expected envelope or the expected protocol error. `encode` cases: an envelope whose encoding must decode back to an equal value. |
| `tests/link/vectors/pairing-v1.json` | Fixed `spkiD`, `spkiP`, `nd` and `np` with the expected commitment and code |

## Feature namespaces

These prefixes are reserved. Each is specified here by the change that
implements it, including any frame that carries user content.

| Prefix | Capability | Purpose |
| --- | --- | --- |
| `notify.` | `notify` | Phone notifications: posted, removed, dismiss, action, inline reply, icons |
| `sms.` | `sms` | Threads, messages (paged), change events, send |
| `input.` | `input` | Phone-as-touchpad/keyboard input to the desktop |
| `macro.` | `macro` | Desktop macro catalog pushed to the phone; run requests |
| `desktop.` | `vault` or `input` | Focused desktop window facts for confirmation |
| `vault.` | `vault` | Secret typing requests. Bodies are never logged by either side. |
| `apps.` | `apps` | Phone app inventory used by the desktop launcher (streaming itself runs over ADB, not this link) |

## Notifications (`notify.*`)

Capability `notify`. The phone mirrors its notifications to the desktop and
the desktop sends dismissals and action requests back.

<!-- AGENT-GUARD: notification titles, texts, and reply texts are personal
content. Neither side logs them (log the frame type and byte length only), and
no test or tool may print them. -->

Lengths below count UTF-16 code units (Java `String.length()`, Qt
`QString::size()`). The sender truncates over-long `title` and `text`; the
receiver treats any other violation of a bound, a type or a pattern as a bad
frame: a request gets `error{code:"bad-request"}` and a frame without an `id`
is dropped. Both sides ignore unknown keys.

| Type | Direction | Body | Reply |
| --- | --- | --- | --- |
| `notify.posted` | phone→desktop | A notification (below). A `key` the desktop already shows is an update. | none |
| `notify.snapshot` | phone→desktop | `items`: array of at most 50 notification objects. Sent once on entering READY. It replaces the desktop's set for this phone: keys that are absent are closed, unchanged keys are left alone. | none |
| `notify.removed` | phone→desktop | `key` (1–512) | none |
| `notify.dismiss` | desktop→phone | `key`. The user dismissed it on the desktop. The phone calls `cancelNotification(key)`. | `ok` if `id` is present |
| `notify.action` | desktop→phone | `key`, `action` (an action `id`, 1–64), `title` (the action title shown, 1–64), optional `text` (0–4096, only for a `reply` action) | `ok`, or `error` (below), if `id` is present |
| `notify.icon` | desktop→phone | `hash` (64 lowercase hex). Asks for the PNG of an icon the desktop does not have cached. | none |
| `notify.icon` | phone→desktop | `hash`, `png` (base64, at most 65 536 characters; empty when the phone no longer has it) | none |

A notification object:

```json
{"key":"0|com.example|7|null|10234","pkg":"com.example","app":"Example",
 "title":"…","text":"…","when":1767225600000,"icon":"<64 hex>",
 "actions":[{"id":"0","title":"Archive","reply":false},
            {"id":"1","title":"Reply","reply":true}]}
```

| Field | Bound |
| --- | --- |
| `key` | string 1–512, the Android notification key; opaque to the desktop |
| `pkg` | string 1–256, the application package |
| `app` | string 1–128, the application label |
| `title` | string 0–256 |
| `text` | string 0–2048 |
| `when` | integer, milliseconds since the epoch |
| `icon` | 64 lowercase hex characters (SHA-256 of the 64 px PNG), or `""` when the phone has no icon |
| `actions` | array of at most 8 `{id: string 1–64, title: string 1–64, reply: bool}` |

Rules:

- **Which notifications.** The phone sends neither ongoing nor
  foreground-service nor group-summary notifications, nor QindaLink's own. Text
  that Android redacts for sensitive content reaches the desktop redacted;
  nothing works around that.
- **Icons.** The desktop requests a hash it has not cached and waits up to 10
  seconds before showing the notification without an icon. It accepts the
  reply only when the decoded bytes start with the PNG signature, are at most
  49 152 bytes, and hash to the requested `hash`; it then caches the file
  under that hash.
- **Action ids are positions.** An app may repost a notification with reordered
  actions between display and tap. The phone therefore compares `title` with
  the title (clipped to 64) of the action now at that index and answers
  `target-changed` without firing anything on a mismatch.
- **Unverified activity actions.** Android does not report whether an action
  that starts an activity actually ran; a foreground-service app is usually not
  allowed to start activities from the background. For an activity
  `PendingIntent` the phone requests background-activity-start permission,
  sends the intent, and answers `error{code:"unsupported", message:"unverified"}`
  instead of `ok`. Broadcast, service and reply actions answer `ok`.
- **Stale copies.** When a notification the desktop may show is reposted as
  ongoing or foreground-service, or removed, the phone sends `notify.removed`
  for it if it ever sent that key.
- **Errors for `notify.action` and `notify.dismiss`.** `target-changed` when the
  key is no longer active on the phone or the action no longer matches, `bad-request` for an unknown action
  `id` or `text` sent to a non-reply action, `denied` when the phone has no
  notification access, and `internal` when Android refuses the action.
- **Dismissal both ways.** `notify.removed` also follows a `notify.dismiss`,
  because the phone reports every removal. The desktop closes a notification
  it no longer shows without sending anything back, so the exchange ends.
- **Rate and size.** The phone sends at most 20 `notify.posted` or
  `notify.removed` frames per 500 ms and coalesces updates to one key. The
  desktop keeps at most 100 notifications per phone and closes the oldest
  beyond that.

## Messages (`sms.*`)

Capability `sms`. The phone reads the Android SMS/MMS provider and the
desktop asks for pages of it; nothing is stored on the desktop disk.
Requests go desktop to phone; the reply is a frame of the **same type** with
`re` set to the request `id` (or `error`). Every list is bounded, and the
phone enforces the bounds even if the desktop asks for more (it clamps
`limit` rather than failing). Times are integer milliseconds since the epoch.
All identifiers are provider integers: thread ids, SMS ids, MMS ids and MMS
part ids. An SMS and an MMS can share a numeric id, so a message is identified
by the pair (`kind`, `id`).

**Paging.** Lists are newest first, ordered by (`date` descending, `id`
descending). A request carries `before` and `beforeId` (both optional, both
integers; `before` of 0 or less, or absent, means "from the newest"); the reply holds only rows strictly older than that pair. `more` is
true when at least one older row exists. The cursor for the next page is the
`date` and `id` of the last row received. For `sms.messages` the order is
(`date` descending, `kind` with `mms` before `sms`, `id` descending) and the
cursor also carries `beforeKind` (`sms` or `mms`, default `sms`; anything
else is `bad-request`), so rows that share a millisecond are never repeated or
skipped even though SMS and MMS ids overlap. MMS dates are stored in seconds
on the phone and reported as `seconds * 1000`.

| Type | Direction | Body | Reply |
| --- | --- | --- | --- |
| `sms.threads` | request, desktop→phone | `limit` (int 1–100, default 50), `before`, `beforeId` | `sms.threads`: `threads` (array ≤ 100 of thread), `more` (bool), `unread` (int, total unread messages on the phone) |
| `sms.messages` | request, desktop→phone | `thread` (int, required), `limit` (int 1–200, default 50), `before`, `beforeKind`, `beforeId` | `sms.messages`: `thread`, `messages` (array ≤ 200 of message), `more` |
| `sms.part` | request, desktop→phone | `part` (int, required), `maxEdge` (int 64–2048, default 1280) | `sms.part`: `part`, `mime`, `size` (int, bytes sent), `scaled` (bool), `data` (base64) |
| `sms.send` | request, desktop→phone | `addrs` (array 1–10 of string ≤ 32), `text` (string 1–1600 chars), `sub` (int, optional subscription id) | `sms.send`: `ref` (int, unique within the connection) |
| `sms.read` | request, desktop→phone | `thread` (int, required) | `sms.read`: `thread`, `unread` (new total) |
| `sms.changed` | event, phone→desktop | `threads` (array ≤ 100 of thread id), `all` (bool, true when more than 100 changed or the set is unknown), `unread` (int total) | none |
| `sms.sent` | event, phone→desktop | `ref`, `state` (`sent`, `failed` or `delivered`), `code` (string, only with `failed`), `unsent` (array of int, optional, only with `failed`) | none |

**thread:** `id` (int), `addrs` (array ≤ 20 of string ≤ 64), `names`
(array of string ≤ 128, the same length as `addrs`, `""` where the number is
not a contact), `snippet` (string ≤ 200 chars), `date`, `unread` (int ≥ 0),
`count` (int ≥ 0, messages in the thread).

**message:** `id` (int), `kind` (`sms` or `mms`), `box` (`inbox`, `sent`,
`outbox`, `failed`, `queued` or `draft`), `addr` (string ≤ 64, the sender for
`inbox`, the first recipient otherwise), `date`, `body` (string ≤ 4000
chars), `trunc` (bool, true when `body` was cut), `read` (bool), `parts`
(array ≤ 16 of part, always present, empty for SMS). **part:** `id` (int),
`mime` (string ≤ 128), `name` (string ≤ 128, may be empty), `size` (int
bytes, or -1 when unknown). MMS text parts are folded into `body` and not
listed; every other part is listed.

Rules:

- **Frame size.** A reply stays below 2 MiB. The phone ends a list early
  and sets `more:true` when the next row would exceed that, so a page can be
  shorter than `limit`.
- **`sms.part`.** The payload before base64 is at most 2 MiB. An image larger
  than that, or with a longest edge above `maxEdge`, is decoded and
  re-encoded as JPEG phone-side (`scaled:true`, `mime:"image/jpeg"`). A
  non-image over the cap is answered `error{code:"unsupported"}`.
- **`sms.send` destinations.** Every address is checked before anything is
  sent. Spaces, dashes, dots and parentheses are stripped; what remains must be
  ASCII digits with an optional leading `+` and 7 to 20 digits. Anything else
  (premium short codes, alphanumeric sender ids, empty strings) is answered
  `error{denied}` and **nothing** is sent, even to the valid recipients. More
  than 10 addresses or more than 1600 characters of text is `bad-request`.
- **`sms.send`.** The phone divides long text into SMS parts and sends each
  to each address through the platform `SmsManager`. The reply only says the
  request was accepted (`ref`). Then `sms.sent` events report, for the whole
  request, `sent` once every part has left for every address, `delivered`
  once every delivery report has arrived (not all carriers send them), and
  `failed` on the first failure, with `code` one of `generic`, `no-service`,
  `null-pdu`, `radio-off`, `limit`, `denied`, `unknown`. After `failed` or
  `delivered` no further event is sent for that `ref`. If the platform refuses a
  recipient after earlier recipients were already handed to the radio, the
  request has been accepted, so the phone sends `failed` with `unsent` listing
  the 0-based indexes of the addresses that were never sent (the desktop must
  not blindly resend the others). A platform failure before the first
  recipient is `error{internal}` and nothing was sent. Invalid input is
  `error{bad-request}`; without the SEND_SMS permission the phone answers
  `error{denied}`.
- **Provider failures.** If the phone's SMS provider throws (a vendor rejecting a
  column, a revoked permission mid-request), the phone answers
  `error{internal}` with a `message` that names the operation and exception
  class only. A failure is never reported as an empty list.
- **Errors.** Without READ_SMS every request except `sms.send` is answered
  `error{denied}`. `sms.read` is answered `error{unsupported}` when the
  phone's SMS role does not allow writing the provider (see below).
- **`sms.changed`.** Sent once on entering READY (so the desktop learns the
  unread total) and then after a 500 ms debounce of provider changes. It
  names the threads whose summary changed; the desktop re-requests what it
  displays.
- **Privacy.** Neither side logs the body of any `sms.*` frame, address, name
  or snippet; logs carry the type, ids and lengths only.

**Limits of the platform.**

- *RCS.* RCS chats are not in the SMS/MMS provider. They never appear in
  these lists, and the desktop cannot see or answer them.
- *Marking read.* Since Android 4.4 only the default SMS app may write the
  provider. QindaLink is not the default SMS app and never asks to be, so
  `sms.read` is answered `error{unsupported}` unless the platform lets the
  update through; the phone attempts one provider update and reports
  `unsupported` when it throws or changes no row.
- *Sent messages.* A message sent by a non-default app is not written to the
  provider by the platform, so it appears in the thread list only after
  the user's own SMS app records it, if ever.

## Remote input (`input.*`)

Not specified yet. The input and macros lane replaces this paragraph with the
frame table for capability `input`.

## Macros (`macro.*`)

Not specified yet. The input and macros lane replaces this paragraph with the
frame table for capability `macro`.

## Desktop facts and secret typing (`desktop.*`, `vault.*`)

Not specified yet. The vault lane replaces this paragraph with the
frame table for capability `vault`.

## Phone apps (`apps.*`)

Not specified yet. The phone apps lane replaces this paragraph with the
frame table for capability `apps`.

<!-- AGENT-GUARD: any logging of frames must redact the body of every vault.*
frame (log the type and length only). A debug flag must not lift this. -->
