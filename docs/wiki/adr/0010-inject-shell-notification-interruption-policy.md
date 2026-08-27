# ADR-0010: Inject shell-side notification interruption policy

- **Status:** Accepted
- **Date:** 2026-08-26
- **Owners:** Shell and notification presentation
- **Supersedes:** None
- **Superseded by:** None

## Context

Do Not Disturb changes whether the shell interrupts the user with a popup; it
must not discard an application's notification, mutate the resident host, or
weaken the authenticated presentation boundary. Putting this decision in the
freedesktop notification server or its private wire protocol would couple a
local presentation preference to service admission and make the host responsible
for shell UI state. Putting it directly in QML would duplicate policy across
surfaces and make nonvisual tests difficult.

The initial slice also precedes the `org.qindaqt.Settings1` service and settings
center. It needs an honest default and lifetime without inventing persistence
that does not exist.

## Decision

Use an injected, shell-side notification interruption policy between accepted
presentation snapshots and the popup projection. The policy is a focused Qt
Core module with no transport, host, QML, persistence, or lock-screen
responsibility. The production shell owns one instance and injects it into the
presentation controller.

Do Not Disturb is session-volatile and disabled by default. While enabled, only
protocol-valid critical urgency (`2`) may enter or remain in the popup
projection. Low (`0`), normal (`1`), and unknown in-process urgency values are
suppressed. Enabling the policy filters the current popup stack immediately.
Active and Recent retain their normal behavior, and the shell never closes or
dismisses a notification merely because interruption is suppressed.

Disabling Do Not Disturb does not replay notifications received or filtered
while it was enabled. Only a later new or replaced notification is reconsidered
under the then-current policy. A replacement that becomes critical may appear;
a replacement that ceases to be critical is removed immediately while Do Not
Disturb remains enabled.

The notification center owns the writable control. The capability-empty panel
applet may observe a read-only Do Not Disturb flag through its existing narrow
shell facade so it can expose an indicator and accessible status, but it cannot
change policy or read notification records.

This decision changes neither `org.freedesktop.Notifications` nor
`org.qindaqt.NotificationPresentation1`. A future `org.qindaqt.Settings1`
adapter may initialize and update the injected policy from a dedicated setting;
the policy module will remain persistence-neutral. Lock-screen redaction and
interruption rules remain a separate pending policy.

## Consequences

- Host admission, expiry, Active state, authenticated snapshots, and protocol
  schema remain stable while the shell can change interruption behavior
  immediately.
- The state resets to off when the shell starts until settings persistence is
  implemented; the UI must not imply that the current choice survives login or
  shell restart.
- Critical notifications explicitly bypass Do Not Disturb. Future sound,
  scheduling, inhibition, and portal work must decide how to compose with that
  rule rather than silently changing it.
- Pure policy and presentation-model tests can cover urgency, immediate
  filtering, replacements, retention, and no replay without D-Bus or a display.
  Offscreen QML tests cover the writable center control and read-only applet
  indicator; live focus, input, and compositor behavior remain separate gates.
- Persistence must be integrated through Settings1 composition, not by teaching
  the policy module to read files or call the settings UI.

## Revisit when

Revisit the exact critical bypass or state lifetime when user research,
accessibility requirements, notification-inhibition standards, or a shipped
Settings1 schema requires schedules, per-application rules, lock-screen modes,
or synchronized policy across multiple shell processes.
