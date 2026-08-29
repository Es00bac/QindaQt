# Lyra Voss repaired-source rereview handoff to Soren Pike

- Worker: Lyra Voss (Anthropic, `claude-opus-5`, reasoning xhigh)
- To: Soren Pike, lead/keeper, Notification Live qualification
- Timestamp: 2026-08-27T18:52-06:00
- Request: `1787877597-soren-pike-lifecycle-repair-rereview-request.md`
- Reviewed state: `worker/notification-live`, HEAD
  `c4982697858c083828bd406f1aa56c4e942bcc10`, uncommitted working tree of 70
  paths (38 tracked modifications, 32 new files)
- Scope: closure of accepted F1-F5 and F8 from
  `1787875314-lyra-voss-cpp-lifecycle-finding.md`

## Verdict: PASS for source rereview

All six accepted findings are closed in the actual source. I found no new
blocker. Four bounded notes follow; none of them should hold the next gate.

## F1 - CLOSED, and stronger than I asked for

Both signal sites now re-resolve, re-authenticate, and containment-check
immediately before delivery.

- Probe side: `tests/session/notificationlivesettingsphases.cpp:142-150`
  re-resolves the Settings1 owner, requires equality with the PID authenticated
  at `:128-134`, and calls `validateNotificationLiveSignalTarget` before the
  `SIGSTOP` at `:151`. Every signal now goes through the single helper at
  `:29-40`, which reports the failing action and `errno`. SIGCONT/SIGKILL at
  `:157`, `:176`, `:182`, `:185` are correctly left unguarded because a
  SIGSTOPped process cannot exit and free its PID.
- Driver side: `tests/session/notification_live_process.py:190-207`
  re-resolves the owner, requires equality with the authenticated PID, calls
  `validate_private_session_process`, then signals; the call site is
  `tests/session/test_notification_live_nested.py:342-344`.
- The predicate itself is `tests/session/notificationliveruntime.cpp:69-98`
  and `notification_live_process.py:46-63`.

The property this achieves is better than a bare re-resolution. Because
`getsid(target)` is evaluated microseconds before the signal, a PID recycled
after the bus lookup is almost certainly refused: a fresh host process is not
in the disposable driver's session. The residual case is a PID recycled *into*
the private session, which is by definition a process of the disposable tree.
The failure mode can therefore no longer leave the private session, which was
the whole substance of F1.

I verified the containment predicate is satisfiable rather than
self-defeating, which was my main counterexample hunt:
`start_new_session=True` appears exactly once in the entire tree
(`notification_live_process.py:120`), there is no `setsid` anywhere in `src/`
or in the generated wrapper (`test_notification_live_nested.py:134-138`),
`_run_probe` (`:95-97`) and `start_logged_process`
(`notification_live_process.py:22-27`) both inherit the inner driver's
session. So probe, Settings1, compositor, supervisor and shell all share one
session id and the guard passes for every legitimate target.

Includes are correct for the new syscalls: `<limits>`, `<unistd.h>`,
`<sys/types.h>`, `<cerrno>`, `<cstring>` at
`notificationliveruntime.cpp:14-19`.

Two bounded notes, below as N1 and the residual pidfd point.

## F2 - CLOSED

`tests/session/notificationliveevidenceclient.cpp:84-104` now refuses to call
at all when unauthenticated (`:87-90`) and rejects any decoded reply whose
`shellPid` differs from the authenticated PID (`:95-102`). `awaitSnapshot`
(`:106-137`) routes exclusively through `snapshot`, and prefers the recorded
`lastError` at `:129-130`, so an identity change surfaces as "shell evidence
snapshot PID changed" rather than a bare timeout.

Counterexamples I checked and could not construct:

- No bypass exists. The only `Snapshot` call on `org.qindaqt.ShellDevelopment`
  in the whole tree is `notificationliveevidenceclient.cpp:91`; the other
  `Snapshot` hits are the unrelated compositor/Hybrid endpoints.
- The provisional assignment at `:67` cannot admit an unauthenticated reply,
  because the very `snapshot` call it enables enforces equality against that
  same value.
- A failed `authenticate` always leaves the field at 0 (`:51`, `:75`, and the
  timeout path), so a client that failed to authenticate is unusable rather
  than silently trusting.

The re-check at `:69-71` is now subsumed by `snapshot`; harmless.

## F3 - CLOSED

`tests/session/notificationlivelock.cpp:121-122` constructs the interface on
`*freedesktopOwner`, the unique name proved at `:104-112` to be the shared
owner of Compositor1 and both locker names with the exact KWin PID. `Lock` at
`:123` and `GetActive` at `:130` both address it, and the `active` lambda
captures that same object, so the whole lock/unlock observation is bound to
the authenticated owner. An owner change now fails the call closed instead of
silently rerouting.

## F4 - CLOSED as specified, with a discrimination note

The predicate is `src/session_supervisor/src/session_process_supervisor.cpp:176-177`.
The regression is
`tests/session_supervisor/tst_session_process_supervisor.cpp:218-242`, which
kills the host and asserts no restart, restart count 0, both PIDs cleared, and
that the finishing role names the notification host - matching the
`"notification host exited"` text produced by `finishSession`.

I checked that the new term cannot suppress a legitimate restart: `m_running`
is set only after both children start, and the existing
`supervisorStartsBothChildrenAndCouplesTheirLifetime` still restarts once
because the host helper has no `--profile` and therefore a 5-second lifetime
while the shell helper exits in 20 ms
(`tests/session_supervisor/session_token_child_helper.cpp:55-60`).

Note N2 below: the new test proves the contract but does not discriminate the
new predicate.

## F5 - CLOSED

`tests/session/notificationliveworkflow.cpp:186-188` arms restoration before
the first mutation, so a `setNotificationLiveShortcut` that changes the
registry and then fails its echo check
(`tests/session/notificationliveshortcut.cpp`, mutate-then-validate shape) now
restores the default binding on unwind. `restore.disarm()` at `:215` still
follows the explicit default restoration at `:212`.

## F8 - CLOSED

`session_process_supervisor.cpp:103-105` now names the actual mechanism - the
`m_running`/`m_stopping` fence held across both waits - and states correctly
that the count reset at `:111` happens after both children are stopped and is
for the next session. The marker matches the code.

## Bounded notes (not blockers, no repair required before the next gate)

- **N1 - the containment guard's session branch is untested.**
  `tests/session/test_notification_live_unit.py:150-153` asserts the positive
  case and PID 1, but PID 1 is rejected by the `process_id <= 1` bound at
  `notification_live_process.py:48`, so the `getsid` comparison at `:57-62`
  never executes in any test. If you want that branch covered, the module
  already has the primitive: spawn a throwaway child with
  `start_new_session=True`, assert `validate_private_session_process` raises
  matching "outside private session", then terminate it.
- **N2 - the host-exit test does not discriminate the new predicate.** In
  `:218-242` the host's `finished` is delivered first, so `finishSession` sets
  `m_stopping` before `stopChild(m_shell)` and the pre-existing fence already
  suppresses the restart; the test would pass with line 177 removed. Reaching
  the state the predicate actually guards requires `m_host.state()` to be
  `NotRunning` while the host's `finished` has not yet run, and Qt sets that
  state inside the same call that emits `finished`. I therefore cannot prove by
  reading that the guard is ever the deciding branch. It is correct,
  defence-in-depth, and cost-free; I would keep it and not chase a test for it.
- **N3 - pre-existing timing coupling worth knowing before sanitizers.** The
  host helper self-quits at 5 seconds
  (`session_token_child_helper.cpp:55-60`, host receives no `--profile`), while
  `tst_session_process_supervisor.cpp:262`, `:271`, `:278` and `:295`, `:299`
  budget 5-second `QTRY` waits and assert `finished.size() == 0`. On a slow or
  sanitizer-instrumented run the host's own timer could fire mid-test and
  produce a spurious `finished`. This predates the repair and does not affect
  source correctness; if a sanitizer run flakes there, raise the host helper
  lifetime rather than suspecting the restart logic.
- **N4 - a limit on this rereview.** 32 of the 70 paths are untracked, so
  `git diff` cannot bound what changed in them between my two reviews. Several
  reviewed files were also reformatted to a wider column limit. I therefore
  re-read the affected code paths in full rather than relying on a diff, but I
  cannot assert "only the six repairs changed" for the untracked set.

## Residual I am not claiming closed

The irreducible window between the last check and the signal remains: the only
way to remove it entirely is `pidfd_open` at resolution plus
`pidfd_send_signal`. Given the session predicate now runs immediately before
delivery, I do not think that is worth doing for this outcome, and I am not
asking for it.

I also record that I accept your F6 and F7 deferrals. I independently
re-confirmed both reasons: center traversal does reject an unnamed focusable
because `injectFocusSequence`
(`tests/session/notificationlivekeyboard.cpp:46-59`) awaits an exact named
item per Tab, and quieting acceptance is a conjunction of the observation
count with current-state and exact-text predicates
(`notificationlivesettingsphases.cpp:163-173`).

## Gates this PASS does not cover

This is a source rereview only. I ran no compiler and no test, and I am not
adopting your four source-only gate results as my own evidence. Still open at
this exact state:

1. Compile and link. No compiler has run since this repair; the new
   `getsid`/`limits`/`unistd` surface, the evidence-client member, and the
   supervisor predicate are unbuilt. The prior Debug 1194/1194 and Release
   1209/1209 are correctly stale.
2. Focused non-nested execution, including the shell offscreen tier, runtime
   options, development input, and the supervisor process tests - the last of
   which now carries a new case.
3. Fresh Debug and Release rebuild for provenance.
4. Sanitizers. My order is unchanged: `qindaqt.session-supervisor` under
   ASan+UBSan first, since it owns all new process lifetime and the restart
   reentry, then LSan on the same target, then the shell offscreen tier
   including `qindaqt.notification-focus-offscreen`.
5. Staged package/install.
6. The private nested matrix: the five named resolution/scale rows plus
   race-10x. In particular, no reading of mine can confirm that
   `getsid`-based containment behaves as analysed in the real nested tree, that
   the unique-owner locker calls are accepted by KScreenLocker 6.6.5, or that
   the per-snapshot PID binding never spuriously rejects a healthy shell.

Requested next action: proceed to the compiler and focused gates when the
manager reconfirms the lane. I remain available for another exact-state
rereview if any of those gates forces a source change.
