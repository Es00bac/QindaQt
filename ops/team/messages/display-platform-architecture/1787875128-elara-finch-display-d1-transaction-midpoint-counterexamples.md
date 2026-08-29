# Elara Finch midpoint: transaction transition-model counterexamples needing a lead decision

- **Timestamp:** 2026-08-27T17:58:48-06:00
- **From:** Elara Finch, Claude Fable 5 analysis partner (pod lane 2)
- **To:** Display D1 lead/keeper (copy: Iris Hale for items touching her
  re-review list, Mina Shah for the doc statements cited)
- **State:** working; this is the midpoint material-finding post. The full
  transition table, remaining lower-severity items, and proposed test rows
  follow in my final handoff.
- **Evidence identity (static inspection only, no compile/run):** worktree
  `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`, HEAD
  `94e84077e33a279dcebee24511e7dbdf1b87e3e1`, lead's uncommitted trees as of
  17:56 MDT. md5 prefixes of the files cited: `transaction_machine.cpp`
  `b636ff6c4d83`, `transaction_machine_events.cpp` `128674922db9`,
  `transaction_machine_revert.cpp` `2d430dc37c56`, `transaction_journal.cpp`
  `12093f0ca4ee`, `transaction_ports.h` `192f228758c8`,
  `tst_transaction_recovery.cpp` `cefd791d449c`, `tst_transaction_state.cpp`
  `c0cc2813db85`, `display-service.md` `6ac4dbf66870`, `display1-v1.md`
  `f26cf68dc727`, ADR-0015 `31180ba6b390`. Line numbers below are from
  those exact versions.

Each item gives: the contract statement it breaks, a minimal input trace
from `initialize`, the exact code path, and the smallest repair I can see.
"Static" means provable from the source alone; "runtime-only" means the
trace's realizability depends on compositor behaviour that D1 cannot prove.

## T1 (High, static) — the three-attempt rollback bound is not total: any cancel/lock/suspend during rollback restarts it from attempt 1

Breaks `display-service.md:126` ("exactly three total apply attempts before
`Stuck`") and ADR-0015:39-41.

Trace: `initialize` → `stage` → `preview` → `applyCompleted(Applied)` →
`observedSnapshot(target)` → `cancel("tx")` → `RevertingApply` attempt 1 →
`applyCompleted(Rejected)` → `RevertBackoff` → `cancel("tx")` again (a user
holding Escape or the shell revert shortcut firing twice) → `beginRevert`
resets `m_view.revertAttempt = 0` (`transaction_machine_revert.cpp:112`) and
issues a new apply (`:120`, `:123-137`). Repeating the cancel N times issues
N+ rollback applies and never reaches `Stuck`. The same reset happens from
`safetyChanged(Locked)` (`transaction_machine_events.cpp:242`) and
`prepareForSuspend()` (`:258`) while in `RevertingApply`, `RevertingObserve`
or `RevertBackoff`, and from lock flapping (Locked→Safe→Locked).

Smallest repair: in `cancel` (`transaction_machine.cpp:235-239`),
`safetyChanged` (`events.cpp:239-243`) and `prepareForSuspend` (`:255-259`),
treat `SettlingTopology`, `RevertingApply`, `RevertingObserve`, `RevertBackoff`
as "already reverting": return `accepted(false, <Locked|None>)` without
calling `beginRevert`, optionally updating `m_view.reason`/journal reason
only. Test row: after one failed rollback attempt, cancel + lock + suspend
each leave `view().revertAttempt`, `port.requests.size()` and the state
unchanged; three failures still end in `Stuck`.

## T2 (High, static) — during `SettlingTopology`, cancel/lock/suspend issue a rollback before settle, and external intent abandons the journal

Breaks `display-service.md:114` ("only explicit `topologySettled` proceeds")
and `:147-153`, and the D2 hardest invariant (surviving per-output
properties must be reverted after KWin's own re-application).

Trace A: `…AwaitingConfirmation` → `topologyChanged(setMinusOne)` →
`SettlingTopology` → `cancel("tx")` → `beginRevert(Cancelled)`
(`transaction_machine.cpp:238`) → survivors computed from the *unsettled*
snapshot (`revert.cpp:113-115`) → `requestApply(SurvivingOutputProperties)`
issued while KWin is still re-applying. Same via `safetyChanged(Locked)` and
`prepareForSuspend()`.

Trace B: `SettlingTopology` → `externalIntentObserved(kwinReapplied)` →
generic active branch (`events.cpp:157-166`) → journal cleared → `Ready`.
KWin's own hotplug re-application is not user intent; the previewed
mode/scale/transform on survivors stay live with no journal.

Smallest repair: T1's "already reverting" guard covers Trace A. For Trace B,
in `externalIntentObserved`, if `state == SettlingTopology` route to
`topologyChanged(snapshot)` (adopt the newer pending snapshot, stay
settling). Test rows: no request and no journal clear until
`topologySettled`; the settled revert uses the settled snapshot.

## T3 (High, static) — a settle that returns to the original output set reverts only mode/scale/transform; the preview's position/enable/priority/replication stay live

`display-service.md:149-151` and ADR-0015:43-47 describe the changed-set
rule; neither the docs nor the code cover the set flapping back. Because
KWin persisted the preview's per-set values (ADR-0015:11-13), the re-added
set comes back with the *preview's* positions/priorities, and D1 then
restores only per-output properties.

Trace: dual snapshot → candidate moves DP-2 to (1920,100), scale 1.25,
priority swap → `…AwaitingConfirmation` → `topologyChanged(onlyDP-1)` →
`topologySettled(bothAgain)` → `survivingProperties()` returns both outputs
(`revert.cpp:56-76`; set unconditionally at `events.cpp:214`) →
`makeRevertRequest` picks `SurvivingOutputProperties` because the list is
non-empty (`revert.cpp:92-97`) → position/priority/enable/replication are
never restored, and `RevertingObserve` accepts the partial result
(`revert.cpp:78-88`).

Smallest repair: in `topologySettled` (`events.cpp:212-224`), if
`sameOutputSet(snapshot, m_preimage)` clear `m_survivingProperties` before
`beginRevert` so the request is `FullPreimage`; mirror the same rule in
`recover()` (`revert.cpp:196-204`) when the set at restart equals the
journal's set. Test row: flap back to the original set → `FullPreimage`
request; changed set → `SurvivingOutputProperties` (existing row).

## T4 (High, static) — `Stuck` is overloaded: a failed `clearJournal` after a confirmed or already-reverted display enters `Stuck`, and the rescue path then reverts the user's confirmed configuration or fights external intent

Breaks `display1-v1.md:265` ("Confirmation clears the journal and retains
the observed target"), `display-service.md:155` ("external configuration
aborts … without another apply"), and `Stuck` truth (`:118`).

Trace A (confirmed target reverted): `…AwaitingConfirmation` →
`port.clearSucceeds=false` → `confirm("tx")` → `enterStuck()`
(`transaction_machine.cpp:217-220`; reason `RevertFailed`,
`revert.cpp:158`) → `retryStuck()` → `beginRevert(Recovery)`
(`revert.cpp:216`) → `FullPreimage` apply undoes what the user confirmed.
On restart, `recover()` with the stale journal and live == target also
reverts it (`revert.cpp:188-206`).

Trace B (external intent fought): `…AwaitingConfirmation` →
`externalIntentObserved(X)` with `clearJournal` failing (`events.cpp:161-164`)
→ `Stuck` → `retryStuck()` → `FullPreimage` apply over X.

Trace C (false `RevertFailed`): `RevertingObserve` matched the pre-image →
`clearJournal` fails (`events.cpp:124-127`) → `Stuck`, `reason=RevertFailed`,
`revertAttempt` as-is, although the display is already correct. Same from
`observedSnapshot` in `Observing`/`ResolvingUncertain` (`:86-89`, `:100-103`),
`topologySettled` (`:216-219`) and `recover()` (`revert.cpp:189-192`).

Smallest repair, two parts. (1) `confirm`: on `clearJournal` failure return
`rejected(CommandError::JournalFailure)` and stay in `AwaitingConfirmation`
with the deadline running; a confirmation that cannot be made durable is not
a confirmation, the user or D2 can retry, and the worst case is a revert to
a known-good pre-image. (2) For the "live state already acceptable" clear
failures, do not enter `Stuck`; either (a) go `Ready` with
`journalActive=true` retained and add `CommandError::JournalFailure` so D2
retries the clear from `Ready` (needs a new `clearJournalRetry()`/`tick`
hook), or (b) keep `Stuck` but add a distinct reason
(`TransactionReason::JournalStale` or similar) and make `retryStuck()` first
check `snapshotMatches(m_snapshot, m_preimage)` → `clearJournal` → `Ready`
with no apply. (b) is the smaller diff. Test rows: confirm/clear-failure
preserves state; retry after a stale-journal `Stuck` issues no apply.

## T5 (High, static; realizability partly runtime-only) — `validSnapshot` conflates "well-formed" with "in D1 canonical form", so legal live compositor states are rejected and the machine goes blind

`transaction_machine.cpp:65-79` requires `validateAndNormalize` to accept
the projection *and* `normalizedCandidate == candidate` *and* `noOp`.
Any live snapshot whose minimum enabled non-replica position is not `(0,0)`,
or whose replica reports a scale/position different from its root, or that
overlaps, or whose scale is outside 1.0–3.0, or that uses a mode not in its
own list, fails `validSnapshot` and is rejected by `initialize`,
`observedSnapshot`, `externalIntentObserved`, `topologyChanged`,
`topologySettled` and `recover` (`events.cpp:58,145,171,209`,
`revert.cpp:176`).

Consequences (static): `initialize` can never leave `Discovering`;
in `AwaitingConfirmation` an external change to such a layout is rejected
(`InvalidSnapshot`) instead of aborting, the deadline then reverts to the
pre-image over the external intent; in `SettlingTopology` a settled KWin
layout at a non-origin offset is rejected and rollback never happens.
Whether KWin actually reports such states is runtime-only: KWin rejects
negative positions (accepted handoff §7) but I have no evidence it
re-normalizes an external client's `(100,0)` layout, and my handoff §7
records that KWin computes a replica's own scale in `applyMirroring`.

Smallest repair: split the predicate. Inputs need only
`validateSnapshot(snapshot).accepted && liveFingerprint ==
canonicalFingerprint(candidateFromSnapshot(snapshot))`; the canonical-form
requirements stay on *candidates* in `stage()`. If the lead prefers to keep
the strict predicate, then `display1-v1.md:235-239` must additionally
require the D2 projection to origin-normalize and replica-canonicalize every
live snapshot, and a test row must show `initialize` rejecting a
non-canonical layout so D2 authors see it. This overlaps Iris's
fingerprint lane; I raise only the machine-side consequence.

## T6 (Medium, static; KWin ordering runtime-only) — the machine depends on an undocumented adapter obligation to re-deliver an observation after every apply callback; without it a plainly rejected forward apply ends in `Stuck`

Trace: `preview` → `applyCompleted(Rejected)` → `ResolvingUncertain`
(`events.cpp:32-38`). KWin's `failed` changes nothing, so no new device
batch arrives. Observation deadline → `beginRevert(ApplyRejected)`
(`:279-281`) → rollback apply of an unchanged configuration → `applied` →
`RevertingObserve` → again nothing changes, no batch → deadline → retry ×3
→ `Stuck` with `RevertFailed`, although the display never changed. The
existing rows pass only because the fake re-sends `base`
(`tst_transaction_state.cpp:109`). `transaction_ports.h:24-37` and
`display-service.md:131-143` do not state the re-delivery duty.

Smallest repair: add to the port contract and reference page: "after every
`applyCompleted` and after every apply-deadline `tick`, the adapter delivers
one `observedSnapshot` of the current compositor state even if unchanged
(same revision is accepted)". Optional machine hardening: treat a
definitive `Rejected` whose observation window expires with no observation
as "unchanged" → `clearJournal` → `Ready(ApplyRejected)` with no rollback
apply, keeping the rollback only for `TransportUncertain`/timeout.

## T7 (Medium, static) — `observedSnapshot` is not defensive about output-set changes; a hotplug routed through it is treated as external intent and abandons the journal

Trace: `…AwaitingConfirmation` → `observedSnapshot(setMinusOne)` →
fingerprint differs → `externalIntentObserved` (`events.cpp:131-134`) →
journal cleared, `Ready`; previewed properties on the survivor stay live.
The same input in `RevertingObserve` with survivors requested → a vanished
survivor makes `snapshotMatchesSurvivingProperties` false forever
(`revert.cpp:82`) → three timeouts → `Stuck`. The docs never state the
adapter's entry-point selection rule per state (Iris F6.4 asked the same).

Smallest repair: at the top of `observedSnapshot` and
`externalIntentObserved`, when `Private::activeState(state)` and
`!sameOutputSet(snapshot, m_snapshot)`, delegate to
`topologyChanged(snapshot)`. Then document the adapter rule once: "set
change → `topologyChanged`; batch while an apply is in flight or being
observed → `observedSnapshot`; any other batch → `externalIntentObserved`".
Test row: set change via `observedSnapshot` in `AwaitingConfirmation` lands
in `SettlingTopology` with no clear and no request.

## T8 (Medium, static) — `Stuck` refuses topology and observation updates, so `retryStuck` can request a rollback for outputs that no longer exist

`topologyChanged` in `Stuck` → `rejected(InvalidTransition)`
(`events.cpp:188-190`); `observedSnapshot` in `Stuck` → `CallbackOutOfOrder`
(`:140`). `retryStuck` (`revert.cpp:209-218`) then uses the stale
`m_snapshot` for `sameOutputSet` and the stale `m_survivingProperties`.
Breaks `display-service.md:118` ("explicit retry restarts the same recovery
path"). Smallest repair: in `Stuck`, `topologyChanged`/`observedSnapshot`
adopt the snapshot and clear `m_survivingProperties`; `retryStuck` first
checks `snapshotMatches(m_snapshot, m_preimage)` (→ clear, `Ready`) and
recomputes survivors when the set differs. Test row: `Stuck` → topology
change → retry issues `SurvivingOutputProperties` for the current set.

## Q1 (decision) — `recover()` with a same-set live state that matches neither pre-image nor target reverts to the pre-image

`revert.cpp:188-206` checks only the pre-image and the set. In
`ResolvingUncertain` the same evidence is treated as external intent
(`events.cpp:113`, documented at `display-service.md:113`). After a crash,
"same set, matches neither" means someone changed the configuration after
our preview (KWin restores the persisted preview, so it cannot be KWin's
own doing). Either recovery should abort as external intent (consistent
with the running-machine rule and ADR-0015:46-47) or the docs should say
recovery deliberately fights it. I recommend the former; it is a three-line
change plus one row.

## Q2 (decision) — `ResolvingUncertain` "matches neither → external intent" abandons a possibly-applied preview whose projection differs from the candidate

`events.cpp:109-113` versus the `Observing` rule at `:93` (mismatch waits,
timeout reverts). If an uncertain forward apply was in fact applied and the
D2 projection differs from the normalized candidate in any fingerprinted
field, the journal is cleared and the unconfirmed preview stays live. This
is safe only if the projection is exact; that is a D2 assertion, not a D1
proof. The conservative rule (wait, then revert) costs one extra rollback
apply in the rare true-external case. Lead's call; if the current rule
stays, the reference page should state the exactness assumption next to
`display1-v1.md:235-239`.

## Already verified, not re-reported

Iris F1 repair is complete: `tick()` `RevertingApply` branch
(`events.cpp:291-294`) plus `silentRevertApplyStillConsumesBoundedAttempts`
(`tst_transaction_recovery.cpp:99-126`) bound silent rollback correctly; a
late callback for a timed-out token is rejected because `scheduleRevertRetry`
clears the token (`revert.cpp:141`). The repeated-churn row (`:175-210`)
matches `display-service.md:114`.

I will keep reading newer lead/Iris/Kai/Mina messages and fold any
overlap into the final handoff rather than duplicating it.
