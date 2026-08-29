# Notification-live static gaps repaired

- **Timestamp:** 2026-08-27T16:21:33-06:00
- **From:** Soren Pike, notification live qualification
- **To:** Manager and future exact-commit reviewer
- **State:** repaired source-only checkpoint; compiler/nested lanes remain held

The five gaps in `1787868689-soren-pike-static-audit-gaps.md` are repaired:

1. Settings1 recovery now seeds one normal, non-transient host record while DND
   suppresses its popup. Its exact ID crosses the Python phase boundary. The
   replacement shell must freshly authenticate and report exactly Active=1,
   Popup=0, Recent=0 before that ID is closed through the standard host D-Bus
   path; the resulting Recent row is cleared through the keyboard-focused
   production control.
2. The session wrapper uses `shlex.quote()` for every POSIX-shell word. A unit
   test round-trips spaces, quotes, `$()`, backticks, semicolons, and variables
   through `shlex.split()` without execution.
3. In development mode only, the supervisor gives the replacement the exact
   predecessor shell PID. Evidence registration accepts only that current PID,
   waits boundedly for its exact owner-to-empty transition, and attempts D-Bus
   acquisition only with `DontQueueService` and `DontAllowReplacement`. Any
   other/racing owner or timeout fails closed; external replacement-PID
   authentication remains required.
4. The primary phase now requires `org.kde.kglobalaccel` to resolve to the exact
   nested KWin PID and records `globalAccelPid` before using the real KF6
   registry.
5. `dbus-run-session` and all descendants run in a new session/process group.
   Cleanup rejects non-leader, self, and host-group targets, then boundedly
   terminates/kills/waits for that exact group after success, failure, or
   timeout. Child diagnostics are redirected into disposable logs so stdout
   remains one JSON result.

ADR-0020 and the testing pages now state the two real containment layers:
shell admission authenticates development mode plus compositor PID/live
capabilities; the external harness creates and verifies the private bus/XDG/
display boundary. An environment marker is not described as bus attestation.
KScreenLocker tag `v6.6.5` source file/group/key provenance and natural focus
traversal wording are also explicit.

Current non-compiling evidence:

- notification-live Python unit: 7/7 pass, including exact process-group
  success/timeout cleanup;
- six Python driver modules compile with `py_compile`;
- `git diff --check`: pass;
- documentation validation: 44 documents pass;
- source-shape audit: 798 files, no warning or error.

No compiler, nested compositor, host Wayland/display, host session bus,
cursor/input, shortcut registry, lock state, uinput, or user configuration was
used.
