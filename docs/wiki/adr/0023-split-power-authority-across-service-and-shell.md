# ADR-0023: Split power authority across Power1 and the shell

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** Platform power services and shell session actions

## Context

Battery/profile observation, idle reporting, caller-relative session actions,
and hardware-key handling have different upstream authorities and privilege
subjects. Combining them in one service would either make `Power1` claim
authorization it does not possess or make the shell own long-lived hardware
adapters.

## Decision

Create one bounded `org.qindaqt.Power1` service for battery, power-source,
profile, sanitized inhibitor, keyboard-backlight, idle, and provider truth.
The shell separately invokes logind session actions, queries every `Can*`
value as the actual caller, and owns the all-or-nothing `handle-*` inhibitor
transaction. `Power1` holds no inhibitor in version 1 and exposes no `Can*`
field. There is no `Brightness1` process.

Transport adapters remain small collaborators; the service object only owns
orchestration and snapshot lineage. See
[Power and brightness architecture](../architecture/power-service.md).

## Consequences

- Authorization truth remains caller-relative and cannot become stale service
  state.
- The resident service remains testable on a private bus without shell UI.
- Shell key handling degrades atomically to logind defaults when any lock is
  absent or lost.
- Cross-process values require fixed bounds, epochs, revisions, and uncertain
  result handling before implementation may be called wired.
