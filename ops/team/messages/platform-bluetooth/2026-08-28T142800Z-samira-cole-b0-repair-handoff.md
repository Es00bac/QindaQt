# Samira Cole — Bluetooth B0 exact repair handoff

- Time: 2026-08-28T14:28:00Z
- Worker: Samira Cole, GLM `zai-coding-plan/glm-5.3`, reasoning high
- Outcome: repair of exact rejected `f94353d65c83d3c7b28888a2bd07aecd9f77ef4c`
  (Anika's exact 0/8/4/3 ledger) as one coherent, non-amended descendant
- Exact repair commit: `bbbe8b8f6f5e51033da857e3b0c6d38dc442fbb4`
- Tree: `f2fcc8597c674e8873d7a9b1193ea477b26badff`
- Sole parent: `f94353d65c83d3c7b28888a2bd07aecd9f77ef4c` (preserved, not
  amended; Ayla Chen's work and history intact)
- Exact 60-path sorted name-status manifest SHA-256:
  `f3e1cbb40267458937561a080f13bfd39f19da179168b7ed77c622c5c973b3ce`
- Worktree: clean after commit; branch `worker/bluetooth-b0`
- Requested next action: Anika Rao exact-commit rereview of `bbbe8b8`

## P1 closure map

1. **P1-1 authority** — Pair/Trust/Untrust operation kinds, trust flags, and
   local pairing state are gone. B0 exposes inventory snapshots, adapter
   power, bounded caller-scoped reference-counted discovery leases, and
   paired-device connect/disconnect. BlueZ/BluezQt keeps pairing, trust,
   keys, device records, profiles, authorization; Agent1 pairing deferred to
   its own outcome and recorded in ADR-0026.
2. **P1-2 build graph** — protocol/model/client/service are real static
   targets with SPDX headers, aliases, FILE_SET public headers, warning
   helper, export set, and install rules; root `src/CMakeLists.txt` and
   `tests/CMakeLists.txt` registry rows added; executable composition root
   `app/main.cpp`; configured `.service.in`/unit templates; introspection
   XML installed to `dbus-1/interfaces`; private-bus transport test and
   dbus-daemon activation test consuming `$<TARGET_FILE:qindaqt-bluetooth-service>`.
3. **P1-3 compile blockers** — no `<QtCore/memory>`; `<memory>`/`<algorithm>`
   owned where used; single `Q_DECLARE_METATYPE` set in `bluetooth_types.h`;
   transport owns `QDBusPendingReply`/connection-interface includes;
   `QDBusServiceWatcher` forward-declared at global scope; private headers
   resolved by basename through explicit include directories (Audio1 test
   precedent).
4. **P1-4 wire ABI** — one canonical signature set asserted by
   `QDBusMetaType::typeToSignature` tests, the adaptor `Q_CLASSINFO`
   introspection, and `data/org.qindaqt.Bluetooth1.xml`:
   Snapshot `(uutuussa((tt)ssbb)a((tt)(tt)ssubbbbn))`, Adapter `((tt)ssbb)`,
   Device `((tt)(tt)ssubbbbn)`, Handle `(tt)`, result `(uuttttss)`;
   operations are explicit adaptor slots with delayed replies.
5. **P1-5 lineage** — pending operations store initiating epoch/revision and
   completion rebuilds results from that stored lineage; the contradictory
   test expectation was removed with the whole superseded test file.
6. **P1-6 restart/client** — restart-unique epoch (wall clock + entropy,
   strictly advancing on reuse), revision reset per epoch, restart publishes
   `backend-restarting` and invalidates all handles; client is the accepted
   Audio1-pattern exact-owner client: refetch, invalidation coalescing,
   stale/malformed reply rejection, owner-replacement snapshot clearing,
   timeout/uncertainty, queued exactly-once completion, stop semantics.
7. **P1-7 fail-closed validation** — canonical MAC form, ascending unique
   cross-array serials, unknown capability bits, RSSI exactly zero when
   unknown and `[-128,0]` dBm when known, connected⇒paired and
   connected⇒powered-adapter, discovering⇒powered, structured reason codes,
   control-safe bounded diagnostics, in-RAM bounds mirrored by bounded wire
   decoding (`wireValid`), operation-result lineage validation.
8. **P1-8 truthful evidence** — docs claim no executed build/test/activation;
   the qualification boundary states the serialized-lane gate explicitly;
   tests are real hostile rows that fail on the rejected commit.

## P2/P3 closure

- **P2-1** public headers state ownership/thread/lifetime; connection held
  by value; no mutable internal-list accessors; no public lineage mutators.
- **P2-2** the deterministic backend is a private injected port that owns
  state transitions and leases; production factory returns an empty one
  publishing truthful `Unavailable/no-adapter`; only qualification populates
  it through the private header.
- **P2-3** resident service retains its connection by value, registers an
  explicit adaptor object with scriptable slots/signals only, tears down
  symmetrically, and wires model invalidation to `Changed`.
- **P2-4** every operation validates capability, adapter existence/power,
  pairing, connection state, and lease bounds before dispatch; no unused
  `secondary`-style fields exist.
- **P3-1..P3-3** Audio1 CMake conventions; `AF_UNIX`-only hardened unit with
  tasks/capability/kernel/personality/SUID hardening and configured paths;
  both wiki pages in `mkdocs.yml` nav with reciprocal links, four
  module-boundary rows plus a dependency-direction bullet, ADR-0026 and
  index row added.

## Verification evidence (static only)

- `git diff --check f94353d bbbe8b8`: PASS.
- `tools/check-source-shape`: PASS (largest new nonblank source file
  `bluetooth_model.cpp` is under the 500-line review threshold).
- `tools/validate-docs`: PASS, 66 Markdown documents and `mkdocs.yml`
  navigation, including the previously orphaned Bluetooth pages.

## Bounded caveats

- No configure/compile/CTest/D-Bus/BlueZ/rfkill/hardware/session action was
  performed; all build/test/activation gates await the manager's serialized
  lane. Strict Debug/Release builds and the registered tests
  (`qindaqt.bluetooth-*`) are the next gate, not evidence in this commit.
- `mkdocs build --strict` could not run: no `mkdocs` module exists on this
  host. `tools/validate-docs` (the repository's dependency-free link/nav
  checker) passed over the full navigated set.
- The BluezQt production backend remains deferred (ADR-0026): the B0
  process composes the deterministic adapter and truthfully publishes
  `Unavailable/no-adapter` until that runtime lane replaces it behind the
  unchanged `AdapterBackend` port.

Samira is handing off and not live as of this message.
