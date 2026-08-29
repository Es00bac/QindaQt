# Mina Shah handoff: seven-contract public API/docs/acceptance trace

- **Timestamp:** 2026-08-27T18:15:00-06:00
- **From:** Mina Shah, Claude Sonnet 5, read-only Display D1 public-API/docs/
  acceptance-trace reviewer
- **To:** Display D1 lead/keeper
- **Assignment:**
  `1787875111-display-d1-mina-docs-trace-rereview-assignment.md`
- **State:** trace complete against the current checkpoint; read-only static
  inspection only, no candidate acceptance claimed
- **Evidence identity:** worktree
  `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`, base
  `94e84077e33a279dcebee24511e7dbdf1b87e3e1` plus the uncommitted checkpoint
  described in `1787875060-display-d1-shared-registry-checkpoint.md`, traced
  at 2026-08-27T18:15Z

I read `docs/wiki/architecture/display-service.md`,
`docs/wiki/reference/display1-v1.md`, `docs/wiki/adr/0015-...md`,
`docs/wiki/adr/0016-...md`, Kai Mercer's prior audit
(`1787874807-kai-mercer-display-d1-api-docs-audit.md`) and your triage of it
(`1787874648-...`), Iris's consolidated audit/triage, and the shared-registry
checkpoint diff, then cross-checked every specific numeric/textual claim
against the four modules' current headers, `.cpp` sources, and CMakeLists. I
did not repeat Iris's algorithmic audit or Elara/Fable's formal transaction
counterexample analysis; I checked whether their already-triaged findings
(F1 `RevertingApply` tick branch, F2 disabled-output canonical form, F3
fingerprint contract) are visible and correctly stated at the public-API/doc
layer, not their proofs.

## Contract-by-contract trace

1. **Typed protocol values, hostile bounds, fail-closed decode.** Every limit
   in `display1-v1.md`'s "Protocol values" table matches
   `display_limits.h:10-36` exactly (32 outputs, 128 modes, 1 transaction,
   1,048,576-byte payload, 128/256/512-byte text bounds, 16,384 px,
   10,000 mm, ±1,000,000 coordinate, 1.0-3.0 scale, 32-byte fingerprint). The
   three documented D-Bus signatures I spot-checked
   (`Mode`, `Candidate`, `Snapshot`, `OperationResult` in `display1-v1.md`'s
   table) match the literal `hasSignature(...)` strings in
   `display_dbus.cpp:71,87-88,104` byte for byte. `display_codec.h:36-38`'s
   AGENT-CONTRACT and `display1-v1.md`'s "Readers first reject payloads above
   1 MiB... leave the caller's destination byte-for-byte unchanged" agree.
   **Trace: clean, no gap.**

2. **Persistent identity precedence, no leakage, explicit ambiguity.** The
   five-row precedence table in `display1-v1.md` (`edid:`, `edidraw:`, `mst:`,
   `conn:`, `connhash:`) matches the literal prefixes in
   `identity_resolver.cpp:92,95,178,181,185` exactly, including the
   connector-vs-hash split. ADR-0016's "duplicate stronger material is
   explicit `ambiguous=true`... residual collision receives deterministic
   `#n` suffixes" matches `identity_resolver.cpp:211`'s collision-index
   suffixing. **Trace: clean, no gap.**

3. **Pure identity/alias registry, rename/hotplug, UUID non-authority, typed
   errors.** `display1-v1.md`'s registry-schema section and ADR-0016's
   eviction description ("evicts the oldest disconnected entry first, with
   stable-ID tie breaking... cannot evict a connected entry") match
   `identity_registry.cpp:299-317` exactly: `min_element` over disconnected
   entries by `(seenSequence, stableId)`, and a connected `evict` candidate
   returns `TooManyEntries` instead of erasing. The alias regex
   `[A-Za-z0-9][A-Za-z0-9_-]{0,63}` in `display1-v1.md` matches
   `identity_registry.cpp:49` literally. Runtime compositor UUID is absent
   from `ResolvedOutput`/`RegistryEntry` (`identity_types.h`,
   `identity_registry.h`), matching "never stored." **Trace: clean, no gap.**

4. **Topology validation, KWin rounding, fingerprints, diffs, no-op.** The
   fractional-rounding table in `display1-v1.md` is explicitly labeled
   "deterministic math evidence only... D2/M0 must measure," which correctly
   avoids Kai's earlier concern about an unpinned hard KWin citation — it now
   reads as a design input, not a proof claim. `canonicalFingerprint`
   (`topology_fingerprint.cpp:95-123`) includes exactly the nine fields the
   reference page lists and excludes epoch/revision/metadata, matching
   verbatim. The replica projection claim ("position/scale... cannot change a
   fingerprint... mode/transform remain target-specific") matches
   `candidateFromSnapshot` (lines 66-86, walks to `root.position`/`root.scale`)
   and `diff()` (lines 139-144, skips Position/Scale only when
   `replicationSourceStableId` is non-empty, always compares Transform).
   **Trace: clean, no gap.**

5. **Transaction state machine: clock/port, full state surface, three bounded
   retries, no forward replay.** The ten-row state table in
   `display-service.md#transaction-model` covers all twelve `MachineState`
   values (two are folded: `Discovering`+`Ready` and `Staged` are listed
   individually, all twelve appear). `tick()`
   (`transaction_machine_events.cpp:262-304`) now has the `RevertingApply`
   branch at line 291 that Kai's F1 flagged missing — confirmed present and
   calling `scheduleRevertRetry()`, matching the table's "Apply timeout
   consumes the attempt and schedules retry." Default timeouts
   (5 s/2 s/15 s/250 ms/500 ms) in both doc pages match `Timing`'s defaults
   in `transaction_types.h:81-85` exactly. **Trace: clean, no gap; F1 is
   verifiably repaired at the source level.**

6. **Hotplug settle, surviving-properties-only revert, external/lock/suspend,
   crash recovery.** `display-service.md`'s "Hotplug and external intent"
   section states D1 "intentionally models only the explicit settle event"
   and assigns the 500 ms/10 s heuristic to the future D2 adapter — this
   directly answers Kai's earlier request (§6) by scoping it out rather than
   silently omitting it. `SurvivingOutputProperties` (`transaction_types.h:
   90-98`) carries only `stableId`/`modeId`/`scale`/`transform`, matching
   "never replays enable, position, priority, primary, or replication."
   **Trace: clean, no gap; the omission Kai raised is now an explicit,
   documented scope boundary rather than a silent gap.**

7. **Class-A confirmation, closed class-B policy, documented ownership/
   lifetime/thread/error/compatibility/port pre/postconditions.** The
   thirteen class-B `ChangeClass` values enumerated in both doc pages'
   "Confirmation policy"/"classification" sections match
   `display_types.h:94-109`'s enum exactly (Brightness through
   CustomModeDefinition), and `display_validation.cpp:298-322`'s switch maps
   every one to `BypassedForClosedPolicy` with an `AGENT-GUARD` fail-safe for
   unknown values — this resolves the open question I raised in my prior
   claim message (`1787875025-...md`, item 4): the docs now explicitly state
   "Class-B transport/ownership is provisional until the corresponding
   platform lanes prove error semantics; no immediate production mutation is
   implemented here," naming the same provisional status the accepted Fable
   decision gave brightness/color, extended to the full closed set.
   `transaction_ports.h:22-30`'s AGENT-CONTRACT and
   `display1-v1.md`'s "Side-effect port preconditions"/"Transaction state and
   ports" sections agree on atomicity, borrowed-for-lifetime, copy-before-
   return, and at-most-one-callback semantics. **Trace: clean, no gap.**

## One concrete drift found

**`docs/wiki/architecture/module-boundaries.md:89-91`** states: "D1's
dependency direction is protocol → identity or topology → transaction;
identity is independent of topology/transaction." Read literally this asserts
a `protocol → identity` edge. It does not exist: `display_identity`'s
`CMakeLists.txt:22` links only `Qt6::Core`, and no file under
`src/services/display_identity/` includes anything from `display_protocol`
(confirmed by grep across all five `.h`/`.cpp` files). The actual graph is a
linear `protocol → topology → transaction` chain with `identity` fully
disconnected from all three other modules, not "independent of
topology/transaction" only. The correct, accurate table already exists two
sections earlier in the same document (`module-boundaries.md:35-38`, the
per-module "Dependencies" column) and in `display-service.md`'s "Pure D1
modules" table — both of those are accurate; only this one summary sentence
is wrong.

**Smallest repair:** replace `module-boundaries.md:90-91` with wording such as
"D1's dependency direction is protocol → topology → transaction; identity
depends only on Qt Core and is independent of protocol, topology, and
transaction," matching the accurate table above it.

## Navigation and reciprocal links

`mkdocs.yml` (Architecture/Reference/Decisions sections), `docs/wiki/index.md`,
`docs/wiki/adr/index.md`, `docs/wiki/architecture/overview.md`, and
`docs/wiki/development/testing-harness.md` all carry the exact additive edits
promised in the checkpoint diff — verified by direct read, not just the diff
text. Every cross-link I followed resolves to a real file at the stated
relative path (`display-service.md` ↔ `display1-v1.md` ↔ ADR-0015 ↔ ADR-0016
↔ `testing-harness.md#d1-deterministic-display-model` ↔
`display1-v1.md#deterministic-acceptance-matrix`). The nine `add_test`
selector names in the four modules' test `CMakeLists.txt` files match the
"Deterministic acceptance matrix" table in `display1-v1.md` one-to-one, and
`src/CMakeLists.txt`/`tests/CMakeLists.txt` both carry the four
`add_subdirectory` lines. **No broken link or stale selector name found.**

## Deterministic-vs-hardware evidence wording

Every place I checked (`display-service.md`'s "Evidence boundary," the
"Deterministic acceptance matrix" in `display1-v1.md`, and the new
`testing-harness.md` D1 section) consistently uses "deterministic model
evidence (`Q-det`)" and explicitly disclaims nested-KWin, `wl_output`
mirror-visibility, service-activation, and physical DRM/GPU/monitor/lid/
suspend proof. I did not find an overclaiming sentence anywhere in the four
documents. Per-test `[Q-det]` source labels are absent, but this matches your
recorded decision in `1787874648-...md` ("Test source names need not embed
documentary evidence tags; CTest labels plus the normative matrix carry that
distinction") — not a drift from an accepted decision.

## Summary

Six of seven contracts trace cleanly to an exact public API, test selector,
and doc/ADR statement with no repair needed. One concrete, smallest-repair
documentation drift (dependency-direction sentence,
`module-boundaries.md:89-91`) contradicts the accurate table two sections
above it in the same file. No forbidden dependency, artifact, or evidence-
language overclaim was found. No compiler, configure, build, runtime, or
host-state action was taken.
