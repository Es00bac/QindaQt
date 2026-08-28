# Audio applet

The audio panel applet is a bounded presentation surface over the integrated
[Audio1](../reference/audio1-v1.md) public client boundary. It lives in
`src/shell/audio_applet` and consumes only the typed `audio_protocol` values
and the public `audio_client` facade; it never contacts PipeWire, WirePlumber,
or any audio service internal. The service-side architecture is described in
[Audio service](../architecture/audio-service.md).

## Slice status

This WIRED slice registers its presentation model, controller, compiled QML
module, hostile tests, and this page in the combined source/test graph. It is
deliberately not yet a production-hosted applet. The remaining integration
seams are additive and listed in
[Applet runtime](applet-runtime.md) terms as:

- a compiled built-in registry and QML dispatcher entry for
  `qindaqt.applets.audio` (the CMake source registration alone is not that
  runtime registry);
- a manifest catalog entry and capability-policy row requesting only the
  audio read/control capabilities this surface actually uses; and
- shell composition that starts the `AudioClient`, injects the controller
  into the applet surface, and (later) narrows the surface behind the
  shell-owned `openAudioSettings()` facade described in the Audio service
  consumer boundary.

Until those seams land, the applet is not part of the production panel. The
ledger credits only its bounded WIRED source behavior, not live interaction.

## Module shape

| Piece | Responsibility |
| --- | --- |
| `AudioAppletModel` (`audio_applet_model.h/.cpp`) | Pure projection of one validated snapshot plus a pending-serial set into phase state, bounded rows, default-device labels, and overflow counts. No QObject, no transport, no retention. |
| `AudioAppletController` (`audio_applet_controller.h/.cpp`) | One borrowed `AudioClient`'s shell-side facade. Reprojects on state/snapshot changes, owns pending-request bookkeeping, clamps and validates requests, and turns typed operation results into stable feedback. The only object QML sees. |
| `QindaQt.Shell.AudioApplet` QML module | Token-styled presentation via `QindaQt.Controls 1.0`: main surface plus one device-row and one stream-row component. Injected controller only; no service import. |

The controller never starts, stops, or parents the client; shell composition
owns that lifecycle. QML receives no client, snapshot, handle, or D-Bus
object, matching the shell rule that QML consumes a model projection while
shell-side C++ consumes the public client.

## Presentation contract

Phases, exposed as `phaseText`:

| Phase | Meaning |
| --- | --- |
| `loading` | Client stopped/starting, or starting/degraded client without a validated snapshot yet. |
| `ready` | Validated ready snapshot; rows and default labels are shown. |
| `degraded` | Validated degraded snapshot; rows stay visible with a stable reason notice. |
| `unavailable` | Client-level loss, snapshot-reported unavailability, or a snapshot that failed wire validation. No rows are shown. |

`phaseReasonText` maps stable reason codes (`unavailable`,
`malformed-snapshot`/`backend-malformed`, `pipewire-replaced`,
`wireplumber-replaced`, `owner-replaced`, `authority-replaced`) to fixed,
localized sentences. Unknown or empty codes fall back to one generic
sentence. Diagnostics are never shown or parsed.

Rows: at most 8 device rows (outputs before inputs, protocol ascending-serial
order) and 8 stream rows. Anything beyond the window is summarized as an
`overflowDeviceCount`/`overflowStreamCount` label; protocol order and the
bounded window stay deterministic. Each row carries label, default/direction
flags, volume with `volumeKnown`, mute with `muteKnown`, capability booleans,
and the `pending` flag. A `volumeKnown`/`muteKnown` false value is shown as
unknown and disables the corresponding control; a missing description falls
back to the short name, then to "Unknown device"/"Unknown stream".

Default state: `defaultOutputLabel`/`defaultInputLabel` resolve the snapshot's
default handles by serial and stay correct even when that device falls
outside the retained window; an unknown `(0,0)` handle yields no label.

## Request rules

`requestVolume(serial, isStream, volume)` and
`requestMute(serial, isStream, muted)` are the only mutations, and this slice
never sets defaults or moves streams.

- Volume is clamped into `[0.0, 1.0]` before dispatch; a non-finite level is
  refused locally with feedback and never dispatched. The client still
  validates every request independently.
- A request for an unknown serial, an uncapable row, or a row whose
  capability evidence (`canSetVolume`/`canSetMute`) is absent is refused
  locally with feedback and no dispatch.
- One request per serial at a time; a second request for the same serial is
  refused with feedback. There is no automatic retry anywhere.
- Every dispatched request returns before completion; the controller tracks
  pending state by the protocol's snapshot-unique serial and clears it when
  the client reports exactly-once completion.
- Completion handling branches on status and stable reason code only:
  success clears pending quietly; `Uncertain` produces explicit
  "could not be confirmed" feedback; rejected/unsupported/busy/failed
  results map to fixed sentences. Late, foreign, or already-pruned request
  IDs are ignored and never replayed into the UI.
- When a serial disappears from the current snapshot (epoch replacement or
  graph change), its pending flag is dropped without feedback; the Audio1
  no-replay contract covers the operation itself.

Feedback is one dismissible error card; a new feedback replaces the previous
one. Feedback text is generated from typed results only.

## Keyboard and accessibility identity

The surface is a single accessible grouping named "Audio". Loading,
degraded, and unavailable states use `QindaQt.Controls` `StateCard` and
`DegradedNotice` alerts, so state changes are announced with complete text
instead of color alone. Every device and stream row exposes a volume slider
(0–100 percent, 5 percent keyboard steps, dispatch on release or per keyboard
step) and a mute switch, each with an explicit accessible name bound to its
row label and an accessible description that explains unknown levels,
in-progress changes, and unsupported capabilities. Default-device state is
plain text, and overflow rows are announced as counts. Controls keep their
native keyboard focus behavior and token focus ring; no component removes
the keyboard outline.

## Verification boundary

Focused selectors (wired by the standalone test seam):

```sh
ctest --test-dir build/dev -R '^qindaqt\.audio-applet-' --output-on-failure
```

`qindaqt.audio-applet-model` covers clamping, missing/invalid-wire fail-closed
behavior, ordering, label fallbacks, unknown levels, bounds and overflow,
default labels beyond the window, pending marking, and degraded retention.
`qindaqt.audio-applet-controller` covers the loading/ready projection through
a fake transport plus the real public client, clamp-before-dispatch, local
refusals, pending serialization, rejected/uncertain/success feedback paths,
stale-prune with ignored late replies, and degraded/unavailable phases.

These focused rows and the combined build compile the QML module, but do not
qualify rendered visual tokens, production shell registration, the
manifest/policy path, a live Audio1 service, or assistive technology.
Qualification of those belongs to the production-host slice above.
