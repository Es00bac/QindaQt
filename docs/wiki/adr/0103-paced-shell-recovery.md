# ADR-0103: Preserve the compositor session during shell recovery

- **Status:** Accepted
- **Date:** 2026-09-07
- **Owners:** Session supervisor and notification presentation
- **Supersedes:** [ADR-0019](0019-restart-the-production-shell-once.md)
- **Superseded by:** None

## Context

The notification host owns the session's freedesktop service and active
in-memory records. A shell crash is recoverable, but the previous fixed
one-restart budget ended the compositor session after a second crash or a
replacement launch failure. That behavior discarded applications and made a
short shell failure equivalent to compositor death.

## Decision

After successful initial session startup, the supervisor keeps the notification
host and in-memory presentation token resident while the host remains healthy.
An unexpected shell exit schedules a
replacement with a fresh one-shot token descriptor, the same validated
compositor PID, and the same profile and theme arguments. Replacement retries
use exponential delays of 1, 2, 4, 8, 16, then 30 seconds, with 30 seconds as
the permanent cap. A shell must run for 30 seconds before the delay and retry
counter reset. A failed launch remains recoverable and schedules the next
retry; shell failures after that successful startup alone never end the
session. Initial host or shell startup failure still rolls back the partially
started session, releases the token, and returns an error to the launcher.

The host's unique-name handling still requires every replacement to
authenticate through the normal presentation path. The token stays in
supervisor memory and never enters argv, environment, files, signals, or
diagnostics. Explicit stop, notification-host exit, supervisor death, and
compositor parent death stop the session and cancel pending retries. During a
replacement delay the session remains running for resident-host purposes, but
`Session1.CanLogout()` remains unavailable because no live shell PID can be
authorized.

## Consequences

Applications and active notification records survive repeated shell crashes and
temporary launch failures. Shell-local transient UI state still starts fresh
after each replacement. Retry pacing prevents a tight loop and the cap bounds
the time between attempts without imposing a destructive lifetime attempt
limit. Logs identify each scheduled delay and successful replacement for
operational diagnosis.

Focused process tests cover repeated crashes, host PID continuity, failed
replacement launch, stable-run reset, stop during the retry timer, host exit,
and rollback when the initial shell cannot start.

## Revisit when

Revisit if the supervisor can checkpoint shell security state transactionally,
if a service manager can restart the shell without widening token authority, or
if measured failure data requires a different pacing or stable-run threshold.
