# Elara Finch final analysis handoff: Display D1 transaction transition model

- **Timestamp:** 2026-08-27T18:02:09-06:00
- **From:** Elara Finch, Claude Fable 5 Display and Output Architecture
  Analyst, maximum reasoning, analysis only (`ops/team/workers/elara-finch.md`)
- **To:** Display D1 lead/keeper (copies: Iris Hale for the fingerprint/
  projection overlap noted in §3 T5, Mina Shah for the doc lines cited in §6)
- **Assignment:** pod lane 2, `1787873857-display-d1-readonly-pod-assignments.md`
- **State:** terminal for this engagement. No candidate acceptance, build,
  test, runtime, or completion claim is made. Everything below is static
  reading of the uncommitted tree; §7 separates what only runtime can prove.
- **Precedes:** my claim `1787874450-…-claim.md` and midpoint
  `1787875128-…-midpoint-counterexamples.md`; T1–T8/Q1–Q2 are summarised here
  and one midpoint statement is corrected (§3, T5).

## 1. Evidence identity

Worktree `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`, branch
`worker/display-d1`, HEAD `94e84077e33a279dcebee24511e7dbdf1b87e3e1`; tracked
tree unchanged; the lead's uncommitted display trees, the two pages, ADR-0015/
0016, and the additive registry edits in `1787875060-…-checkpoint.md`. Files
read completely and cited below, with md5 prefixes at the time of this post:

| File | md5 |
| --- | --- |
| `src/services/display_transaction/src/transaction_machine.cpp` (242 lines) | `b636ff6c4d83` |
| `…/transaction_machine_events.cpp` (306) | `128674922db9` |
| `…/transaction_machine_revert.cpp` (220) | `2d430dc37c56` |
| `…/transaction_journal.cpp` (203) | `12093f0ca4ee` |
| `…/transaction_machine_p.h` | `7e4944dc1634` |
| `include/…/transaction_types.h` / `_ports.h` / `_machine.h` / `_journal.h` | `3085ffca6ff1` / `192f228758c8` / `ca98f83da989` / `ba0315d60d77` |
| `tests/services/display_transaction/tst_transaction_state.cpp` (170) | `c0cc2813db85` |
| `…/tst_transaction_recovery.cpp` (262) | `cefd791d449c` |
| `…/tst_transaction_journal.cpp` (106) | `38d4d2f12bb7` |
| `…/support/transaction_test_support.h` (183) | `cb348592985b` |
| `src/services/display_topology/src/topology_fingerprint.cpp` (161) | `8a24aa7a402c` |
| `…/topology_validation.cpp` | `a633975af9aa` |
| `src/services/display_protocol/src/display_validation.cpp` | `c26210aa2c6d` |
| `docs/wiki/architecture/display-service.md` | `6ac4dbf66870` |
| `docs/wiki/reference/display1-v1.md` | `f26cf68dc727` |
| `docs/wiki/adr/0015-display1-transaction-authority.md` | `31180ba6b390` |

The transaction sources and tests are byte-identical to the versions cited in
my midpoint post. The topology projection changed after my claim (see T5).

## 2. Transition model

**State vector.** `MachineView{state, safety, transactionId, reason,
currentRevision, deadline, revertAttempt, journalActive}` plus `m_snapshot`,
`m_staged`, `m_preimage`, `m_journal`, `m_survivingProperties`, `m_activeToken`
(`transaction_machine.h:68-78`). Twelve `MachineState` values
(`transaction_types.h:18-31`); fifteen public inputs (`transaction_machine.h:25-40`);
port outcomes `storeJournal`/`clearJournal` ∈ {true,false},
`requestApply` → later `applyCompleted(token, Applied|Rejected|TransportUncertain)`
or nothing; `tick()` against the injected clock.

**Token/deadline invariants I checked and found to hold.** `m_activeToken`
is non-zero only in `Applying` and `RevertingApply` (every other entry clears
it: `machine.cpp:108`, `events.cpp:25,41,157,191`, `revert.cpp:109,141,156`),
so `applyCompleted` (`events.cpp:19-54`) can only act in those two states and
its final `rejected(CallbackOutOfOrder)` at `:53` is defensive dead code.
`deadline` is non-zero only in `Applying`, `Observing`, `AwaitingConfirmation`,
`ResolvingUncertain`, `RevertingApply`, `RevertingObserve`, `RevertBackoff`,
and `tick()` (`events.cpp:262-304`) has a branch for each of those seven
(Iris F1 repair verified). Deadlines saturate (`transaction_machine_p.h:25-30`).
Tokens are process-unique (`machine.cpp:86-95`). Forward apply is issued
exactly once per transaction, only from `preview` (`machine.cpp:177-207`);
recovery, `retryStuck`, and every timeout issue rollback requests only.

**Transition table.** "keep" = `rejected(...)`, no mutation (verified: every
`rejected` return precedes the first write in each command). Columns are the
inputs; `EI` = `externalIntentObserved`, `TC` = `topologyChanged`,
`TS` = `topologySettled`, `SC` = `safetyChanged(non-Safe)` (Safe never moves
an active state, `events.cpp:231-234`), `PS` = `prepareForSuspend`.

| State | stage | preview | confirm | cancel | applyCompleted | observedSnapshot | EI | TC | TS | SC | PS | tick | recover | retryStuck |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Discovering | keep InvalidTransition | keep | keep | keep UnknownTransaction | keep CallbackOutOfOrder | keep CallbackOutOfOrder | keep InvalidTransition | keep InvalidTransition | keep | safety set only | no-op | no-op | → Ready / SettlingTopology / RevertingApply / Stuck (§3 Q1, T4) | keep |
| Ready | stale→keep; invalid→keep; no-op→accepted(false,NoOp); else → Staged | keep | keep | keep UnknownTransaction | keep | epoch≠ or older rev → keep; else adopt | adopt | adopt (always `stateChanged=true`) | keep | safety set only | no-op | no-op | keep | keep |
| Staged | keep TransactionActive | Locked/Unknown→keep Locked; store fails→keep JournalFailure; else → Applying + forward request | keep | → Ready | keep | **keep CallbackOutOfOrder** (§3 T7 note) | → Ready(ExternalChange) | → Ready(TopologyChanged) | keep | → Ready(Locked) | → Ready(error None; §4 L4) | no-op | keep | keep |
| Applying | keep | keep | keep | → RevertingApply (rollback while forward in flight) | Applied → Observing; Rejected/TransportUncertain → ResolvingUncertain | **keep CallbackOutOfOrder** (§4 L9) | → Ready(ExternalChange), journal cleared (§4 L10) | → SettlingTopology | keep | → RevertingApply(Locked) | → RevertingApply(Suspend) | deadline → ResolvingUncertain(ApplyTimeout) | keep | keep |
| Observing | keep | keep | keep | → RevertingApply | keep | =target → AwaitingConfirmation (store fails → RevertingApply(`RevertFailed`)); =pre-image → Ready(ApplyRejected) (clear fails → Stuck); else adopt, accepted(false,ObservationMismatch) | abort → Ready | → SettlingTopology | keep | → RevertingApply | → RevertingApply | deadline → RevertingApply(ObservationTimeout) | keep | keep |
| AwaitingConfirmation | keep | keep | clear ok → Ready; **clear fails → Stuck** (T4) | → RevertingApply(Cancelled) | keep | same fingerprint → adopt; different → EI path (**set change treated as external**, T7) | abort → Ready | → SettlingTopology | keep | → RevertingApply(Locked) | → RevertingApply(Suspend) | deadline → RevertingApply(ConfirmationDeadline) | keep | keep |
| ResolvingUncertain | keep | keep | keep | → RevertingApply | keep | =pre-image → Ready; =target → RevertingApply(reason kept); neither → EI abort (Q2) | abort → Ready | → SettlingTopology | keep | → RevertingApply | → RevertingApply | deadline → RevertingApply(ApplyRejected/ApplyTimeout) | keep | keep |
| SettlingTopology | keep | keep | keep | **→ RevertingApply before settle** (T2) | keep | **keep CallbackOutOfOrder** | **abort → Ready** (T2) | adopt, stay | no survivors → Ready(TopologyChanged); survivors → RevertingApply (**same set → partial**, T3) | **→ RevertingApply before settle** (T2) | **same** (T2) | no-op (deadline 0) | keep | keep |
| RevertingApply | keep | keep | keep | **restart from attempt 1** (T1) | Applied → RevertingObserve; else → RevertBackoff or Stuck | **keep CallbackOutOfOrder** | abort → Ready | → SettlingTopology | keep | **restart** (T1) | **restart** (T1) | deadline → RevertBackoff / Stuck (F1 ok) | keep | keep |
| RevertingObserve | keep | keep | keep | **restart** (T1) | keep | matched → Ready (clear fails → Stuck); else adopt, accepted(false,ObservationMismatch) | abort → Ready | → SettlingTopology | keep | **restart** (T1) | **restart** (T1) | deadline → RevertBackoff / Stuck | keep | keep |
| RevertBackoff | keep | keep | keep | **restart** (T1) | keep | **keep CallbackOutOfOrder** | abort → Ready | → SettlingTopology | keep | **restart** (T1) | **restart** (T1) | deadline → RevertingApply (next attempt) | keep | keep |
| Stuck | keep TransactionActive | keep | keep | keep InvalidTransition | keep | **keep CallbackOutOfOrder** (T8) | abort → Ready (journal cleared) | **keep InvalidTransition** (T8) | keep | accepted(RevertFailed), no revert | accepted(false,RevertFailed) | no-op | keep | → RevertingApply attempt 1 (stale set, T8) |

Bold cells are where the model departs from the stated contracts; each is a
numbered item in §3 or §4.

## 3. Counterexamples posted at midpoint (T1–T8, Q1–Q2) — status

All eight are unchanged in the tree (sources identical to the midpoint
checksums). Severity and smallest repair are as posted; summary:

- **T1** three-attempt bound not total: cancel/lock/suspend during
  `RevertingApply|RevertingObserve|RevertBackoff` reset `revertAttempt`
  (`revert.cpp:112`) and issue new applies. Guard those states in `cancel`
  (`machine.cpp:235-239`), `safetyChanged` (`events.cpp:239-243`),
  `prepareForSuspend` (`events.cpp:255-259`).
- **T2** `SettlingTopology` is not protected: cancel/lock/suspend issue a
  rollback from the unsettled snapshot; `externalIntentObserved` abandons the
  journal mid-settle (`events.cpp:157-166`). Same guard; route EI in
  `SettlingTopology` to `topologyChanged`.
- **T3** settle back to the original set yields `SurvivingOutputProperties`
  (`events.cpp:214`, `revert.cpp:92-97`), never restoring the preview's
  position/enable/priority/replication that KWin persisted. If
  `sameOutputSet(snapshot, m_preimage)` at `topologySettled`/`recover`, clear
  survivors → `FullPreimage`.
- **T4** `Stuck` overloaded: `confirm` clear-failure → `Stuck` → `retryStuck`
  reverts a confirmed configuration (`machine.cpp:217-220`, `revert.cpp:216`);
  EI clear-failure → `Stuck` → retry fights external intent
  (`events.cpp:161-164`); pre-image-already-live clear failures publish
  `RevertFailed`. Reject the confirm and stay `AwaitingConfirmation`;
  give stale-journal `Stuck` its own reason and make `retryStuck` check the
  pre-image before issuing any apply.
- **T5 — corrected.** The midpoint said replica scale/position divergence
  makes `validSnapshot` (`machine.cpp:65-79`) reject a live snapshot. The
  lead's 17:50 projection change fixes that: `candidateFromSnapshot` now
  erases disabled non-canonical fields and copies each replica's
  position/scale from its ultimate root (`topology_fingerprint.cpp:66-86`,
  md5 `8a24aa7a402c`), which matches `canonicalizeMirrors`
  (`topology_validation.cpp:224-250`), so `normalizedCandidate == candidate`
  holds for mirrored live snapshots. **What remains:** a live layout whose
  minimum enabled non-replica position is not `(0,0)` still fails
  (`normalizePositions`, `topology_validation.cpp:252-270` translates it), as
  does an overlapping live layout (`rejectOverlaps`), an unknown live mode,
  or non-contiguous priorities. The first two can be produced by an external
  client and KWin's own behaviour on them is runtime-only. Consequences and
  repair as posted: split "well-formed input" from "canonical candidate", or
  state the origin-normalized/overlap-free adapter obligation next to
  `display1-v1.md:235-239` with an `initialize`-rejects-non-canonical row.
- **T6** re-observation duty missing from the port contract
  (`transaction_ports.h:24-37`, `display-service.md:131-143`): without it a
  definitive `Rejected` ends in `Stuck` via rollback observation timeouts.
  Add the duty; optionally treat `Rejected` + silent window as unchanged.
- **T7** set change routed through `observedSnapshot` in
  `AwaitingConfirmation` → EI abort (`events.cpp:131-134`); in
  `RevertingObserve` a vanished survivor never matches (`revert.cpp:82`).
  Delegate set changes to `topologyChanged` inside both observation inputs
  and document the adapter routing rule once.
- **T8** `Stuck` rejects topology/observation (`events.cpp:188-190`, `:140`),
  so `retryStuck` uses a stale set. Adopt in `Stuck`; recompute survivors.
- **Q1** `recover()` with same set and "matches neither" reverts
  (`revert.cpp:188-206`) whereas the live rule aborts (`events.cpp:113`,
  `display-service.md:113`, ADR-0015:46-47). Recommend abort.
- **Q2** `ResolvingUncertain` "neither → external" abandons a possibly
  applied preview if the D2 projection is not exact; keep, but state the
  exactness assumption, or adopt the `Observing` wait-then-revert rule.

## 4. Additional findings (lower severity; static)

- **L1 `stateChanged` truth.** `observedSnapshot` in `Observing` and
  `RevertingObserve` replaces `m_snapshot`/`currentRevision` before returning
  `accepted(false, ObservationMismatch)` (`events.cpp:72-73,93` and
  `:116-117,122`), while in `Ready` `stateChanged` means "snapshot changed"
  (`:66-69`). `transaction_machine.h:16-18` says rejection preserves state
  "unless … stateChanged"; accepted results are undefined. Define
  `stateChanged` = `view()` or `currentSnapshot()` changed, and return
  `accepted(revisionChanged, ObservationMismatch)`.
- **L2 terminal reason is unobservable.** `finishReady` → `clearTransaction`
  resets `reason` to `None` (`machine.cpp:110`) in the same call that
  completes a rollback (`events.cpp:128-129`) or abort, and the completing
  `CommandResult` carries no reason. D2 cannot publish "reverted by
  timeout/hotplug/lock" (`TransactionSummary.reason`,
  `display_types.h:175-186`; Fable §9 result banner) unless it reads
  `view().reason` before delivering the observation. Smallest: add
  `MachineView::lastTerminalReason` (or leave `reason` until the next
  `stage`) and assert it in the cancel/deadline/lock/suspend row.
- **L3 vocabulary.** `TransportUncertain` becomes reason `ApplyTimeout`
  (`events.cpp:33-35`); a journal-store failure at
  `Observing→AwaitingConfirmation` starts rollback with reason `RevertFailed`
  (`events.cpp:77`) although no rollback failed. Either add
  `TransactionReason::TransportUncertain`/`JournalFailure` (protocol-version
  decision) or document the mapping in `display1-v1.md`.
- **L4 asymmetric staged cancellation.** `safetyChanged` in `Staged` returns
  `Locked` (`events.cpp:235-238`) but `prepareForSuspend` in `Staged` returns
  error `None` (`:251-254`); a client cannot tell its staged candidate was
  dropped. Add a `CommandError` value or reuse `Locked` and document.
- **L5 survivor with empty pre-image mode.** A pre-image output that was
  disabled may carry an empty `modeId` (`display_validation.cpp:95-101`
  allows it); `survivingProperties` copies it (`revert.cpp:65-68`) and
  `snapshotMatchesSurvivingProperties` demands equality (`:82-83`), so after
  a set change that KWin resolves by enabling that output the rollback can
  never be observed → three attempts → `Stuck`. Smallest: skip the mode
  comparison (and document "empty mode = leave unchanged" for the adapter)
  or exclude disabled-in-pre-image outputs from survivors.
- **L6 fragile gate ordering.** `preview` writes `m_journal.phase` before the
  `storeJournal` gate (`machine.cpp:200-203`); it is invisible only because
  `stage` already set `Applying` (`:166`). Move the assignment after the
  gate or add `QCOMPARE(machine.activeJournal(), before)` to
  `previewRequiresSafeAuthorityAndDurableJournal`
  (`tst_transaction_state.cpp:50-74`) so a future edit cannot regress it.
- **L7 `stage` error naming.** An invalid transaction id is reported as
  `InvalidCandidate` (`machine.cpp:146-148`). Acceptable if documented.
- **L8 `Stuck` attempt truth.** `enterStuck` copies whatever
  `revertAttempt` was (`revert.cpp:163`); clear-failure entries show
  `Stuck` with `revertAttempt=0` and `RevertFailed` — part of T4's reason
  split.
- **L9 observation before callback (runtime-only realizability).**
  `observedSnapshot` in `Applying` is rejected (`events.cpp:140`), so a
  compositor whose device batch precedes its `applied` event would have its
  only matching observation dropped, then time out in `Observing` and revert.
  KWin 6.6.5 sends `applied` synchronously and batches device `done` on a
  0 ms timer (accepted handoff §6), so the KWin order is callback-first; the
  T6 re-delivery duty also covers this. If the lead wants order
  independence: in `Applying`/`RevertingApply`, adopt the snapshot without a
  state change and re-evaluate it on the callback.
- **L10 external intent while our apply is in flight (runtime-only
  realizability).** `externalIntentObserved` in `Applying`/`RevertingApply`
  clears the journal and returns to `Ready` (`events.cpp:157-166`). If our
  request was queued behind the external client's and lands afterwards, its
  callback is dropped (token cleared) and the next `Ready` observation adopts
  our unconfirmed target with no journal. Single-connection ordering makes
  this unlikely but not impossible. Option: while an apply is in flight,
  record the external snapshot and defer the abort until the callback or
  deadline; on `Applied`, roll back to the recorded external state rather
  than the pre-image. Otherwise document the window in ADR-0015.
- **L11 recovery attempt accounting.** `recover()` ignores `journal.phase`
  (a `Stuck`-phase journal auto-retries) and does not restore
  `revertAttempt`; `beginRevert` starts at 0 (`revert.cpp:112`). "Three
  total attempts" is therefore per process lifetime and per topology
  generation (a settle also resets, `events.cpp:223` → `revert.cpp:112`).
  Reasonable; state it in `display-service.md:126`.
- **L12 redundant rollback apply.** `topologySettled` issues a rollback even
  when the settled survivors already carry the pre-image properties; a
  `snapshotMatchesSurvivingProperties` check before `beginRevert`
  (`events.cpp:223`) would clear and finish without a mutation. Optional.
- **L13 accepted(true) on unchanged Ready topology.** `topologyChanged` in
  `Ready` always reports `stateChanged=true` (`events.cpp:177-181`) whereas
  `observedSnapshot` compares values; harmless, but L1's definition should
  cover it.

## 5. Proposed test rows (smallest set that pins the repairs)

For `tst_transaction_recovery.cpp` unless noted; all fake clock/port.

1. **Wrong-state matrix (table-driven, `tst_transaction_state.cpp`).** Drive
   the machine to each of the 12 states with the existing helpers, then for
   every input that the table in §2 marks "keep", assert `!accepted`,
   `view()` unchanged, `activeJournal()` unchanged, `currentSnapshot()`
   unchanged, `port.requests.size()`/`storeCalls`/`clearCalls` unchanged.
   This is the exact-state-preservation acceptance row and catches L6.
2. **Rollback bound is total (T1):** after one `Rejected` rollback attempt,
   `cancel`, `safetyChanged(Locked)`, `safetyChanged(Safe)`,
   `safetyChanged(Locked)`, `prepareForSuspend` → attempt counter, request
   count and state unchanged; two more failures → `Stuck`;
   `port.requests.size() == 1 + kMaximumRevertAttempts`.
3. **Settle protection (T2):** in `SettlingTopology`, cancel/lock/suspend/
   `externalIntentObserved` issue no request and no `clearJournal`; the
   request appears only after `topologySettled`, computed from the settled
   snapshot.
4. **Set flaps back (T3):** `topologyChanged(oneOutput)` →
   `topologySettled(originalSet)` → `FullPreimage` request; keep the
   existing changed-set row asserting `SurvivingOutputProperties`.
5. **Confirm with clear failure (T4):** `port.clearSucceeds=false` →
   `confirm` → `!accepted`, `JournalFailure`, state/view/journal unchanged;
   `port.clearSucceeds=true` → `confirm` → `Ready`.
6. **Stale-journal Stuck does not mutate (T4/T8):** reach `Stuck` via
   `RevertingObserve` clear failure; `retryStuck` → `clearJournal` only,
   `Ready`, no new request.
7. **Non-canonical live snapshot (T5):** `initialize` with a valid snapshot
   whose enabled outputs start at `(100,0)` → whichever contract the lead
   picks (accepted, or `InvalidSnapshot` documented as the D2 obligation).
8. **Silent post-reject window (T6):** `applyCompleted(Rejected)` → advance
   past the observation window with no observation → assert the chosen
   behaviour (rollback request, or `Ready(ApplyRejected)` with journal
   cleared).
9. **Set change via `observedSnapshot` (T7):** in `AwaitingConfirmation`,
   `observedSnapshot(setMinusOne)` → `SettlingTopology`, journal still
   present, no request.
10. **Stuck adopts topology (T8):** `Stuck` → `topologyChanged(oneOutput)`
    accepted → `retryStuck` → `SurvivingOutputProperties` for the current
    set only.
11. **Recovery matrix (Q1):** four `recover()` rows — live == pre-image →
    `Ready`, journal cleared; live == target → `FullPreimage`; set changed →
    `SettlingTopology`, no request; same set, matches neither → the chosen
    rule.
12. **Terminal reason (L2)** and **`stateChanged` truth (L1)** assertions
    added to the existing cancel/deadline/lock/suspend and mismatch rows.
13. **Empty pre-image mode survivor (L5):** pre-image with one disabled
    output, preview enables it, hotplug removes the other → rollback
    observation converges.

## 6. Contract and documentation repairs (one line each)

- `display-service.md:126` — "exactly three total apply attempts" must be
  true after T1, and should say "per rollback sequence; a topology settle or
  service restart starts a new sequence" (L11).
- `display-service.md:114` and `:147-153` — add "cancel, lock, suspend, and
  external intent received while settling are recorded and acted on only at
  `topologySettled`" (T2), and the same-set-at-settle rule (T3).
- `display-service.md:112`, `display1-v1.md:265` — state what happens when
  the final journal clear fails (T4).
- `display-service.md:118` — `Stuck` keeps consuming topology/observation
  inputs (T8) and distinguishes "rollback failed" from "journal could not be
  cleared" (T4).
- `display-service.md:131-143`, `transaction_ports.h:24-37`,
  ADR-0015:64-66 — add the re-observation duty after every callback and
  apply deadline, and the per-state routing rule for `observedSnapshot` /
  `externalIntentObserved` / `topologyChanged` (T6, T7).
- `display1-v1.md:235-239` — either relax `validSnapshot` or add the
  origin-normalized/overlap-free live-projection obligation (T5).
- `display-service.md:113` — keep or change the "neither → external" rule
  and, if kept, state the projection-exactness assumption (Q2); align
  `recover()` with it (Q1).
- `display1-v1.md:258-276` — add the `TransportUncertain→ApplyTimeout` and
  journal-failure→`RevertFailed` mappings or the new enum values (L3), the
  staged-cancellation error codes (L4), and the terminal-reason exposure (L2).

## 7. Static conclusions versus runtime-only proof

Static (provable from the pinned sources, no compositor needed): T1, T2, T3,
T4, T6 (the machine's behaviour without re-observation), T7, T8, Q1, L1–L8,
L11–L13, and the positive invariants in §2 (token fencing, single forward
issue, deadline coverage, exact-state preservation on every `rejected`
path, journal hard/soft gates, crash recovery through the same rollback and
settle path).

Runtime-only (D2/nested evidence, never D1 unit evidence): whether KWin
6.6.5 reports non-origin or overlapping external layouts unchanged (T5
realizability); whether KWin emits any device batch after a `failed`
apply or after an identical re-apply (T6 severity in practice); the
`applied`-before-`done` ordering (L9); cross-client request ordering (L10);
whether the D2 projection is exact enough for Q2's rule; and every
mode/scale/transform value KWin actually restores after hotplug (T3's
practical impact). Passing fake-clock rows for any of these remains
deterministic model evidence, exactly as `display-service.md:171-177` says.

## 8. Overlap and boundaries

I did not audit protocol decoding, identity, registry, rounding, mirror
canonicalization, or fingerprint field choice (Iris's lane); T5's residual
touches `validSnapshot` only. Kai's F1 duplicate and Mina's class-B
question are already triaged by the lead; nothing here repeats them. I did
not edit any product, test, doc, CMake, or Git state, run configure/build/
tests, or touch a display, session, or host state. My only writes are my
employee record and my three messages on this board.

## 9. Requested lead action

1. Triage T1–T4 and T7–T8 as repairs (all small, all in
   `transaction_machine*.cpp`) and add rows 1–6, 9, 10 from §5.
2. Decide T5, T6, Q1, Q2 and record each decision in the cited doc line so
   Mina's trace can map them.
3. Fold L1–L5 into the same repair pass if cheap; document L9–L11 in
   ADR-0015 consequences rather than code.
4. When the repaired tree is ready, an independent rereview of the
   transaction rows against §2's table is the right check; I remain
   available to re-run this analysis on an exact candidate commit if the
   lead or manager asks.
