# Network1 version 1 values and pure protocol

This page fixes the N0 value, identity, validation, and canonical-codec
boundary reserved for `org.qindaqt.Network1`. N0 does not own a bus name,
contact NetworkManager or any daemon, read a radio, store a secret, or expose a
user interface; those are later slices in the
[Network service architecture](../architecture/network-service.md).

## Identity and lineage

| Property | Value |
| --- | --- |
| Reserved bus name and interface | `org.qindaqt.Network1` |
| Reserved object path | `/org/qindaqt/Network1` |
| Protocol version | `1` |
| Canonical codec version | `1` |

Every snapshot carries a nonzero service epoch and positive monotonic revision.
Within one owner and epoch, revisions must strictly increase. Any owner or
epoch change requires an epoch strictly greater than every previously observed
accepted epoch, including after current state is cleared; a same-owner epoch
change is rejected. Owners are exact valid D-Bus unique-name strings bounded to
255 UTF-8 bytes. A decoded snapshot owner must equal both the initiating
request owner and the current transport owner. An operation result carries its
initiating epoch/revision and kind; a reply that does not match the request's
lineage is not evidence about that operation.

## Snapshot values

The snapshot contains fixed fields, never an `a{sv}` bag: protocol version,
owner, epoch, revision, availability (`Starting`, `Ready`, `Unavailable`,
`Degraded`), capability bits, connectivity (`Unknown`, `Offline`, `Portal`,
`Limited`, `Full`), bounded reason/diagnostic, radios, devices, access points,
known networks, active connections, and the scan phase plus lease.

Unavailable and degraded snapshots must carry a reason code. Capability bits
are exactly `Connectivity`, `Scan`, `KnownNetworkControl`, `RadioControl`, and
`ActiveConnectionControl`; unknown bits are rejected.

Referential integrity is structural: an access point must sit on a Wi-Fi
device; an active connection must reference an existing device and known
network; radio kinds, device interfaces, per-device BSSIDs, network ids, and
connection devices are unique.

## Identity normalization

- SSIDs are at most 32 raw octets. Valid presentation-safe UTF-8 is published
  as text; anything else (including empty) is a hidden network whose octets
  never leave the adapter. Unsafe categories are controls, line/paragraph
  separators, format controls, surrogates, private-use, and unassigned
  scalars. Safe supplementary Unicode remains representable.
- BSSIDs are exactly seventeen lowercase `xx:xx:xx:xx:xx:xx` hex characters.
- Interface names are 1–15 octets of `[A-Za-z0-9._-]` starting alphanumeric,
  matching Linux IFNAMSIZ.
- A known-network id is the 64-character lowercase hex SHA-256 over the raw
  SSID octets and the security suite; validation accepts only digests of this
  shape. It is a correlation pseudonym, not confidentiality for a guessable
  SSID. Security suites are `Open`, `Wep`, `Wpa2Personal`, `Wpa2Enterprise`,
  `Wpa3Personal`, and `Wpa3Enterprise`.

## Text and numeric limits

| Field | Version-1 limit |
| --- | ---: |
| Radios / devices / active connections | 4 / 8 / 8 |
| Access points / known networks | 64 / 128 |
| Canonical payload | 1,048,576 bytes |
| Raw SSID octets | 32 (longer rejected, never truncated) |
| BSSID / interface name | 17 / 15 UTF-8 bytes |
| Network id / lease id / reason code | 64 / 64 / 64 UTF-8 bytes |
| Diagnostic / owner | 512 / 255 UTF-8 bytes |
| Signal strength | 0 through 100 |
| Frequency | 0 (unknown) or 2,412 through 7,125 MHz |
| Scan-lease remaining duration | 1,000 through 120,000 ms |
| Request timeout / retry schedule | 100–60,000 ms / at most 8 entries, each ≤ 60,000 ms |
| Intent wire traversal | depth 8, 16 entries per container, 64 visited nodes |

Text is strict UTF-8 without unsafe presentation scalars and is bounded by
encoded byte count. Nonzero frequency uses a broad observed 2.4/5/6 GHz range,
not a regulatory-domain channel allowlist; wired devices leave it at zero.

## Scan lease truth

An idle snapshot must carry no lease. A non-idle snapshot's lease has a
nonempty bounded id, the granting epoch equal to the snapshot epoch, a granting
revision no greater than the snapshot revision, and a bounded remaining
duration. The model adopts a lease only from the snapshot's own epoch and
converts its duration to a local deadline only after admission; invalid clocks
and integer overflow fail atomically. Expiry is evaluated against the injected
monotonic clock, and a live lease makes a second scan intent busy.

## Canonical codec and total decoding

Snapshot magic is `QN1S`; operation-result magic is `QN1R`. Both use big-endian
Qt 6.0 `QDataStream` primitive encodings, an explicit codec version, and
length/count prefixes. Encoding the same accepted value produces identical
bytes; list order is retained.

Decoders reject oversize input before copying it, validate magic and version,
check every count and length against its cap before allocation, require strict
UTF-8 and the exact end of the buffer, then run semantic validation on a
temporary. A failure returns a typed `CodecError` and leaves the caller's prior
destination unchanged; a decoder never publishes a prefix. Boolean fields use
only canonical bytes 0 and 1, and the decoded `wireValid` field must be true.

## Intents and operation results

User intents are the only mutation inputs: `RequestScanIntent` (bounded
deadline), `ConnectIntent` (known-network id), `DisconnectIntent` (device
interface), and `SetRadioIntent` (radio kind plus enable). No intent can carry
a credential; the shared redaction helper recognizes secret-shaped key names
and bounded nested wire maps, and the client refuses credential-shaped or
over-budget parameter maps before transport. Public diagnostics pass through
the same canonical redactor; quoted, unquoted, suffix-shaped, and malformed
secret fragments cannot enter public state, errors, or signal strings.

Intent admission refuses absent/not-ready snapshots, unsupported capabilities,
out-of-bounds scan deadlines, busy scans, live leases, unknown networks,
already-active connections, unknown or idle devices, absent or
hardware-disabled radios, and redundant radio state, each with a stable reason
code. A rejected intent changes no state.

Operation status is `Succeeded`, `Rejected`, `Unsupported`, `Failed`,
`Uncertain`, or `Busy`. Any status other than `Succeeded` requires a reason.
Once dispatched, timeout or authority loss is `Uncertain`; clients resnapshot
and never replay the operation.
