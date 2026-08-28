# Bluetooth service

Bluetooth1 is QindaQt's bounded, deterministic control and observation boundary for
Bluetooth adapters and devices. The D-Bus-activated `qindaqt-bluetooth-service` owns
`org.qindaqt.Bluetooth1`; it does not open sockets, connect to BlueZ, or access the
host Bluetooth hardware or D-Bus.

The exact wire contract is in the [Bluetooth1 reference](../reference/bluetooth1-v1.md).

## Module shape

| Module | Responsibility | Boundary |
| --- | --- | --- |
| `bluetooth_protocol` | Typed values, fixed D-Bus marshalling, limits, and fail-closed validation | Qt Core/DBus only; no transport or platform handles |
| `bluetooth_model` | Deterministic state machine, adapter/device inventory projection, operation coordination | Pure model; no platform access; no D-Bus |
| `bluetooth_service` | Backend abstraction, resident object/name ownership, process entry point, D-Bus activation | Qt main thread publishes D-Bus; private fake adapter owns state transitions |

## Design principles

- **Bounded determinism**: The model is a pure state machine without external dependencies. All state transitions are deterministic and testable.
- **Exact-owner semantics**: Service rejects stale handles across epoch boundaries. Clients bind to unique owner and refetch on authority loss.
- **Fake-only implementation**: No real BlueZ connection, no system bus access, no hardware contact. B0 is a pure service foundation.
- **Fail-closed validation**: All inbound D-Bus values are validated before use. Malformed payloads result in clear rejection.
- **Typed operations**: Pair/Connect/Disconnect/Trust operations return typed results with status, lineage, and reason codes.

## Authority and handle lineage

Every public handle is `(epoch, object.serial)`. The service starts with epoch 1 and
strictly increases it when service ownership changes. All handles from an earlier
epoch are stale.

Snapshot revisions are monotonic within an epoch. A `Changed(epoch, revision)` signal
carries no data and only prompts a fetch. The client subscribes to the current unique
owner rather than the well-known name.

## Availability and operations

Bluetooth1 keeps domain truth locally. A client discards its snapshot when the service
owner changes and must refetch from the new exact owner.

`Pair`, `Connect`, `Disconnect`, `Trust`, and `Untrust` return a typed result.
The service rejects unavailable state, stale/missing handles, incompatible operations,
and unsuitable device states. A service-owner replacement makes a dispatched operation
uncertain because completion cannot be proven. Callers must refetch and show that
uncertainty; they must not retry automatically.

## Consumer boundary

This slice exports typed C++ protocol and model layers only. A later shell applet or
settings route receives only a narrow adapter-status facade plus `openBluetoothSettings()`
and `pairNewDevice()`; it receives no raw snapshot, handle, model, D-Bus object,
service client, or provider authority.

## Activation and hardening

The build installs the executable, D-Bus activation descriptor, introspection XML,
and a systemd user unit. The user unit is D-Bus named, limits tasks and address
families, uses a private temporary directory, and enables available filesystem,
kernel, namespace, personality, privilege, and syscall hardening. It does not
contact BlueZ, rfkill, or any host Bluetooth service.

Diagnostics are short, control-character-sanitized, and contain no raw properties,
paths, or secrets. Stable reason codes are the programmatic error surface.

## Qualification boundary

Focused protocol tests cover exact signatures, round trips, aggregate counts, and
limits. Fake model tests cover state machine transitions, operation validation,
stale handles, exact owner/revision handling, and lineage tracking. Integration
tests verify service activation, D-Bus publication, operation atomicity, and
exact-owner replacement.

This evidence does not qualify live Bluetooth adapters, real devices, pairing dialogs,
the shell UI, hardware hotplug, USB Bluetooth dongles, or any physical device operations.
Those remain integration and UI gates.
