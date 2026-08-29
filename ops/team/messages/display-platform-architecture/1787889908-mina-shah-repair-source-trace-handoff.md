# Mina Shah handoff: source-trace of the P1/P2 repair — one build-breaking regression found

- **Timestamp:** 2026-08-27T22:05:08-06:00
- **From:** Mina Shah, Claude Sonnet 5 (high), read-only Display D1
  public-API/docs/acceptance-trace reviewer
- **To:** Display D1 lead/keeper (and Elara Finch/Fable for awareness before
  rereview)
- **Assignment:** continuation of `1787875111-display-d1-mina-docs-trace-
  rereview-assignment.md`; this pass traces the uncommitted repair described in
  `1787881515-display-d1-exact-review-repair-claim.md` and
  `1787882078-display-d1-exact-review-source-repair-checkpoint.md`
- **Evidence identity:** worktree
  `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`, HEAD unchanged at
  failed candidate `0e38fa726af69e34be3cacdd6b71d40350ac8092`, plus the current
  uncommitted diff (`git diff --stat`: 15 files, 244 insertions(+), 26
  deletions(-), matching the checkpoint exactly). Read-only static inspection
  only; no compiler, configure, build, test, or host-state action taken.

## Verdict: do not qualify or commit as-is — one build-breaking regression

**P0 (new, caused by this repair): `transaction_types.h` is not self-contained
and breaks the primary translation units of `display_transaction`.**

- `src/services/display_transaction/include/qindaqt/services/display_transaction/transaction_types.h:13`
  now reads
  `inline constexpr quint32 kMaximumRevertAttempts = Display::kMaximumRevertAttempts;`
  (P3.4's literal-dedup, sourced from
  `display_limits.h:18`'s new `Display::kMaximumRevertAttempts`). This is the
  right contract (protocol validation and the transaction machine sharing one
  value), but `transaction_types.h`'s own includes are only
  `qindaqt/services/display_protocol/display_types.h` plus Qt containers
  (`transaction_types.h:5-8`) — it never includes
  `qindaqt/services/display_protocol/display_limits.h`, and
  `display_types.h` does not include it either (confirmed by reading
  `display_types.h:5-10`, only `QtCore` headers).
- This compiles today **only** where some other header happens to include
  `display_limits.h` earlier in the same translation unit. That accident holds
  for `transaction_machine_p.h` (`display_limits.h` at line 5, before its own
  `transaction_types.h` include at line 7) and for the test support header
  `tests/services/display_transaction/support/transaction_test_support.h`
  (`display_limits.h` at line 5, before `transaction_machine.h` at line 7) —
  which is exactly why the three modified `tst_transaction_*.cpp` files still
  look fine at a glance.
- It does **not** hold for the module's own production sources, each of which
  includes the public header chain first:
  - `src/services/display_transaction/src/transaction_machine.cpp:3` includes
    `transaction_machine.h` before `transaction_machine_p.h` (line 8).
  - `src/services/display_transaction/src/transaction_machine_events.cpp:3`
    — same order, `transaction_machine_p.h` at line 7.
  - `src/services/display_transaction/src/transaction_machine_revert.cpp:3`
    — same order, `transaction_machine_p.h` at line 7.
  - `src/services/display_transaction/src/transaction_journal.cpp:3` includes
    `transaction_journal.h` (which chains straight to `transaction_types.h`)
    **before** its own `display_limits.h` at line 6.

  In every one of these four files, `transaction_machine.h`/
  `transaction_journal.h` pulls in `transaction_ports.h`/`transaction_types.h`
  and needs `Display::kMaximumRevertAttempts` before `display_limits.h` has
  been seen anywhere in that translation unit. I traced the exact
  `#include` order in each file by direct read (not grep alone) to confirm
  this is a real, order-dependent break, not a hypothetical one — this is the
  entire non-test implementation of the `display_transaction` module. No unity
  build or precompiled-header target exists in `CMakeLists.txt` to mask it (I
  checked; none configured).
- This is genuinely new: before the repair, `transaction_types.h:13` was the
  self-contained literal `inline constexpr quint32 kMaximumRevertAttempts = 3;`
  and had no dependency on `display_protocol`'s limits header. The repair's
  P3.4 dedup introduced the cross-header dependency without adding the
  `#include` that makes it safe on its own.
- **Smallest repair:** add
  `#include <qindaqt/services/display_protocol/display_limits.h>` to
  `transaction_types.h`'s own include block (next to its existing
  `display_types.h` include). This is the header that actually uses the
  symbol, so it should carry the include regardless of what any consumer
  happens to include first.
- I could not confirm this with an actual compile — no compiler lane is
  available to me and I was asked to stay read-only/static — so this is
  reported as a static, high-confidence trace, not a build log. Given the lead
  noted no configure/build ran on this checkpoint either, this would be the
  first thing a real build hits.

## Everything else I traced: clean

- **Topology mirror fix (Elara's P1).**
  `src/services/display_topology/src/topology_fingerprint.cpp:92-117`: I read
  the full function. Pass 1 (lines 92-108) resolves every replica's
  position/scale from its root's still-untranslated live coordinates (origin
  computed at lines 76-91 from non-replica outputs only, before any mutation).
  Pass 2 (lines 112-117) then subtracts that same origin from every enabled
  output uniformly, root and replica alike. Because replica resolution reads
  root state before the origin subtraction touches it, the result no longer
  depends on array order. This matches the new test
  `translatedMirrorProjectionIsOrderIndependent`
  (`tst_topology_candidate.cpp`) exactly: root/replica built in both orders,
  equal projections, equal fingerprints, both validate as no-op. Matches
  contract 4. No drift.
- **P2 settling/staged routing.** `transaction_ports.h:41-47`'s new
  AGENT-CONTRACT and `display-service.md`'s new `Staged`-row/port-precondition
  sentences describe pre-existing, unmodified code
  (`transaction_machine_events.cpp` — `observedSnapshot`'s `Staged` fallthrough
  to `CommandError::CallbackOutOfOrder`, and `SettlingTopology`'s forced
  `topologyChanged` routing at lines 95-97). I traced both paths directly; the
  new doc sentences accurately describe existing behavior, not aspirational
  text. No drift.
- **P3.3 lineage guard.** The new `followsCurrentLineage` helper
  (`transaction_machine_events.cpp:13-19`) and its three call sites (`Ready`
  branches of `observedSnapshot`, `externalIntentObserved`, `topologyChanged`)
  match `display-service.md`'s new sentence and the new test
  `readyInputsEnforceCurrentLineage` (older revision and other-epoch both
  rejected with `InvalidSnapshot` and no state/snapshot mutation; newer
  revision accepted). `Machine::currentSnapshot()` used by the test already
  existed pre-repair (`transaction_machine.cpp:38`, unmodified). No drift.
- **P3.2 external-abort journaling.** `externalIntentObserved`'s two edited
  branches (`transaction_machine_events.cpp:224-235`, `239-251`) now store
  `m_journal.reason = ExternalChange` before attempting `clearJournal()`, and
  `enterStuck`'s new `durableReason` parameter
  (`transaction_machine_revert.cpp:176-196`) keeps `m_view.reason` at
  `JournalFailure` for the observable cleanup-only view while
  `m_journal.reason` carries the durable `ExternalChange` instruction when
  supplied. This exactly matches the checkpoint's claimed behavior and the new
  assertions in `tst_transaction_recovery.cpp` (`failedClear...` /
  `restart...` block): failed clear leaves `Stuck`/`JournalFailure` live but
  journals `ExternalChange`; recovery on that journal enters
  `SettlingTopology` with no apply request, then `topologySettled` reaches
  `Ready` with still no apply request. No drift.
- **P3.5 suspend-during-apply ADR text.** New ADR-0015 paragraph matches
  pre-existing, unmodified `requestRevert`
  (`transaction_machine.cpp:197-207`): in `Applying`, it only sets
  `m_revertRequested`/journals the reason and returns, it does not revert
  immediately — confirming the "waits for its callback or the deadline" claim
  is accurate, not new code. No drift.
- **P3.4 shared limit, semantics.** Aside from the missing include above, the
  value itself is correctly unified: `display_limits.h:18` defines
  `Display::kMaximumRevertAttempts = 3`;
  `display_validation.cpp:216` (same module, already includes
  `display_limits.h`) and `transaction_types.h:13` both resolve to it; every
  in-module use (`transaction_machine_revert.cpp:163`,
  `transaction_machine_events.cpp` call sites) still refers to
  `DisplayTransaction::kMaximumRevertAttempts`, so this is a value alias, not a
  behavior change. Once the include is added, semantics are correct.
- **P3.1 D-Bus signature assertions.** `Output`/`Snapshot` registration in
  `display_dbus.cpp:65,69` is pre-existing and unmodified; the new
  `tst_display_protocol_codec.cpp` assertions just pin the already-registered
  signatures. No drift.
- **Forbidden artifacts / source shape.** `git diff` over every touched
  `src/**`/`tests/**` path has no KWin/Wayland/QML/QSettings/QTimer/real-clock/
  logind/libkscreen token. Line counts after the repair: largest touched file
  is `transaction_machine_events.cpp` at 478 lines — under the 500-line
  decomposition-review threshold, not a finding, just noted since it grew by
  43 lines this pass and is the closest of the touched files to that
  threshold.
- **Dependency direction.** No new `#include` crosses a forbidden boundary;
  the one new cross-module reference (`transaction_types.h` → `Display::
  kMaximumRevertAttempts`) is the correct existing direction
  (`display_transaction` depending on `display_protocol`), it is just missing
  its own include statement (the P0 above).
- **Scope note, not a finding:** I did not re-derive Iris Hale's protocol/
  identity/topology adversarial results (her repair rereview
  `1787875087` predates this exact P1/P2 repair and covered different items —
  bounded revert attempts, canonical-snapshot fixpoint, mirror
  canonicalization, port pre/postconditions as they stood before Elara's P1
  was found) or Elara's transition-model analysis; I focused on whether this
  specific diff introduces drift against the seven contracts, the docs/ADRs,
  and build-hygiene boundaries.

## Requested next action

Add the one missing `#include` to `transaction_types.h`, re-run the static
sweep (`git diff --check`, the host-path/shell-diagnostic sweep, and
`tools/check-source-shape` you already ran clean once) to confirm nothing else
changed, then take the compiler lane before any commit — this repair has not
been compiled yet by anyone's account, and the break I found sits in the
module's core implementation files, not an edge case. Please do not treat this
candidate as accepted, and route the corrected commit back to Elara for exact
rereview before integration, per the manager's exact-candidate policy. I
remain available to retrace a corrected diff or the eventual immutable commit.
