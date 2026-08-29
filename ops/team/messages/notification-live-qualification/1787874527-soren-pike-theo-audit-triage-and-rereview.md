# Lead triage and repaired-state rereview assignment for Theo Marsh

- Lead/keeper: Soren Pike
- Timestamp: 2026-08-27T17:48:47-06:00
- Reviewed finding: `1787874245-theo-marsh-qualification-audit.md`
- Exact current source: 69 paths on base
  `c4982697858c083828bd406f1aa56c4e942bcc10`

Theo's C1 timeout finding is accepted and corroborates Omar's finding; it is
one defect, not two. The repair is now present in:

- `tests/session/NotificationLiveTests.cmake`: race-10x timeout 2400 seconds
  plus the cleanup-margin guard comment;
- `tests/session/notification_live_outer.py`: named
  `REPETITION_TIMEOUT_SECONDS = 180` used by process execution;
- `tests/session/test_notification_live_unit.py`: a static assertion requiring
  at least 300 seconds beyond `repeat * repetition-timeout`.

A configure-time material finding arose after Theo's audit: the live target and
rows were silently absent because `KF6::GlobalAccel` was not visible across the
src/shell → tests/session directory scope. Current source now imports
`KF6GlobalAccel 6.0 REQUIRED CONFIG` inside `NotificationLiveTests.cmake` before
the target gate, with an import-order unit assertion.

## Theo rereview assignment

Read-only recheck the exact repaired state of those three C1 files and the
directory-scoped KF6 import repair. Confirm that 2400 provides the asserted
margin, the unit guard reads the actual registration, and the required import
will make the probe/six rows constructible without widening production
dependencies. Look for a remaining silent-skip or false-green path. Do not
edit, configure, build, install, or launch. Post a new timestamped reply with
exact anchors and pass/blocker verdict; runtime/build registration remains the
lead's responsibility after compiler reassignment.
