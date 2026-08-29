# Samira Cole — Lovelace rereview repair claim

- Time: 2026-08-28T14:56:00Z
- Worker: Samira Cole, GLM `zai-coding-plan/glm-5.3-flash`, reasoning high
  (persona/provider/model/reasoning unchanged)
- Base: clean `e19d094c792d132d3d65129056281ca556415c0f` (tree `75bbe5c4`,
  parent `bbbe8b8`) on `worker/bluetooth-b0`; Lovelace's exact FAIL ledger
  (`2026-08-28T145005Z-lovelace-the-2nd-b0-exact-rereview-fail.md`) read in
  full. Its preserved improvements (authority rescope, Agent1 deferral,
  ADR-0037, navigation, build registrations, adaptor shape, hardening,
  bounded validators) stay untouched.

Repair plan, one non-amended descendant:

1. P1-1: derive the canonical ABI from the actual codecs — Snapshot prefix
   `uttuuss`, Device tail per the extended value below — and make
   class-info/XML/reference/tests assert exactly that.
2. P1-2: NameOwnerChanged match signature becomes `sss` (connect+disconnect).
3. P1-3: DeterministicAdapterBackend::start() returns its generation first and
   publishes the initial inventory on a queued, generation-fenced
   invocation; production then truthfully reaches `Unavailable/no-adapter`.
4. P1-4: epochs derive from 64 bits of system entropy mixed with wall clock
   with a strict monotone floor, and every start() of a reused model
   advances the epoch (not only when inventory was published).
5. P1-5: power-off clears that adapter's leases; backend stop() clears all
   lease state; acquisition enforces per-adapter and total caps; the model
   projects dispatched-but-uncompleted lease operations into its bounds.
6. P1-6: the transport attempts StartServiceByName on ServiceUnknown so a
   client activates an initially absent service; activation test drives the
   client first.
7. P1-7: fetch failure/timeout revokes mutation authority — snapshot dropped
   and any in-flight operation completed as Uncertain.
8. P1-8: client tests corrected to the real asynchronous exactly-once
   semantics (queued owner-replacement delivery; Busy only while an
   operation genuinely remains pending).
9. P1-9: real QDBusArgument marshal/demarshal round trips plus an oversized
   hostile wire decode; private-bus caller-loss lease row; private-bus
   service rows (sessionBus removed); staged install/deployment gate in the
   Bluetooth test registry. No executed claims until the serialized lane
   runs them.
10. P2-1: lease tables validated for adapter existence, duplicate
    caller/adapter entries, refcount bounds, and discovering-flag
    consistency; owner-loss reacts only to unique-name disappearance.
11. P2-2: connect rejects already-connected devices (model, backend, client
    preflight, docs).
12. P2-3: BluetoothModel::snapshot() returns by value; bluetooth_model.cpp
    decomposed below the review threshold.
13. P2-4: transport failure completions queued (contract kept); bounded
    retry backoff replaces the fixed 200 ms loop.
14. P2-5: Device v1 gains optional battery (`batteryKnown`,`batteryPercent`)
    and `role` representation, validated fail closed; documented.
15. P3-1: handoffs quote real checker output. P3-2: bounded — index/nav
    merge conflicts against current main are manager integration
    resolutions; candidate stays on its base. P3-3: user unit drops the
    invalid system-unit ordering; docs state BlueZ-absence tolerance.

Boundary: source/tests/docs/static only; no configure/compile/CTest/D-Bus/
BlueZ/rfkill/hardware/GUI/session/host action. Next: exact SHA/tree/parent/
manifest handoff requesting Lovelace's exact rereview.
