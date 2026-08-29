# Dorian Vale exact descendant verdict: Display D1 `aa3a4f4d` — PASS

- **Timestamp:** 2026-08-28T06:02:04Z
- **Reviewer:** Dorian Vale, independent architecture reviewer
- **Exact descendant:** `aa3a4f4d7fcd636eabc008e686f9b1c1c8ebd2de`
- **Exact tree:** `09f512e420e74794d8ecb59e04360e1fded5b799`
- **Exact parent/failed candidate:** `0a8d0e0eac9e0d7c5932fb54b875667b5d7f1639`
- **Subject:** `Fence Display snapshots at equal revisions`
- **Verdict:** **PASS**
- **Open findings:** P0/P1/P2/P3 = **0/0/0/0**

I reviewed the exact handoff
`1787896705-kellan-ward-d1-same-revision-repair-candidate-handoff.md`, then
verified the immutable commit directly. Commit/tree/parent/subject match;
scope is exactly four owned paths and `+86/-17`; `git diff --check` and the
candidate source status are clean apart from the known excluded `.omc/` and
Kai record.

## Reproduced P1 is closed

The descendant's `followsCurrentLineage` now requires same epoch and either a
strictly newer revision or complete snapshot equality at an equal revision
(`src/services/display_transaction/src/transaction_machine_events.cpp:14-23`).
That preserves the required exact unchanged redelivery while rejecting changed
truth that reuses the current fence. Architecture and protocol reference say
the same rule, and the single table-driven test covers all three Ready entry
points with state and side-effect preservation.

I rebuilt and reran my original ignored-build counterexample against the
descendant's strict-Debug libraries. Exact output for each of
`observedSnapshot`, `externalIntentObserved`, and `topologyChanged` was:

- unchanged same-revision snapshot accepted with `stateChanged=0`;
- changed same-revision snapshot rejected with `InvalidSnapshot`,
  `stateChanged=0`;
- strictly newer changed snapshot accepted at revision 3; and
- candidate projected from revision-2 pre-change truth rejected with
  `StaleRevision`, leaving revision 3 current.

The probe compiled and exited 0. One earlier reviewer command attempted to link
an identity library that this focused build had not produced and stopped at
link time; removing that unused library from the probe command produced the
successful run above. No product test or candidate defect was involved.

## Proportionate independent gates

| Gate | Result |
| --- | --- |
| Strict Debug complete transaction suite | 5/5 passed, exit 0 |
| Strict Release complete transaction suite | 5/5 passed, exit 0 |
| ASan+UBSan complete transaction suite | 5/5 passed, exit 0 |
| Independent three-entry adversarial probe | all expected transitions, exit 0 |
| Documentation/navigation validation | 51 documents, exit 0 |
| Exact descendant diff check | exit 0 |

The four-path diff makes no unrelated topology, identity, protocol, package,
or runtime change, so the prior exact review's successful mirror/recovery/
header/package evidence remains applicable. I found no regression or remaining
P0–P3 issue. This exact non-amended descendant closes my only P1 and is
acceptable for manager integration as the tip of the preserved D1 series.

No display, Wayland/XWayland, D-Bus/session, GUI, input, hardware, host service,
or nested runtime ran; those remain later D0/D2/D8 boundaries.

