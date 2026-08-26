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

## Runtime methods

| Method | Input | Current result |
| --- | --- | --- |
| `Capabilities` | None | Protocol, KWin ABI, methods, events, operations, limits, and control mode |
| `Windows` | None | Normal windows with UUID, title, application ID, current/requested frames, minimized state, and container owner |
| `Outputs` | None | Ordered outputs with name, logical frame, scale, numeric refresh rate in mHz, semantic transform, and internal flag |
| `InputCapabilities` | None | Schema-1 sanitized device inventory and observer properties |
| `Containers` | None | Published container IDs and decimal-string revisions |
| `DockWindows` | Two window IDs, orientation, position, ratio | Atomically creates and tiles one two-member container at revision 1 |
| `ReleaseContainer` | Container ID | Restores every member and terminates the container |
| `Snapshot` | Container ID | Current schema-versioned snapshot and revision, or rejection |
| `Submit` | Request JSON byte array | Committed, rejected, or conflict reply JSON |

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
cannot implement docking consumption.

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

## Runtime signals

- `ContainerCommitted(ay eventJson)` follows one successful transaction;
- `WindowsChanged()` covers manageable-window membership, captions, frames,
  minimized state, and ownership;
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

The live API does not yet mutate outputs, workspaces, focus,
maximize/fullscreen state, decorations, or pointer/keyboard grabs. Those are
owned by later hybrid-interaction and platform milestones. See
[Compositor and session integration](../architecture/compositor-session.md)
for exact runtime evidence and remaining limits.
