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

Not specified yet. The notifications lane replaces this paragraph with the
frame table for capability `notify`.

## Messages (`sms.*`)

Not specified yet. The SMS lane replaces this paragraph with the
frame table for capability `sms`.

## Remote input (`input.*`)

Capability `input`. The phone controls the desktop pointer and keyboard. All
input reaches the desktop through one RemoteDesktop portal session that the
desktop user approves once; the phone must ask for it first.

**Controller.** At most one phone controls input at a time. The desktop drops
every `input.*` event frame unless the sender is the controller, the portal
session is active and the desktop screen is unlocked. Event frames get no
reply on success. A frame that has an `id` and fails validation gets
`error{bad-request}`.

| Type | Direction | Body | Reply |
| --- | --- | --- | --- |
| `input.begin` | phone→desktop, with `id` | `{}` | `ok` when input is active. `error{denied}` when the desktop user did not approve. `error{busy}` when another phone controls input, or the desktop is locked (`message` is `locked`; the phone is not made controller and asks again after unlocking). `error{internal}` when the portal is unavailable. |
| `input.end` | both | phone→desktop: `{}` (an optional `id` is answered `ok`). desktop→phone: `reason`, currently `revoked` | none / `ok` |
| `input.state` | desktop→phone | `state`: `pending` (waiting for the desktop user), `active`, `denied`, `unavailable`, `locked`, `busy` (another phone controls input) | none |
| `input.pointer` | phone→desktop | `dx`, `dy`: numbers, relative, finite, absolute value at most 1000 | none |
| `input.button` | phone→desktop | `button`: `left`, `right`, `middle`; `state`: `down`, `up`, `click` | none |
| `input.scroll` | phone→desktop | `mode`: `smooth` or `discrete`; `dx`, `dy`; optional `end` (bool, ends a smooth gesture). Smooth: absolute value at most 500. Discrete: integers, at most 20 detents. Positive `dy` moves the content toward its end (down). | none |
| `input.key` | phone→desktop | exactly one of `keysym` (integer) or `name`; `state`: `down`, `up`, `tap` | none |
| `input.text` | phone→desktop | `text`: 1 to 4096 UTF-8 bytes | none; with an `id`, `ok` once typed. `error{busy}` if another text frame is still being typed |

Rules:

- **State frames carry the UI state.** The phone engine does not route `ok`
  and `error` replies to its feature handlers, so the desktop also pushes
  `input.state` on every change. A phone treats `input.state` as the truth and
  a reply only as confirmation.
- **Rates.** A phone sends at most 120 `input.pointer` or `input.scroll`
  frames per second, coalescing movement between sends. The desktop drops
  pointer and scroll frames above 240 per second with a burst of 60, and key and
  button presses (`down`, `tap`, `click`) above 100 per second with a burst of 30.
  Releases (`up`) are never limited, so a limit can never leave something held. Text is typed in bounded batches (24 symbols
  every 4 ms), and a second `input.text` while one is typing gets `busy`.
- **Keys.** A `keysym` is an X11 keysym in one of these ranges: 0x20–0x7E,
  0xA0–0xFF, 0xFE00–0xFFFF, the XF86 media and brightness keys
  0x1008FF02, 0x1008FF03, 0x1008FF11–0x1008FF17, 0x1008FF31 and 0x1008FFB2, and
  Unicode `0x01000000 | codepoint` for U+0100 to U+10FFFF (surrogates
  excluded). Session-control keysyms (PowerOff, Sleep, Suspend, Hibernate,
  WakeUp, LogOff, ScreenSaver, Standby, Eject) are refused. A
  `name` is one of `Escape`, `Tab`, `Return`, `BackSpace`, `Delete`, `Insert`,
  `Home`, `End`, `PageUp`, `PageDown`, `Left`, `Right`, `Up`, `Down`, `Space`,
  `Menu`, `Print`, `CapsLock`, `Shift`, `Ctrl`, `Alt`, `Meta`, `F1` to `F12`.
  A chord is `down` of each modifier, then the key, then `up` in reverse.
- **Text.** The desktop types `input.text` as keysyms. Characters from U+0100
  up use `0x01000000 | codepoint`, a line feed is Return, a tab is Tab, and
  other control characters, including U+0080 to U+009F, are skipped. The text is never logged or stored.
- **Releasing.** On `input.end`, a new `input.begin`, disconnect, desktop lock,
  portal revocation, service shutdown or loss of READY, the desktop releases
  every key and button the phone still holds, while the portal session is still
  open. It also releases any key or button held for more than 30 seconds
  without being pressed again. A lock that cancels a typing `input.text` answers
  it with an error. A phone still sends `up` for what it pressed.
- **Revocation.** If the portal session is closed or revoked, the desktop
  sends `input.end{reason:"revoked"}`, forgets the portal restore token and
  ends the phone's control. The next `input.begin` starts a new portal
  session, which the desktop user must approve again; if they decline, the
  reply is `error{denied}`.
- **Logging.** Neither side logs the body of an `input.*` frame.

## Macros (`macro.*`)

Capability `macro`. The phone shows the desktop user's existing macro library
(Settings → Input → Macros) and runs a macro by id. The library lives only on
the desktop
([ADR-0436](../adr/0436-qindalink-macros-reuse-the-input-actions-library.md));
the phone never receives steps, text or shortcuts.

| Type | Direction | Body | Reply |
| --- | --- | --- | --- |
| `macro.list` | desktop→phone | `rev` (string), `macros`: array of at most 64 `{id, name}`; `id` matches `[a-zA-Z0-9_-]{1,40}`, `name` is 1–80 characters. Sent on entering READY and whenever the library changes. | none |
| `macro.list` | phone→desktop, with `id` | `{}`; asks for a fresh list | `ok`, after the desktop sends `macro.list` |
| `macro.run` | phone→desktop, with `id` | `id` (string, the macro id) | `ok` when the macro finished. Errors: `denied` (the phone is not the input controller, or input is not active: send `input.begin` first), `busy` (a macro is already running), `bad-request` (unknown id), `internal` (could not start, failed or was cancelled; `message` gives the reason) |
| `macro.state` | desktop→phone | `id`, `state`: `running`, `done`, `cancelled`, `failed`; optional `reason` (`cancelled`, `input-lost`, `input-unavailable`, `text-failed`, or for a refused run the error code: `denied`, `busy`, `bad-request`, `internal`) | none |
| `macro.cancel` | phone→desktop | `{}` | `ok` |

Rules:

- A macro runs only for the phone that controls input, and only while input
  is active and the desktop is unlocked. A lock, a revocation, `input.end` or
  a disconnect cancels the run, and the shared runner releases what it holds.
- Steps that need the compositor rather than the portal (maximize, close
  window, show desktop) fail the run with reason `input-unavailable`.
- One macro runs at a time on the desktop.
- A phone treats `macro.state` as the truth for its UI, as for `input.state`.

## Desktop facts and secret typing (`desktop.*`, `vault.*`)

Not specified yet. The vault lane replaces this paragraph with the
frame table for capability `vault`.

## Phone apps (`apps.*`)

Not specified yet. The phone apps lane replaces this paragraph with the
frame table for capability `apps`.

<!-- AGENT-GUARD: any logging of frames must redact the body of every vault.*
frame (log the type and length only). A debug flag must not lift this. -->
