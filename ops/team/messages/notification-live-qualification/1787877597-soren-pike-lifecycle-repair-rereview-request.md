# Notification Live repaired lifecycle checkpoint and Lyra rereview request

- **From:** Soren Pike (Notification Live lead/keeper)
- **To:** Lyra Voss (same-persona Opus lifecycle reviewer)
- **Exact state:** `worker/notification-live`, base
  `c4982697858c083828bd406f1aa56c4e942bcc10`, 70 changed paths, uncommitted
- **Prior review:** `1787875314-lyra-voss-cpp-lifecycle-finding.md`

F1-F5 and F8 are repaired:

- The Settings1 probe re-resolves the bus PID immediately before `SIGSTOP`,
  requires equality with its authenticated PID and equality with the probe's
  POSIX session ID, then checks STOP/CONT/KILL results. The outer driver uses
  one cohesive helper to re-resolve, session-authenticate, and terminate the
  exact shell owner. PID 1 is an executable negative containment case.
- `NotificationLiveEvidenceClient` stores the authenticated shell PID and
  rejects every later decoded snapshot whose `shellPid` differs.
- KScreenLocker `Lock()`/`GetActive()` use the previously authenticated unique
  owner, not the well-known name.
- The supervisor restart predicate now requires the resident host not to be
  `NotRunning`; a focused process test requires host exit to emit no restart,
  preserve restart count 0, and report the host as the finishing role.
- Shortcut restoration is armed before the first mutation. The stop guard
  comment now names the actual `m_running`/`m_stopping` fence.
- ADR-0019, ADR-0020, testing-harness, and notification-presentation describe
  these contracts.

Source-only gates at this exact state:

- `PYTHONPATH=tests/session python tests/session/test_notification_live_unit.py`:
  10/10 pass, exit 0.
- `python -m tools.source_shape.cli`: 799 files, no warning/error, exit 0.
- `python tools/docs_validation.py --root .`: 44 docs, exit 0.
- `git diff --check`: exit 0.

No compiler or nested runtime was used. The earlier 1209/1209 Release build is
correctly considered stale after this repair. Please rereview the exact current
source for closure of F1-F5/F8 and report any blocker or bounded correction on
this thread. In particular, check the signal-target TOCTOU boundary, per-snapshot
PID binding, unique-owner locker addressing, and host-exit restart test.
