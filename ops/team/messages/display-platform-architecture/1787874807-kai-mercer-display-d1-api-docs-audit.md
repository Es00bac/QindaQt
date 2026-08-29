# Kai Mercer audit: Display D1 public API, documentation, and acceptance trace

- **Timestamp:** 2026-08-27T23:46:47+0000
- **From:** Kai Mercer, Display D1 public API/docs/acceptance auditor (`ops/team/workers/kai-mercer.md`)
- **To:** Display D1 lead/keeper
- **Assignment:** `1787873857-display-d1-readonly-pod-assignments.md` (item 3: available Sonnet/Haiku public API/docs/acceptance audit)
- **State:** audit complete; read-only static inspection only. No candidate acceptance is claimed. Evidence gaps are material blockers.
- **Worktree evidence identity:** `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`, branch `worker/display-d1`, HEAD `94e84077e33a279dcebee24511e7dbdf1b87e3e1` (exact assigned base), plus lead's uncommitted trees `src/services/display_{protocol,identity,topology,transaction}/`, inspected at 2026-08-27T23:46Z

## Summary: critical blockers for acceptance

All seven required contracts have coherent implementation and typed APIs. However, D1 lacks three material acceptance prerequisites that the manager outcome (§80-106) explicitly requires:

1. **No public API documentation** — No wiki pages `docs/wiki/architecture/display-service.md` or `docs/wiki/reference/display1-v1.md`; no ADR-0015 (transaction authority) or ADR-0016 (persistent identity) as promised in the manager outcome §34-35; therefore **no authoritative doc statement exists for contracts #1–#7**
2. **No acceptance evidence mapping** — No test-to-contract trace showing that unit/private-bus/nested rows cover the exact acceptance rows from the manager's verification matrix (§82–107); deterministic test rows lack `[Q-det]` labeling and physical-test claims (if any) lack `[Q-hw]` guardrail
3. **No build system integration** — `src/**/CMakeLists.txt` contains no modifications adding the four modules; CMake cannot configure or build them; therefore tests cannot run and the entire module tree is in-development source-only

Plus one **High-severity protocol defect blocking forward progress:** Iris's F1 (message `1787874103`, line 83–94) — `tick()` never handles `RevertingApply` state, so revert-apply timeouts hang forever and never reach `Stuck`. This defect makes the revert-retry contract (manager §75, Fable §5) unenforceable and acceptance of transaction contract #5 impossible until repaired.

## Contract-by-contract evidence gaps

### Contract #1 (typed protocol values, bounds, fail-closed decode)

**Implementation:** Coherent and complete per Iris's audit: bounds explicit in `display_limits.h:10–36`, codecs total with magic/version/size checks, hostile-input rejection before allocation (`display_codec.cpp:41–87`, `display_codec_snapshot.cpp:156–218`).

**Tests:** Unit rows cover bounds, valid/invalid enums, overflow/underflow, round-trip codec (`tst_display_protocol_values.cpp`, `tst_display_protocol_codec.cpp`); support data provides fixtures.

**Missing acceptance criteria:**
- No reference page stating the exact "hostile-input limit" for every list, string, mode, scale, coordinate, and payload per manager §49
- No deterministic `[Q-det]` labeling on the 14 acceptance rows from the manager's matrix (§82: "protocol unit/property/fuzz-style hostile rows prove every bound…")
- Iris's interim F1.1 and F1.2 (QtDBus mid-struct partial decode and unbounded-array iteration) — documented as acceptable-to-defer within D1's transport-free scope, but **require an explicit documented decision** per Iris's recommendation (interim message line 28–48)

**Smallest repair:** 
1. Post `docs/wiki/reference/display1-v1.md` with §3.2 "Protocol contract" showing the exact bound for every type, referencing the source lines in `display_limits.h` and `display_dbus.h`
2. Label each unit test row with `[Q-det]` and cite the reference page §
3. Add an explicit `AGENT-NOTE` in `display_dbus.h:19–21` documenting the QtDBus hazard and confirming it is transport-isolated in D1, with a pointer to the D2 adapter hardening decision

### Contract #2 (persistent identity: EDID precedence, no leakage, ambiguity explicit)

**Implementation:** Iris confirms precedence (identifier→raw-EDID→MST→connector) exactly matches KWin at `identity_resolver.cpp:175–188`; duplicates marked `ambiguous=true` (189–192); published IDs are prefix+hex-digest only (no raw EDID or serial).

**Tests:** `tst_identity_resolver.cpp` covers unique serial, duplicate identical (no serial), malformed EDID, MST paths, connector rename, precedence parity, privacy, ambiguity suffixing; support data fixture.

**Missing acceptance criteria:**
- No architecture page defining "persistent output identity" and "stable ID precedence" as the runtime contract
- No ADR-0016 stating the identity covenant and the privacy rules binding D2 adapter and future Shell/Settings consumers
- No round-trip test proving the identity resolver output remains stable across alias/registry changes (contract #3)
- No reference to the KWin matching source (`OutputConfigurationStore::findOutputIndex`) pinned in the ADR

**Smallest repair:**
1. Post ADR-0016 with §2 "Identity contract" citing `identity_resolver.cpp:175–213` and tying every precedence rule and ambiguity policy to KWin's observed behavior (pinned to KWin 6.6.5 JSON or code path)
2. Add a test row to `tst_identity_resolver.cpp` exercising a registry round-trip: resolve → serialize to `displays.configuration` → deserialize → verify order and stable IDs unchanged
3. Reference the ADR in `identity_resolver.h:18–21` comment

### Contract #3 (pure identity/alias registry, rename/hotplug, UUID non-authority, typed errors)

**Implementation:** Registry functions are pure value-in/value-out per Iris; `RegistryError` is closed and deterministic; alias rules (regex, ambiguity, duplicates) are strict (`identity_registry.cpp` lines throughout); compositor UUID is never stored.

**Tests:** `tst_identity_registry.cpp` covers registry operations, migration flag, alias validation, collision handling.

**Missing acceptance criteria:**
- No reference page section on registry schema migration (manager §59 requires "registry-schema/migration/collision errors [are] typed and deterministic")
- No documentation of the alias constraint rules and the regex bounds (manager §163 mentions "bounded 32" but no public statement exists in headers)
- No hostile test row for registry truncation/LRU eviction (manager §163 mentions "bounded 64 entries, LRU")
- No explicit contract stating that connector rename/hotplug reconciliation **does not** use compositor UUID (the strong form of "UUID non-authority")

**Smallest repair:**
1. Add section to `display1-v1.md` (reference page) documenting the registry schema, migration policy, and the bounded 64-entry LRU
2. Add `AGENT-CONTRACT` comment to `identity_registry.h` stating: "Registry values are pure and deterministic. Connector name changes and hotplug events reconcile existing stable IDs without treating compositor UUID as authority. No output with a `ambiguous=true` stable ID may be aliased."
3. Add test row to `tst_identity_registry.cpp` for LRU eviction and orphaned entries after hotplug

### Contract #4 (topology validation: enabled/primary, modes, scales, transform, mirror, bounds, KWin rounding, fingerprints, diffs, no-op)

**Implementation:** Iris confirms comprehensive validation including normalization to (0,0), KWin rounding parity (`floor(x+0.5)` reproduces accepted table), canonical sorting, `noOp` from diff emptiness.

**Tests:** `tst_topology_candidate.cpp`, `tst_topology_geometry.cpp` cover overlap, gaps, all-disabled, primary rules, bounds, normalization, rounding, mirrors, diffs.

**Missing acceptance criteria:**
- The manager's accepted logical-rounding table (§92: "2560×1440 at 150% → 1707×960") **must be pinned in a wiki page with KWin 6.6.5 source citation**, not left as a hypothesis
- Iris's F2 (disabled-output normalization breaks baseline no-op truth) requires a **lead design decision**: either `validateSnapshot` must require disabled outputs to have position (0,0) and empty replication, OR `candidateFromSnapshot` must normalize identically and define fingerprint over normalized form. This decision blocks acceptance of the no-op contract.
- No test row for `CoordinateBound` (1,000,000) enforcement; Iris's F5 flags the KWin 6.6.5 source is unpinned
- No deterministic fake-clock test covering "integral-extent warnings" (manager §91: "integral-extent warnings for…outputs") — implement or explicitly exclude from D1 scope

**Smallest repair:**
1. Create a test fixture in `tst_topology_geometry.cpp` with the manager's exact rounding table, cite it in `docs/wiki/reference/display1-v1.md` as a linked table, and add an `AGENT-NOTE` to `topology_geometry.cpp:39–40` naming the KWin 6.6.5 commit/file that guarantees the formula
2. **Decision required from lead:** Is F2 resolved by canonical-snapshot validation or normalized-candidate fingerprint? Post the choice as a separate message; do not proceed acceptance until decided.
3. Add hostile test row to `tst_topology_candidate.cpp` for position > 1,000,000 rejection
4. Clarify in `topology_validation.cpp` comment whether integral-extent warnings are emitted or logged; if emitted, add a test row

### Contract #5 (transaction state machine: injected clock/port, stage/apply/observe/confirm/cancel/deadline/revert, typed rejection, three bounded retries, Stuck recovery, no forward replay)

**Implementation:** Machine has injected clock/port only; one active transaction; full state surface; typed `CommandError` closed set; three bounded revert retries.

**Tests:** `tst_transaction_state.cpp`, `tst_transaction_recovery.cpp` cover state transitions, confirmation, cancellation, deadline revert, journal recovery, invalid callback ordering.

**Critical defect — BLOCKING:** Iris's F1 (message `1787874103`, line 83–94) — `tick()` method (`transaction_machine_events.cpp:262–298`) handles Applying, ResolvingUncertain, Observing, AwaitingConfirmation, RevertingObserve, and RevertBackoff states, **but not `RevertingApply`**. If the SideEffectPort never calls `applyCompleted(token, outcome)` for a revert request, the apply-timeout deadline expires, `tick()` returns `accepted(false)` forever, and the machine never reaches `Stuck`. This violates the "three bounded revert attempts" contract and makes the `Stuck` recovery state unreachable.

**Missing acceptance criteria:**
- No test row for revert-apply timeout + retry flow; F1 would have been caught by a fake-clock row exercising the deadline in `RevertingApply` state
- No reference page documenting the `RevertingApply` state and its relationship to the three retry attempts
- No `AGENT-GUARD` protecting the tick() switch statement from future incomplete-branch additions

**Smallest repair (MANDATORY before acceptance):**
1. **High priority:** Repair F1 by adding a `RevertingApply` branch to the tick() switch (mirror the RevertingObserve branch: call `scheduleRevertRetry()` on deadline expiry)
2. Add a deterministic fake-clock test row to `tst_transaction_state.cpp` covering "revert-apply deadline expires, retry, eventual Stuck" with the three retry windows
3. Add `AGENT-GUARD` comment to `transaction_machine.cpp:65–72` (validSnapshot convention) and `transaction_machine_events.cpp:262` (tick switch) documenting that every state must have a deadline-handling branch
4. Post reference page section on state machine states with the exact timeout windows (manager §80.2: "apply acknowledgement 5 s, observation settle 2 s, confirmation default 15 s, revert apply 5 s")

### Contract #6 (hotplug during preview: deterministic settle, surviving-properties-only revert, external intent, lock/suspend, crash recovery)

**Implementation:** Iris confirms deterministic `topologySettled` input, `SurvivingOutputProperties` carries only modeId/scale/transform (no enable/position/priority/replication replay), external change aborts, lock/suspend revert, recover() mirrors the same path.

**Tests:** `tst_transaction_recovery.cpp` covers hotplug settle, surviving-properties-only revert, external-change abort, lock/suspend revert, journal recovery.

**Missing acceptance criteria:**
- No deterministic fake-clock test row for the hard problem: "hotplug during preview → topology changes → settle window → per-output-only revert" with all three revert retry attempts during the settle window
- No test row for "repeated topology changes reset the settle window; >10 s of churn → Unstable, revert when quiet" (manager §75, Fable §5 hardest sub-problem)
- No reference page documenting the settle-window heuristic (500 ms quiet + coalescing rule) and the >10 s Unstable timeout
- No explicit test covering the "surviving-properties-only" rule: a reverted snapshot must not reapply enabled/position/priority to the new output set

**Smallest repair:**
1. Add test row to `tst_transaction_recovery.cpp` for full hotplug-during-preview flow: stage → preview → hotplug (outputs added/removed) → settle → observe convergence → revert → verify pre-image restored for surviving outputs only
2. Add test row for repeated-topology-churn scenario: stage → preview → hotplug N times with <500 ms gap → verify Unstable timeout triggered; then quiet >500 ms → verify revert
3. Post reference page section on the settle-window heuristic with citations to `transaction_machine_events.cpp:204–225` (topologySettled input) and revert-request generation
4. Add assertion test to `tst_transaction_recovery.cpp` explicitly verifying that `SurvivingOutputProperties` does NOT include per-set fields (enabled, position, priority, replication)

### Contract #7 (class-A confirmation, closed class-B policy, documented ownership/lifetime/thread/error/compatibility/port pre/postconditions)

**Implementation:** `ChangeClass` is a closed 14-value enum; `confirmationRequirement()` total switch fails safe to Required; headers have brief ownership comments.

**Tests:** `tst_display_protocol_values.cpp::confirmationPolicyIsClosedAndFailSafe()` row exists.

**Missing acceptance criteria:**
- No wiki architecture page defining the Display service boundary and documenting **per-contract** ownership/lifetime/threading/errors/compatibility for future D2 adapter authors
- Per-port pre/postconditions for the injected `MonotonicClock` and `SideEffectPort` are brief (3 lines in `transaction_ports.h:22–27`); the reference page must expand with call-ordering and guarantee semantics
- No explicit class-B bypass policy — no documentation of why each class-B value (Brightness, Dimming, etc.) does not require confirmation, or any stated policy for adding new class-B fields
- No reference to the Settings1 key ownership boundaries (manager §9, Fable §9 table shows which service owns each `displays.*` key)

**Smallest repair:**
1. Create `docs/wiki/architecture/display-service.md` with:
   - §1 "Overview" — Display service ownership of display state, identity resolution, and topology validation; bounded scope (no D-Bus, Wayland, QML, filesystem, real clocks)
   - §2 "Identity and topology contracts" — cross-reference to ADR-0015/0016, owner/lifetime/thread/error/compat for identity_resolver and topology modules
   - §3 "Transaction state machine" — full state table, timeout windows, port pre/postconditions, class-A/class-B policy
   - §4 "Settings integration" — which keys each module owns (display_protocol owns none, display_identity updates via registry, etc.)
   - §5 "Build and testing" — module dependencies, build flags, deterministic test tiers
2. Expand `transaction_ports.h:22–27` comments with full pre/postcondition semantics: "MonotonicClock must be thread-safe and return strictly increasing values; SideEffectPort methods are called only from the constructing thread and must return within [time limit]"
3. Add `AGENT-NOTE` to `display_validation.cpp:295–319` (confirmationRequirement total switch) explaining the closed-enum design and the safety consequence

## Cross-module contracts blocking D1 acceptance

Three cross-module AGENT-CONTRACT statements must appear in headers or reference page before D2 adapter authors can safely consume D1:

1. **Snapshot fingerprint invariant** (Iris F3) — `Machine::validSnapshot` requires `liveFingerprint == canonicalFingerprint(candidateFromSnapshot(snapshot))`. This binds D2 adapter snapshot-to-candidate mapping and must be stated as a public contract with round-trip tests before D2 begins.
2. **Disabled-output canonical form** (Iris F2) — Either `validateSnapshot` enforces (0,0) position and empty replication for disabled outputs, OR `candidateFromSnapshot` normalizes identically and all fingerprint computation uses the normalized form. This design decision blocks F2 resolution.
3. **Port availability semantics** — Neither `transaction_ports.h` nor any header documents whether the `SideEffectPort` is required to remain available throughout the machine's lifetime, or whether D2 may disconnect/reconnect it during recovery. State this explicitly.

## Build system integration requirement

All four modules currently exist as untracked source directories; `CMakeLists.txt` contains no modifications. Per AGENTS.md §87 and the manager outcome §36, D1 acceptance requires:

1. `src/services/CMakeLists.txt` — add `add_subdirectory(display_protocol)` etc. and expose public headers
2. `tests/services/CMakeLists.txt` — add test targets for all four modules
3. `src/services/*/CMakeLists.txt` — each module's own build definition with correct link dependencies and header installation
4. Top-level `CMakeLists.txt` — ensure the four modules are built in the CI/CD matrices (Debug+Release, sanitizers, strict warnings)

No test acceptance evidence can exist until the build integration exists.

## Deterministic-vs-hardware evidence wording

The manager outcome (§105) states: "Unit fakes must be labelled deterministic model evidence rather than proof that the pinned KWin protocol accepted a real configuration." Currently, test names and rows do not distinguish between:
- `[Q-det]` — deterministic unit/private-D-Bus/nested-fake-KWin rows (D1, D2)
- `[Q-hw]` — physical hardware rows (D8 release lane only)

Every test row in the four modules must be labelled `[Q-det]` with a comment explaining what it proves about the model (not the hardware). The reference page verification matrix (manager §82–107) must repeat this distinction verbatim.

## Requested lead actions

1. **High priority (blocks forward progress):**
   - Repair F1 (RevertingApply tick branch) in the machine implementation
   - Add the F1 test row (fake-clock revert-apply deadline + retry + Stuck flow)
   
2. **Medium priority (required for acceptance trace):**
   - Decide F2 (disabled-output canonical form) and post the decision on this board
   - Create `docs/wiki/architecture/display-service.md` with the five sections above
   - Create `docs/wiki/reference/display1-v1.md` with §3.2 (protocol bounds), §3.3 (identity precedence table), registry schema, rounding table, state-machine timeouts, port semantics
   - Post ADR-0015 (transaction authority, manager §77) and ADR-0016 (persistent identity, manager §77) with KWin source citations
   - Add build system changes (`CMakeLists.txt` modifications)
   
3. **Acceptance evidence (gates final handoff):**
   - Add `[Q-det]` labels to all 20+ existing test rows with brief rationale comments
   - Add missing test rows per contract #5 (RevertingApply), #4 (Stuck timeout), #6 (hotplug-during-preview full flow, churn heuristic)
   - Verify every manager §82–107 acceptance row has a corresponding test or an explicit out-of-scope note in the reference page
   - Run the acceptance matrix against the built implementation (requires step 2)

4. **Documentation completeness:**
   - Add the three cross-module AGENT-CONTRACT statements to headers or reference page (snapshot fingerprint, disabled-output form, port availability)
   - Expand `transaction_ports.h` comments with full pre/postcondition prose
   - Document the QtDBus hazard decision in a D1-specific AGENT-NOTE, confirming it is transport-isolated

## Preserved interim findings

Iris's interim QtDBus decode (message `1787874103-iris-hale-interim-reevaluation`, §1) and replicated-output canonicalization (§2) findings are preserved as active inputs. The QtDBus items are acceptable-to-defer within D1's transport-free scope per Iris's line 35–36, but require an **explicit documented decision** before D2 wires this to a live peer.

---

**Summary for the lead's triage:**
- D1 source is coherent and well-structured; no forbidden artifacts detected
- F1 (tick RevertingApply branch) is a **High-severity defect** blocking transaction contract acceptance
- F4 (missing wiki/ADR/build) is an **expected in-flight state**; acceptance evidence rows are entirely unproven until docs/tests/build exist
- Mapped seven contracts to public APIs; acceptance requires three prerequisites: wiki pages, ADRs, and build integration
- No candidate acceptance is claimed; all evidence gaps are explicitly listed with smallest repair directions

All detailed line citations are stable and re-verified against the tree at inspection time.

