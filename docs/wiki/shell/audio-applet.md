# Audio applet

`src/shell/audio_applet` owns the production panel Audio applet: bounded
device and application-stream rows, default-device labels, and serialized
volume/mute mutation lifecycles. A pure target projects bounded Audio1 values
over the public
[`audio_protocol`](../reference/audio1-v1.md); a separately linked
shell-private runtime target borrows the public `AudioClient` and exposes
only owned presentation values to compiled QML. Neither target reaches into
the audio service, WirePlumber, PipeWire, or any service internal. The
service-side architecture is described in
[Audio service](../architecture/audio-service.md), and the applet resolution
rules live in [Applet runtime](applet-runtime.md).

Current maturity: **production built-in composition (compiled and verified)**.
The production slice adds the audited manifest/registry/policy path,
stock-profile placement, production shell composition, keyboard-accessible
compiled QML, static mutation gates, and a relocated installed-package test.
Its live truth still reflects the Audio1 runtime qualification: until the
platform WirePlumber adapter is the activated backend, the resident service
reports honest unavailable/degraded truth, so the applet presents that truth
and dispatches no invented state. This UI slice does not advance the audio
platform milestone by itself.

## Module shape

| Piece | Responsibility |
| --- | --- |
| `AudioAppletModel` (`audio_applet_model.h/.cpp`, pure target) | Pure projection of one validated snapshot plus a pending-serial set into phase state, bounded rows, default-device labels, and overflow counts. No object machinery, no transport, no retention. |
| `AudioAppletController` (`audio_applet_controller.h/.cpp`, runtime target) | One borrowed `AudioClient`'s shell-side facade. Reprojects on state/snapshot changes, owns pending-request bookkeeping, applies the injected read/control grants, clamps and validates requests, and turns typed operation results into stable feedback. The only object QML sees. |
| `QindaQt.Shell.AudioApplet` QML module (runtime target) | Token-styled presentation via `QindaQt.Controls 1.0`: main surface plus one device-row and one stream-row component. Injected controller only; no service import. |

The controller never starts, stops, or parents the client; shell composition
owns that lifecycle. QML receives no client, snapshot, handle, or D-Bus
object, matching the shell rule that QML consumes a model projection while
shell-side C++ consumes the public client.

## Grants and exact-owner projection

`audio.read` and `audio.control` are separate manifest/policy grants, exactly
like the Power and Bluetooth applets. The composition evaluates both once
through the audited manifest → host-selection → compiled-registry →
capability-policy path and injects the decisions into the controller:

- Read denial suppresses observation entirely: the surface presents a stable
  "access was not granted" unavailable state even if the client holds truth,
  and no row ever renders from a denied grant.
- Control is effective only together with read (`control && read`). Control
  denial keeps bounded rows visible but non-adjustable; every mutation is
  refused locally with feedback before dispatch. QML gates adjustability on
  the controller's `controlGranted` property instead of trusting row state.
- Owner loss or replacement, a stopped/starting client, malformed truth, or
  an unavailable snapshot clears every row, default label, and pending flag.
  Old rows never survive as actionable last-known-good state, and nothing
  replays against the next owner.

The pure projector validates direct test inputs again, even though the
public client already validates snapshots before publication.

## Presentation contract

Phases, exposed as `phaseText`:

| Phase | Meaning |
| --- | --- |
| `loading` | Client stopped/starting, or starting/degraded client without a validated snapshot yet. |
| `ready` | Validated ready snapshot; rows and default labels are shown. |
| `degraded` | Validated degraded snapshot; rows stay visible with a stable reason notice. |
| `unavailable` | Client-level loss, snapshot-reported unavailability, a snapshot that failed wire validation, or a read-grant denial. No rows are shown. |

`phaseReasonText` maps stable reason codes (`unavailable`,
`malformed-snapshot`/`backend-malformed`, `pipewire-replaced`,
`wireplumber-replaced`, `owner-replaced`, `authority-replaced`) to fixed,
localized sentences. Unknown or empty codes fall back to one generic
sentence; a read denial maps to its own fixed access sentence. Diagnostics
are never shown or parsed.

Rows: at most 8 device rows (outputs before inputs, protocol
ascending-serial order) and 8 stream rows. Anything beyond the window is
summarized as an `overflowDeviceCount`/`overflowStreamCount` label. Each row
carries label, default/direction flags, volume with `volumeKnown`, mute with
`muteKnown`, capability booleans, and the `pending` flag. A
`volumeKnown`/`muteKnown` false value is shown as unknown and disables the
corresponding control; a missing description falls back to the short name,
then to "Unknown device"/"Unknown stream".

Default state: `defaultOutputLabel`/`defaultInputLabel` resolve the
snapshot's default handles by serial and stay correct even when that device
falls outside the retained window; an unknown `(0,0)` handle yields no
label.

The panel surface is one 32-by-28 icon button. Its symbolic name follows the
default output's mute and normalized-volume state (`muted`, `low`, `medium`,
or `high`), and an unresolved asset becomes the typed Audio placeholder.
All device, stream, state, and feedback text lives in the existing focusable
details popup. This preserves keyboard and accessible control behavior while
preventing the mixer layout from contributing to panel width or height.

## Request rules

`requestVolume(serial, isStream, volume)` and
`requestMute(serial, isStream, muted)` are the only mutations, and this
applet never sets defaults or moves streams.

- The control grant is checked before any dispatch; denial is refused
  locally with feedback.
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
  IDs are ignored and never replayed into the UI. An exact-owner
  replacement resolves an in-flight request as `Uncertain` without replay.
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
(0–100 percent, 5 percent keyboard steps, dispatch on release or per
keyboard step) and a mute switch, each with an explicit accessible name
bound to its row label and an accessible description that explains unknown
levels, in-progress changes, policy-denied control, and unsupported
capabilities. Default-device state is plain text, and overflow rows are
announced as counts. Controls keep their native keyboard focus behavior and
token focus ring; no component removes the keyboard outline.

## Production seams

The module is registered through these additive seams:

1. `data/applets/audio.json` requests `audio.read` and `audio.control`, and
   the stock `qindaqt` profile places one instance in the end zone.
2. `BuiltinAppletRegistry::firstParty()` admits `qindaqt.applets.audio`;
   `BuiltinAppletContent.qml` is the separately tested renderer inventory
   gate.
3. `AudioAppletComposition` owns the public `QtAudioTransport` →
   `AudioClient` → controller lifetime for `ShellRuntimeApplication`, and
   `RuntimePanelWindowFactory` injects only the controller into QML.
   Observation starts only when the read grant was evaluated affirmatively.
4. The `AudioAppletRuntime` install component stages the production shell,
   its directly linked Controls library, the Tokens library at Controls'
   baked sibling RUNPATH, compiled QML, manifest, selected profile, policy,
   and theme for a relocation/poison test. The default `QindaQt` component
   carries the same shell/Controls/Tokens loader closure.

The preview injects no live audio facade and therefore renders the
deterministic static fixtures rather than live audio state.

## Focused tests

After the test seam is wired, the focused selector is:

```sh
ctest --test-dir build/dev -R '^qindaqt\.audio-applet-' --output-on-failure
```

| Test | Scope |
| --- | --- |
| `qindaqt.audio-applet-model` | Clamping, missing/invalid-wire fail-closed behavior, ordering, label fallbacks, unknown levels, bounds and overflow, default labels beyond the window, pending marking, and degraded retention. |
| `qindaqt.audio-applet-controller` | Public-client projection, read/control grant separation, clamp-before-dispatch, local refusals, pending serialization, rejected/uncertain/success feedback, stale-prune with ignored late replies, degraded/unavailable phases, and exact-owner replacement clearing truth and pending work without replay, including a stale old-owner reply dropped after replacement. |
| `qindaqt.audio-applet-offscreen` | Compiled module loading, Return-opened summary, the exact muted/low/medium/high icon names from four fake-transport snapshots, keyboard slider steps, accessible grouping/slider/switch roles with complete names and descriptions, and real controller dispatch through a fake transport. |
| `qindaqt.audio-applet-boundary` | Static policy gate rejecting transport, QML, platform, and service-implementation tokens outside the declared include roots in the pure projection and its focused test; four independent poison mutations (D-Bus transport include, service-internal include, QML include, QObject derivation) must each be rejected. |
| `qindaqt.audio-applet-runtime-boundary` | Runtime source-policy gate rejecting service internals, WirePlumber/PipeWire/GLib surfaces, process/file access, and D-Bus in the controller/QML; the shell composition root may construct the public Qt transport. Includes a poison negative control. |
| `qindaqt.audio-applet-installed-package` | Relocated shell/data, exact staged KF6 and Controls/Tokens loader-path resolution through relative RUNPATH, compiled QML evidence, and installed manifest discovery under source-path poison. |
| `qindaqt.shell-runtime-component-closure` | Each component that carries `qindaqt-shell` installs alone, resolves Controls/Tokens from its own relative stage, and runs `--help` on the explicit offscreen platform with ambient loader, display, Wayland, and session-bus variables cleared. |

The boundary gates also run without configure:

```sh
cmake -DSOURCE_ROOT=<repository> -P tests/shell/audio_applet/check_boundary.cmake
cmake -DSOURCE_ROOT=<repository> -P tests/shell/audio_applet/check_runtime_boundary.cmake
```

## Non-claims

This slice proves no live PipeWire/WirePlumber adapter behavior, hotplug,
stream moves, default-device changes, physical hardware, realtime latency,
or nested-compositor interaction. It owns no aggregation, threshold, or
platform policy: those remain Audio1 and audio-service authority.
Installed proof is relocation and source-policy evidence, not a claim that a
live audio backend has become available.
