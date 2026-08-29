# Iris Hale consolidated source-repair verdict: Display D1 repair diff over `0e38fa72`

- **Timestamp:** 2026-08-27T22:04:30-06:00
- **From:** Iris Hale, Display D1 adversarial audit assistant
- **To:** Display D1 lead/keeper
- **State:** bounded read-only source checkpoint complete. This is a source
  verdict, not acceptance of an immutable repair commit; Elara Finch remains
  the exact candidate rereviewer. No compiler/test/runtime evidence exists or
  is claimed by me.
- **Evidence identity:** worktree
  `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`, HEAD exactly
  `0e38fa726af69e34be3cacdd6b71d40350ac8092` (unchanged, no amend) plus the
  uncommitted tracked repair diff — 15 files, +244/−26, matching your
  checkpoint `1787882078`. Inputs read: root `AGENTS.md`
  (`/home/cabewse/work_SPaC3/container-wm/AGENTS.md`), Elara's material
  finding `1787881270` and FAIL verdict `1787881375`, your checkpoint
  `1787882078`, the manager format directive `1787889721`, and the full
  `git diff` of all 15 paths. All line citations below are against the
  current uncommitted tree at inspection time.

## Verdict: the repair is correct and complete against Elara's P1/P2/P3 list. No regressions found. Nothing above Low.

### P1 — mirror projection double-translation: VERIFIED REPAIRED

`src/services/display_topology/src/topology_fingerprint.cpp`: root
position/scale resolution now runs untranslated (lines 92-108) and the origin
subtraction is a separate second pass over enabled outputs (112-117) with an
AGENT-GUARD stating the exact trap (109-111). Traced on Elara's
counterexample: `[A(100,50) root, B replica]` → min `(100,50)` computed before
any mutation (76-91), `B` copies untranslated `A` `(100,50)` (102-103), then
both translate once → `A=(0,0)`, `B=(0,0)`; order `[B, A]` gives the identical
result — order-independent, no negative positions, no double translation.
Projection and `validateAndNormalize` (which was already correct) now agree on
this input class. Fingerprint-order-independence and disabled/replica
canonicalization are unaffected.

Both required rows are present and match the specification:

1. `tests/services/display_topology/tst_topology_candidate.cpp:103-139`
   (`translatedMirrorProjectionIsOrderIndependent`): translated mirrored truth
   `(100,50)`/`(610,320)` in both output orders; asserts identical sorted
   projections, root/replica both at `(0,0)`, equal replica/root scale,
   identical canonical and live fingerprints, and accepted `noOp` baselines
   with equal fingerprints. Fails on `0e38fa72` (root-first would project
   `B=(-100,-50)`), passes with the second pass — correct failing-row
   semantics. Support overloads it uses pre-exist in the committed
   `topology_test_data.h` (11, 61-74).
2. `tests/services/display_transaction/tst_transaction_adversarial.cpp:133-173`
   (`translatedMirrorRollbackConvergesAfterOriginRestore`): full
   stage→preview→cancel flow over translated mirrored `[A,B]`; asserts the
   `FullPreimage` rollback carries both outputs at `(0,0)` with equal scale
   (154-160) — impossible on the failed candidate — then observes the
   correctly restored origin snapshot and requires `Ready` with no journal
   (161-172). Exactly the Elara-traced `Stuck` sequence, now converging.

### P2 ×2 — routing contracts: VERIFIED CLOSED

- Settling routing: `transaction_ports.h:43-46` prohibits delivering
  `externalIntentObserved` while settling and routes platform post-hotplug
  truth through `observedSnapshot`/`topologyChanged` until explicit settle;
  `docs/wiki/architecture/display-service.md:159-164` states the same for D2
  and warns against misclassifying KWin's own set restoration as client
  intent, and the settle section (`:178-180`) requires intent routing before
  the window.
- Staged routing: `transaction_ports.h:43-44` and the state table
  (`display-service.md:111`) require same-set external changes through
  `externalIntentObserved` while `Staged`, since ordinary observation is
  invalid there — closing the stale-candidate-preview hole.

### P3.1–P3.6: VERIFIED PRESENT AND COHERENT

1. `tst_display_protocol_codec.cpp:117-126` asserts the registered `Output`
   `(ssssssiibbbbbsiiiiduusa(siiub))` and `Snapshot`
   `(ustaya(...)a(suustttu))` signatures; both strings match the marshaller
   field-for-field (verified previously against `display_dbus.cpp`).
2. Both external-abort paths store an `ExternalChange` journal before
   clearing (`transaction_machine_events.cpp:227-233` deferred-in-settle,
   `:243-251` immediate, AGENT-GUARD 242-243); clear failure now lands in
   cleanup-only `Stuck` with the durable reason preserved via
   `enterStuck(true, ExternalChange)`
   (`transaction_machine_revert.cpp:176-181,193-197`, AGENT-CONTRACT 192-195).
   The new restart row (`tst_transaction_recovery.cpp:233-258`) proves the
   journaled abandon survives restart on a changed set, settles to `Ready`,
   and issues no apply.
3. All three `Ready` entry points now share `followsCurrentLineage`
   (`transaction_machine_events.cpp:11-17`; gates at :103, :213, :266).
   Regression sweep over every existing Ready-path call site: the only
   same-revision row (`tst_transaction_adversarial.cpp:492-493`) passes the
   `>=` gate by design; all other rows deliver newer revisions or hit
   active/`Stuck` states that are intentionally ungated. New dual rows
   (`tst_transaction_state.cpp:53-90`) prove older/other-epoch rejection with
   exact view/snapshot preservation and newer-truth acceptance for both
   entry points.
4. `Display::kMaximumRevertAttempts` now lives once in
   `display_limits.h:18`; `display_validation.cpp:216` and
   `transaction_types.h:15` (source-compatible alias) consume it. Value
   unchanged (3), no new dependency edge.
5. ADR-0015 records the suspend/apply-deadline vs logind delay-window reality
   and the resume-recovery permission (`0015-...md:80-83`).
6. The service page records the uncertain-callback/retained-pending-reason
   behavior (`display-service.md:143-146`).

## Regressions: none found

Beyond the lineage-gate sweep above: the diff does not touch the F1 tick
branches, `scheduleRevertRetry`/`issueRevertApply` budgets, canonical-snapshot
validation, `validateAndNormalize` mirror phases, format rejection, or the
churn/settle core — F1–F7 remain intact wherever previously verified; the
only protocol change is the P3.4 constant swap with identical semantics.
Source shape: `topology_fingerprint.cpp` is 193 lines, far under thresholds.

## Low notes (no action required; optional one-line doc each)

1. `candidateFromSnapshot` rewrites an enabled replica sourcing a *disabled*
   output to the disabled root's zeroed position (`topology_fingerprint.cpp:68-75`
   zeroes the root before :102 copies it), while `validateAndNormalize`
   rejects that relationship as `UnknownMirrorSource`
   (`topology_validation.cpp:194`): divergence is unreachable for
   machine-accepted snapshots because `validSnapshot` requires the projection
   to pass topology validation.
2. The origin fallback `minimum=(0,0)` when no enabled non-replica exists
   (`topology_fingerprint.cpp:76-78,83-86`) is unreachable in valid snapshots:
   every enabled replica chain terminates at an enabled non-replica root, and
   `primaryCount==1` forces an enabled output.

## Evidence limits and requested action

Static reading only; I executed no gate. The checkpoint's `git diff --check`,
shape sweep, and future compile/CTest/sanitizer/doc runs remain your
compiler-lane evidence; the two new P1 rows, the restart row, the lineage
rows, and the two new signature asserts are compile-gated. Requested action:
none from me — proceed to lane-assignment verification and the new immutable
commit for Elara's bounded rereview. I claim no candidate acceptance.
