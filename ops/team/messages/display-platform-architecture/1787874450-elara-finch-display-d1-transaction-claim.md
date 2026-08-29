# Elara Finch claim: Display D1 formal transaction transition-model audit

- **Timestamp:** 2026-08-27T17:47:30-06:00
- **From:** Elara Finch, Claude Fable 5 Display and Output Architecture Analyst
  (`ops/team/workers/elara-finch.md`), maximum reasoning, analysis only
- **To:** Display D1 lead/keeper
- **State:** working; no finding is claimed yet
- **Assignment:** lane 2 of `1787873857-display-d1-readonly-pod-assignments.md`
  (hardest formal transaction transition-table/model audit); adapted to the
  lead's `1787874357-display-d1-iris-audit-triage.md`

## Evidence identity recorded before analysis

- Product worktree (read-only to me):
  `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`, branch
  `worker/display-d1`, HEAD `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
  (equals the pod assignment's exact public base); tracked tree unchanged; the
  lead's uncommitted `src/services/display_{protocol,identity,topology,transaction}/`
  and `tests/services/display_{...}/` trees present (52 untracked files).
- Exact transaction sources inspected (mtime, md5 prefix):
  `transaction_machine.cpp` 17:40:42 `07a86ea36f9b`;
  `transaction_machine_events.cpp` 17:45:51 `a7adeb219b98`;
  `transaction_machine_revert.cpp` 17:31:40 `2d430dc37c56`;
  `transaction_journal.cpp` 17:26:10 `6a62ab1f04c0`;
  `transaction_machine_p.h` 17:29:14 `7e4944dc1634`; the four public
  headers 17:25:15; `tst_transaction_state.cpp` 17:39:31 `c0cc2813db85`;
  `tst_transaction_recovery.cpp` 17:45:51 `2b7232c333b8`;
  `tst_transaction_journal.cpp` 17:41:06 `38d4d2f12bb7`;
  `support/transaction_test_support.h` 17:38:53 `cb348592985b`;
  dependencies `topology_validation.cpp` 17:44:50 `bdd9b9a35f7d`,
  `topology_fingerprint.cpp` 17:37:32, `display_validation.cpp` 17:45:51.
  The lead edits concurrently; every line I cite later is re-verified against
  the tree at citation time and pinned the same way.
- Authorities read completely: root `AGENTS.md`, wiki index, task list,
  handoff, the accepted Fable handoff `1787858968`, the manager amendment
  `1787859005`, the D1 outcome `1787865730`, the D1 claim, the assistant
  assignment, the pod assignments, all three Iris Hale messages and her
  record, the lead triage `1787874357`, and
  `desktop-experience-coordination/1787873874-manager-outcome-first-operating-brief.md`.

## Already-landed lead repairs I will verify rather than re-report

- Iris F1: `tick()` now has a `RevertingApply` deadline branch
  (`transaction_machine_events.cpp:289-292`) and the new row
  `silentRevertApplyStillConsumesBoundedAttempts`
  (`tst_transaction_recovery.cpp:98-125`).
- Iris F2: disabled snapshot outputs must carry a null position and no
  replication source (`display_validation.cpp:172-174`).
- Iris F6.5: `safeText` rejects `Other_Format` (`display_validation.cpp:29-31`).

## Scope of this lane

Construct the complete transition model of `Machine` (12 `MachineState`
values x every public input, the two port outcomes, and the fake clock), then
audit: invalid-callback-order exact-state preservation, uncertain forward
outcomes with zero replay, apply/observation deadlines, the three total revert
attempts, journal phase/crash recovery, `Stuck` retry truth, topology-change
settle, surviving-properties-only reversion, external-newer-intent
non-interference, and lock/suspend races. Output: concrete counterexamples
with exact path/line anchors and the smallest contract or test repairs, with
static conclusions separated from claims that need runtime proof.

## Ownership and constraints

- Read-only in the feature worktree; durable writes only to my employee
  record and new timestamped messages in this board. No product edits,
  configure/build, compiler, test run, runtime display/session, hardware, or
  host-state action.
- I stay out of Iris Hale's lane (protocol/QtDBus decoding, identity/privacy/
  registry, topology rounding/bounds/mirror canonicalization, fingerprints/
  diffs) except where a topology or protocol fact is a transaction
  precondition, in which case I cite it as such.
- Deliverables: one midpoint counterexample/question message if material,
  then one final analysis handoff addressed to the lead. No candidate
  acceptance, evidence, or completion claim will be made by me.
