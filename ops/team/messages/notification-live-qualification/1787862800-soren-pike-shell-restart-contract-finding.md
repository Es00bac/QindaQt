# Material finding: independent shell restart needs supervisor recovery policy

- **Timestamp:** 2026-08-27T14:33:20-06:00
- **From:** Soren Pike
- **To:** Manager and session/shell owners
- **State:** production-contract finding; no change applied yet
- **Exact base:** `c4982697858c083828bd406f1aa56c4e942bcc10`

The queued acceptance outcome requires an independent production-shell restart,
but the current `SessionProcessSupervisor` tears down the notification host and
ends the compositor session on any shell exit. A test cannot restart the shell
without either bypassing descriptor authentication or changing production
session policy: the presentation token correctly never leaves the supervisor,
and an external driver cannot provision a replacement shell.

The smallest production-consistent repair is a bounded supervisor-owned shell
recovery policy:

1. retain the generated presentation token only in supervisor memory while the
   session is live;
2. on one unexpected shell exit, keep the authenticated resident host alive and
   start a replacement shell through `TokenizedProcessLauncher` with a fresh
   one-shot descriptor and the same exact compositor-PID argument;
3. require a bounded restart window/budget, and tear down the host/session if
   replacement start fails or the replacement crashes again before stability;
4. preserve current fail-closed parent-death witnessing for every replacement;
5. clear retained token/restart state during `stop()` before child teardown.

The host already releases its presenter binding when the old shell's unique
bus name disappears, and the replacement can authenticate with the same token.
This changes an accepted essential-process boundary and therefore needs an ADR,
updated supervisor tests (host PID continuity, fresh shell PID, token secrecy,
budget exhaustion, stop/restart races), and notification/session wiki updates.

I will continue designing and implementing the independent driver portions
that do not depend on this decision. Unless the manager amends the queued
outcome, the exact acceptance wording authorizes this bounded recovery slice;
it cannot be truthfully replaced by a whole-session restart claim.

