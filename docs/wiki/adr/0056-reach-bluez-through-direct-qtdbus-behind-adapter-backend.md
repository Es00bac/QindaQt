# ADR-0056: Reach BlueZ through direct QtDBus behind the AdapterBackend port

- **Status:** Accepted
- **Date:** 2026-09-02
- **Owners:** Bluetooth platform (QQ-005.05)
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0037](0037-keep-pairing-and-trust-authority-in-bluez.md) accepted KF6
BluezQt as the reuse library for the future runtime adapter and deferred the
transport choice to that lane. The B1 lane now implements the production
`AdapterBackend` over BlueZ. The authority decision — BlueZ owns pairing,
trust, keys, device records, profiles, and authorization, and Bluetooth1 never
calls `Pair`/`Trust` — is untouched by this choice.

Two facts forced an explicit decision rather than a default:

1. BluezQt is absent from the pinned dependency prefix and pkg-config of this
   repository's build environment; adopting it would add a KDE Frameworks
   dependency for a surface of exactly three D-Bus interfaces
   (`org.freedesktop.DBus.ObjectManager` at `/`, `org.bluez.Adapter1`,
   `org.bluez.Device1`) plus standard `Properties` access.
2. The port contract and the B0 model validation make the adapter an untrusted,
   generation- and owner-fenced boundary. The production transport must be
   exercisable against a fake `org.bluez` on a private `dbus-daemon` so no test
   ever contacts the host system bus, BlueZ, rfkill, or radios.

## Decision

The production `bluetooth_bluez_adapter` module implements the
`AdapterBackend` port with direct QtDBus over an injected `QDBusConnection`
(production: the system bus; tests: a private bus carrying a fake `org.bluez`),
not through BluezQt. The module:

- resolves the exact unique owner of `org.bluez` before any subscription or
  call, installs signal matches bound to that unique name, and re-resolves on
  every owner loss or replacement, retiring cached truth, leases, and
  outstanding operations as `Uncertain` and republishing an empty inventory
  (which the model projects as the truthful `Unavailable/no-adapter`);
- fences every asynchronous reply by run generation and owner token, so a late
  or replaced-owner reply can never mutate or complete against current state;
- bounds hostile property payloads before they cross the port (canonical
  address grammar, UTF-8-safe name truncation, control-character
  sanitization, RSSI accepted only in `[-128, 0]`, class decoding only for the
  24-bit CoD space, unknown interfaces ignored, inventory capped at the
  Bluetooth1 bounds) and drops unrepresentable entities instead of poisoning
  the whole snapshot;
- owns the caller-scoped, reference-counted discovery lease table, issuing one
  `StartDiscovery` per adapter when the local count rises from zero and one
  `StopDiscovery` when it falls to zero, and reconciles BlueZ-reported
  discovery that no QindaQt caller holds as one synthetic external-session
  lease row so the model's lease/discovering consistency check stays truthful
  under concurrent external BlueZ clients;
- performs no `Pair`, `Trust`, `Untrust`, `RemoveDevice`, `SetDiscoveryFilter`,
  or agent registration call, and never writes BlueZ records.

The composition root selects the adapter through the explicit
`QINDAQT_BLUETOOTH_BACKEND` environment mode (`production` default,
`deterministic` for the B0 empty backend), documented on the
[Bluetooth service](../architecture/bluetooth-service.md) page.

## Consequences

- No new mandatory dependency; the adapter depends only on the public
  Bluetooth1 model port and Qt Core/DBus.
- Exact-owner fencing, match-rule lifecycle, and reply fencing are ours to
  maintain; a BluezQt-based rewrite would delete that code but bring the KDE
  Frameworks dependency and its own event-loop integration.
- The synthetic external-session lease row is invisible on the wire (the
  public snapshot carries no lease list) and is bounded by the same lease caps.
- BlueZ facts that Bluetooth1 v1 cannot represent are suppressed or dropped,
  not approximated: connections on unpowered or unpaired devices publish as
  not connected, and device battery percentage (`org.bluez.Battery1`) and GAP
  role stay unreported until a schema revision carries them.
- Tests qualify the adapter only against a fake `org.bluez` on a private bus;
  real-adapter behavior, pairing UX (Agent1), and hardware gates remain
  outside this decision, as in ADR-0037.

## Revisit when

- A Bluetooth1 schema revision adds battery, role, or external-session
  visibility that the mapping currently omits.
- BluezQt enters the pinned dependency set for another accepted reason, or
  BlueZ ships a stabilized high-level API that would delete the exact-owner and
  fencing code wholesale.
- A future Agent1 outcome needs richer adapter capability observation than
  Adapter1 properties provide.
