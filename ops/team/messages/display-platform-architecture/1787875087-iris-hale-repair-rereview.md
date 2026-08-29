# Iris Hale repair rereview: Display D1 consumed-fix diff

- **Timestamp:** 2026-08-27T17:58:50-06:00
- **From:** Iris Hale, Display D1 adversarial audit assistant
- **To:** Display D1 lead/keeper
- **State:** bounded read-only repair-diff rereview complete; static source and
  test reading only. This is not candidate acceptance and no runtime evidence
  is claimed or implied.
- **Evidence identity:** worktree
  `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`, HEAD unchanged at
  `94e84077e33a279dcebee24511e7dbdf1b87e3e1`; review scope is the untracked
  `src/services/display_{protocol,identity,topology,transaction}/` and
  `tests/services/display_{protocol,identity,topology,transaction}/` trees as
  of 2026-08-27T17:58-06:00. Versus my 17:41 audit, at minimum
  `topology_validation.cpp`, `transaction_machine_events.cpp`,
  `transaction_machine.cpp`, `display_dbus.cpp`, `display_validation.cpp`,
  `identity_resolver.cpp`, `topology_fingerprint.cpp`, and the port/header
  files changed; `transaction_machine_revert.cpp` is byte-identical to my
  prior read.

## Assignment-item verdicts (all seven verified closed in source and tests)

1. **Silent `RevertingApply` timeout → exactly three bounded attempts →
   durable `Stuck`; no uncertain forward replay: CLOSED.**
   `transaction_machine_events.cpp:291-294` adds the missing tick branch
   routing an expired `RevertingApply` deadline through
   `scheduleRevertRetry()`; guards at
   `transaction_machine_revert.cpp:125-128,141-145` bound attempts to three
   and `enterStuck()` (154-165) stores a `JournalPhase::Stuck` journal.
   `tst_transaction_recovery.cpp:99-126`
   (`silentRevertApplyStillConsumesBoundedAttempts`) drives exactly this trace
   with a never-calling-back port and asserts 1 forward + 3 revert requests,
   `Stuck` state, and stuck-phase journal; the paired explicit-callback
   variant at 64-97 confirms attempt counting, backoffs, and `retryStuck`
   re-arming. Forward replay remains impossible: uncertainty resolves only via
   observation, revert scopes, or abort (`transaction_machine_events.cpp:95-114`).
2. **Canonical disabled form, projection, baseline no-op, live fingerprint
   agreement (incl. mirrored live snapshots): CLOSED.**
   `display_validation.cpp:173-175` now rejects disabled snapshot outputs with
   non-zero position or a replication source; `candidateFromSnapshot`
   (`topology_fingerprint.cpp:44-93`) projects disabled outputs and erases
   replica-derived position/scale; `Machine::validSnapshot`
   (`transaction_machine.cpp:65-79`) now demands the topology validation accept
   the projection as a normalized no-op fixpoint whose fingerprint equals
   `liveFingerprint` — non-canonical snapshots are rejected at
   `initialize`/every observation rather than trusted. Tests:
   `tst_topology_candidate.cpp:88-97` (mirrored live snapshot no-op +
   fingerprint parity), `:100-123` (baseline no-op, reorder stability,
   single-field diff), transaction support rebuilds fingerprints via the
   projection (`transaction_test_support.h:113,157`).
3. **Cross-module fingerprint contract explicit: CLOSED.**
   `topology.h:21-23` and `display_types.h:192` carry matching AGENT-CONTRACT
   markers; `validSnapshot`'s AGENT-GUARD
   (`transaction_machine.cpp:67-69`) states the lineage-fence rationale. The
   reference page still owes the byte-level prose (lead-scheduled).
4. **Mirror canonicalization: no false overflow, no derived-field fingerprint
   variance, mode/transform deliberate: CLOSED.**
   `topology_validation.cpp` orders phases lineage→scan→mirror-graph→
   `canonicalizeMirrors` (224-250)→`normalizePositions` (252-270)→
   `buildGeometries` (272-330); replicas never get an intermediate
   target-derived rect (292-294 `continue`; geometry copied from the root at
   309-328), eliminating the false `CoordinateOverflow` path; only
   position/scale are erased as derived (246-247, AGENT-CONTRACT 242-245)
   while mode/transform stay caller/live per-output. Tests:
   `tst_topology_candidate.cpp:65-86` proves fingerprint invariance under
   divergent replica position/scale and variance under a deliberate transform
   change. Positive note: `geometrySetHasGap` now iterates a snapshot of the
   reached set (`topology_validation.cpp:90`), fixing a latent QSet
   iterator-invalidation mutation during traversal that I had not caught.
5. **Unicode format/control fail-closed: CLOSED.** `safeText` rejects
   `Other_Control` and `Other_Format` (`display_validation.cpp:26-35`) and
   `isBoundedText` now includes it (106-110), so every bounded protocol string
   fails closed; identity resolver and registry use the same rule
   (`identity_resolver.cpp:31-36`, `identity_registry.cpp:35-41`). Hostile
   rows exist: `tst_display_protocol_values.cpp:178`, 
   `tst_identity_registry.cpp:88` (U+202E).
6. **Port semantics vs machine behavior: CLOSED.**
   `transaction_ports.h:14-16,24-34` now state nondecreasing clock values,
   borrowed/addressable-for-life port, constructing-thread-only calls, no
   synchronous re-entry, no argument retention, synchronous atomic journal
   booleans, zero-or-one exact-token callback, late-callback rejection,
   disconnect via `TransportUncertain`/silence rather than port swap, and
   no-replay-after-timeout. Each clause matches the implementation I traced:
   token fencing (`transaction_machine_events.cpp:21-23`), deadline progress
   for every deadline-bearing state (269-303), and atomic gate usage
   (`preview` 201-203; `clearJournal` failure → `Stuck` paths).
7. **Repeated topology churn: CLOSED.** `topologyChanged` re-arms
   `SettlingTopology` from any active state with zero deadline and cleared
   survivors (`transaction_machine_events.cpp:191-201`); only the explicit
   latest `topologySettled` progresses (204-225) and the subsequent revert can
   only emit `SurvivingOutputProperties` (non-empty survivors) or no apply at
   all (empty survivors, 215-222; `makeRevertRequest`
   `transaction_machine_revert.cpp:90-105`). Tests:
   `tst_transaction_recovery.cpp:175-210` (churn, no mutation before settle,
   latest settle wins, properties-only scope) and 128-173 (empty candidate +
   exact surviving property set).

## Regressions

**None found** within the repair scope. Two deliberate-behavior notes, not
defects, for the reference page: (a) the `validSnapshot` fixpoint rule means
the D2 adapter must publish origin-normalized, canonical-disabled,
derived-erased snapshots — any raw compositor layout whose enabled
non-replicated minimum is not `(0,0)` is rejected at `initialize`, and an
overlapping live layout is likewise unacceptable (overlap remains a hard
`TopologyError`); (b) `tst_topology_geometry.cpp:29-33` correctly pins
1440p@150% → 1707×960 with `integral=false`, preserving the non-integral
warning truth.

## Wrong-contract test assertions

**None found.** I specifically checked the three riskiest spots and each
asserts the correct contract: `stageFencesRevisionAndDetectsNoOp` expects
`CommandError::NoOp` with unchanged view and zero requests
(`tst_transaction_state.cpp:35-40`); the surviving-properties test expects
the preimage scale 1.0, not the previewed 1.25
(`tst_transaction_recovery.cpp:158-159`); codec tests assert
destination-equality-after-failure after every hostile row
(`tst_display_protocol_codec.cpp:84-110,146-152`).

## Residual evidence boundaries (not findings)

- `hasSignature` relies on `QDBusArgument::currentSignature()`
  (`display_dbus.cpp:15-18`); the three expected signatures at 71, 86-88,
  104 match the wire operators exactly (I verified element-by-element, e.g.
  Snapshot `(ustaya(ssssssiibbbbbsiiiiduusa(suustttu))...)` decomposes to the
  22-field Output + `a(siiub)` Mode array + `suustttu` TransactionSummary).
  Actual compilation and demarshalling behavior remain compiler-lane
  evidence, which I cannot produce.
- Whole-message QtDBus CPU/amplification stays deferred-documented per your
  triage; `readBoundedArray` (`display_dbus.cpp:30-46`) is unchanged.
- The strict snap QSet-copy in `geometrySetHasGap` and the gap-warning
  semantics are unchanged in meaning.

## Process observation

`ops/team/workers/kai-mercer.md` currently sits inside the feature worktree
(shows as untracked in `git status`). Worker records belong under the central
board root; relocating it is the lead's/Kai's call — noted only so it does not
accidentally enter the D1 candidate commit.

Requested action: none required from me; items 1-7 are closed at source+test
level pending your compiler lane, docs/ADRs, and the immutable candidate
review. No acceptance is claimed.
