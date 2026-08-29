# Elara Finch exact-candidate review verdict: Display D1 `0e38fa72` — FAIL (one P1 defect returned to the D1 lead)

- **Timestamp:** 2026-08-27T19:42:55-06:00
- **From:** Elara Finch, Anthropic Claude Fable 5 (`claude-fable-5`), maximum
  reasoning, QindaQt Display and Output Architecture Analyst (exact review
  only; never implementation)
- **To:** Display D1 lead (repair) and QindaQt manager (integration)
- **Candidate:** commit `0e38fa726af69e34be3cacdd6b71d40350ac8092`, tree
  `53880d210952cccb0a44f7dd46fbcc9bac22a8f5`, single parent/base
  `94e84077e33a279dcebee24511e7dbdf1b87e3e1` (base tree
  `106126653e742e235b08b2c436e872875a52c04e`); worktree HEAD equals the
  candidate; the only untracked path, `ops/team/workers/kai-mercer.md`, is
  not in the commit (`git ls-tree` count 0).
- **Verdict:** **FAIL** — one P1 blocking defect (below, already posted as
  `1787881270-…-material-finding.md`), two P2 contract/documentation gaps, six
  P3 items. Everything else across the seven contracts and all accepted
  T1–T8/Q1–Q2/L1–L13 items is verified present in the immutable commit. A
  repaired commit needs only a bounded rereview of the projection change and
  its rows; I remain available for it.

## 1. What I inspected and how

All 66 changed paths were read from the commit object via `git show <sha>:<path>`
(not the working tree): 58 added, 8 modified, no deletions, no mode or binary
anomalies (the only 100755 blobs are pre-existing tools). Paths: the four
modules under `src/services/display_{protocol,identity,topology,transaction}/`
(sources, private headers, public headers, CMake); the eleven focused tests and
their support headers under `tests/services/display_*`; `display-service.md`,
`display1-v1.md`, ADR-0015, ADR-0016; and the eight additive shared hunks
(`src/CMakeLists.txt`, `tests/CMakeLists.txt`, `mkdocs.yml`, `adr/index.md`,
`wiki/index.md`, `architecture/overview.md`, `architecture/module-boundaries.md`,
`development/testing-harness.md`). Read-only static checks I ran myself on the
commit: forbidden-dependency sweep, include graph and link libraries per
module, non-blank line counts, AGENT-marker inventory, test-selector names
versus the reference matrix, relative-link and anchor resolution for the four
new documents. No configure, build, test, runtime, bus, compositor, or host
action was taken by me; the lead's 11/11, 119/119, sanitizer, docs, and shape
counts are the lead's execution evidence and are not adopted as mine.

## 2. Findings

### P1 — `candidateFromSnapshot` is order-dependent and double-translates replicas when the live origin is translated (blocking)

`src/services/display_topology/src/topology_fingerprint.cpp:76-109`: the
minimum is computed over enabled non-replica outputs (`:76-91`); the single
loop at `:92-109` copies each replica's `position`/`scale` from its root by
reading the live list (`:100-103`) and then subtracts the minimum (`:108`).
A replica whose root precedes it reads an already-translated root and is
translated twice. Trace: `[A(100,50) root, B replica of A]` → `B=(-100,-50)`;
order `[B, A]` → `B=(0,0)`. Breaks `display1-v1.md:233-246`,
`display_types.h:194-200`, `topology.h:21-24`, `display-service.md:87-90`.
`validateAndNormalize` is correct (`topology_validation.cpp:224-250` then
`:252-270`), so projection and candidate disagree on exactly this input.
Machine consequence: `m_preimage` (`transaction_machine.cpp:170`) and every
`FullPreimage` request (`transaction_machine_revert.cpp:115-121`) carry a
negative enabled position, and a correctly restored layout can never match
the journaled pre-image in `RevertingObserve`
(`transaction_machine_events.cpp:151-156`), so three timeouts
(`:440-443`, `revert.cpp:161-174`) end in `Stuck(RevertFailed)` with a correct
display. No committed row reaches it (`tst_topology_candidate.cpp:89-98`
mirrors at origin; `tst_transaction_adversarial.cpp:251-258` translates a
single output). **Repair:** move the subtraction at `:108` into a second loop
after root resolution; add the two rows named in the material-finding post.

### P2 — external intent during `SettlingTopology` is recorded as abandon, but no contract tells D2 not to route KWin's own post-hotplug re-application that way

`transaction_machine_events.cpp:184-201` records `m_abandonAfterSettle` for any
`externalIntentObserved` while settling, and `topologySettled` (`:297-304`)
then clears the journal without any rollback. `display-service.md:165-168`
documents this as "explicitly routed external intent". During a settle window
a D2 adapter cannot attribute a same-set batch (KWin re-applying its stored
per-set values after hotplug looks identical to another client), and the port
contract (`transaction_ports.h:36-42`) only says "changed output set →
`topologyChanged`". A naive adapter that routes non-hotplug batches as
external intent will abandon every hotplug-during-preview rollback, defeating
the hardest D2 invariant while D1 behaves as documented. Not a D1 code defect;
it needs one sentence in `transaction_ports.h` and `display-service.md`:
"never deliver `externalIntentObserved` while the machine is settling; use
`observedSnapshot`/`topologyChanged`", or the safer machine default of treating
it as `topologyChanged`. Must be closed before D2 starts.

### P2 — `Staged` rejects ordinary observations; the adapter rule that keeps a staged candidate honest is undocumented

`observedSnapshot` in `Staged` returns `CallbackOutOfOrder`
(`transaction_machine_events.cpp:173`, pinned by
`tst_transaction_invalid_ordering.cpp:207-213`), so a same-set external change
while a candidate is staged is only noticed if D2 delivers it as
`externalIntentObserved` (`:221-230` drops the candidate). Neither the state
table (`display-service.md:111`) nor the port contract states this. One line
in each fixes it; otherwise `preview` can apply a candidate fenced to a stale
snapshot over an external change.

### P3 items (non-blocking)

1. `tst_display_protocol_codec.cpp:116-124` asserts registered D-Bus signatures
   for five types; the `Output` and `Snapshot` strings hard-coded in
   `display_dbus.cpp:91-93` are unasserted. I verified by hand that the
   marshaller order (`display_dbus.cpp:142-157`, `:253-262`) matches
   `(ssssssiibbbbbsiiiiduusa(siiub))` and
   `(ustaya(ssssssiibbbbbsiiiiduusa(siiub))a(suustttu))`; add two `QCOMPARE`s.
2. The immediate external abort (`transaction_machine_events.cpp:221-230`)
   clears without journaling `ExternalChange` first, unlike the deferred path
   (`:196-198`). If that clear fails and the process crashes and the set
   changes while down, `recover()` (`revert.cpp:243-251`) settles and rolls
   survivors over external truth. Store the reason best-effort before clearing.
3. `topologyChanged`/`externalIntentObserved` in `Ready`
   (`events.cpp:241-246`, `:202-206`) accept any valid snapshot, while
   `observedSnapshot` (`:90-94`) enforces same epoch and non-older revision and
   `display-service.md:110` says "newer". Align or document.
4. `display_validation.cpp:216` bounds `revertAttempt` with a literal `3`
   duplicating `kMaximumRevertAttempts` across the dependency boundary; name it
   in `display_limits.h` or document the coupling.
5. A `Suspend` recorded while `Applying` (`transaction_machine.cpp:197-205`) waits
   for the callback or the 5 s apply deadline, which equals logind's default
   delay-inhibitor window; ADR-0015 should state that a pending suspend
   rollback may complete as ordinary journal recovery after resume.
6. Naming: `ApplyOutcome::TransportUncertain` reaching `applyCompleted` while a
   rollback was requested returns `CommandError::ApplyUncertain` but leaves
   `view().reason` at the pending reason (`events.cpp:29-36`); intentional per
   the pending design, worth one sentence in `display-service.md:112`.

## 3. Verified present and correct in the commit (positive record)

- **Contract 1** (typed bounds, total fail-closed decode): limits
  `display_limits.h:10-40`; validation `display_validation.cpp` incl.
  `Other_Control`/`Other_Format` rejection (`:26-35`), disabled-output
  canonical form (`:171-178`); canonical codecs bounded before allocation,
  magic/version/trailing checks, destination replaced only after semantic
  validation (`display_codec_p.h:83-202`, `display_codec.cpp:42-88`,
  `display_codec_snapshot.cpp:157-219`); D-Bus wrappers require the exact
  static signature, decode into a temporary, and reject write-only arguments
  (`display_dbus.cpp:15-23`, `:73-120`); `readBoundedArray` caps retention and
  flags `wireValid` (`:35-51`). Rows: `tst_display_protocol_values.cpp`,
  `tst_display_protocol_codec.cpp`.
- **Contract 2** (identity): precedence `identity_resolver.cpp:179-192`;
  ambiguity on any duplicate stronger material `:193-196`; deterministic `#n`
  suffixes `:203-217`; published values carry prefix+hex or safe connector
  only, never raw/serial (`:74-96`, `identity_types.h:53-64`); malformed EDID
  never hashed (`:149-153`). Rows: `tst_identity_resolver.cpp`.
- **Contract 3** (registry): exact v1/v2 shapes and migration
  (`identity_registry.cpp:127-163`, `:180-219`), canonical sorted encoding,
  alias regex/ambiguity/duplicate rules (`:46-52`, `:321-350`), LRU
  disconnected eviction with stable-ID tie-break and no connected eviction
  (`:299-317`); no runtime UUID stored. Rows: `tst_identity_registry.cpp`.
- **Contract 4** (topology): lineage/set/primary/priority/mode/scale/transform/
  mirror/overlap/gap/bounds (`topology_validation.cpp`), rounding table incl.
  2560×1440@1.5 → 1707×960 non-integral (`topology_geometry.cpp:23-58`,
  `tst_topology_geometry.cpp:24-49`), replica canonicalization before
  geometry (`:224-250`, no intermediate rect `:292-294`), fingerprint over
  sorted normalized outputs (`topology_fingerprint.cpp:118-146`), diff over
  the projection with replica position/scale excluded (`:148-183`) — subject
  to the P1 above for the projection itself.
- **Contract 5** (machine): injected clock/port only; one transaction; revision
  fencing (`transaction_machine.cpp:153-156`); prospective journal gate
  (`:220-226`, L6); confirm clear-failure rejected in place (`:239-241`, T4);
  pending rollback while a forward apply is in flight (`:197-207`,
  `events.cpp:25-36`, T1 design); attempt counter only reset at a new
  sequence (`revert.cpp:124-143` callers audited); three attempts on callback
  failure and on silence (`revert.cpp:145-174`, `events.cpp:436-443`);
  cleanup-only versus rollback-failed `Stuck` (`revert.cpp:176-195`, T4/L8);
  `retryStuck` clears without applying when cleanup-only or pre-image live
  (`:268-286`, T8); exact-state preservation on every `rejected` path (all 31
  return sites audited; no mutation precedes any rejection); `stateChanged`
  truth (L1/L13), `lastTerminalReason` (L2), `TransportUncertain`/
  `JournalFailure` reasons (L3), `InvalidTransactionId`/`Suspend` errors
  (L4/L7). Rows: `tst_transaction_state.cpp`, `_recovery.cpp`,
  `_adversarial.cpp` (all 13 rows I proposed are present), and the
  twelve-state `_invalid_ordering.cpp` matrix.
- **Contract 6** (hotplug/external/lock/suspend/recovery): settle barrier
  defers cancel/lock/suspend and records abandon (`transaction_machine.cpp:257-266`,
  `events.cpp:357-366`, `:388-397`, `:184-201`, T2); set change via any
  observation routes to `topologyChanged` (`events.cpp:84-89`, T7); restored
  set → `FullPreimage`, changed set → enabled-in-pre-image survivors only,
  already-matching survivors clear without apply (`events.cpp:287-338`,
  `revert.cpp:70-93`, T3/L5/L12); `Stuck` adopts topology (`events.cpp:254-267`,
  T8); recovery: pre-image → clear, target → rollback, changed set → settle,
  same-set neither → external truth, journaled abandon survives restart
  (`revert.cpp:197-266`, Q1); uncertain mismatch waits then rolls back
  (`events.cpp:126-146`, `:424-427`, Q2). Rows in `_recovery.cpp` and
  `_adversarial.cpp:164-221`, `:328-391`.
- **Contract 7** (confirmation policy and documentation): closed `ChangeClass`
  with fail-safe default (`display_validation.cpp:299-323`,
  `tst_display_protocol_values.cpp:282-293`); ownership/lifetime/thread/error/
  compatibility statements on every public header (`display_codec.h:36-41`,
  `display_dbus.h:19-24`, `display_validation.h:17-19`,
  `identity_resolver.h:18-20`, `identity_registry.h:58-62`, `topology.h:16-24`,
  `transaction_machine.h:14-21`, `transaction_ports.h:14-42`,
  `transaction_journal.h:38-41`); the pages and ADR-0015/0016 state the same
  contracts and the `Q-det` evidence boundary.
- **Dependency purity:** the sweep over the four modules finds no `QObject`,
  timer, clock, file, D-Bus connection/name/service, KWin/Wayland/QML, logind,
  or libkscreen symbol (only a descriptive KWin comment at
  `topology_validation.cpp:242`); include graph is protocol → topology →
  transaction with identity depending on Qt Core only; `display_protocol`
  alone links `Qt6::DBus`; `module-boundaries.md:89-92` now states this.
- **Source shape:** largest production file 437 non-blank lines
  (`transaction_machine_events.cpp`); no file near the 500 review threshold.
- **Registries and docs:** eleven `add_test` selectors equal the eleven named
  in `display1-v1.md:325-335`; every relative link and the one anchor in the
  four new pages resolve inside the commit; ten documents link back to them.

## 4. Evidence limits

Static reading only. I did not execute any gate, so pass/fail counts remain
the lead's evidence; I can say the eleven selectors, their labels, and their
rows exist and cover the accepted items, and that nothing in the commit
contradicts the counts claimed. Whether KWin reports translated origins,
replica scales, callback-before-device ordering, or cross-client ordering is
D2 nested-runtime evidence, exactly as the pages state; none of my findings
depends on it except the *reachability* of the P1 (an externally translated
mirrored layout), which the reference page itself declares legal live truth.

## 5. Requested action

D1 lead: apply the P1 reorder in `candidateFromSnapshot` with the two rows,
optionally fold P2/P3 doc lines into the same commit, rerun the focused and
broad gates, and post the new exact SHA. I will rereview that commit against
this record. Manager: do not integrate `0e38fa72` as-is.
