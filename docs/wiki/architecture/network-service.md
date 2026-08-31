# Network service architecture

QindaQt now has an executable **N1 resident Network1 boundary** for Platform
milestone QQ-005.04. N0 remains the platform-free protocol/model/client base;
N1 adds an exact-owner Qt D-Bus transport, a resident
`org.qindaqt.Network1` service, and a separately linked libnm adapter. The
contract is fixed by [Network1 version 1](../reference/network1-v1.md),
[ADR-0045](../adr/0045-fence-network1-pure-boundary.md), and
[ADR-0052](../adr/0052-confine-networkmanager-behind-network1.md).

N1 provides production observation and permitted scan, stored-known-network
connect, active disconnect, and Wi-Fi/WWAN radio dispatch. N1 itself owns no
Settings or shell UI; the [Network Settings route](../apps/network-settings.md)
is a separate public-client-only consumer. Neither layer claims connection-
profile editing, credential entry, an in-process secret agent, persistence, or
physical Wi-Fi/Ethernet/radio qualification.

## Authority map

| Concern | Truth authority | QindaQt responsibility |
| --- | --- | --- |
| Connectivity, radios, devices, access points | NetworkManager via libnm | Normalize, bound, validate, and publish observed truth |
| Scan, connect, disconnect, radio request | NetworkManager permissions and completion | Admit an N0 intent, dispatch once, then resnapshot |
| Stored connection profiles | NetworkManager | Reference only by derived known-network id; never edit settings |
| Credentials | External NetworkManager secret agent | Never request, receive, cache, log, or transport through Network1 |
| Public owner/epoch/revision | Resident Network1 process | Exact-owner service lineage and atomic revision publication |
| Scan result freshness | Snapshot-published lease | N0 model converts remaining duration to its local monotonic clock |

## Module composition

```text
consumer -> network_client -> NetworkTransport
                                |
                                v
                     network_qt_transport -> session D-Bus
                                                  |
                                                  v
network_protocol <- network_model <- network_service (resident owner)
                                             |
                                             v
                              network_manager_adapter -> libnm -> system D-Bus
```

| Module | Cohesive responsibility |
| --- | --- |
| `network_protocol` | Bounded values, identity, validation, redaction, canonical codecs |
| `network_model` | Lineage gate, scan-lease reconciliation, intent admission, atomic projection |
| `network_client` | Async client state, deadlines/retries, uncertain outcomes, injected transport seam |
| `network_qt_transport` | Activation, exact unique-owner binding, fixed Qt D-Bus mapping, bounded broker errors |
| `network_service` | Resident name/object, epoch/revision stamping, hostile-backend validation, serialized operations |
| `network_manager_adapter` | Private libnm observation and typed dispatch through an injectable platform port |

The N0 dependency direction remains protocol → model → client. Qt D-Bus does
not enter N0. The service depends on the public protocol/model boundary but not
the client or transport. Only the adapter links libnm; public adapter headers
contain no `NM*`, `GObject`, D-Bus object-path, or connection-setting handle.
All resident objects are owned by one composition root and confined to its Qt
thread.

The client publishes one read-only operation-admission predicate in addition
to mutation-in-flight state. It is false while an authoritative snapshot fetch
is scheduled or outstanding, including post-operation and invalidation
refreshes. A consumer that combines this predicate with the model's typed
intent verdict advertises exactly the operation the client can accept at that
moment; snapshot scheduling remains private client state.

## Resident lifecycle and restart fence

The installed D-Bus descriptor activates `qindaqt-network-service` for
`org.qindaqt.Network1`; the matching hardened user systemd unit declares the
same bus name, restarts failures, restricts address families to `AF_UNIX`, and
applies read-only system/home and kernel/device protections. The process owns
exactly `/org/qindaqt/Network1` on the constructing session bus. Partial start
rolls back object and name registration; stop is idempotent and resolves
pending work before releasing ownership.

Each process derives a nonzero epoch from the boot-monotonic clock and keeps a
local high-water. This is deliberately paired with its D-Bus unique owner. An
initially absent NetworkManager may later appear and become ready. Once a
nonempty NetworkManager unique owner has been observed, owner loss or
replacement makes dispatched work uncertain, publishes unavailable truth, and
retires the process with status 75. The libnm port binds
`notify::dbus-name-owner` and establishes this admission fence at the owner
event boundary; its one-second timer refreshes facts only. The watch carries a
per-start generation and exact `NMClient` identity, so a notification from a
stopped or replaced client cannot retire a later run. The user unit restarts it, or the Qt
transport asks D-Bus to activate it after public-owner loss. The next process
has a new Network1 unique owner and a strictly greater epoch; it never changes
epoch beneath one surviving public owner. Session-bus disconnect also exits so
lineage cannot migrate to a new broker.

## Observation and publication

The libnm port polls public NetworkManager facts on the constructing Qt thread;
authority loss/replacement is event-driven and is not delayed until a poll.
It publishes only value copies: normalized interface names, presentation-safe
SSIDs, normalized BSSIDs, derived known-network ids, radio/device/connectivity
state, permission-derived capabilities, and bounded scan-lease duration.
Malformed facts are skipped where referential integrity can be repaired and
cause a degraded reason; an unavailable or wholly invalid backend yields a
valid empty unavailable snapshot. The service validates a complete candidate
with N0 before atomically advancing the revision and emitting `Changed`.

The facts type cannot represent passwords, PSKs, certificates, private keys,
hardware addresses, drivers, UIDs/PIDs, or NetworkManager object paths. The
adapter never invokes NetworkManager secret getters. Connect activates only a
stored `NMRemoteConnection`; NetworkManager may consult a separately registered
secret agent, but neither that exchange nor credential-entry UI crosses
Network1.

## Operation lifecycle

The service accepts only the four fixed Network1 methods. Every request carries
the initiating epoch and revision, is converted to one typed N0 intent, and is
admitted against the current validated snapshot. Rejection is immediate and
does not call the backend. At most one admitted operation is in flight; a
second is returned as busy. The backend deadline is five seconds. Timeout,
authority replacement, or shutdown cancels the platform request and completes
the client-visible result exactly once as uncertain; late callbacks are
generation/operation-id fenced and discarded. No mutation is automatically
replayed. Accepted backend dispatch is queued for the next turn of the same Qt
thread. This lets the D-Bus object retain the original delayed call before even
a synchronous backend failure can complete, while stop, timeout, and authority
replacement fence a dispatch that has not started.

A scan deadline is provisional while libnm dispatch is pending. Successful
dispatch changes `Scanning` to `Leased`. A definite libnm failure clears both
the in-progress flag and deadline, publishes `Idle` before the failed reply,
and therefore permits an immediate retry. Cancellation is deliberately
conservative: NetworkManager may already have accepted the scan, so the
callback clears `Scanning` but retains the bounded lease until its deadline.

The production dispatch surface is intentionally narrow:

- request a Wi-Fi scan with a bounded lease deadline;
- activate an already stored known-network connection;
- deactivate the active connection on a named device interface; and
- request Wi-Fi or WWAN software-radio state and confirm observed state.

NetworkManager remains final policy authority. Permission absence removes the
corresponding capability, so the service does not advertise an operation it
cannot attempt.

## Failure and public error behavior

The Qt transport calls only the current exact unique owner, drops replies from
foreign/retired owners and stale tokens through N0, and translates D-Bus
failures to stable bounded reason codes. Raw broker or libnm error messages do
not become public diagnostics. Credential-shaped operation parameters are
rejected locally before a bus call. Accepted unavailable and degraded
snapshots retain their honest client state rather than being projected Ready.

## Qualification

The strict Debug and Release Network proof includes the thirteen N0 rows plus
N1 transport, coordinator, residency, libnm-mapping/dispatch, activation,
installed-package, boundary, and poison-policy rows. Private-bus tests cover
exact introspection, unique-owner replacement, malformed/foreign/stale replies,
delayed and synchronous operation completion, timeout/stop/late exactly-once
replies, unavailable backend, process activation, broker loss, and fresh higher
epoch after restart. A private-system-bus concrete-libnm probe proves owner
loss and A→B→A replacement retire below the one-second fact poll, including a
production activated-process retirement. Adapter lease-state proof distinguishes
definite scan failure (`Idle`, immediate retry) from conservative cancellation
(`Leased`). Tests never touch host networking.

The activation fixture gives the production binary a private session D-Bus and
a separate empty private system D-Bus. The installed fixture stages
`QindaQtNetworkN0` and `QindaQtNetworkN1`, builds an external CMake consumer,
checks the binary and three activation/interface descriptors, and repeats the
service lifecycle from the staged prefix. Source/package-policy poison cases
prove the checker rejects libnm in the resident service, a reversed transport dependency,
a NetworkManager secret getter, public NM handles, an `a{sv}` D-Bus wire, and
an incomplete N1 package registry.

This is deterministic process and software-boundary evidence only. The
[Network Settings route](../apps/network-settings.md) adds a secret-free UI
consumer without expanding N1 authority. Physical Wi-Fi, Ethernet, radios,
stored-profile compatibility, external secret agents, credential entry, and
host policy remain explicit later qualification.
