# Network1 version 1

This page fixes the value, canonical-codec, and resident D-Bus contract for
`org.qindaqt.Network1`. N0 owns the platform-free values, model, and injected
client transport seam. N1 implements the resident service, Qt D-Bus transport,
and NetworkManager adapter described in the
[Network service architecture](../architecture/network-service.md).

## Endpoint and fixed D-Bus wire

| Property | Value |
| --- | --- |
| Well-known name and interface | `org.qindaqt.Network1` |
| Object path | `/org/qindaqt/Network1` |
| Protocol version | `1` |
| Canonical codec version | `1` |
| Install components | `QindaQtNetworkN0`, then `QindaQtNetworkN1` |

The interface contains no property or `a{sv}` bag. Snapshot and operation
results are canonical N0 byte arrays (`ay`):

| Member | Signature | Result |
| --- | --- | --- |
| `GetSnapshot` | `() → (ay)` | canonical `Snapshot` |
| `RequestScan` | `(t epoch, t revision, x deadlineMs) → (ay)` | canonical `OperationResult` |
| `ConnectKnownNetwork` | `(t epoch, t revision, s knownNetworkId) → (ay)` | canonical `OperationResult` |
| `ConnectVisibleNetwork` | `(t epoch, t revision, s accessPointId) → (ay)` | canonical `OperationResult` |
| `DisconnectActive` | `(t epoch, t revision, s deviceInterface) → (ay)` | canonical `OperationResult` |
| `SetRadio` | `(t epoch, t revision, u radioKind, b enable) → (ay)` | canonical `OperationResult` |
| `Changed` | signal `(t epoch, t revision)` | tells clients to refetch |

Mutation methods may hold their D-Bus reply until backend completion or the
five-second service deadline. Immediate admission failures return a complete
operation result. An admitted platform dispatch begins on the next same-thread
event turn, after the resident object has retained the delayed D-Bus call;
synchronous backend completion therefore still returns exactly once. Stop,
timeout, or authority retirement before that turn fences the dispatch and
returns the applicable uncertain result. No method accepts credentials or arbitrary settings. D-Bus
activation and the user systemd unit both start `qindaqt-network-service`.

## Identity and lineage

Every snapshot carries a nonzero service epoch and positive monotonic revision.
Within one owner and epoch, revisions strictly increase. Any owner or epoch
change requires an epoch greater than every previously accepted epoch,
including after current state is cleared; a same-owner epoch change is
rejected. Owners are exact valid D-Bus unique-name strings bounded to 255 UTF-8
bytes. A decoded snapshot owner must equal both the request owner and current
transport owner. An operation result must repeat its initiating epoch,
revision, and kind.

The resident service binds one boot-monotonic epoch to one D-Bus unique owner.
After the first nonempty upstream owner is observed, NetworkManager
`dbus-name-owner` notification loss/replacement immediately retires that
process; the one-second observation poll is not an authority fence. The watch
is exact-client and per-start-generation fenced. Retirement makes pending work
uncertain, and relies on systemd restart or fresh D-Bus activation for a new
Network1 owner and greater epoch. Session-bus disconnect retires the process.
Clients drop late, duplicate, malformed, foreign-owner, stale-token, and
retired-lineage replies and never replay a mutation.

## Snapshot values

The snapshot contains fixed fields: protocol version, owner, epoch, revision,
availability (`Starting`, `Ready`, `Unavailable`, `Degraded`), capability bits,
connectivity (`Unknown`, `Offline`, `Portal`, `Limited`, `Full`), bounded
reason/diagnostic, radios, devices, access points, known networks, active
connections, and scan phase/lease.

Unavailable and degraded snapshots require a reason. Capability bits are
exactly `Connectivity`, `Scan`, `KnownNetworkControl`, `RadioControl`,
`ActiveConnectionControl`, and `VisibleNetworkControl`; unknown bits are
rejected. Referential integrity is
structural: an access point sits on a Wi-Fi device; an active connection
references an existing device and known network; radio kinds, device
interfaces, per-device BSSIDs, network ids, and active device references are
unique.

## Identity normalization

- SSIDs are at most 32 raw octets. Presentation-safe UTF-8 is published as
  text; anything else, including empty, becomes a hidden network whose octets
  never leave the adapter. Controls, line/paragraph separators, format
  controls, surrogates, private-use, and unassigned scalars are unsafe.
- BSSIDs are exactly seventeen lowercase `xx:xx:xx:xx:xx:xx` hex characters.
- Interface names are 1–15 octets of `[A-Za-z0-9._-]`, starting alphanumeric.
- A known-network id is the 64-character lowercase SHA-256 hex digest over the
  raw SSID octets and security suite. It is a correlation pseudonym, not
  confidentiality for a guessable SSID.
- A visible-access-point id is the 64-character lowercase SHA-256 hex digest
  over the normalized device interface, a zero separator, and normalized
  BSSID. It selects only an access point already present in the initiating
  snapshot and reveals neither its SSID nor any credential on the method wire.
- Security suites are `Open`, `Wep`, `Wpa2Personal`, `Wpa2Enterprise`,
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
| Client request timeout / retry schedule | 100–60,000 ms / at most 8 entries, each ≤60,000 ms |
| Resident backend-operation timeout | 5,000 ms |
| Intent wire traversal | depth 8, 16 entries per container, 64 visited nodes |

Text is strict UTF-8 without unsafe presentation scalars and is bounded by
encoded byte count. Nonzero frequency uses a broad observed 2.4/5/6 GHz range,
not a regulatory-domain allowlist; wired devices leave it zero.

## Scan lease truth

An idle snapshot carries no lease. A non-idle lease has a bounded nonempty id,
the snapshot epoch, a granting revision no greater than the snapshot revision,
and a bounded remaining duration. Only after snapshot admission does the
consumer convert the duration to its injected local monotonic deadline.
Invalid clocks and overflow fail atomically. A live lease makes a second scan
intent busy, and a foreign epoch can never revive retired scan truth.

The adapter treats the requested deadline as provisional during scan dispatch.
A definite libnm failure clears it and publishes `Idle` before returning
`Failed`, so an immediate retry is admissible. Cancellation is not proof that
NetworkManager rejected the scan: cancellation publishes `Leased` and retains
the bounded deadline conservatively, while never replaying the request.

## Canonical codec and total decoding

Snapshot magic is `QN1S`; operation-result magic is `QN1R`. Both use big-endian
Qt 6.0 `QDataStream` primitives, an explicit codec version, and length/count
prefixes. Encoding an accepted value is deterministic and retains list order.

Decoders reject oversize input before copying, validate magic/version, check
every count and length before allocation, require strict UTF-8 and exact end of
buffer, then semantically validate a temporary. Failure returns a typed
`CodecError` without changing the caller's prior destination. Boolean bytes
are only 0 or 1 and decoded `wireValid` must be true.

## Intents and operation results

Inputs are `RequestScanIntent`, `ConnectIntent` for a known-network id,
`ConnectVisibleIntent` for an opaque visible-access-point id,
`DisconnectIntent` for a device interface, and `SetRadioIntent` for a radio kind
and boolean state. No input can carry a credential. The redactor recognizes
secret-shaped keys and bounded nested maps; both client transport and resident
service reject credential-shaped or over-budget values before dispatch. Public
diagnostics pass through the same fail-closed redactor.

Admission refuses absent/not-ready snapshots, unsupported capabilities,
invalid scan deadlines, busy/live scans, unknown networks or access points,
already-active connections, unknown or idle devices, absent/hardware-disabled
radios, and redundant radio state. First-use connection additionally refuses
hidden, WEP, enterprise, and already-known networks. Those network-type
refusals are typed `Unsupported`; rejection changes no state.

Operation status is `Succeeded`, `Rejected`, `Unsupported`, `Failed`,
`Uncertain`, or `Busy`; non-success requires a reason. At most one service
operation is dispatched. Timeout, shutdown, or authority replacement cancels
it and reports `Uncertain` exactly once. A late callback cannot create a second
reply. Backend dispatch is queued only to close delayed-reply registration; it
remains confined to the coordinator's Qt thread and is still covered by the
five-second deadline. A success means the request dispatch
completed, not that Network1 manufactured state; clients refetch authoritative
observation.

## Credential and error boundary

Network1 never requests `GetSecrets`, receives a password/PSK/certificate or
private key, or exposes NetworkManager setting maps. `ConnectKnownNetwork`
activates an existing stored connection. `ConnectVisibleNetwork` may submit
one bounded partial 802.11 wireless profile to NetworkManager and activate it
against the selected observed access point. Open profiles have no wireless
security setting. WPA2 Personal uses `key-mgmt=wpa-psk`; WPA3 Personal uses
`key-mgmt=sae`. Both secured forms omit the PSK property and set its secret
flags to `AGENT_OWNED`, so NetworkManager must request the value separately
from a registered secret agent.

The partial profile and its `AddAndActivateConnection` call remain private to
the libnm adapter. If NetworkManager requires credentials, it talks to the
separately deployed first-party registered secret agent outside this interface
and process. Raw D-Bus/libnm error text is not public; callers see stable
bounded reason codes. Network1 remains unqualified for credential payloads by
design; the separate [Network secret
agent](../architecture/network-secret-agent.md) owns that interoperability
claim.
