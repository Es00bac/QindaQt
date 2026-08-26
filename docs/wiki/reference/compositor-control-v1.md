# Compositor control protocol 1.0

`org.qindaqt.Compositor1` is QindaQt's experimental compositor inventory and
development-transaction boundary. Its complete method and signal surface is
declared in `compositor/dbus/org.qindaqt.Compositor1.xml`; a parity test keeps
that descriptor, deployment metadata, and the live capability set aligned.

## Address, encoding, and control mode

| Property | Value |
| --- | --- |
| Bus name | `org.qindaqt.Compositor` |
| Object path | `/org/qindaqt/Compositor` |
| Interface | `org.qindaqt.Compositor1` |
| Protocol | Major 1, minor 0 |
| Payload | UTF-8 JSON carried in D-Bus byte arrays (`ay`) |
| Revisions | Unsigned decimal JSON strings, never JSON numbers |

The plugin registers on the current user session bus. There is no caller
authentication. Consequently, production sessions advertise
`controlMode: "read-only"` and reject every external mutator with
`control-disabled`. The launcher enables `controlMode: "development-test"`
only when an explicitly supplied test scenario also creates the private
development-control marker. Inherited markers are cleared first.

This deployment gate is not an authorization mechanism. Production hybrid
interaction executes as process-local compositor policy and does not enable the
public mutation methods.

The process-local Hybrid topology and this protocol's per-container bridge are
separate authorities. Compositor1 remains a compatibility, diagnostic, and
isolated-development surface; it is not the transport used by pointer or
keyboard docking.

## Runtime methods

| Method | Input | Current result |
| --- | --- | --- |
| `Capabilities` | None | Protocol, KWin ABI, methods, events, operations, limits, control mode, and optional Hybrid diagnostics |
| `Windows` | None | Normal windows with UUID, title, application ID, current/requested frames, minimized/active/task/switcher state, absolute stack index, container owner, server-decoration flag, and live decoration class |
| `Outputs` | None | Ordered outputs with name, logical frame, scale, numeric refresh rate in mHz, semantic transform, and internal flag |
| `InputCapabilities` | None | Schema-1 sanitized device inventory and observer properties |
| `Containers` | None | Observable bridge and process-local Hybrid container IDs, actual decimal-string revisions, and explicit authority |
| `DockWindows` | Two window IDs, orientation, position, ratio | Atomically creates and tiles one bridge-owned two-member container at revision 1 |
| `ReleaseContainer` | Container ID | Restores and terminates one bridge-owned container |
| `Snapshot` | Container ID | Current schema-versioned bridge or Hybrid snapshot, authority, and revision, or rejection |
| `Submit` | Request JSON byte array | Bridge transaction committed, rejected, or conflict reply JSON |
| `InjectTestInput` | Schema-1 event-batch JSON byte array | Development-only normal-chain injection, or a production pre-parse rejection |
| `ReinitializeCompositingForTest` | None | Development-only queued KWin scene reinitialization, or a production pre-runtime rejection |

`DockWindows` currently accepts two live, distinct, independently owned
windows. Orientation is `horizontal` or `vertical`; position is `first` or
`second`; ratio is finite and strictly between zero and one. It creates a
single-leaf model only as synchronous unpublished staging, submits the split,
and unregisters staging on every failure. A successful reply has
`status: "docked"`, revision `"1"`, the generated IDs, and the complete
snapshot. No external call can observe or retain a singleton container.

`InputCapabilities` never exports key text, native scan codes, serials, pointer
addresses, or an input-event stream. Runtime device IDs last only for the
adapter lifetime. `observerActive` reports spy installation and
`consumesEvents` is always false; the KWin spy API observes before filters and
cannot implement docking consumption. Those fields describe only the inventory
spy. They do not describe the separate process-local Hybrid input filter, which
consumes events only after it acquires an exact QindaQt gesture grab.

Output transforms use these protocol strings: `normal`, `rotate-90`,
`rotate-180`, `rotate-270`, `flip-x`, `flip-x-90`, `flip-x-180`, and
`flip-x-270`. `refreshRateMilliHz` is a JSON integer.

For each window, `geometry` is KWin's current acknowledged frame.
`targetGeometry` is QindaQt's committed planned frame for a container member,
or KWin's most recent requested bounding frame for an independent window. They
may differ while a Wayland configure is pending, particularly for an inactive
minimized page whose client resumes only when that page activates. Planned
frames update atomically with ownership and are removed on detach, release,
close, or rollback; neither field is silently substituted for the other.
`serverDecorated` reports whether KWin attached a server decoration, and
`decorationClass` is the live decoration instance's Qt meta-object class (empty
when no server decoration exists). These fields distinguish a mapped
`QindaDecoration` from artifact discovery or `kwinrc` selection alone.
`active`, `skipTaskbar`, and `skipSwitcher` expose KWin's current presentation
policy. `stackIndex` is the window's absolute zero-based index in
`Workspace::stackingOrder()`, ordered bottom to top, or `-1` if a managed
window is momentarily absent from that list. Window activation, stacking, and
either skip flag invalidate `Windows` through `WindowsChanged()`.

## Development input seam

`Capabilities.developmentInput` always describes the fixed input schema with
`enabled`, `available`, `schemaVersion`, `maxEvents`,
`maxLogicalCoordinateMagnitude`, `deviceId`, and `eventTypes`. Only an explicit
isolated development scenario can make `enabled` true. That session constructs
the `qindaqt-development-input` keyboard/pointer device and adds it to KWin's
normal input redirection; events pass through the same observer, consuming
filter, and Hybrid controller as admitted seat input.

The input request contains exactly `schemaVersion` and a nonempty `events`
array. It is limited to 256 KiB and 64 events. Each event must have exactly one
of these shapes:

```json
{"type":"pointer-absolute","x":640.0,"y":350.0}
{"type":"key","key":"left-meta","pressed":true}
{"type":"key","key":"left-shift","pressed":true}
{"type":"button","button":"left","pressed":true}
```

Coordinates must be finite logical values between -1,000,000 and 1,000,000.
No other key, button, relative movement, text, delay, or device selector is
accepted. Success returns `status: "injected"`, the event count, and the fixed
device ID. Held modifiers/buttons are released before the device is removed.

In production, the injector object does not exist and `InjectTestInput` returns
`control-disabled` before inspecting payload size, JSON syntax, or schema. The
method therefore discloses no parse oracle and is not a production automation
surface.

## Development compositor-restart seam

`ReinitializeCompositingForTest` exists solely to qualify scene-resource
lifetime in an isolated scenario session. It takes no arguments. When
`controlMode` is `development-test` and the KWin compositor callback is
available, it schedules `KWin::Compositor::reinitialize()` for the next
event-loop turn and returns:

```json
{"status":"scheduled"}
```

Scheduling after the method returns prevents the blocking D-Bus reply from
racing the synchronous scene teardown it requested. If the development
callback is unavailable, the reply is `status: "rejected"` with failure code
`compositor-reinitialize-unavailable`. The caller must observe KWin's complete
inactive-to-active transition and then verify the restored scene state;
`scheduled` proves admission only.

Production checks the control gate before consulting compositor state and
returns `control-disabled`. It therefore exposes neither an availability oracle
nor a remotely callable scene restart. Like `InjectTestInput`, this is a
deterministic nested-test seam, not a supported automation API.

## Hybrid diagnostics

When the process-local runtime is constructed, `Capabilities` includes a
`hybrid` object:

| Field | Encoding | Meaning |
| --- | --- | --- |
| `ready` | JSON boolean | Runtime initialization succeeded and shutdown has not begun |
| `inputFilterInstalled` | JSON boolean | The consuming KWin filter is installed; it does not mean a grab is active |
| `shortcutRegistered` | JSON boolean | KGlobalAccel accepted all 13 Hybrid semantic actions in this session |
| `topologyRevision` | Unsigned decimal JSON string | Session-wide process-local revision, including lifecycle commands |
| `containerCount` | JSON integer | Number of process-local Hybrid containers |
| `chromeOverlayCount` | JSON integer | Number of reconciled per-container chrome records; the legacy field name does not imply a native overlay window or prove scene attachment |
| `visibleChromeOverlayCount` | JSON integer | Reconciled chrome records whose scene presentation is currently visible; this alone does not prove a live stacking anchor |
| `anchoredChromeSceneItemCount` | JSON integer | Reconciled scene items attached to a live member `WindowItem`, whether visible or intentionally hidden |
| `visibleAnchoredChromeSceneItemCount` | JSON integer | Published items that are visible and attached to a live member `WindowItem` |
| `quarantinedContainerCount` | JSON integer | Containers whose chrome and input are persistently suppressed after context adoption and release both failed; ordinary reconciliation cannot clear this state |
| `publishedGroupStackingCount` | JSON integer | Containers with a currently verified contiguous active-member stack block and scene anchor; a failed synchronization or raise removes the affected publication before hiding chrome |
| `lastGroupStackingFailure` | JSON string | Most recent group-stack synchronization or raise failure since the last successful complete stack synchronization; empty after a successful synchronization |

This object is an operational snapshot taken synchronously with
`Capabilities`; it emits no dedicated change signal and does not expose
constraints or restore-state values. Counts describe live publication state,
not cumulative telemetry. Its global revision is the same revision reported
for every currently published Hybrid container.

Every `Containers` entry contains `id`, `revision`, and `authority`. The older
per-container transaction bridge reports `authority: "control-bridge"` and its
own container revision. Process-local groups report
`authority: "hybrid-process"` and the actual session topology revision, never a
fabricated zero. `Snapshot` routes by authority and returns `status: "ok"`, the
protocol, container ID, matching revision and authority, and the schema-1 model
under `snapshot`. These reads do not make `Submit`, `DockWindows`, or
`ReleaseContainer` Hybrid mutators.

## Runtime signals

- `ContainerCommitted(ay eventJson)` follows one successful D-Bus-bridge
  transaction; process-local Hybrid commits do not emit it;
- `WindowsChanged()` covers manageable-window membership, captions, frames,
  minimized/active/task/switcher state, stacking order, and ownership;
- `OutputsChanged()` follows KWin output-set changes; and
- `InputCapabilitiesChanged()` follows input-device inventory/lifecycle
  changes.

All four signals are declared in XML and advertised by
`Capabilities.events`. Inventory signals are invalidation hints: clients read
the corresponding snapshot again rather than reconstructing state from signal
order.

## Submit transaction

A request has this shape:

```json
{
  "protocol": {"major": 1, "minor": 0},
  "transactionId": "caller-unique-id",
  "containerId": "container-id",
  "expectedRevision": "1",
  "operations": [{"type": "activate-page", "pageId": "page-id"}]
}
```

`transactionId`, `containerId`, and `operations` must be non-empty.
`expectedRevision` is a decimal string so every `quint64` remains exact. Major
versions must match; a caller minor newer than 0 is rejected. Unknown operation
fields are rejected rather than ignored.

| Operation | Required fields |
| --- | --- |
| `add-page` | `pageId`, `leafNodeId`, `windowId` |
| `activate-page` | `pageId` |
| `move-page` | `pageId`, non-negative integer `destinationIndex` |
| `split-window` | `targetWindowId`, `newWindowId`, `newLeafNodeId`, `splitNodeId`, `orientation`, finite `ratio`, `position` |
| `set-split-ratio` | `splitNodeId`, numeric `ratio` |
| `swap-windows` | `firstWindowId`, `secondWindowId` |
| `detach-window` | `windowId` |

The request limits advertised by `Capabilities.limits` are 256 KiB of JSON,
128 operations, and 256 characters for every identifier. The core
[window-container invariants](../architecture/window-containers.md) supply the
final structural and ratio validation.

## Commit and failure semantics

Operations apply to a candidate copy. An invalid operation reports its
zero-based `operationIndex`; no scene transition is prepared. A stale revision
returns `status: "conflict"`, code `revision-conflict`, and the current
revision. Unsupported protocol, malformed/oversized input, unknown containers,
exhausted revisions, nested transactions, and scene preparation or commit
failures return `status: "rejected"` with a stable code and message.

The scene adapter preflights live KWin objects, stages geometry, minimized
state, restore frames, and ownership, and reverses applied state if commit or
finalization fails. Only then does the bridge replace the model, increment its
revision once, and emit one event. Removing a member that would leave a
singleton also detaches the survivor in that same transaction. The committed
empty snapshot is the terminal event and the bridge immediately removes the
container.

The production public API does not mutate outputs, workspaces, focus,
maximize/fullscreen state, decorations, pointer/keyboard grabs, or compositor
scene lifetime. The isolated development methods are the explicit exceptions
for bounded input injection and queued compositor reinitialization. The
process-local Hybrid runtime owns a subset of group policies without expanding
Compositor1; output configuration remains a Platform-services boundary. See
[Compositor and session integration](../architecture/compositor-session.md)
for exact runtime evidence and remaining limits.
