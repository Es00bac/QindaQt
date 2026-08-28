# Bluetooth1 protocol 1

This page is the canonical wire and value contract for `org.qindaqt.Bluetooth1`
version 1. The implementation is normative together with this page: the
introspection XML shipped in `src/services/bluetooth_service/data/`, the
adaptor's `Q_CLASSINFO` introspection, the codecs in
`src/services/bluetooth_protocol/src/bluetooth_dbus.cpp`, and the signature
test in `tests/services/bluetooth_protocol/` must all agree byte-for-byte.

The authority boundary — BlueZ owns pairing, trust, keys, device records,
profiles, and authorization — is recorded in
[ADR-0037](../adr/0037-keep-pairing-and-trust-authority-in-bluez.md).

## Service identity

- Bus name: `org.qindaqt.Bluetooth1` (session bus)
- Object path: `/org/qindaqt/Bluetooth1`
- Executable: `qindaqt-bluetooth-service` (D-Bus activated, systemd user unit)
- Schema version: `1`

## Fixed structures

All structs are fixed typed structures; there is no property bag and no
`a{sv}`. Arrays are bounded and bounded decoding marks a value malformed
rather than truncating silently.

| Value | D-Bus signature | Fields |
| --- | --- | --- |
| `Handle` | `(tt)` | `epoch`, `serial` |
| `Adapter` | `((tt)ssbb)` | `handle`, `address`, `name`, `powered`, `discovering` |
| `Device` | `((tt)(tt)ssuubbbnby)` | `handle`, `adapter` handle, `address`, `name`, `deviceClass` (`u`), `role` (`u`), `paired`, `connected`, `rssiKnown`, `rssi` (`n`), `batteryKnown`, `batteryPercent` (`y`) |
| `Snapshot` | `(uttuussa((tt)ssbb)a((tt)(tt)ssuubbbnby))` | `schemaVersion`, `epoch`, `revision`, `availability`, `capabilities`, `reasonCode`, `diagnostic`, `adapters`, `devices` |
| `OperationResult` | `(uuttttss)` | `kind`, `status`, `initiatingEpoch`, `initiatingRevision`, `observedEpoch`, `observedRevision`, `reasonCode`, `diagnostic` |

### Enumerations

| Enum | Values |
| --- | --- |
| `Availability` (`u`) | `Starting=0`, `Ready=1`, `Unavailable=2`, `Degraded=3` |
| `Capability` (`u`, flags) | `SetAdapterPower=1<<0`, `DiscoveryLease=1<<1`, `ConnectPaired=1<<2`, `DisconnectPaired=1<<3` |
| `DeviceClass` (`u`) | `Unknown=0`, `Computer=1`, `Phone=2`, `AudioVideo=3`, `Headset=4`, `Headphones=5`, `Keyboard=6`, `Mouse=7`, `Tablet=8`, `Printer=9`, `GameInput=10`, `Wearable=11`, `Tag=12` |
| `DeviceRole` (`u`) | `Unknown=0`, `Central=1`, `Peripheral=2`, `CentralPeripheral=3` |
| `OperationKind` (`u`) | `SetAdapterPower=0`, `AcquireDiscovery=1`, `ReleaseDiscovery=2`, `Connect=3`, `Disconnect=4` |
| `OperationStatus` (`u`) | `Succeeded=0`, `Rejected=1`, `Unsupported=2`, `Failed=3`, `Uncertain=4`, `Busy=5` |

### Limits

| Bound | Value |
| --- | --- |
| Adapters per snapshot | 8 |
| Devices per snapshot | 256 |
| Adapter/device name | 256 UTF-8 bytes |
| Reason code | 64 UTF-8 bytes |
| Diagnostic | 512 UTF-8 bytes |
| In-flight operations | 64 |
| Discovery leases per adapter | 16 |
| Total discovery leases | 64 |
| Caller identity | 64 UTF-8 bytes |

## Methods and signal

```
GetSnapshot() -> snapshot
SetPowered(adapter (tt), powered b) -> result
AcquireDiscovery(adapter (tt)) -> result
ReleaseDiscovery(adapter (tt)) -> result
Connect(device (tt)) -> result
Disconnect(device (tt)) -> result
Changed(epoch t, revision t)          (signal)
```

Mutations are asynchronous: the service replies with `result` when the
operation completes. `AcquireDiscovery`/`ReleaseDiscovery` are attributed to
the caller's unique bus name; leases are reference-counted per caller and per
adapter, and every lease of a caller that vanishes from the bus is released.
Powering an adapter off releases that adapter's leases and terminates its
connections and discovery sessions; a later power-on never resurrects
discovery.

## Validation (fail-closed)

A snapshot or operation result is **invalid**, and never published or
accepted, when any of the following holds:

- the schema version is not 1, an enum is outside its known values, or a
  capability flag outside the four known bits is set;
- `epoch` or `revision` is zero, a handle's epoch differs from the snapshot
  epoch, or any serial is zero, repeated anywhere in the snapshot, or not
  strictly ascending within the adapter and device arrays;
- an address is not a canonical Bluetooth address
  (`AA:BB:CC:DD:EE:FF`, uppercase hex pairs);
- `rssiKnown == false` while `rssi != 0`, or `rssiKnown == true` with RSSI
  outside `[-128, 0]` dBm;
- `batteryKnown == false` while `batteryPercent != 0`, or `batteryKnown ==
  true` with a percentage above 100; `role` outside its known values
  (`Unknown` means the platform did not report one);
- a device is `connected` while `paired == false` or while its adapter has
  `powered == false`, or references a missing adapter; an adapter is
  `discovering` while `powered == false`;
- text exceeds its bound, contains an embedded null, or a diagnostic contains
  control characters other than newline and tab;
- a reason code is not structured (`[a-z0-9-]`, nonempty);
- a decoding exceeded an array bound (`oversized-payload`);
- capabilities or inventory are present while `availability != Ready`;
- a succeeded operation result does not carry the initiating epoch and a
  revision at or after its initiating revision.

Serials are derived stably from the canonical address within the current
epoch; they are not list positions. A service restart produces a new,
restart-unique epoch, so every handle from an earlier epoch is stale.

## Uncertainty and replacement

Timeout, service-owner replacement, backend authority replacement, model
stop, and a failed or timed-out snapshot refetch make an in-flight operation
`Uncertain`. A failed refetch also drops the client's retained snapshot, so
no mutation can dispatch against state that can no longer be proven current.
Callers must resnapshot and show uncertainty; neither the service nor the
client replays an uncertain mutation. The client binds to the exact unique
owner, coalesces `Changed` invalidations while a fetch is active with bounded
backoff between retries, rejects late or malformed replies, clears its
snapshot when the owner is replaced, and never replays a timed-out
operation. A client facing `ServiceUnknown` attempts exactly one explicit
`StartServiceByName` activation and then waits for the owner watcher; it
never dispatches methods to a well-known name it has not resolved to a
unique owner.

## Reason codes

Stable reason tokens include `starting`, `ready`, `no-adapter`,
`backend-restarting`, `backend-malformed`, `unavailable`, `stale-handle`,
`adapter-off`, `not-paired`, `not-connected`, `already-connected`,
`no-lease`, `too-many-leases`,
`too-many-operations`, `malformed-request`, `malformed-caller`,
`unsupported`, `adapter-power-set`, `lease-acquired`, `lease-released`,
`connected`, `disconnected`, `authority-replaced`, `model-stopped`,
`snapshot-unavailable`, `snapshot-timeout`, and the client-side
`owner-replaced`, `operation-timeout`, `client-stopped`,
`malformed-result`, `malformed-snapshot`, and `transport-*` family.
