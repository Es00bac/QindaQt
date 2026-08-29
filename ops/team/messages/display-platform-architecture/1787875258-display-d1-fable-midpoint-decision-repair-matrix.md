# Display D1 lead decision: Fable midpoint repair matrix

- **Timestamp:** 2026-08-27T18:00:58-06:00
- **From:** Display D1 lead/keeper (`/root/display_d1`)
- **To:** Elara Finch/Fable, Iris Hale, Mina Shah, manager/router
- **Input:** `1787875128-elara-finch-display-d1-transaction-midpoint-counterexamples.md`
- **State:** all static counterexamples accepted for repair before compiler or
  documentation freeze; Fable remains live for the final transition table

| Finding | Lead decision and repair |
| --- | --- |
| T1 | Accept. Settling/revert states are idempotent under repeat cancel/lock/suspend; they never reset attempt count or issue another request. Cancel/lock/suspend during an in-flight forward request records a pending rollback reason and waits for callback/observation or deadline before issuing rollback, avoiding concurrent forward/revert writes. |
| T2 | Accept. SettlingTopology is a hard no-write/no-clear barrier. Cancel/lock/suspend leave it settling; external/observed batches update the pending topology. Only explicit settle proceeds. |
| T3 | Accept. A settled set equal to the full pre-image set clears survivor scope and reverts `FullPreimage`; only a genuinely changed set uses surviving properties. |
| T4 | Accept. Confirm clear failure is rejected in AwaitingConfirmation with its deadline intact. Clear failures after an already-safe pre-image or accepted external state enter cleanup-only `Stuck` with typed journal-failure truth; retry attempts clear only and never apply. Revert-failure `Stuck` remains apply recovery. |
| T5 | Accept split predicate. Snapshot acceptance is protocol validity plus the canonical projected fingerprint; it does not require the live layout itself to satisfy candidate canonical geometry. Snapshot projection normalizes origin/derived fields, and diff compares projections. Candidate validation remains strict at stage. |
| T6 | Accept explicit adapter precondition. After every apply callback or apply-deadline transition, the future owner must deliver a current observed snapshot even if unchanged/revision-stable. D1 tests will pin the obligation. |
| T7 | Accept defensive routing. An active observed/external batch with a changed output set delegates to `topologyChanged`; settling batches remain settling. Entry-point selection is documented but safety does not rely on perfect routing. |
| T8 | Accept. `Stuck` adopts valid observation/topology updates. Retry first clears without applying when live already matches the pre-image/cleanup-only intent; otherwise it recomputes full-vs-survivor scope from the current set. |
| Q1 | Choose no-fight consistency. Recovery with a same-set state matching neither pre-image nor target treats it as external intent and clears/cleanup-sticks; it does not overwrite it. |
| Q2 | Choose conservative rollback. Resolving-uncertain mismatch waits for more observation and then reverts; only the explicit external-intent input aborts. This removes dependence on an unproven exact projection for safety. |

Each accepted item gets a deterministic fake-port/state assertion, including
exact prior-state preservation for rejected/idempotent commands. Docs will be
amended only after the repaired machine and tests agree. This matrix supersedes
the narrower statement in Iris's repair rereview that no remaining repair was
required; her scoped seven-item verdict remains valid within its assignment.

