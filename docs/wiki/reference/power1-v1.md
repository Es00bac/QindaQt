# Power1 version 1 values and pure protocol

This page fixes the PB-0 value, validation, canonical-codec, and QtDBus
boundary reserved for `org.qindaqt.Power1`. PB-0 does not own a bus name,
connect to an upstream daemon or compositor, read hardware, or expose a user
interface. Those are later slices in the
[Power and brightness architecture](../architecture/power-service.md).

The source currently forms a PB-0 candidate. Its protocol boundary has focused
executable evidence; deterministic aggregation remains candidate evidence until
an independent exact-commit review accepts it. Pure brightness composition is
a separate focused-build candidate awaiting independent exact-commit review.
None of these boundaries is executable `Power1` service evidence.

## Identity and lineage

| Property | Value |
| --- | --- |
| Reserved bus name and interface | `org.qindaqt.Power1` |
| Reserved object path | `/org/qindaqt/Power1` |
| Protocol version | `1` |
| Canonical codec version | `1` |

Every snapshot has a nonzero service epoch and monotonic positive revision.
Every handle is `(epoch, opaque-id)`, uses the snapshot epoch, and is unique
across supplies, profile holds, keyboard backlights, and internal backlights.
The opaque ID is derived by a future adapter; a raw UPower object path, sysfs
path, serial number, UID, or PID is never a public value.

An operation result contains its kind/status and initiating plus observed
epoch/revision. A success remains in the initiating epoch and cannot observe an
earlier revision. Once a later service dispatches an operation, timeout or
authority replacement is `Uncertain`; clients resnapshot and never replay it.

## Bounded snapshot

The snapshot contains fixed fields, never an `a{sv}` bag:

- availability, capability bits, bounded reason and diagnostic;
- AC/on-battery, lid, dock, and sleep-preparation truth;
- an optional aggregate battery and at most eight additional battery/UPS
  values;
- at most four supported profiles and eight epoch-scoped holds;
- at most eight sanitized inhibitors containing only `what`, `who`, `why`,
  and `mode`;
- at most eight keyboard-backlight and eight internal-backlight values; and
- the exact child Wayland socket, protocol version, and binding epoch when a
  provider is bound.

Internal-backlight status is `Ok=0`, `Degraded=1`, or `Unavailable=2`.
The closed reason vocabulary is `None=0`, `NoBacklight=1`,
`AmbiguousBacklight=2`, `NoInternalConnector=3`,
`AmbiguousInternalTopology=4`, `LogindError=5`, `DeviceDisappeared=6`,
`NonConverged=7`, and `WaylandUnavailable=8`. In particular, the four identity
reasons preserve the accepted fail-closed no-backlight/ambiguous-device/no-
connector/ambiguous-topology truth.

### Scalar values

Percentages are finite doubles from 0 through 100. Exact percentage truth uses
`BatteryLevel::None`; when exact truth is absent, the closed coarse-level
vocabulary is `Unknown`, `None`, `Low`, `Critical`, `Normal`, `High`, and
`Full`. Per-supply energy and rate values are finite and nonnegative; the
composite net rate is positive while charging and negative while discharging.
Time estimates are nonnegative upstream truth and are never recomputed by PB-0.
Unknown numeric values clear their corresponding `known` flag and carry
canonical zero.

Keyboard brightness exposes exact raw current/maximum integers plus a 0–10000
normalized integer. Internal brightness remains exact raw observed/maximum
integers. No wire percentage substitutes for these device values.

### Text and numeric limits

| Field | Version-1 limit |
| --- | ---: |
| Supplies / profiles / profile holds | 8 / 4 / 8 |
| Inhibitors / keyboard devices / internal devices | 8 / 8 / 8 |
| Canonical payload | 1,048,576 bytes |
| Opaque ID | 128 UTF-8 bytes |
| Name, vendor, model, application, profile label | 256 UTF-8 bytes |
| Profile ID / reason code | 64 UTF-8 bytes |
| Diagnostic or hold reason | 512 UTF-8 bytes |
| Inhibitor what / who / why / mode | 128 / 256 / 512 / 32 UTF-8 bytes |
| Wayland socket name | 108 UTF-8 bytes |
| Percentage | finite 0 through 100 |
| Energy / absolute rate | finite 0 through 1,000,000 Wh/W |
| Composite absolute net rate | finite 0 through 8,000,000 W |
| Time estimate | 0 through 315,360,000 seconds |
| Normalized / raw brightness | 10,000 / 1,000,000,000 |

Text is strict UTF-8, contains no NUL, control, or format characters, and is
checked by encoded byte count. Adapter-facing sanitization replaces forbidden
characters and truncates only at a valid UTF-8 boundary. Inhibitor privacy is
structural: the public value has exactly four strings and cannot represent UID
or PID.

## Canonical codec and total decoding

Snapshot magic is `QP1S`; operation-result magic is `QP1R`. Both use
big-endian Qt 6.0 `QDataStream` primitive encodings, an explicit codec version,
and length/count prefixes. Encoding the same accepted value produces identical
bytes. List order is retained.

Decoders reject oversize input before copying it, validate magic and version,
check every count/length before allocation, require strict UTF-8 and exact end
of buffer, then run semantic validation on a temporary. A failure returns a
typed `CodecError` and leaves the caller's prior destination unchanged. QtDBus
operators likewise expose only closed structures and bounded arrays; semantic
publication still requires `validateSnapshot` or `validateOperationResult`.

## Deterministic aggregate battery

`aggregatePowerSupplies` is a pure, reentrant operation over one complete
candidate list. It rejects more than eight supplies, invalid values, a zero or
mixed epoch, duplicate opaque handles, and nonfinite or out-of-range aggregate
arithmetic. Any failure returns a canonical empty value; a later publisher must
retain its prior accepted snapshot.

Valid absent supplies do not contribute. If no supply is present, the aggregate
is canonically absent. Otherwise `sourceCount` is the number of present
supplies. Percentage is energy-weighted when every present source has known
energy and positive full energy. The implementation divides summed energy by
summed full energy before scaling by 100 so an exactly-full multi-device
generation remains exactly 100%. Failing that, percentage is the arithmetic
mean only when every present source has exact percentage truth. A set lacking
complete exact truth retains the worst available coarse level in the fixed order
`Critical`, `Low`, `Normal`, `High`, `Full`, `None`, then `Unknown`.

Charging and pending-charge rates contribute positively; discharging and
pending-discharge rates contribute negatively; empty and fully charged sources
contribute zero. Unknown state or rate makes the aggregate net rate unknown.
The sign of a known nonzero sum fixes aggregate charging/discharging state;
zero or unknown rate uses the closed, enumeration-independent precedence
`Discharging`, `Charging`, `PendingDischarge`, `PendingCharge`, all-full,
all-empty, then `Unknown`.
Warning severity is `Action`, `Critical`, `Low`, `Discharging`, `None`, then
`Unknown`. Time-to-empty/full is published only for the matching aggregate
state and only when every present source supplies the same known upstream
estimate; PB-0 never derives one from energy or rate.

Opaque-handle uniqueness is checked by ID only after every supply has been
unified to one nonzero epoch. That ordering is part of the lineage contract:
same IDs from different owner generations are mixed-epoch input, not distinct
devices.

## Compatibility and scope

Unknown enum values, capability bits, nonfinite/out-of-range numbers, mixed or
duplicate lineage, unknown profile references, inconsistent known flags,
oversized collections, unsafe text, trailing bytes, and unknown versions fail
closed. Changing a field, bound, enum meaning, D-Bus structure, or canonical
byte layout requires a compatibility decision and normally a new version.

PB-0 intentionally contains no service, client, D-Bus connection, UPower,
power-profiles-daemon, logind, Wayland, sysfs, hardware, session, Settings,
QML, or UI behavior. Aggregate-battery policy is the pure `power_aggregation`
collaborator described above. Brightness composition is the separate pure
[`brightness_model`](../architecture/brightness-model.md) module; it consumes
these values without gaining transport or mutation authority.
