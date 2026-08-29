# Soren Pike observability gap checkpoint

- **Timestamp:** 2026-08-27T15:10:35-06:00
- **From:** Soren Pike, notification live qualification implementer
- **To:** manager and later exact-commit reviewer
- **State:** implementation correction in progress; no nested-run success is
  claimed by this record
- **Base:** `c4982697858c083828bd406f1aa56c4e942bcc10`

## Material finding

The first `notificationliveworkflow` draft proved that production input and
submission calls returned successfully, but it did not yet prove every required
user-visible postcondition. In particular, an injected key or returned
notification id is not evidence that the production shell mapped, focused, or
updated the intended surface.

Before any nested row runs, I am adding the narrowest development-gated,
read-only evidence boundary owned by the shell. It will expose observations
derived from the production presentation controller, privacy/DND state, and the
shell-owned notification windows. It will not expose QML mutation, accept test
commands, bypass presentation tokens, or exist outside a compositor-validated
development session.

The driver will assert:

- the exact center role, output, geometry, activation, focused item, and the
  complete named forward/reverse focus sequence;
- no dispatch for the disabled shortcut and dispatch only for the remapped
  shortcut;
- normal popup visibility, DND suppression with Active/Recent retention,
  critical bypass, and no suppressed-popup replay after disabling DND;
- visible busy, confirmed-rejection, uncertain-operation, last-confirmed, and
  fail-quiet outage states;
- lock-time clearing/denial of popup, center, status, and operation surfaces,
  followed by a double-inactive unlock baseline with no replay; and
- evidence-service PID continuity with the supervisor-reported replacement
  shell PID and fresh authentication after the one allowed shell restart.

The external harness will retain separate 100%, 125%, and 150% result rows; it
will not relabel a merely declared or ignored scale as applied evidence.

Heavy builds and nested sessions remain deferred under the manager's resource
coordination instruction. Source, documentation, and non-compiling checks are
the only active lane until released.
