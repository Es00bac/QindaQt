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

## Module shape

| Module | Responsibility | Boundary |
| --- | --- | --- |
| `bluetooth_protocol` | Typed values, fixed D-Bus marshalling, limits, and fail-closed validation | Qt Core/DBus only; no transport or platform handles |
| `bluetooth_model` | Backend port, authoritative lineage/lease coordination, operation validation, and the deterministic platform adapter | Public Bluetooth protocol plus Qt Core/DBus; no QML, no shell, no D-Bus service registration |
| `bluetooth_client` | Exact-owner discovery, snapshot fetching, invalidation coalescing, timeout recovery, and serialized public operations | Depends only on the protocol and Qt Core/DBus |
| `bluetooth_service` | Resident D-Bus object/name ownership, caller-scoped lease watching, process entry point, and activation artifacts | Qt main thread publishes D-Bus; composes the model through the backend port |

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

## Backend port and the B0 deterministic adapter

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

Until the serialized BluezQt runtime lane opens, the production composition
root constructs the deterministic in-memory adapter. It reports an empty
inventory, so an activated B0 process truthfully publishes
`Unavailable/no-adapter` instead of fabricated devices; qualification
populates it through its private header. Device values carry the optional
battery percentage and GAP role only where the platform reports them
(`batteryKnown == false` otherwise); the deterministic adapter reports
neither, and fail-closed validation rejects any fabricated value. The future
BluezQt adapter replaces it behind the same port with no consumer change
(ADR-0037). No BlueZ, rfkill, or host Bluetooth contact exists in this slice.

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
plus the activation artifacts. A later Settings view model owns the stable
route ID `bluetooth` and a shell applet receives only a narrow facade; neither
UI is implemented or qualified here. Consumers link only the public client and
model libraries and never see QDBus types, backend objects, or the service
implementation.

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

That evidence does not qualify real-adapter behavior, pairing UX (Agent1),
Bluetooth audio correlation, suspend/resume, hotplug churn, memory/CPU
budgets, or the future UI. Those remain hardware and integrated-session gates.
