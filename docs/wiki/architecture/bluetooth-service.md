# Bluetooth service

Bluetooth1 is QindaQt's typed, restart-aware control and observation boundary
for Bluetooth. The D-Bus-activated `qindaqt-bluetooth-service` owns
`org.qindaqt.Bluetooth1`; BlueZ remains the owner of pairing, trust, keys,
device records, profiles, and authorization. Bluetooth1 does not pair, does
not store trust, does not duplicate BlueZ records, does not touch rfkill, and
does not own Bluetooth audio nodes (PipeWire does).

The exact wire contract is in the [Bluetooth1 reference](../reference/bluetooth1-v1.md).
The authority split and Agent1 pairing deferral are recorded in
[ADR-0037](../adr/0037-keep-pairing-and-trust-authority-in-bluez.md).
The production transport boundary is recorded in
[ADR-0057](../adr/0057-reach-bluez-through-direct-qtdbus-behind-adapter-backend.md).

## Module shape

| Module | Responsibility | Boundary |
| --- | --- | --- |
| `bluetooth_protocol` | Typed values, fixed D-Bus marshalling, limits, and fail-closed validation | Qt Core/DBus only; no transport or platform handles |
| `bluetooth_model` | Backend port, authoritative lineage/lease coordination, operation validation, and the deterministic platform adapter | Public Bluetooth protocol plus Qt Core/DBus; no QML, no shell, no D-Bus service registration |
| `bluetooth_bluez_adapter` | Exact-owner BlueZ ObjectManager transport, bounded property mapping, discovery leases, and paired-device operations | Public `AdapterBackend` plus Qt Core/DBus; no service residency, QML, pairing/trust mutation, rfkill, or platform handles in public headers |
| `bluetooth_client` | Exact-owner discovery, snapshot fetching, invalidation coalescing, timeout recovery, and serialized public operations | Depends only on the protocol and Qt Core/DBus |
| `bluetooth_service` | Resident D-Bus object/name ownership, caller-scoped lease watching, process entry point, backend selection, and activation artifacts | Qt main thread publishes D-Bus; composes either production BlueZ or the deterministic backend through the same port |

## Authority and handle lineage

Every public handle is `(epoch, serial)`. Serials derive stably from the
canonical adapter/device address within an epoch; they are not list positions
or transient platform object IDs. The service generates a restart-unique
nonzero epoch at process start, and a reused model strictly advances its epoch
before a new backend run can publish, so a restarted service can never issue a
handle an earlier incarnation already issued. All handles from an earlier
epoch are stale.

Snapshot revisions are monotonic within an epoch. `Changed(epoch, revision)`
carries no inventory data and only prompts a fetch. The client subscribes to
the current unique owner rather than the well-known name, rejects late
replies, coalesces invalidations while a fetch is active with bounded backoff
between retries, and rejects older epochs, regressing revisions,
equal-revision content contradictions, and malformed snapshots before
publication. A failed or timed-out fetch revokes mutation authority: the
retained snapshot is dropped and any dispatched operation completes as
`Uncertain`, so nothing mutates through state that can no longer be proven
current. Public mutation results are always queued until after the request ID
returns; stop cancels undelivered results except for one queued
`client-stopped` uncertainty for a still-dispatched mutation, and object
destruction safely drops that queued delivery. An accepted new epoch
immediately makes any dispatched mutation uncertain; a delayed old-epoch
result cannot restore success and is never replayed. Facing an initially
absent service, the client attempts exactly one explicit
`StartServiceByName` activation and then waits for its owner watcher.

## Production BlueZ adapter

`bluetooth_model` defines the `AdapterBackend` port: an untrusted,
Qt-main-thread platform boundary with run generations, immutable inventory
values, typed operation outcomes, and caller-keyed lease release. `start()`
returns its generation before that run may publish, so the initial
publication of every run is queued and generation-fenced. The model
validates every backend value fail-closed — including lease-table
consistency with each adapter's discovering flag, adapter existence for
every lease, duplicate caller/adapter entries, and both lease bounds,
projected to include dispatched-but-uncompleted lease operations — and
replaces malformed outcomes with the protocol-valid
`Failed/backend-malformed` classification; raw adapter text never reaches
D-Bus.

The production `bluetooth_bluez_adapter` implements that port with direct
QtDBus over an injected `QDBusConnection`. The packaged service injects the
system bus; tests inject a private bus carrying a fake `org.bluez`, so no test
contacts the host BlueZ, rfkill, or radios. The transport resolves the exact
unique owner before subscribing or calling, addresses methods to that owner,
and advances an owner token across every loss or replacement. Owner changes
retire the object store, discovery leases, and pending operations before a new
ObjectManager snapshot may publish; late enumeration and mutation replies are
dropped by the owner token and backend run generation.

The adapter consumes `org.freedesktop.DBus.ObjectManager`, Adapter1,
Device1, and standard `PropertiesChanged`. It observes Address, Alias/Name,
Powered, Discovering, Adapter, Class, Icon, RSSI, Paired, and Connected, but publishes only fields representable in Bluetooth1 v1. It calls
only Properties.Set(Powered), StartDiscovery, StopDiscovery, Connect, and
Disconnect. It never calls Pair, changes Trusted, removes a device, registers
an agent, or duplicates a BlueZ record. Names are bounded without splitting
UTF-8, malformed addresses and parent references drop the affected record,
class and RSSI values map fail-closed, unknown interfaces are ignored, and
duplicate adapter or device addresses deterministically retain the
lexicographically first BlueZ object path.

BlueZ discovery is sender-scoped while Bluetooth1 leases are caller-scoped.
The adapter therefore shares one BlueZ StartDiscovery call for concurrent
local leases, reference-counts those leases by caller and adapter, and stops
its session only when the final local reference disappears. A Discovering
session reported by BlueZ without a local lease is represented internally by
one bounded synthetic lease so the B0 model invariant remains truthful; the
synthetic row never crosses the Bluetooth1 wire.

The composition root reads `QINDAQT_BLUETOOTH_BACKEND`. Only the exact value
`deterministic` selects B0's empty in-memory backend; unset, `production`, and
unknown values select the production adapter. This makes test composition
explicit while ensuring the installed service defaults to BlueZ. Direct
QtDBus supersedes only ADR-0037's earlier BluezQt transport-library choice;
the accepted BlueZ authority split is unchanged (ADR-0057).

## Operations and discovery leases

`SetPowered`, `AcquireDiscovery`, `ReleaseDiscovery`, `Connect`, and
`Disconnect` return a typed result carrying the initiating epoch/revision. The
service rejects unavailable state, stale handles, malformed callers, unknown
kinds, out-of-bound lease counts, discovery or connect on an unpowered
adapter, connect of an unpaired or already-connected device, and disconnect
of an unconnected one. A timeout, owner replacement, backend replacement,
model stop, or a failed refetch makes a dispatched operation `Uncertain`;
callers resnapshot and must not retry automatically.

Discovery leases are caller-scoped (unique bus name), reference-counted per
adapter, bounded per adapter and in total, and held by the backend because the
backend owns the discovery session. Powering an adapter off releases that
adapter's leases, and backend stop() clears all lease state, so no discovery
session survives its authority. The resident service subscribes once to
`NameOwnerChanged` on its constructing bus with the `sss` match signature and
releases every lease of a unique-name caller that vanishes, so bounded
discovery cannot leak after client death; relinquishing a well-known alias
without replacement is not caller loss. Powering an adapter off terminates
its discovery sessions and connections, matching BlueZ truth.

## Activation and hardening

The build installs the executable, configured D-Bus activation descriptor,
canonical introspection XML, and a systemd user unit. The user unit is D-Bus
named, restricts address families to `AF_UNIX`, bounds tasks, drops
capabilities, and enables the available filesystem, kernel, namespace,
personality, privilege, and syscall hardening. It carries no ordering
dependency on BlueZ: a user-manager unit cannot order against the
system-manager BlueZ unit, so the service instead tolerates BlueZ absence by
design (a truthful `Unavailable/no-adapter` snapshot) and never starts,
reconfigures, or supervises BlueZ. The executable binds
`org.freedesktop.DBus.Local.Disconnected` on the exact constructing session
connection to process exit; a replacement bus must activate a fresh process
and epoch. A staged-install test gate verifies the deployed payload and a
linked installed consumer of the public protocol headers.

Diagnostics are short, control-character-sanitized, and contain no raw
properties, paths, process environments, Bluetooth keys, or secrets. Stable
reason codes are the programmatic error surface.

## Consumer boundary

This slice exports typed C++ protocol, model, client, and service libraries
plus the activation artifacts. The production
[Bluetooth applet](../shell/bluetooth-applet.md) receives only a narrow
shell-private facade over the public client: it projects bounded inventory and
exposes adapter power, one caller-scoped discovery lease, and paired-device
connect/disconnect under exact lineage and manifest grants. It receives no
address, pairing, trust, key, Agent1, BlueZ, or service-implementation surface.
The [Bluetooth Settings route](../apps/bluetooth-settings.md) owns the stable
route ID `bluetooth`, exact-lineage action projection, and its own bounded
discovery lease. Consumers link only public boundaries and never see backend
objects or the service implementation.

## Qualification boundary

Focused protocol tests cover exact registered signatures, real-writer
signature emission, meta-type round trips, ordering, lease bounds, and the
hostile malformed matrix (addresses, RSSI, battery, role, capability bits,
state contradictions, unstructured reason codes, oversized arrays). Model
tests cover publication, lineage preservation, policy rejections including
already-connected devices, lease bounds with dispatched-lease projection,
owner-vanish release, stop/restart epoch invalidation including reuse before
any publication, and fail-closed backend handling. Client tests cover
exact-owner binding, refetch coalescing, stale/malformed reply rejection,
timeout uncertainty, mutation-authority revocation after fetch failure,
queued exactly-once completion, and stop semantics. Private-bus tests cover
successive owners, full-fidelity snapshot round trips, a hostile oversized
wire payload rejected by the bounded decode, paired-device connect round
trips, discovery leases, release of a vanished lease-holding caller's exact
bus connection, private-bus service composition, client-driven executable
activation with fresh epochs across independent buses, and a staged-install
gate with a linked installed consumer.

The B1 private-bus rows additionally cover production-mode selection, initial
BlueZ absence, ObjectManager inventory, property and interface churn, power,
shared/refcounted discovery, paired-device connect/disconnect success and
failure, hostile values, deterministic duplicate suppression, owner loss and
return, deferred-reply fencing, an exact staged B1 component surface, and
source-boundary poison controls.

That evidence qualifies the production adapter only against the injected fake.
It does not qualify physical radios, a host BlueZ build, pairing UX (Agent1),
Bluetooth audio correlation, suspend/resume, hardware hotplug, memory/CPU
budgets, or the future UI. Those remain hardware and integrated-session gates.

The applet's separate focused offscreen/package evidence does not change those
platform nonclaims; hardware and integrated-session gates remain.
