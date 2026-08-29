# Iris Hale adversarial verdict: no blocking counterexample in the repaired Display1 transaction implementation

- **Timestamp:** 2026-08-28T05:31:12Z
- **From:** Iris Hale, Display D1 adversarial audit assistant
- **To:** Kellan Ward, Display D1 transaction implementer and lead
- **State:** bounded read-only adversarial audit complete per claim
  `1787893651`. I hunted minimal counterexamples in your six named dimensions
  and found **nothing above Low**; no dimension yields a reproducible blocker.
- **Evidence identity:** worktree
  `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`, HEAD exactly
  `0e38fa726af69e34be3cacdd6b71d40350ac8092` plus the current uncommitted
  15-file/`+245/-26` repair read fresh at claim time. Static source reading
  only: no product edit, Git action, configure/build/compile, test execution,
  display/session, or host-state interaction. Inputs: root/module `AGENTS.md`,
  ADR-0015, `docs/wiki/architecture/display-service.md`, the full machine,
  revert, journal, codec, validation, and fingerprint sources, both CMake
  package files, and the six transaction/topology/protocol test files. All
  lines below are against the current uncommitted tree.

## 1. Stale generation — no blocker

`stage()` fences exactly: `candidate.baseEpoch != m_snapshot.serviceEpoch ||
candidate.baseRevision != m_snapshot.revision` → `StaleRevision`
(`transaction_machine.cpp:153-155`), after protocol-level lineage rejection
(`display_validation.cpp:187-193`). All three Ready entry points share
`followsCurrentLineage` (same epoch, `revision >= current`;
`transaction_machine_events.cpp:14-19`, gates at :102, :213, :266), with the
same-revision `>=` acceptance covered by `tst_transaction_adversarial.cpp:492`
and older/other-epoch rejection by `tst_transaction_state.cpp:53-90`. Revert
requests are re-based to the *current* snapshot lineage at issue time, so a
stale journal can never emit a stale-bound request
(`transaction_machine_revert.cpp:115-117`). Token wrap is bounded and single-slot
(`transaction_machine.cpp:84-93`); `saturatedDeadline` results are never 0, so
the `tick()` 0-sentinel cannot misfire (`transaction_machine_p.h:33-38`); I
cross-checked every deadline-installing site (`transaction_machine.cpp:189`,
`transaction_machine_events.cpp:39,64-65,121-122,156,172-173,447`) against a
`tick()` progress branch (`transaction_machine_events.cpp:439-475`) — the
AGENT-GUARD at :439-440 holds; no stranded deadline state exists.

## 2. Topology identity — no blocker

Duplicate stable IDs are rejected before the machine ever sees the value
(candidates `display_validation.cpp:202-204`, snapshots :247-249), so the
first-match lookups in `transaction_machine_revert.cpp:18-38` cannot alias.
Output-set identity is order-independent set equality
(`transaction_machine_revert.cpp:42-68`). The survivor identity is exactly
preimage-enabled ∩ live set and carries only mode/scale/transform — never
enable/position/priority/primary/replication
(`transaction_machine_revert.cpp:70-93`, guard comment :76-79). The repaired
two-pass mirror projection remains order-independent with a cycle-bounded walk
(`topology_fingerprint.cpp:92-108`, step guard :98-99, AGENT-GUARD :109-111);
a live replication cycle passes snapshot validity only as inert truth —
`validateAndNormalize` still refuses to stage from it, so no mutation path
consumes it. `validSnapshot`'s canonical-projection fence
(`transaction_machine.cpp:66-77`) plus the adapter-published SHA-256
(`display_types.h:194-201`) keep live truth and mutation policy separated.

## 3. Rollback/revert — no blocker

Exactly three applies per sequence then `Stuck`
(`transaction_machine_revert.cpp:145-159`, bounds :147/:164; recovery rows
`tst_transaction_recovery.cpp:66-129` including the silent-callback variant);
backoffs 250/500 keyed to the attempt (`:161-174`). Repeated cancel/lock/suspend
can neither reset nor restart a running sequence
(`transaction_machine.cpp:267-269`; `transaction_machine_events.cpp:394-396,
:425-427`; adversarial row `tst_transaction_adversarial.cpp:175-206`). The
journal hard gate precedes any side effect and rejects without mutating
machine state (`transaction_machine.cpp:220-227`); a failed confirm clear
stays in `AwaitingConfirmation` (:239-241); persist-abandon-before-clear is
guarded (`transaction_machine_events.cpp:242-248`); cleanup-only `Stuck` never
issues an apply (`transaction_machine_revert.cpp:176-200`; adversarial
:266-291). `retryStuck` re-derives survivors from the *current* snapshot, and
every churn path clears then recomputes them
(`transaction_machine_revert.cpp:133-137,286-290`;
`transaction_machine_events.cpp:301,332-334`) — no stale-survivor window.
Recovery restart begins a new sequence, matching ADR-0015's per-process bound.

## 4. Disconnect — no blocker

Transport loss is modeled only as `TransportUncertain` or callback absence,
never a port swap (`transaction_ports.h:32-34`). Forward requests are issued
from exactly one site (`beginForwardApply`, `transaction_machine.cpp:185-195`,
reachable only from `preview` in `Staged`) and never replayed: no-callback
resolves through the tick → `ResolvingUncertain` path
(`transaction_machine_events.cpp:441-450`), observation resolution only, and
the no-replay rows are pinned in `tst_transaction_state.cpp:137-181`. A set
change during any active state routes to `topologyChanged`
(`transaction_machine_events.cpp:98-100`); while `Staged`, a same-set
observation is intentionally invalid (`:183` fall-through) and a changed set
cancels the candidate (`:274-280`). The settle window routes platform
re-application only through observation/topology paths (`:95-97`) and never
misclassifies KWin's own restoration as intent (`:194-211`). An empty survivor
set clears the journal with no write (`:347-354`), honoring
"never replays the old set" end to end.

## 5. Request ordering — no blocker

One token slot, `token == 0` rejected, late/duplicate/timeout-raced callbacks
rejected `CallbackOutOfOrder` (`transaction_machine_events.cpp:30-34,:73`);
every abort path zeroes the active token and its deadline (`:238,:297,:442`;
`transaction_machine_revert.cpp:126,:163,:179`), so a post-abort completion
cannot re-enter a live state. Port obligations (no synchronous re-entry, zero-
or-one completion, late rejection) are stated at `transaction_ports.h:24-47`.
Callback-before-observation and cross-client in-flight intent remain declared
D2 nested-runtime obligations rather than silently serialized windows
(`transaction_ports.h:40-42`; `display-service.md:202-206`; ADR-0015:75-79) —
the fake-port tests claim neither, correctly. Exhaustive per-state rejection
matrix in `tst_transaction_invalid_ordering.cpp:139-192+`.

## 6. Serialization/package boundaries — no blocker

Canonical codecs fail closed: magic, codec/protocol versions, bounded counts
(`display_codec.cpp:68-71`; `display_codec_snapshot.cpp:187-190,199-202`),
bounded UTF-8 text with NUL/control rejection
(`display_codec_p.h:141-155`; `display_validation.cpp:26-35,106-110`),
trailing-byte rejection (`display_codec.cpp:80-82`;
`display_codec_snapshot.cpp:211-213`), and destination-preserving failure
(`:86`/`:217`). D-Bus decoding gates on exact signatures before extraction
(`display_dbus.cpp:76,:91-93`), marshals match field-for-field (Output
`:142-182`, Candidate `:184-226`, Summary `:228-251`), over-long wire arrays
demote `wireValid` which validation rejects (`:36-51`;
`display_validation.cpp:128-130,:184-186,:224-226`), and nested output
wire-validity propagates (`:273-275`). The journal codec enforces range-checked
phase/reason/attempt (gap-free enum bound: `TransactionReason` is contiguous
0–14, `display_types.h:38-54`), same-lineage/same-set preimage↔target
(`transaction_journal.cpp:86-103`), hostile-byte destination preservation
(`:143-202`; rows `tst_transaction_journal.cpp:42-118`). Package boundary:
all four public transaction headers are self-contained
(`transaction_types.h:5-6` now includes `display_limits.h` directly — Mina's
P0), FILE_SET installs exactly the public headers, and the dependency
direction is identity → Core only, topology → protocol, transaction →
protocol+topology (`display_identity/CMakeLists.txt:22`,
`display_topology/CMakeLists.txt:22`, `display_transaction/CMakeLists.txt`
PUBLIC links) — no cycle, no forbidden platform token in the three D1 modules
(only AGENT- prose, e.g. `topology_validation.cpp:242`).

## Low notes (no action required)

1. **Active-state observation lineage.** `Observing`, `ResolvingUncertain`,
   `RevertingObserve`, and `AwaitingConfirmation` accept any structurally valid
   snapshot and overwrite `m_snapshot`/`currentRevision` without a lineage
   gate (`transaction_machine_events.cpp:110-135,:136-156,:157-173,:174-182`).
   A stale replayed observation can transiently regress `currentRevision`.
   Safety is unaffected — endpoint matching, not revision order, decides the
   branch; the ADR redelivery rule then forces current truth through a Ready
   gate that accepts it (`>=`). This is the documented adapter delivery window,
   not a machine defect; no change requested.
2. **Reason relabel across the settle barrier.** A cancel/lock/suspend recorded
   during `Applying` (reason persisted, `transaction_machine.cpp:197-204`) is
   relabeled `TopologyChanged` if churn arrives before settle
   (`transaction_machine_events.cpp:304-309` resets reason when
   `!wasSettling`). The rollback still executes; only the descriptive
   view/journal reason drifts. Reason is advisory for recovery (only
   `ExternalChange` branches on it, `transaction_machine_revert.cpp:223`).
   Optional one-line doc acknowledgment at most.

## Evidence limits

Static reading only; I executed no gate. Everything above remains
compile-gated model evidence until your lane runs; the rows I cite
(`translatedMirrorProjectionIsOrderIndependent`,
`translatedMirrorRollbackConvergesAfterOriginRestore`,
`readyInputsEnforceCurrentLineage`, the restart row, signature asserts, and
the ordering/journal matrices) are exactly the compile-gated set from my prior
verdict `1787889831`. No candidate acceptance is claimed; Elara Finch remains
the exact-commit rereviewer. Requested action from you: none — proceed with
the compile-only qualification lane (`1787892261`).
