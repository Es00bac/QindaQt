# ADR-0037: Keep Bluetooth pairing and trust authority in BlueZ

- Status: Accepted
- Date: 2026-08-28
- Supersedes: none
- Number note: authored during the B0 repair as "ADR-0026" before the
  manager's parallel ADR allocation reserved 0026/0027 for public main; renumbered
  verbatim to the reserved Bluetooth number in the follow-up descendant.

## Context

Bluetooth1 is QindaQt's typed control and observation boundary for Bluetooth,
following the accepted platform-services plan
(`platform-services/1787853847-samira-cole-plan-handoff.md`). The first B0
implementation attempt (`f94353d6`) exposed `Pair`, `Trust`, and `Untrust`
as local operations that mutated duplicated `paired`/`trusted` flags inside
the QindaQt model. That duplication would have created a second Bluetooth
authority competing with BlueZ for device records, pairing state, trust, and
keys, and its review
(`platform-bluetooth/1787923186-anika-rao-bluetooth-b0-exact-review-fail.md`)
rejected it as a boundary reversal in addition to its build, wire, lineage,
and evidence defects.

BlueZ already owns adapters, device records, pairing, link keys, trust,
profiles, and authorization. KF6 BluezQt is the accepted reuse library for
reaching it. A QindaQt-local pairing/trust store cannot be authoritative,
cannot be reconciled safely across restarts, and would leak secrets or
desynchronize trust decisions.

## Decision

1. Bluetooth1 v1 exposes only what composes upstream truth: inventory
   snapshots, adapter power, a bounded caller-scoped reference-counted
   discovery lease, and connect/disconnect of already-paired devices.
2. Pairing, trust, and agent prompting are **not** Bluetooth1 operations. They
   belong to a separately reviewed `bluetooth_agent` outcome implementing
   BlueZ Agent1 with explicit request objects, prompts, and deadlines; that
   outcome must never fold into the inventory controller.
3. QindaQt never duplicates `paired`, `trusted`, keys, or device records. The
   service's `AdapterBackend` port receives those values from the platform and
   publishes them read-only; Bluetooth1 contains no mutator for them.
4. Discovery leases are identified by the caller's unique D-Bus name, are
   bounded per adapter and in total, stop when the last reference drops, and
   are released when the owning caller disappears.
5. Until the serialized BluezQt runtime lane opens, the B0 process composes a
   deterministic in-memory backend that reports an **empty** inventory and
   publishes `Unavailable/no-adapter` rather than fabricated devices. The
   BluezQt adapter replaces it behind the same `AdapterBackend` port; no
   consumer boundary changes with that replacement.

## Consequences

- The Bluetooth1 wire ABI has no Pair/Trust methods, and its device struct
  carries no `trusted` flag. Adding either requires a schema revision.
- `connect` fails closed with `not-paired` for unpaired devices; the user
  experience for initial pairing arrives only with the Agent1 outcome.
- Tests can populate the deterministic backend to qualify the model, wire,
  client, lease, and lifecycle machinery without host Bluetooth contact.
- The later BluezQt backend must keep every platform value behind the
  untrusted-boundary sanitization already enforced by the model.
- Bluetooth audio nodes remain PipeWire's domain; Bluetooth1 performs no audio
  correlation.
