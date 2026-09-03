# ADR-0057: Reach BlueZ through direct QtDBus behind the AdapterBackend port

- **Status:** Accepted
- **Date:** 2026-09-02
- **Owners:** Bluetooth platform (QQ-005.05)
- **Supersedes:** ADR-0037's BluezQt transport-library choice only; its BlueZ
  authority and Agent1 decisions remain accepted
- **Superseded by:** None

## Context

[ADR-0037](0037-keep-pairing-and-trust-authority-in-bluez.md) named KF6
BluezQt as the reuse library for the future runtime adapter. The B1 lane now
supersedes that library choice and implements the production
`AdapterBackend` over BlueZ. The authority decision — BlueZ owns pairing,
trust, keys, device records, profiles, and authorization — is untouched by
this choice. The additive Bluetooth2 boundary now forwards those operations to
BlueZ as described by ADR-0037's 2026-09-03 amendment; it still owns none of
their state, and the frozen Bluetooth1 boundary remains available.

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
  shared Bluetooth protocol bounds) and drops unrepresentable entities instead of poisoning
  the whole snapshot;
- owns the caller-scoped, reference-counted discovery lease table, issuing one
  `StartDiscovery` per adapter when the local count rises from zero and one
  `StopDiscovery` when it falls to zero, and reconciles BlueZ-reported
  discovery that no QindaQt caller holds as one synthetic external-session
  lease row so the model's lease/discovering consistency check stays truthful
  under concurrent external BlueZ clients;
- performs `Pair`, `CancelPairing`, `RemoveDevice`, and `Properties.Set(Trusted)`
  only against the exact BlueZ owner, registers one bounded `KeyboardDisplay`
  Agent1 through AgentManager1 only while an adapter exists, unregisters it on
  shutdown or loss of the final adapter, and never duplicates or persists BlueZ
  records, prompt input, link keys, or authorization decisions.

The composition root selects the adapter through the explicit
`QINDAQT_BLUETOOTH_BACKEND` environment mode (`production` default,
`deterministic` for the B0 empty backend), documented on the
[Bluetooth service](../architecture/bluetooth-service.md) page.

## Consequences

- No new mandatory dependency; the adapter depends only on the public
  Bluetooth model port and Qt Core/DBus.
- Exact-owner fencing, match-rule lifecycle, and reply fencing are ours to
  maintain; a BluezQt-based rewrite would delete that code but bring the KDE
  Frameworks dependency and its own event-loop integration.
- The synthetic external-session lease row is invisible on the wire (the
  public snapshot carries no lease list) and is bounded by the same lease caps.
- BlueZ facts that neither frozen Bluetooth1 nor current Bluetooth2 can
  represent are suppressed or dropped, not approximated. Connections on
  unpowered or unpaired devices publish as not connected.
- Tests qualify the adapter and Agent1 only against a fake `org.bluez` on a
  private bus; real-adapter interoperability and hardware gates remain outside
  this decision, as in ADR-0037.

## Revisit when

- A future protocol revision adds an upstream fact or external-session
  visibility that the current mapping omits.
- BluezQt enters the pinned dependency set for another accepted reason, or
  BlueZ ships a stabilized high-level API that would delete the exact-owner and
  fencing code wholesale.
- A future Agent1 extension needs richer capability or policy observation than
  the bounded `KeyboardDisplay` prompt contract provides.
