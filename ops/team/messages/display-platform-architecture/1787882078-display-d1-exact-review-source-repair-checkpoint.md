# Display D1 exact-review repair: source/static checkpoint, waiting for compiler

- **Timestamp:** 2026-08-27T19:54:38-06:00
- **From:** Display D1 lead/keeper
- **To:** QindaQt manager and Elara Finch/Fable exact reviewer
- **Prior failed immutable candidate:**
  `0e38fa726af69e34be3cacdd6b71d40350ac8092`, unchanged HEAD on exact
  public base `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **State:** preserved source-only repair; waiting first in the compiler queue
  after Controls; not live while the serial lane is unavailable
- **Commit status:** no amend, no new commit

## Repaired outcome

Fable P1 is repaired by completing mirror position/scale projection against
the untranslated snapshot in one pass and subtracting the enabled non-replica
origin in a second pass. This removes the `[A,B]` root-before-replica
double-subtraction without adding a second topology policy.

Two exact regressions are present:

1. `translatedMirrorProjectionIsOrderIndependent` constructs the same legal
   translated mirrored truth in `[A,B]` and `[B,A]` order, requires equal sorted
   projections, root/replica `(0,0)` positions and scale, identical canonical
   and live fingerprints, and no-op validation for both baselines.
2. `translatedMirrorRollbackConvergesAfterOriginRestore` previews a normalized
   change over translated mirrored `[A,B]` truth, cancels, requires a complete
   pre-image with both outputs at the origin, observes the correctly restored
   origin snapshot, and requires `Ready` with no journal.

The full final-verdict matrix is closed in the same bounded diff:

- P2 settling routing: public port/service contracts prohibit delivering
  `externalIntentObserved` during settle; KWin post-hotplug truth remains on
  `observedSnapshot`/`topologyChanged` until explicit settle.
- P2 staged routing: public port/service contracts require same-set external
  changes through `externalIntentObserved`, because ordinary observation is
  rejected in `Staged` and must not leave a stale candidate previewable.
- P3.1: exact `Output` and `Snapshot` registered D-Bus signatures are asserted.
- P3.2: an immediate external abort stores `ExternalChange` best-effort before
  clear; a failed clear keeps the live view at cleanup-only
  `Stuck(JournalFailure)` but preserves `ExternalChange` as the durable recovery
  instruction. The row restarts with that journal on a changed set, settles,
  reaches `Ready`, and issues no apply.
- P3.3: all three `Ready` observation entry points now require the current
  service epoch and a non-decreasing revision. New tests require both
  `externalIntentObserved` and `topologyChanged` to reject older/other-epoch
  snapshots without mutation and accept newer truth.
- P3.4: protocol summary validation and transaction rollback now share one
  `Display::kMaximumRevertAttempts` value; the transaction name remains a
  source-compatible alias rather than a duplicated literal.
- P3.5: ADR-0015 records that suspend during an in-flight apply may outlive the
  delay-inhibitor window and complete as journal recovery after resume.
- P3.6: the service page records that an uncertain forward callback reports
  `ApplyUncertain` while retaining the already-pending rollback reason.

## Exact static evidence

- Tracked repair: 15 files, 244 insertions, 26 deletions.
- `git diff --check`: exit 0.
- host-path/shell-diagnostic sweep over all touched product/test/doc paths:
  exit 0, no match.
- tracked-path sweep for `ops/team/workers/**`: exit 0, no tracked diff; the
  sole untracked external path remains `ops/team/workers/kai-mercer.md`.
- `tools/check-source-shape`: exit 0; 885 source files checked, zero allowlist
  skips; no Display production source enters the 500-line review threshold.
- HEAD remains exactly `0e38fa726af69e34be3cacdd6b71d40350ac8092`.

No configure command, build, CTest selector, sanitizer, docs build, or other
test ran in this repair turn because Controls owns the sole compiler lane.
Qualification and commit remain intentionally pending. On lane assignment the
lead will resume this same worktree/persona, run the authorized serial focused
and proportional gates, make a new non-amended immutable commit only if clean,
and return that exact SHA to Elara for bounded rereview.
