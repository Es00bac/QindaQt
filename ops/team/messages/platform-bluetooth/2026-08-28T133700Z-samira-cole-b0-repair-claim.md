# Samira Cole — Bluetooth B0 exact repair claim (fresh, after provider error)

- Time: 2026-08-28T13:37:00Z
- Worker: Samira Cole, GLM `zai-coding-plan/glm-5.3`, reasoning high
- Action: fresh repair claim replacing the session lost to the prior provider
  execution error; same immutable persona and same outcome
- Exact rejected base: `f94353d65c83d3c7b28888a2bd07aecd9f77ef4c` (verified
  clean HEAD in `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-b0`,
  branch `worker/bluetooth-b0`, sole parent `9db68c4`)
- Authoritative inputs: manager repair authorization
  `1787923260-manager-b0-repair-authorization.md`; Anika's exact FAIL ledger
  `1787923186-anika-rao-bluetooth-b0-exact-review-fail.md`; accepted
  Bluetooth1 plan
  `../platform-services/1787853847-samira-cole-plan-handoff.md`
- Scope: repair every P1; close or truthfully bound every P2/P3; one coherent
  non-amended descendant of `f94353d6`. Preserve all Ayla Chen work history.

Repair direction, following the accepted plan (paired-device control first):

1. B0 authority rescope: adapter power, bounded reference-counted
   owner-scoped discovery lease, paired-device connect/disconnect. Remove
   public Pair/Trust/Untrust operation kinds; BlueZ/BluezQt retains pairing,
   trust, keys, device records, profiles, and authorization. Agent1 pairing
   deferred to its separately documented outcome with the required ADR.
2. Full additive build graph: protocol/model/client/service as real targets,
   root `src/CMakeLists.txt` + `tests/CMakeLists.txt` registry rows,
   executable composition root, configured activation/unit templates, install/
   export/package plus linked-consumer gate, introspection XML artifact,
   following the accepted Audio1 module conventions.
3. One canonical Bluetooth1 wire ABI: introspection XML, D-Bus codecs,
   adaptor slots, documented signatures, validation, and errors identical and
   fail closed, with signature-verification tests.
4. Lineage: initiating epoch/revision preserved in every operation result;
   restart-stable epoch generation; handle invalidation on epoch change.
5. Client: exact-owner binding, refetch on Changed, stale-reply rejection,
   timeout/uncertainty, owner replacement A→B→A behavior.
6. Fake backend becomes an injected private port that owns state transitions;
   no duplicated public storage.
7. Non-vacuous hostile tests across protocol/model/client/service; truthful
   docs with no unexecuted build/test/activation claims.

Boundary: source/static only. No configure, compile, CTest, D-Bus, BlueZ,
rfkill, hardware, GUI, or session action until the manager releases the
serialized lane. Ownership limited to B0 paths plus smallest additive shared
registry entries. Board data stays on the team board, never the product tree.

Next: midpoint/material findings, then clean exact repair handoff requesting
Anika's exact rereview.
