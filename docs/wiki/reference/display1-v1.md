# Display1 version 1 service and values

This page specifies the version-1 Display values shared by the pure D1 modules
and the bounded D2 resident-service surface. D1 still registers no bus name,
exports no object, connects no client, opens no Wayland object, and applies no
configuration. `display_service` now owns the activated
`org.qindaqt.Display1` process at `/org/qindaqt/Display1`, reads the integrated
D0 inventory through an exact-owner adapter, and composes D1 through injected
ports. Its packaged mutation port remains unavailable and fail-closed.

The authority and dependency rules are in
[Display service](../architecture/display-service.md),
[ADR-0016](../adr/0016-display1-transaction-authority.md), and
[ADR-0017](../adr/0017-persistent-output-identity.md).

## Version and lineage

`protocolVersion` is exactly `1`. Every snapshot carries a non-empty
`serviceEpoch`, a positive monotonic `revision` within that epoch, and a
32-byte `liveFingerprint`. Every candidate carries the epoch/revision it was
built from. A transaction stage is stale unless both equal the current
snapshot. An owner/service restart changes epoch; revisions from the prior
epoch are never ordered against the new one.

While the machine is `Ready`, it accepts the current epoch/revision again only
when the complete snapshot is exactly equal. Any changed contents or
fingerprint at that same revision are invalid; changed truth in the same epoch
must use a strictly greater revision. This permits required unchanged
redelivery without allowing a pre-change candidate to reuse an obsolete fence.

Version-1 readers reject unknown protocol, canonical-codec, registry, or
journal versions. They do not guess field defaults for a different version.
Changing a field's meaning, enum mapping, limit, canonical projection, or byte
layout requires an explicit compatibility decision and usually a new version.

## Hostile-input limits

All text limits are UTF-8 byte counts. Embedded NUL, Unicode control, and
Unicode format characters are rejected. Counts and lengths are checked before
D1 allocates variable-size decode destinations.

### Protocol values

| Value | Version-1 limit |
| --- | --- |
| Outputs per snapshot/candidate | 32; at least 1 |
| Modes per output | 128 |
| Active transaction summaries | 1 |
| Canonical candidate/snapshot payload | 1,048,576 bytes |
| Stable ID, connector, runtime UUID, mode ID, service epoch, transaction ID | 128 UTF-8 bytes each |
| Label | 256 UTF-8 bytes |
| Manufacturer | 128 UTF-8 bytes |
| Model | 256 UTF-8 bytes |
| Operation diagnostic | 512 UTF-8 bytes |
| Pixel width/height | 1 through 16,384 |
| Physical width/height | 0 through 10,000 mm |
| Input X/Y coordinate | -1,000,000 through +1,000,000 |
| Normalized rectangle right/bottom | at most +1,000,000 |
| Scale | finite 1.0 through 3.0 |
| Refresh | 1 through 1,000,000 millihertz |
| Fingerprint | exactly 32 bytes |
| Rollback attempts | exactly 3 per rollback sequence before `Stuck` |
| Journal payload | 1,048,576 bytes |

The coordinate bound comes from the pinned KWin 6.6.5 output-configuration
validation investigated for the accepted Display plan. D1 pins it as a model
contract; D2 must still prove the public protocol against nested KWin. It is
not physical-output evidence.

### Identity and registry values

| Value | Version-1 limit |
| --- | --- |
| Connected outputs presented to resolver | 32 |
| EDID identifier material | 1,024 bytes |
| Raw EDID | 4,096 bytes |
| MST path | 256 UTF-8 bytes |
| Runtime compositor UUID | 128 UTF-8 bytes; input validation only |
| Stable ID | 128 UTF-8 bytes |
| Digest exposed in stable ID | 16 bytes / 32 lowercase hex characters |
| Registry entries | 64 |
| Non-empty aliases | 32 |
| Alias | 64 UTF-8 bytes and `[A-Za-z0-9][A-Za-z0-9_-]{0,63}` |
| Registry JSON document | 262,144 compact-JSON bytes |

## Fixed values

`Transform` maps `Normal`, `Rotate90`, `Rotate180`, `Rotate270`, `FlipX`,
`FlipX90`, `FlipX180`, and `FlipX270` to unsigned values 0 through 7. Unknown
values fail validation.

Snapshots contain bounded descriptive output metadata, current configuration,
mode inventory, and at most one transaction summary. A disabled snapshot
output is canonical: `primary=false`, `priority=0`, `position=(0,0)`, and an
empty replication source. Its retained mode, scale, and transform remain
per-output state. At least one output is enabled and exactly one enabled output
is primary.

Candidates contain only requested configuration and base lineage. Validation
and normalization return a new value; the input is never mutated. Operation
results carry operation kind, status, typed error, initiating lineage, observed
revision, optional transaction ID, and a bounded diagnostic. A success cannot
carry an error or regress below the initiating revision; rejected/uncertain
results require a non-`None` error.

## Canonical byte codec

The hostile-byte candidate and snapshot codecs use big-endian Qt 6.0
`QDataStream` primitive encodings with explicit length prefixes. Candidate
magic is `QD1C`, snapshot magic is `QD1S`, and the canonical codec version is
`1`. Each payload contains its protocol version. List order is retained as part
of the value; encoding the same accepted value produces identical bytes.

Readers first reject payloads above 1 MiB without copying them. They then check
magic, codec version, every length/count, strict UTF-8, semantic values, and
exact end-of-buffer. Old/new/truncated/trailing/invalid payloads return a typed
`CodecError` and leave the caller's destination byte-for-byte unchanged.

The transaction journal uses the same candidate envelopes inside `QDJ1`,
codec/schema version 1. It contains transaction ID, phase, reason, attempt,
complete pre-image, and target. Decode is total and replaces a journal only
after both candidate envelopes and cross-candidate lineage/output-set
invariants pass.

## QtDBus serialization boundary

D1 provides value metatypes and raw QtDBus operators only. Future adapters use
the safe decode wrappers, which require the exact static v1 signature, decode
into a temporary, apply semantic bounds, and replace a destination only on
success.

| Value | D-Bus signature |
| --- | --- |
| Mode | `(siiub)` |
| Candidate output | `(sbbsiiduus)` |
| Candidate | `(usta(sbbsiiduus))` |
| Transaction summary | `(suustttu)` |
| Operation result | `(uuusttss)` |
| Output | `(ssssssiibbbbbsiiiiduusa(siiub))` |
| Snapshot | `(ustaya(ssssssiibbbbbsiiiiduusa(siiub))a(suustttu))` |

QtDBus/libdbus owns whole-message demarshalling before D1 sees an argument.
Exact signature checking prevents a type-divergent tail from becoming
plausible defaults, and D1 retains at most its declared list caps, but an
oversized correctly typed array is still traversed to leave the Qt argument in
a valid state. D2 must keep the session-bus whole-message limit and adapter CPU
budget explicit; for untrusted opaque storage, use the bounded canonical byte
envelope. Raw `operator>>` is metatype machinery, not an adapter boundary.

No D1 code registers a service, claims a name, exports XML, or sends a call.
The focused D1 row proves the registered static signatures and fail-closed,
non-mutating rejection of write-only or type-divergent arguments. A positive
read-only `QDBusArgument` can only come from QtDBus whole-message
demarshalling, so that path is D2 private-bus integration evidence rather than
a fabricated unit argument.

## Resident D2 wire surface

| Property | Value |
| --- | --- |
| Bus name / interface | `org.qindaqt.Display1` |
| Object path | `/org/qindaqt/Display1` |
| Activation | D-Bus activation delegating to `qindaqt-display-service.service` |
| Process lifetime | Exact constructing session bus; disconnect terminates |

The installed XML exports `GetSnapshot`, `Stage(s, Candidate)`, `Preview(s)`,
`Confirm(s)`, and `Cancel(s)`. Mutators return `OperationResult`. `Changed(s,
t, b)` carries epoch, revision, and availability as a complete-read
invalidation hint; clients must call `GetSnapshot` and never reconstruct output
state from signal order. When no accepted inventory exists, `GetSnapshot` and
mutators fail with `org.qindaqt.Display1.Error.Unavailable`; they never return a
plausible revision-zero snapshot or invalid result value.

The current inventory adapter calls `org.qindaqt.Compositor1.Outputs` on its
resolved unique D-Bus owner and binds one public Display1 epoch to that owner.
The positive D0 `outputGeneration` is the Display1 revision. Exact typed
redelivery is accepted at equal generation; changed content at equal
generation, revision regression, and a newer generation with unchanged content
all reject atomically. Owner replacement or transport loss discards the public
snapshot and active machine. A later accepted frame starts a fresh epoch, so
revisions are never compared across source owners.

Projection is intentionally narrower than full output management. It publishes
only D0 enabled outputs and one synthesized current mode, requires integral
geometry and Display1's 32-output/scale-3 limits, uses connector-fallback stable
identity because D0 supplies no EDID/MST material, and retains runtime UUID only
as non-persistent metadata. The first output in D0 semantic order is primary;
priorities are canonical contiguous order. Replication and disabled-output mode
inventory are not invented.

The resident owns actual single-shot scheduling for D1 deadlines and routes
typed inventory changes into the D1 machine. The packaged process has safety
`Unknown`, so `Stage` may validate but `Preview` rejects `Locked`. If a test or
later authenticated composition supplies `Safe`, the packaged transaction
port still cannot persist the hard-gate journal and rejects `JournalFailure`
without a compositor request. Consequently these methods establish
service/transaction ownership without claiming a production writer. There is
no KWin private ABI, Wayland output-management object, journal file, Settings,
QML, lock client, or logind adapter in this slice.

## Persistent identity

Resolution evaluates one connected batch in input/output order:

| Precedence | Usable when | Published form |
| --- | --- | --- |
| EDID identifier | Valid, non-empty, unique in batch | `edid:` + 16-byte digest hex |
| Raw EDID | Valid, complete, unique in batch | `edidraw:` + digest hex |
| MST composite | Non-empty path and unique composite | `mst:` + digest hex |
| Safe connector | Earlier materials unavailable/duplicate | `conn:` + connector |
| Hashed connector | Connector cannot safely appear directly | `connhash:` + digest hex |

The MST digest domain-separates a v1 marker, accepted EDID identifier (or
SHA-256 of raw EDID), a NUL separator, and the MST path. Malformed EDID bytes
are ignored as identity material. The runtime compositor UUID is never hashed
or copied into a result. Published values include only stable ID, connector,
identity source, ambiguity, manufacturer, model, `hasSerial`, and `internal`.

Any duplicate stronger material marks the affected result ambiguous even when
a later discriminator separates it. A residual stable-ID collision receives
`#1`, `#2`, and so on in connected-output order; every colliding result is
ambiguous. Digest injection exists only as a pure test seam for collision
proof.

## Registry schema 2

The pure registry document has exactly two root keys:

```json
{
  "schemaVersion": 2,
  "outputs": []
}
```

Each output has exactly `stableId`, `alias`, `label`, `lastConnector`,
`manufacturer`, `model`, `internal`, `ambiguous`, and `seenSequence`.
`seenSequence` is a positive canonical decimal string so its full 64-bit value
round-trips through JSON. Entries are encoded in stable-ID order. Unknown or
missing keys, duplicates, invalid aliases, alias-on-ambiguity, out-of-bounds
documents, and zero/exhausted sequences return typed `RegistryError` values.
Encoding preserves the caller's destination object on failure.

Schema 1 is accepted only with its exact older entry shape, which lacks
`alias` and `ambiguous`; decode supplies empty/false and reports
`migrated=true`. Writers always emit schema 2. Unknown schemas fail closed.

Reconciliation takes accepted resolved outputs and one caller-supplied seen
sequence. It refreshes connector/metadata/ambiguity by stable ID, clears an
alias that becomes ambiguous, retains disconnected entries, and evicts the
oldest disconnected sequence first with stable-ID tie breaking. It cannot
evict a connected entry to force capacity. Settings persistence belongs to a
later Settings-owned migration.

## Topology normalization and fingerprint

Validation requires exact output-set and epoch/revision equality. Enabled
priorities are unique and contiguous `1..N`, a QindaQt canonical policy
stronger than mere uniqueness. Unknown modes, non-finite/out-of-range scales,
invalid transforms, all-disabled, non-single-primary, mirror unknown/self/cycle,
positive-area overlaps, and coordinate overflow fail with `TopologyError` and
no partial result.

The minimum enabled non-replica X/Y is translated to `(0,0)`. Disabled
candidate fields are canonicalized like disabled snapshots. Replicas use the
ultimate source's projected position/scale and geometry; target mode and
transform remain per-output fields. Gap and non-integral-extent conditions are
sorted warnings.

Logical dimensions use nearest rounding after transform transposition:

| Pixels | Scale | Logical size | Integral exact extent |
| --- | --- | --- | --- |
| 1920×1080 | 1.25 | 1536×864 | Yes |
| 1920×1080 | 1.50 | 1280×720 | Yes |
| 1920×1200 | 1.25 | 1536×960 | Yes |
| 2560×1440 | 1.25 | 2048×1152 | Yes |
| 2560×1440 | 1.50 | 1707×960 | No; rounded width warning |

This table pins D1 parity with the accepted KWin 6.6.5/Qt nearest-rounding
design input. The tests are deterministic math evidence only; D2/M0 must
measure the compositor, Qt and public protocol paths at 1.25 and 1.5 before
claiming nested parity.

The canonical fingerprint is SHA-256 over `QDTF`, fingerprint format 1, and
stable-ID-sorted normalized outputs. Per output it includes stable ID, enabled,
primary, mode ID, normalized position, scale, transform, priority, and
replication source. It excludes service epoch/revision and descriptive
metadata; those are independent lineage/metadata fields. Replica
position/scale and disabled noncanonical fields are erased by the projection.

`Snapshot::liveFingerprint` must equal this fingerprint over
`candidateFromSnapshot(snapshot)`. Live snapshots may have a translated origin,
overlap, non-contiguous priority, or another compositor-authored form that is
not legal as a new candidate; snapshot acceptance validates the bounded value
and canonical projection fingerprint, while strict topology policy belongs to
`stage`. This is the adapter-to-model `AGENT-CONTRACT`: an otherwise valid
snapshot with a different projection fingerprint is rejected.

Diff output is stable-ID sorted and names exact changed fields. Replica
position/scale are derived and excluded; target mode/transform are included.
`noOp=true` exactly when the diff is empty.

## Transaction state and ports

The full state progression and ownership rules are documented in
[Display service](../architecture/display-service.md#transaction-model).
Default timeouts are:

| Window | Default |
| --- | --- |
| Forward or rollback apply acknowledgement | 5 seconds |
| Forward or rollback observation | 2 seconds |
| User confirmation | 15 seconds |
| Rollback backoff before attempts 2 and 3 | 250 ms, then 500 ms |

One machine owns one transaction. Stage is side-effect free and rejects stale,
invalid, active, and no-op candidates with typed `CommandError`. Preview is
denied unless safety is exactly `Safe` and durable journal storage succeeds.
Only an exact active token can complete an apply. Rejected, transport-uncertain,
and timed-out forward requests enter observation/rollback and are never
reissued.

Transaction reasons have fixed values: `None=0`, `Cancelled=1`,
`ConfirmationDeadline=2`, `Locked=3`, `Suspend=4`, `TopologyChanged=5`,
`ExternalChange=6`, `Recovery=7`, `ApplyRejected=8`, `ApplyTimeout=9`,
`ObservationMismatch=10`, `ObservationTimeout=11`, `RevertFailed=12`,
`JournalFailure=13`, and `TransportUncertain=14`. Unknown values fail closed.
`MachineView::lastTerminalReason` retains the completing reason in `Ready`
until the next successful stage. Invalid transaction IDs and staged suspend
use distinct `InvalidTransactionId` and `Suspend` command errors.

Confirmation clears the journal and retains the observed target; a clear
failure rejects and remains awaiting confirmation. Cancel,
deadline, lock, suspend, observation timeout, or observed uncertain target
begins rollback. Three failed/silent/mismatched rollback attempts enter
`Stuck`; repeated safety inputs cannot reset that sequence. Cleanup-only
`Stuck(JournalFailure)` retries only journal clearing, while
`Stuck(RevertFailed)` may explicitly restart the recovery path. An output-set
change waits for `topologySettled`: restoration of the original set uses the
full pre-image, while a changed set emits only `(stableId, modeId, scale,
transform)` for outputs enabled in the pre-image. Already matching survivors
do not cause an apply. External intent clears without an apply, including
recovery with a same-set snapshot matching neither journal endpoint. A
journaled external-abandon reason survives restart and waits behind the same
explicit settle barrier.

The injected clock is nondecreasing and wall-clock independent. The borrowed
side-effect port remains available for the machine lifetime, does not reenter,
atomically stores/clears the journal, copies request values, and reports zero
or one callback with the exact token. The owner must redeliver the current live
snapshot after every callback and apply deadline, even when unchanged, and
must route every active-state output-set change through `topologyChanged`.
Callback-before-device-observation and cross-client in-flight ordering remain
D2 runtime obligations. D1 has no mechanism to swap a disconnected port or
replay an uncertain forward mutation.

The resident D2 composition adds a process-local machine lineage around the D1
token. It advances that lineage before every replacement machine, requires the
transaction port to tag each completion with it, and accepts a completion only
when both values match. A late callback from a lost owner therefore cannot be
mistaken for a numerically reused token in a recovered machine. The lineage is
copied with the apply request and is not recomputed from the port's current
lineage when the callback arrives.

## Confirmation classification

`Topology` is class A and maps to `Required`. The closed class-B values are
brightness, dimming, SDR brightness, ICC profile, VRR policy, RGB range,
overscan, DDC-CI permission, maximum bits per color, extended dynamic range,
sharpness, auto-rotate policy, and custom-mode definition. Each maps through an
explicit switch to `BypassedForClosedPolicy`; any unknown value maps to
`Required`.

D1 only classifies values. Class-B transport/ownership is provisional until
the corresponding platform lanes prove error semantics; no immediate
production mutation is implemented here.

## Deterministic acceptance matrix

Every row below is **deterministic model evidence (`Q-det`)**, not proof that
KWin accepted a real configuration and not DRM/GPU/monitor/lid/suspend
qualification.

| Contract | Focused rows |
| --- | --- |
| Protocol bounds and total decode | `qindaqt.display-protocol-values`, `qindaqt.display-protocol-codec`: all collection/text/numeric bounds; enums; NaN/infinity; old/new/truncated/trailing/oversized bytes; stable round-trip; exact D-Bus signatures; no partial destination |
| Identity/privacy | `qindaqt.display-identity-resolver`: unique/duplicate identifier and raw EDID, MST, malformed/empty EDID, connector/UUID changes, precedence, collision suffixes, privacy, hostile inputs |
| Registry | `qindaqt.display-identity-registry`: v1 migration, v2 canonical round-trip, exact schema, ambiguity/alias errors, rename/hotplug reconciliation, duplicate connected IDs, LRU capacity |
| Topology | `qindaqt.display-topology-geometry`, `qindaqt.display-topology-candidate`: rounding table, 90/270 transposition, normalization/bounds, overlap/gap, all-disabled/primary/priority/mode, mirror unknown/self/cycle/projection, diff/no-op, integral warning |
| Transaction states | `qindaqt.display-transaction-state`: stale/no-op, safety/journal gates, apply/observe/confirm, reject/timeout without forward replay, mismatch/timeout, callback ordering |
| Rollback/hotplug/recovery | `qindaqt.display-transaction-recovery`: cancel/deadline/lock/suspend, callback and silent three-attempt paths, `Stuck` retry, latest-settle churn, surviving-only rollback, external intent, crash recovery |
| Adversarial transaction ordering | `qindaqt.display-transaction-adversarial`: exact rejected-state preservation, journal gates, total retry bound, settle barrier, set flap/full pre-image, cleanup-only `Stuck`, observation routing, live projection, same-set recovery no-fight, current-topology retry, disabled pre-image survivors, terminal reason and `stateChanged` truth |
| Invalid transition preservation | `qindaqt.display-transaction-invalid-ordering`: wrong transaction, recovery, callback, observation, confirmation, and settle inputs across all twelve states preserve view, snapshot, active journal, and port effects exactly |
| Journal bytes | `qindaqt.display-transaction-journal`: invariants, canonical round-trip, versions, torn/trailing/oversized bytes, no partial destination |
| Resident inventory adapter | `qindaqt.display-service-inventory`: exact owner/schema/generation JSON, bounds, privacy-preserving connector projection, current-mode geometry, transform/fractional scale, fingerprint |
| Resident lineage and transaction composition | `qindaqt.display-service-model`: add/remove/change, exact equal-generation fence, regression/owner/loss reset, fresh epochs, outer-lineage plus token callback fence, stale candidate rejection, preview/confirm/revert port ownership |
| Deployment surface | `qindaqt.display-service-deployment`: fail-closed invalid connection plus activation/systemd/XML names, methods, signals, and hardening metadata |

The D2 service-focused rows add deterministic decoder/projection, owner and
generation collision, loss/reset, add/remove/change, transaction-port, timer,
descriptor, staged-install, and public-header-consumer evidence. They still do
not exercise a display. Nested KWin output-management, restart recovery,
mirror visibility, and physical output rows remain later D2/D8 work in the
[testing harness](../development/testing-harness.md).
