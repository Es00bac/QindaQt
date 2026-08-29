# Church the 2nd — Terminal S0 P1: shutdown failure discards ownership and quits with a surviving child

- Time: 2026-08-28T08:39:38-06:00
- Owner: Church the 2nd
- Addressees: Micah Stone; Program Manager
- Exact candidate: `2386e7464bcebe17dd074299ac20f1739a5bf8b1`
- Severity: P1; teardown ownership/quit-state repair required

The repaired close path waits for `shutdownFinished`, but it still quits and
abandons the child when the bounded escalation reports failure. In
`src/apps/terminal/session/terminal_session.cpp:184-197`,
`completeShutdown(false, diagnostic)` stops polling, resets the backend, and
sets `m_childPid = 0` before entering `ShutdownFailed`. The implementation has
therefore discarded both its PTY owner and the only captured process-group
identity while explicitly believing the child survived SIGKILL.

`src/apps/terminal/ui/terminal_window.cpp:320-331` prints the failure to stderr
but emits `closeShutdownFinished()` whenever close requested, without checking
`clean`. Production has connected that signal to application quit. The process
exits immediately with the survivor now untracked. This directly contradicts
the wiki's “guarantees child teardown” and “a surviving child can never be
orphaned” S0 boundary. It also creates a misleading second-close path:
`beginShutdown()` from `ShutdownFailed` sees the already-zero pid at
`terminal_session.cpp:255-260` and reports a clean shutdown on the next tick.

The existing session test at `tests/apps/terminal/tst_terminal_session.cpp:325-346`
asserts only the isolated `ShutdownFailed` state and start refusal. The window
close test uses an instant-exit monitor (`tst_terminal_window.cpp:317-367`) and
never covers close + escalation failure + quit suppression/continued ownership.

Repair should preserve the captured identity and ongoing reap observation in a
failure/awaiting-exit state, must not emit the application quit signal while a
child may be alive, and should provide a deliberate user-visible recovery path.
At minimum, add a combined window/session regression proving failed escalation
cannot quit, lose the pid, or turn into a false clean result on a second close.

No product/Git mutation, compiler, PTY, GUI, session, or host interaction was
performed. The exact review continues.
