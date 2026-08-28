# Compositor control protocol 1.1

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
| Protocol | Major 1, minor 1 |
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
| `Outputs` | None | One generation of ordered outputs with stable identity, logical geometry, mode, priority, and display metadata |
| `InputCapabilities` | None | Schema-1 sanitized device inventory and observer properties |
| `ShellVisibilitySnapshot` | None | One revisioned, atomic output/window/scope generation for shell panel visibility policy |
| `DevelopmentShellSurfaces` | None | Development-only compositor-owned notification layer-surface evidence, or a production pre-inspection rejection |
| `Containers` | None | Observable bridge and process-local Hybrid container IDs, actual decimal-string revisions, and explicit authority |
| `DockWindows` | Two window IDs, orientation, position, ratio | Atomically creates and tiles one bridge-owned two-member container at revision 1 |
| `ReleaseContainer` | Container ID | Restores and terminates one bridge-owned container |
| `Snapshot` | Container ID | Current schema-versioned bridge or Hybrid snapshot, authority, and revision, or rejection |
| `Submit` | Request JSON byte array | Bridge transaction committed, rejected, or conflict reply JSON |
| `InjectTestInput` | Schema-1 event-batch JSON byte array | Development-only normal-chain injection, or a production pre-parse rejection |
| `AddVirtualOutputForTest` | Name, logical width/height, scale | Virtual-backend-only bounded test output creation, or a pre-parse rejection |
| `RemoveVirtualOutputForTest` | Name | Removes only a virtual output created by this plugin instance, or rejects |
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

`Outputs` returns `status`, schema version 1, decimal-string
`outputGeneration`, and an `outputs` array. Each entry retains the 1.0 fields
`name`, `geometry`, `scale`, `refreshRateMilliHz`, `transform`, and `internal`,
and 1.1 additively supplies `uuid`, unconstrained unsigned 32-bit `priority`,
`physicalSizeMm`, `manufacturer`, and `model`. `0x0` physical size means KWin
does not know it; it is not a measurement. Ordering is KWin's semantic
`Workspace::outputOrder`, including its stable order for equal priorities.
Names must be unique and nonempty; nonempty UUIDs must also be unique.
Output names share the shell wire's 512-UTF-16-unit identifier bound. UUID and
manufacturer text are at most 128 UTF-8 bytes, model text at most 256, and
physical dimensions are non-negative and at most 10,000 mm per axis. Control,
format, embedded-null, and malformed-surrogate text rejects the whole sample.

The first accepted projection has generation `"1"`. One generation advances
only when the complete canonical ordered projection changes, never once per
KWin signal. Malformed, ambiguous, empty, or over-limit candidates are rejected
atomically and retain the previous response and generation. The inventory uses
the shell visibility wire bounds of 64 outputs and scale `(0, 16]` so its
projection cannot exceed what `ShellVisibilitySnapshot` represents.

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

## Shell visibility snapshot

`ShellVisibilitySnapshot` is a separate read-only inventory for panel hiding.
It samples user windows, workspace, and activity against one immutable accepted
`Outputs` generation; clients must not combine the independent `Windows` read
with it. A successful payload has this schema:

```json
{
  "status": "ok",
  "schemaVersion": 1,
  "epoch": "3d975df8-3ee6-4cc5-bf64-3d46cab972d0",
  "revision": "7",
  "outputGeneration": "12",
  "scope": {"workspaceId": "workspace-1", "activityId": "activity-1"},
  "outputs": [
    {"id": "DP-1", "geometry": {"x": 0, "y": 0, "width": 1920, "height": 1080}, "scale": 1.0}
  ],
  "windows": [
    {
      "id": "6bbdbb45-18d7-4f32-a850-0ca467706f62",
      "outputId": "DP-1",
      "frameGeometry": {"x": 40, "y": 60, "width": 900, "height": 700},
      "workspaceIds": ["workspace-1"],
      "onAllWorkspaces": false,
      "activityIds": [],
      "active": true,
      "maximized": false,
      "minimized": false,
      "hidden": false
    }
  ]
}
```

Output IDs are KWin stable output names. Window IDs are KWin internal UUIDs,
workspace IDs are stable virtual-desktop IDs, and an empty `activityIds` array
means all activities. `onAllWorkspaces: true` requires an empty `workspaceIds`
array. When Activities is unavailable, `scope.activityId` is the canonical
KWin null UUID `00000000-0000-0000-0000-000000000000`, never empty.

Output geometry is KWin's exact integral desktop-logical geometry, matching the
Qt screen boundary. Fractional window frames are conservatively aligned
outward (floor origin, ceil far edge), so a subpixel overlap cannot disappear.
Schema 1 is bounded to 4 MiB, 64 outputs, 4,096 windows, 256 workspace or
activity memberships per window, 512 UTF-16 code units per identifier, and
output scales in `(0, 16]`. A non-finite, overflowing, malformed, duplicate,
over-limit, unknown-output, outside-output, or internally inconsistent admitted
value rejects the complete candidate. The compositor retains the prior valid
generation and emits no invalidation; it never publishes a selectively pruned
user-window inventory.

Visibility admission deliberately differs from Hybrid topology admission.
Ordinary managed normal, dialog, and utility windows participate, including
eligible transients. Desktop, dock/layer-shell, splash, tooltip, menu, popup,
internal, deleted, and unmanaged surfaces do not. `hidden` combines KWin's
ordinary hidden state and Show Desktop hiding; `minimized` remains separate.
`maximized` means both native axes or QindaQt whole-container maximize, not
quick-tiled or partially maximized.

The first valid generation has revision `"1"`; zero is never a successful
revision. Revision advances exactly once when canonical state changes and is
unchanged for reordered/equivalent input. `epoch` is a fresh UUID for each
plugin/service instance. Clients bind `(unique D-Bus owner, epoch, revision)`
and reject replies from an older owner or epoch, because revision restarts with
the compositor. Before any valid generation, the method returns
`status: "unavailable"`, schema 1, the current epoch, revision `"0"`, and a
stable failure object; that reply is not policy input.

`ShellVisibilityChanged()` is a coalesced no-argument invalidation hint.
Clients mark their cache dirty and reread the complete snapshot; they never
reconstruct state from signal order.

The production shell binds the invalidation and method call to the current
unique D-Bus owner rather than the replaceable well-known name. It accepts only
one coherent `(owner, epoch, revision)` lineage, uses fixed-leading debounce
with one in-flight read, and switches to all-visible panel policy on service
loss, unavailable/malformed data, timeout, revision regression/collision, or
exact Qt-output mismatch. Forward gaps are accepted because every payload is a
complete generation and invalidations may coalesce. Recovery requires a later
complete valid generation; no partial inventory is retained as policy input.

## Development notification-surface evidence

`DevelopmentShellSurfaces` is a read-only qualification seam, not a supported
desktop or automation API. It is absent from the shell's production behavior
and returns `control-disabled` before inspecting KWin state unless the launcher
has admitted an explicit isolated development scenario. The compositor filters
the result to QindaQt's exact `notification-popup` and
`notification-center` scopes; it never inventories unrelated user surfaces.

Each returned record pairs a live KWin window with its exported, committed
layer-shell protocol object by their shared Wayland surface. The plugin never
depends on KWin's unexported internal layer-window class. It reports the client PID, scope,
committed/mapped/active/focus-acceptance state, layer, anchors, margins,
exclusive zone, desired size, KWin frame geometry, and current plus desired
output names. A live driver must reject duplicate notification roles and bind
the PID to the separately authenticated production shell process. The center
must be active when its keyboard traversal is sampled; the popup must remain
inactive so an incoming notification cannot steal focus.

The production shell's complementary `org.qindaqt.ShellDevelopment1` snapshot
is described in [Notification presentation](../shell/notification-presentation.md).
Both halves are admitted only on a private bus after the shell authenticates
the exact compositor PID and verifies `controlMode: "development-test"` plus
enabled mutations. [ADR-0020](../adr/0020-authenticate-private-live-evidence.md)
records why this deliberately narrow cross-process evidence boundary exists.

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
{"type":"key","key":"n","pressed":true}
{"type":"key","key":"tab","pressed":true}
{"type":"key","key":"escape","pressed":true}
{"type":"key","key":"space","pressed":true}
{"type":"key","key":"down","pressed":true}
{"type":"key","key":"enter","pressed":true}
{"type":"button","button":"left","pressed":true}
```

Coordinates must be finite logical values between -1,000,000 and 1,000,000.
The complete key allowlist is left Meta, left Shift, N, Tab, Escape, Space,
Down, and Enter. The notification live-session rows need the additional
non-text keys to exercise the production global shortcut, focus traversal,
activation, dismissal, and lock-screen user-activity paths without host input.
No other key, button, relative movement, text, delay, or device selector is
accepted. Success returns `status: "injected"`, the event count, and the fixed
device ID. Held keys and buttons are released before the device is removed.

In production, the injector object does not exist and `InjectTestInput` returns
`control-disabled` before inspecting payload size, JSON syntax, or schema. The
method therefore discloses no parse oracle and is not a production automation
surface.

## Development virtual-output seam

The typed output methods exist in the D-Bus descriptor for compatibility but
are advertised in `Capabilities.methods` with a `developmentOutput` bounds
object only when all three construction facts hold: the launcher supplied an
explicit test scenario, enabled development control, and selected its exact
KWin virtual backend. The launcher clears inherited backend proof first. This
marker is construction metadata in a private test workflow, not caller
authentication.

Names are 1–128 ASCII bytes, begin with an alphanumeric byte, and thereafter
use only alphanumerics, `-`, `_`, or `.`. Width and height are logical extents:
width is 320–16,384, height is 200–16,384, and scale is finite in `[1, 3]`.
The virtual backend derives the pixel mode from logical size times scale. At
most eight plugin-owned outputs and 32 total backend outputs are admitted.
Creation rejects both the requested name and KWin's `Virtual-<requested>`
connector spelling if either is already present.

The adapter records the requested name to the exact borrowed output pointer
returned synchronously by KWin. Removal never searches by connector name and
cannot remove a pre-existing output. Backend failure retains ownership and the
previous public inventory generation. Plugin teardown unpublishes D-Bus, then
synchronously removes only still-live owned pointers. In production and every
non-virtual development session, valid and malformed calls both return the
same `control-disabled` response before string/number validation or backend
inspection.

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
- `OutputsChanged()` follows one newly accepted output inventory generation;
- `InputCapabilitiesChanged()` follows input-device inventory/lifecycle
  changes; and
- `ShellVisibilityChanged()` coalesces relevant output, admitted-window,
  workspace, activity, and Hybrid group-maximize changes.

All five signals are declared in XML and advertised by
`Capabilities.events`. Inventory signals are invalidation hints: clients read
the corresponding snapshot again rather than reconstructing state from signal
order.

## Submit transaction

A request has this shape:

```json
{
  "protocol": {"major": 1, "minor": 1},
  "transactionId": "caller-unique-id",
  "containerId": "container-id",
  "expectedRevision": "1",
  "operations": [{"type": "activate-page", "pageId": "page-id"}]
}
```

`transactionId`, `containerId`, and `operations` must be non-empty.
`expectedRevision` is a decimal string so every `quint64` remains exact. Major
versions must match; a caller minor newer than 1 is rejected. Unknown operation
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
