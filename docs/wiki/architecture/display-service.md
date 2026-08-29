# Display service

The Display foundation owns bounded display values, privacy-preserving
identity, pure topology validation, a deterministic preview/revert model, and
a resident Display1 service foundation. D1 remains transport-free. The bounded
D2 service slice adds the bus object/process, activation metadata, monotonic
timer scheduling, and an exact-owner adapter over D0's read-only
`Compositor1.Outputs` inventory. D4 now adds a bounded public output-management
writer module. D5 adds the separate crash-safe filesystem journal adapter and
deterministic startup-load seam. The packaged process does not compose these
adapters until authenticated lock/logind, recovery orchestration, and contained
nested proof exist. Settings integration, UI, and physical runtime proof also
remain later outcomes.

[ADR-0016](../adr/0016-display1-transaction-authority.md) fixes transaction
authority. [ADR-0017](../adr/0017-persistent-output-identity.md) fixes persistent
identity. The exact values and limits are in
[Display1 version 1](../reference/display1-v1.md).

## Authority boundary

Pinned KWin 6.6.5 remains the sole live-output and restore authority. It owns
the active topology and its private `kwinoutputconfig.json` store. QindaQt must
not read, write, watch, or recreate that store.

The resident Display1 process is the only component allowed to become a
QindaQt production writer to KWin's public KDE output-management protocol. It
already owns the D1 machine and monotonic confirmation/revert scheduling, but
the packaged transaction port deliberately rejects journal storage and never
applies. The accepted D4 [compositor writer](display-writer.md) supplies the
direct public-protocol boundary. The accepted D5 filesystem adapter supplies
canonical durable load/store/clear behind that writer's journal seam, as fixed
by [ADR-0051](../adr/0051-persist-display-journal-in-injected-state-root.md).
Later composition must still supply lock/logind, startup recovery routing, and
nested convergence evidence before writer authority is operational. Settings
Center, shell overlays, global
shortcuts, Color, and brightness consumers will cross typed client boundaries.
They do not own the timer or apply output configurations directly.

Settings1 will own bounded display policy and registry persistence after a
Settings-owned schema migration. It will not become desired live topology.
Display geometry used by the shell continues to come from compositor/Qt
inventory and never waits for Display1.

## Pure D1 modules

| Module | Owns | Dependencies | Explicitly does not own |
| --- | --- | --- | --- |
| `display_protocol` | Versioned snapshot/candidate/result values, hostile-input limits, semantic validation, canonical byte codec, QtDBus value serialization | Qt Core; Qt DBus only for serialization | Bus connection/name/service, XML, client, policy, platform objects |
| `display_identity` | One-batch identity resolution, privacy/ambiguity rules, schema-v2 registry values and v1 migration | Qt Core | EDID acquisition, Settings persistence, compositor UUID authority, logging raw hardware material |
| `display_topology` | Candidate validation, normalization, logical geometry, mirror projection, warnings, canonical fingerprint, diff/no-op | `display_protocol`, Qt Core | Current-state discovery, stored preferences, KWin objects, mutation |
| `display_transaction` | One pure injected-clock/port state machine, journal value/codec, retry and recovery truth | `display_protocol`, `display_topology`, Qt Core | Real time, timers, filesystem, D-Bus/Wayland, lock/logind observation, compositor mutation |

All APIs borrow inputs only for a call and return owning Qt value types.
Identity, topology, protocol codecs, and journal codecs are reentrant and may
run on any thread. A `Machine` and its borrowed clock/port are confined to the
thread that constructs the machine; both dependencies remain addressable and
outlive it. No provider is a `QObject` in D1.

Rejection is typed and fail-closed. Decoders and pure transformations build a
temporary and do not partially replace caller-owned state. A rejected machine
command preserves its view, accepted snapshot, and active journal exactly. On
accepted results, `stateChanged` is true exactly when the view or snapshot
changed; port call counters are not machine state. Unknown versions and enum
values fail closed. Adding or
reinterpreting a public value requires protocol/schema compatibility review.

## Resident D2 service foundation

`display_service` is a thread-confined Qt Core/DBus composition layer over the
four D1 public modules. Its dependencies are constructor-visible:

- an `InventorySource` publishes complete typed D0 inventory frames and
  transport loss;
- a `TransactionPort` implements the D1 journal/apply side-effect interface and
  may report one completion tagged with the exact resident-machine lineage and
  D1 token;
- a monotonic clock supplies D1 time; and
- an epoch factory supplies a bounded restart-unique seed; the model hashes it
  into a public epoch beside a process-monotonic lineage number for each
  accepted upstream owner lineage.

The production inventory source watches `org.qindaqt.Compositor`, resolves its
current unique D-Bus owner, calls `Outputs` on that unique name, and treats
`OutputsChanged` only as an invalidation hint. It serializes one read, rejects
late replies after owner replacement, caps JSON at 4 MiB before parsing, then
converts the complete schema-1 response into bounded values. D0 permits 64
outputs and scale through 16; Display1 deliberately rejects any sample outside
its stricter 32-output and 1.0–3.0 limits. Geometry must be exactly integral
because the Display1 v1 topology values are integral.

One accepted source lineage is `(uniqueOwner, outputGeneration, complete typed
outputs)`. Display1 derives a bounded public epoch from the factory's
restart-unique seed and the next process-monotonic machine lineage, then maps
its public revision to the positive `outputGeneration`. The lineage component
prevents an A/B/A owner or repeated-seed sequence from republishing any epoch
already accepted in that process without retaining an attacker-controlled
history set. At equal generation, only exact typed equality is accepted.
Changed equal-generation truth, a regression, or a newer generation with
unchanged contents rejects without partial replacement. An owner change first
removes the old snapshot/machine, then establishes a fresh epoch; explicit
transport loss also makes `GetSnapshot` and mutations unavailable. Revisions
are never ordered across owners.

D0 exposes only enabled outputs and the observed current mode. Projection
therefore publishes one deterministic `current:WIDTHxHEIGHT@MILLIHERTZ` mode
per output, canonical first-output primary and contiguous priority, and no
invented disabled modes or replication relation. D0 supplies no EDID or MST
material, so D1 connector fallback is the only stable-ID authority in this
adapter. Runtime compositor UUID remains descriptive adapter metadata and is
never hashed or copied into the stable ID. The projector accepts only a
complete `validateSnapshot` value whose `liveFingerprint` is the D1 canonical
projection fingerprint.

The service model owns routing into the D1 state machine. An active output-set
change uses `topologyChanged`; a same-set change while `Staged` uses
`externalIntentObserved`; other active observations use `observedSnapshot`.
Before constructing each replacement D1 machine, the model advances a
process-local outer lineage and gives it to the transaction port. A completion
must match both that lineage and the machine's exact token. This prevents a
late pre-loss completion from colliding with a token number reused by the new
machine after owner replacement or transport recovery. The port copies the
outer lineage beside each apply request; advancing the current lineage never
retags an already-issued request.
Stage, preview, confirm, cancel, exact-token completion, tick, safety, suspend,
and settle calls remain D1 transitions. The resident owns a single-shot Qt
timer and re-arms it from the injected monotonic deadline. It exports
`GetSnapshot`, `Stage`, `Preview`, `Confirm`, and `Cancel`, plus a complete-read
`Changed(epoch, revision, available)` hint. Unavailable reads return a typed
D-Bus error rather than an invalid placeholder snapshot.

Every `GetSnapshot` read publishes the machine's accepted snapshot composed,
by one whole-value copy at the read boundary, with exactly zero or one
validated public `TransactionSummary` projected from the machine view:
transaction id, mapped public state, active reason, service-clock
monotonic deadline, and revert attempt, with the staged candidate's base
revision as `baseRevision` and the machine's current accepted revision as
`observedRevision`. `Discovering` and `Ready` publish no summary; the three
rollback states (`RevertingApply`, `RevertingObserve`, `RevertBackoff`)
project as the single public `Reverting` state; `Stuck` exposes its
consumed attempts. The protocol's `PersistingJournal` state is never
published because journal storage is a synchronous hard gate inside
preview, not an observable machine state. A projection that cannot produce
a complete summary validated against the snapshot's own epoch and revision
publishes none — never a partial or lineage-divergent value — so consumers
must treat confirmation readiness as exactly "the snapshot carries a
summary in `AwaitingConfirmation`", never an inference from a Preview
result.

The installed executable starts with safety `Unknown` and an unavailable
transaction port. Preview therefore fails closed at the D1 safety gate; even
if an in-process composition supplies `Safe`, the packaged port cannot store a
journal and fails the next hard gate without an apply request. This is a
deliberate source-complete stopping point, not a simulated writer. The service
has no KWin headers/private ABI, Wayland objects, direct filesystem access,
Settings, QML, or platform lock/session dependency. D5 exists as an injectable
library but is not silently constructed from an environment path.

## Durable journal adapter

`FileJournalStore` implements the D4 `JournalStore` seam without moving
filesystem policy into D1, D2, or the compositor writer. Its constructor takes
an existing effective-user-owned state root. It addresses only
`display1-transaction.journal` and its fixed same-directory temporary name;
there is no `HOME`, XDG, Settings, or global lookup.

Store accepts only a valid D1 journal, writes its canonical versioned bytes to
a newly created mode-0600 temporary file, syncs content, atomically renames in
the same directory, and syncs directory metadata where supported. The typed
mutation result is `Unchanged` before the pathname commit, `Durable` after a
successful barrier, or `DurabilityUncertain` when rename/unlink succeeded but
the following directory barrier failed. Load never
follows links, accepts only a restrictive regular single-link file owned by the
effective user, enforces the 1 MiB limit on both pathname and opened-descriptor
metadata before allocation and again while streaming, and returns absent/
loaded/rejected without partially publishing a value. It rejects non-canonical
bytes after decode. A stale temporary name is ignored by load and safely
replaced by the next store, which models an interruption before the commit
point. Clear never deletes an unsafe final entry.

The future resident startup path must call `load()` before enabling writer
authority, pass a loaded journal to D1 `recover`, and keep rejected or active
truth until model policy authorizes clear. D5 does not itself choose the state
root, run recovery, observe lock/session safety, or claim that KWin converged.

## Identity and registry

Identity precedence is unique EDID identifier, unique raw EDID, unique
EDID/MST composite, then bounded connector fallback. Published IDs contain
only a source prefix and safe connector or truncated digest. Serial and raw
EDID material never leave the resolver. Duplicate stronger materials and
residual collisions are explicit ambiguity; an ambiguous output cannot be
aliased.

The runtime compositor UUID is an adapter handle, not persistence identity. It
is validated on input and intentionally omitted from resolved and registry
values. Registry reconciliation takes a caller-owned monotonic seen sequence,
updates metadata for accepted stable IDs, retains disconnected entries, and
evicts the least-recently-seen disconnected entry at the 64-entry cap. The
pure registry never opens Settings or a file.

## Topology contract

A candidate is bound to the current `(serviceEpoch, revision)` and must name
exactly the snapshot's output set. At least one output is enabled, exactly one
is primary, and priorities form QindaQt's canonical contiguous `1..N` order.
Modes must belong to their output; scale is finite from 1.0 through 3.0;
transforms are closed; enabled coordinates are bounded to ±1,000,000 before
normalization and the resulting rectangles cannot exceed +1,000,000.

Negative input layouts are translated so the minimum enabled non-mirror origin
is `(0,0)`. Positive-area overlap is rejected; edge-disconnected layouts are
accepted with a gap warning. Logical extents use nearest integer rounding
`floor(pixel / scale + 0.5)`, with dimensions transposed for 90/270-degree
rotations and their flipped variants. A non-integral exact extent is an
explicit warning, not rejection.

Replication is a relationship between distinct enabled identities. Unknown
sources, self-reference, and cycles fail closed. Position and scale of a
replica are canonical projections of its ultimate source and cannot change a
fingerprint. Mode and transform remain target-specific per-output fields.
Candidate geometry represents the source rect for each replica; actual
`wl_output` visibility remains a later nested assertion, not D1 evidence.

Disabled live outputs are canonical: not primary, priority zero, position
`(0,0)`, and no replication source. This makes the snapshot projection,
baseline no-op diff, and fingerprint identical. Topology never infers current
state from a registry or preference.

## Transaction model

The machine stages one class-A candidate only when its base lineage equals the
current accepted snapshot. In `Ready`, a redelivered snapshot at the current
epoch/revision is accepted only when the complete snapshot is exactly equal;
changed truth must carry a strictly newer revision. This keeps a candidate
projected before an external change behind the revision fence. Staging performs
no side effect. Preview first stores a complete pre-image/target journal
atomically, then emits one immutable token-fenced apply request. A forward
timeout or transport-uncertain result is resolved through observation; it is
never replayed.

| State | Accepted progress | Deadline behavior |
| --- | --- | --- |
| `Discovering` | `initialize` or `recover` with a valid snapshot/journal | None |
| `Ready` | stage a valid non-no-op candidate; accept exact unchanged redelivery or strictly newer same-epoch observed/external/topology state | None |
| `Staged` | preview when safety is `Safe`; cancel without mutation; same-set external changes must use `externalIntentObserved`, because ordinary observations are rejected | None |
| `Applying` | exact-token completion enters observation or uncertainty resolution | Apply timeout enters `ResolvingUncertain` |
| `Observing` | target fingerprint enters confirmation; pre-image returns ready; other valid observations remain mismatches | Observation timeout begins rollback |
| `AwaitingConfirmation` | confirm clears journal; cancel, lock, suspend, topology or external intent resolve safely | Confirmation deadline begins rollback |
| `ResolvingUncertain` | observed pre-image clears; observed target rolls back; other ordinary observations remain mismatches; explicitly routed external intent aborts | Observation timeout begins rollback; no forward replay |
| `SettlingTopology` | repeated topology changes replace the pending snapshot; cancel/lock/suspend or external-abandon action is recorded; only explicit `topologySettled` acts | Timing/coalescing belongs to D2 adapter |
| `RevertingApply` | exact-token success enters revert observation; failure schedules retry | Apply timeout consumes the attempt and schedules retry |
| `RevertingObserve` | matching full pre-image or surviving properties clears journal | Observation timeout schedules retry |
| `RevertBackoff` | no command issues another write | Injected 250/500 ms defaults issue attempts two/three |
| `Stuck` | consume current snapshots/topology; explicit retry either clears a stale journal or restarts rollback against the current set | Journal remains active; no silent success |

Defaults are a 5-second apply acknowledgement, 2-second observation, 15-second
confirmation, and 250/500-millisecond rollback backoffs. D1 clamps injected
zero durations to one millisecond and uses saturating deadline arithmetic. The
resident service now schedules these model deadlines with one single-shot Qt
timer; the D1 machine still sees only `tick()` and the injected monotonic value.

Rollback makes exactly three apply attempts per rollback sequence in one
process before `Stuck`; an explicit topology settle or service restart begins
a new sequence. Repeated cancel, lock, and suspend inputs during a sequence do
not reset its counter. Initial journal storage is a hard gate. A failed
`confirm` clear is rejected while remaining in `AwaitingConfirmation`. If a
terminal clear fails after pre-image, external, or already-restored survivor
truth is known, `Stuck(JournalFailure)` is cleanup-only and retry never applies.
`Stuck(RevertFailed)` means convergence was not proven and exposes the consumed
attempt. Once a durable pre-image exists, other journal phase refreshes are
best effort so storage loss cannot suppress the safer rollback attempt.

`MachineView::reason` describes the active transaction. Completion moves it to
`lastTerminalReason`, which remains observable in `Ready` until the next
successful stage. `TransportUncertain` and `JournalFailure` are distinct from
`ApplyTimeout` and `RevertFailed`; an invalid transaction ID and a staged
suspend also have distinct command errors. If an uncertain completion arrives
after rollback was requested, the command reports apply uncertainty while the
view deliberately retains the pending rollback reason (cancel, lock, suspend,
or timeout).

### Side-effect port preconditions

- `storeJournal` and `clearJournal` return one typed journal-mutation outcome.
  `Unchanged` guarantees the prior durable value, `Durable` proves the requested
  value/absence crossed every supported barrier, and `DurabilityUncertain`
  records a pathname commit followed by a failed directory barrier. Only
  `Durable` authorizes a forward apply. Clear uncertainty remains conservative
  cleanup/recovery failure; it is never collapsed into a lying Boolean.
- `requestApply` copies the request before returning and does not synchronously
  call the machine. It may later produce zero or one completion for the exact
  token; late and duplicate/out-of-order callbacks are rejected.
- The owner redelivers the current live snapshot through `observedSnapshot`
  after every apply callback and apply deadline, even if revision and contents
  appear unchanged. A changed output set is routed through `topologyChanged`
  in every active state.
- While a candidate is `Staged`, a same-set change from another client is
  routed through `externalIntentObserved`; `observedSnapshot` is intentionally
  invalid there so the adapter cannot leave a stale candidate staged.
- While the machine is `SettlingTopology`, platform post-hotplug re-application
  is delivered only through `observedSnapshot`/`topologyChanged`, never
  `externalIntentObserved`. D2 must not misclassify KWin's own set restoration
  as a new client intent that abandons rollback.
- The port serializes platform-side apply ordering. A timeout never authorizes
  it to replay a forward request.
- Temporary transport loss is represented as `TransportUncertain` or absence
  of a callback. The borrowed port object itself is not replaced.
- D1 requests are values only. No Wayland, KWin, D-Bus, file, or QObject handle
  may enter the machine.

### Hotplug and external intent

On an output-set change, the machine waits through any repeated change inputs
until D2 supplies one deterministic settled snapshot. Cancel, lock, and suspend
are recorded but do not apply before settle; explicitly routed external intent
is recorded as abandon and clears only after settle. The adapter must route
that intent before entering the settle window; observations during the window
remain topology truth. If the original output set returns, rollback uses the
complete pre-image. For a genuinely changed set, the machine computes the
intersection with pre-image outputs that were enabled and requests only stable
ID, mode, scale, and transform. It never treats an empty disabled-output mode
as a wildcard and never replays enable, position, priority, primary, or
replication fields from the old set. If the current survivor properties
already match, the journal clears without a redundant apply. `Stuck` continues
to accept snapshot/topology updates so retry uses the current set. The future
adapter owns the accepted 500 ms quiet-window and long-churn reporting policy;
D1 intentionally models only the explicit settle event.

A valid newer external configuration aborts and clears the QindaQt transaction
without another apply. Recovery follows the same rule: pre-image live clears,
target live rolls back, a changed set waits for settle, and a same-set layout
matching neither endpoint is external truth and is not fought. Lock-authority
loss and suspend input revert. D2 must
consume the existing authenticated fail-closed session-lock client and a
logind delay inhibitor; D1 contains neither dependency. Crash recovery loads a
valid journal and enters this same rollback/settle path; a journaled external
abandon remains abandon across a crash and clears only after the recovered set
settles.

D1 assumes an apply callback is delivered before its post-apply device
observation and relies on the mandatory re-observation rule above. Whether the
pinned compositor preserves this ordering, and how cross-client external
intent orders against an already queued apply, are D2 nested-runtime evidence.
Fake-port tests do not claim either property or serialize that runtime window.

## Confirmation policy

Topology is class A and always requires confirmation. The public class-B enum
is closed and each current member maps explicitly to bypass; an unknown value
fails safe to confirmation. The values describe policy classification only in
D1. Brightness, color, DDC-CI, ICC, ambient, and custom-mode owners must prove
device error semantics before D7 makes any bypass operational. New members
require a protocol decision and tests; no generic `requiresConfirmation` flag
is accepted.

## Evidence boundary and next slices

D1 unit rows are **deterministic model evidence**. They exercise hostile
values, identities, geometry, fake clocks/ports, journal bytes, hotplug and
recovery transitions without opening a display. They do not prove that pinned
KWin accepted a real configuration, that a mirror appears in `wl_output`, or
that DRM/GPU/monitor/lid/suspend behavior works.

D2 also has two serial private-D-Bus rows. Each creates a disposable root,
removes host display/session addresses from the daemon environment, and uses
only explicitly named connections to its own bus. They prove exact-owner
asynchronous inventory reads, replacement and stale-reply fences, dirty-read
coalescing, stop suppression, resident name/object registration, typed
unavailable errors, `Changed`, deadline fire/re-arm, and teardown. They do not
open a compositor or display and are not KWin output-management evidence.

D0 owns the development-only compositor inventory/hotplug seam. The current D2
foundation owns resident read/service lineage and injected transaction
orchestration. D4 now owns the bounded direct public output-management port;
remaining display work owns journal persistence, lock/logind integration,
settled-hotplug policy, and nested hotplug/restart proof. D3 provides the typed asynchronous client
and server-projected reversible transaction coordinator; later Settings,
shell, Hybrid, application, policy, and hardware lanes consume those public
boundaries. The required release scenarios remain in the
[testing harness](../development/testing-harness.md).
