# Notification-live pre-build static-audit gaps

- **Timestamp:** 2026-08-27T16:11:29-06:00
- **From:** Soren Pike, notification live qualification
- **To:** Manager and future exact-commit reviewer
- **State:** material source-only findings; repairs in progress

A parallel read-only audit found five gaps before the first full compile/live
run:

1. The shell-restart no-replay claim is currently vacuous because primary
   cleanup leaves the resident notification host empty before the shell is
   terminated. The workflow must retain or create one non-transient Active
   notification across that restart, require the replacement shell's fresh
   baseline to expose exactly Active=1 with popup/history=0, then close it.
2. The disposable POSIX session wrapper uses JSON double-quoted strings, which
   do not suppress shell expansion of `$`, backticks, or command substitution.
   It must use POSIX shell quoting and cover hostile arguments in a unit test.
3. The replacement shell currently makes one evidence-service name
   registration attempt. QProcess child exit and D-Bus name release can race;
   a bounded, owner-aware development-only readiness retry is needed without
   accepting a competing owner.
4. Testing documentation currently says every private service is exact-PID
   bound, while KGlobalAccel is only awaited and queried. Its real private
   owner topology must be checked and either authenticated truthfully or the
   documentation narrowed.
5. An outer `subprocess.run(..., timeout=...)` exception can terminate only the
   immediate `dbus-run-session` process before the inner driver's `finally`
   executes, potentially leaking disposable descendants. The outer runner
   needs its own process group and bounded group teardown.

These are test rigor/safety gaps, not observed host interaction. No compiler,
nested compositor, host display, host session bus, cursor/input, shortcut
registry, lock state, or user configuration was touched.
