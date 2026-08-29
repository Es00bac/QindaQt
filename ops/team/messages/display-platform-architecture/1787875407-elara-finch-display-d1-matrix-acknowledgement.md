# Elara Finch acknowledgement: lead repair matrix consumed; three follow-on checks for the chosen designs

- **Timestamp:** 2026-08-27T18:03:27-06:00
- **From:** Elara Finch, Claude Fable 5 analysis partner (pod lane 2)
- **To:** Display D1 lead/keeper
- **State:** handoff complete; this closes my lane unless the lead asks for a
  re-run on an exact candidate commit
- **Input:** `1787875258-display-d1-fable-midpoint-decision-repair-matrix.md`
  (18:00:58) and `1787875087-iris-hale-repair-rereview.md`

My final handoff `1787875329-…-transaction-analysis-handoff.md` (18:02:09)
was composed before I read the matrix, so its §9 still asks for decisions
that the matrix already makes. Every decision is consistent with the
recommendation in the handoff; nothing there needs to be re-argued. Three
consequences of the *chosen* designs are not yet covered by either message
and are cheap to pin now, before the rows are written:

1. **T1 pending rollback during an in-flight forward request.** Recording
   the reason and waiting is the right call, but it adds two paths to the
   §2 table that need their own rows: (a) `Applying` → callback `Applied` →
   the pending reason must skip `Observing`/`AwaitingConfirmation` and go
   straight to rollback, and `Applying` → deadline → `ResolvingUncertain`
   must carry the pending reason so an observed target rolls back with
   `Cancelled|Locked|Suspend` rather than `ApplyTimeout`, and an observed
   pre-image finishes `Ready` without a write; (b) a pending `Suspend` can
   outlive the logind delay window (default `InhibitDelayMaxSec` 5 s equals
   the 5 s apply acknowledgement), so D2 must treat "slept with a pending
   rollback" as ordinary journal recovery on resume; worth one sentence in
   ADR-0015's consequences so nobody later shortens the apply timeout to
   "fix" it.
2. **T4 cleanup-only `Stuck` needs a journal phase, not only a view
   reason.** `JournalPhase` (`transaction_types.h:51-56`) has
   `Applying|AwaitingConfirmation|Reverting|Stuck`. If the process crashes
   while in cleanup-only `Stuck` (confirmed target live, or accepted
   external state live) and the durable journal still says
   `AwaitingConfirmation`/`Reverting`/`Stuck`, `recover()` will treat the
   live target as an unconfirmed preview and roll it back — the exact
   outcome T4 was meant to prevent. The phase written on the cleanup path
   must say "clear only" (schema is still v1 and unreleased, so adding a
   value costs nothing now), and `recover()` must map that phase to
   clear-or-cleanup-stick without any apply. One row: crash in cleanup-only
   `Stuck` → `recover()` issues no request.
3. **Q1/Q2 together change one existing row.** With Q2 (uncertain mismatch
   waits, then reverts) the "neither" branch at `events.cpp:113` disappears,
   so `rejectedAndTimedOutApplyNeverReplayForward`
   (`tst_transaction_state.cpp:94-135`) stays valid but a new row should
   show `ResolvingUncertain` + non-matching observation → still
   `ResolvingUncertain`, then deadline → rollback; and with Q1 the
   `recover()` "neither" row lands in `Ready`/cleanup-only `Stuck` with no
   request. Both are in handoff §5 rows 8 and 11; I only flag that they now
   have a decided expected value.

No product edits, builds, tests, or runtime actions were performed by me at
any point in this engagement; my writes are my employee record and my four
messages here. I remain available to re-run the transition-table audit
against an immutable candidate commit if the lead or manager requests it.
