# Display client

The `display_client` module is the public, asynchronous consumer boundary for
the resident [Display1 service](display-service.md). It lets Settings and other
future presentation code observe validated display truth and sequence one
reversible transaction without importing service implementation, compositor
writer, QML, or platform-session dependencies. It does not make the packaged
Display1 transaction port capable of applying a configuration; that remains a
later writer milestone.

The wire values and limits are defined by
[Display1 version 1](../reference/display1-v1.md). Transaction authority stays
with Display1 as fixed by
[ADR-0016](../adr/0016-display1-transaction-authority.md): client countdowns and
states are projections, never a second transaction machine.

## Components and ownership

| Component | Responsibility | Lifetime and threading |
| --- | --- | --- |
| `DisplayTransport` | Injected asynchronous owner, activation, snapshot, invalidation, and mutation seam | Borrowed by `Client`; both objects remain on one Qt thread and the transport outlives the client |
| `QtDisplayTransport` | Qt D-Bus implementation addressed to the current unique owner | Holds a value handle to a registered connection; suppresses watcher completions after `stop()` |
| `Client` | Validates and atomically publishes snapshots, serializes mutations, and produces exactly one typed result per accepted request ID | Borrows the transport, owns all public state, and supports repeated `start()`/`stop()` on one Qt thread |
| `Coordinator` | Sequences one `Stage` → `Preview` → `Confirm` or `Cancel` interaction | Borrows and must not outlive its `Client`; owns no service timer or compositor mutation |

The Qt transport uses `StartServiceByName` when no owner exists. Activation
success is only evidence that a start request was accepted: `GetNameOwner` and
`NameOwnerChanged` remain the authority for a usable unique owner. Every call
is addressed to that unique name. Replies are decoded through the bounded
Display1 D-Bus adapters, emitted asynchronously, and carry the submitted owner
and request ID back to the client. Bus disconnect, empty owner, timeout,
malformed payload, and service-declared unavailability are distinct outcomes.

## Snapshot lineage and publication

The client treats one publication lineage as `(unique owner, service epoch,
revision, complete validated snapshot)`. Owner replacement clears the prior
lineage before fetching the new one; request IDs and exact-owner checks reject
late A/B/A replies. A bounded epoch announced by `Changed` fences the next
complete read. Revisions are ordered only within one accepted epoch.

Publication is whole-value and fail closed:

- an incoming snapshot must pass the public semantic validator before it can
  replace the last-known-good value;
- a lower revision is rejected;
- at the same revision, protocol, epoch, fingerprint, and outputs must remain
  identical, although `transactions` may change as the server state machine
  advances without changing output inventory;
- an exact duplicate produces no duplicate `snapshotChanged` signal;
- a malformed or hybrid reply moves the client to `Degraded` without partially
  publishing it; and
- explicit `available=false` or owner loss clears the last-known-good snapshot
  and completes any transport-backed mutation as uncertain.

`snapshot()` returns an optional owning value. Consumers must not manufacture a
revision-zero placeholder when no snapshot is held.

## Immediate brightness

`Client` also binds Display1's D7 immediate brightness methods
([ADR-0150](../adr/0150-admit-immediate-external-output-brightness-through-display1.md)).
After each accepted complete snapshot read, including the repeated topology
revision that `Changed` carries for a brightness-only republication, it issues
one `GetBrightness` read.

`brightness()` holds rows only while they pass the public join against the
held snapshot: same epoch, same topology revision, and the same stable output
order.

- Within one epoch, an older or same-revision hybrid reply never replaces the
  published rows, and an exact duplicate emits no `brightnessChanged`.
- Rows that name a newer topology revision are withdrawn, and the snapshot is
  read first.
- A new topology revision, owner loss, explicit unavailability, `stop()`, or a
  failed read withdraws the rows without changing the topology client state.
  An older Display1 without brightness therefore simply publishes none.

`setOutputBrightness()` shares the single serialized mutation slot. Before any
wire call it refuses:

- a stopped client;
- a pending operation;
- missing rows;
- an invalid value;
- a request whose epoch or brightness revision differs from the published rows
  (`stale-revision`);
- a stable ID that is not exactly one joined row (`unknown-output`).

An accepted request is forwarded unchanged. Its result has kind
`ImmediatePolicy` and arrives only when the service finishes. A success must
name the submitted epoch and brightness revision. Owner loss, timeout, and
mismatched results complete once as `Uncertain` and are never replayed.

## Client states and operation results

| State | Meaning |
| --- | --- |
| `Stopped` | Transport is stopped; no snapshot is published |
| `Starting` | Initial owner resolution or the first complete read is in flight |
| `Ready` | A validated snapshot is held and no mutation is pending |
| `Unavailable` | No owner exists or the owner declared the service unavailable |
| `Degraded` | An owner exists, but the last transport, decode, or lineage attempt failed; a prior valid snapshot may remain |
| `Busy` | Exactly one mutation is transport-backed |

Public mutation calls allocate monotonically increasing IDs. Local
preconditions are also completed asynchronously as typed `OperationResult`
values, so a caller can use one completion path. `client-not-running` and
`no-snapshot` map to `InvalidTransition`; `operation-pending` maps to
`TransactionActive`; invalid candidates and transaction IDs map to
`InvalidCandidate`; and stale lineage maps to `StaleRevision`. Timeouts, stop, owner replacement, malformed
replies, and mismatched owner/kind/transaction/epoch produce a single
fail-closed result and never authorize replay. Already-queued completions
survive a stop/start cycle; object destruction is the boundary that drops
undelivered signals.

`cancel()` may supersede another in-flight client operation because an abort
attempt must remain possible. The superseded request completes `Uncertain` and
its eventual late transport reply is ignored. Higher-level policy prevents
this mechanism from superseding a confirm after the coordinator reaches its
point of no return.

## Reversible transaction coordinator

The coordinator binds a transaction to the client owner and service epoch held
at `begin()`. It serializes one interaction and reports a closed outcome:
`Confirmed`, `Reverted`, `Uncertain`, or `NoOp`. A no-op stage finishes without
sending Preview. Owner loss, epoch replacement, or ambiguous completion while
active finishes `Uncertain` rather than guessing which topology is live.

A successful Preview reply means only that the server accepted the apply
request. The coordinator enters `AwaitingConfirmation` only after a validated
server snapshot contains its transaction summary in that state. Confirm is
valid only there. Cancel is valid while staging, previewing, or awaiting
confirmation; it is refused once Confirming begins because the service may
already have committed.

The optional rescue deadline begins only after the server-projected
`AwaitingConfirmation` state and is clamped to at least 25 seconds, beyond the
service's apply, observation, and 15-second confirmation windows. It is a
last-resort cancel request, not timer authority. A server-projected `Stuck`
state, lost lineage, or failed uncertain-cancel resolution produces
`Uncertain`.

## Evidence and remaining boundary

The focused D3 selector is:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  -u XDG_RUNTIME_DIR \
  ctest --test-dir build/dev --output-on-failure \
  -R '^qindaqt\.display-client-'
```

Five deterministic rows cover lineage, publication, operation completion,
coordinator policy, and immediate brightness through the injected public
transport seam. The brightness row (`qindaqt.display-client-brightness`)
covers:

- the topology join and monotonic republication;
- withdrawal on lineage loss;
- local refusals that never reach the transport;
- the delayed reply and a success that names another revision;
- a typed busy result;
- owner loss and timeout without replay.

One serial row composes the real Qt transport and resident service on a
disposable private session bus. It covers absent-owner activation, owner
replacement, stale candidate rejection, and teardown. It also covers a joined
`GetBrightness` read and an exact `SetOutputBrightness` request: the delayed
reply follows the observed republication, and service loss completes the
request uncertain. These rows open no display and do not prove
that KWin accepted an output configuration. Public output-management writer,
durable journal, Settings UI, nested compositor convergence, hardware, and
resource qualification remain later milestones.

See [Testing harness](../development/testing-harness.md) for the distinction
between deterministic, private-bus, nested-runtime, and hardware evidence.
