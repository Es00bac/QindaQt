# Ordinary compositor attachment and idle observation

[ADR-0305](../adr/0305-public-ordinary-compositor-attachment.md) and
[ADR-0307](../adr/0307-public-ordinary-fd-idle-observation.md) extract reusable
platform boundaries from the keyring daemon. Candidate public targets are
additive; each consumer owns its separately reviewed migration and policy.

## Selected ordinary identity and lifetime

The public `QindaQt::CompositorAttachment` target exposes
`QindaQt::Platform::Compositor::CompositorAttachment`. Constructor-visible
`SessionAdmission` selects an already accepted session unique owner. Admission
is readonly/non-reentrant and must not query or mutate its own attachment. `attach`
takes that owner, a canonical native socket basename and optionally an
independently accepted expected compositor owner/PID. It proves the actual
ordinary UNIX peer matches exact daemon-resolved compositor owner/PID and same
UID, in a private owner-only runtime reached without symlinks. Retained socket
and kernel PIDFD join current owner/session liveness; no bare UID/service or
native lock payload admits authority. Callback captures and bus dependencies
are same-thread; synchronous daemon calls are bounded by their 250ms timeout.

`identity()` returns an optional session owner/compositor owner/PID/basename
only while current admission and lifetime remain valid. `openConnection()`
transfers one CLOEXEC ordinary connected FD or -1. Consumers use this actual FD,
never a pathname reconnect. Loss suppresses getters before queued watchers run;
periodic checking and owner signals notify revocation. Attaching/revoking changes
generation, so reentrant loss cannot report false success.

This boundary proves selected ordinary identity/lifetime, not executable or
independent supervisor-process attestation. Existing consumer first-owner policy,
PAM system-owner approval and native private locker trust keep their own owners.
No legacy fallback, privileged lock connection, authentication or unlock API
exists on this platform target.

## True compositor idle

`QindaQt::IdleObservation` exposes `QindaQt::Platform::Idle::IdleObservation`
and `WaylandIdleObservation`. Borrowed same-thread callbacks transfer an ordinary
FD from a live attachment and check current lineage. The lineage callback is
readonly/non-reentrant and cannot query or mutate its observer. Signal handlers
remain reentrant for the documented rearm/refresh/revocation operations.
The adapter owns its FD,
including libwayland failure paths, and privately speaks standard
ext-idle-notify-v1 without Qt GUI/QML or an input-method dependency.

Timeouts request fresh notifications; elapsed local time never supplies idle.
The public observer provides same-thread timeout, availability, idle, refresh
and revoke operations so a policy owner can disable/rearm it without reaching
into the Wayland implementation. Exactly one wl_seat and one notifier must exist. Missing/duplicate globals,
dynamic second-seat/notifier, global removal, peer failure and lineage loss make
both availability and idle false. A handshake deadline only fails unavailable.
Invalidation precedes deferred teardown during dispatch. Callback identity and
generation reject prior queued events; reentrant timeout changes and refresh
remain safe. Retained notification contexts are bounded to 64 per connection,
then the ordinary connection rotates through current admission.

The platform owns no persistence, policy timer, inhibition, RequestLock, sleep
or unlock authority. Native lock state remains in the separate
[readonly observer](native-session-lock.md). Keyring, Power1 and native lock
policy consumers compose these public boundaries without reaching daemon-private
headers or sharing the privileged locker connection.

See [module boundaries](module-boundaries.md) and the focused
[testing harness](../development/testing-harness.md).

The additive `sameBus(connection)` query compares daemon `GetId` replies with
explicit 250 ms call timeouts while rechecking live attachment admission. This
is necessary for manual native requests because owner/PID strings can collide
across two independent bus daemons. A queued initial advertisement of the
already admitted owner does not revoke a freshly attached display; actual loss
of the retained owner still revokes even when it reclaims its service name.
