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
are same-thread. The proof's daemon calls carry an explicit per-call bound
(`AttachmentRecovery::callTimeoutMilliseconds`, 1 s by default); the
attachment never changes the connection's shared interface timeout.
The compositor's normal socket mode is `0755`: the owner-only `0700` runtime
directory prevents other users from reaching it, and socket mode must deny
group/other write access. Read/execute bits on the socket are not an attachment
failure. A live peer with the exact bus owner/PID remains mandatory.

`identity()` returns an optional session owner/compositor owner/PID/basename
only while current admission and lifetime remain valid. `openConnection()`
transfers one CLOEXEC ordinary connected FD or -1. Consumers use this actual FD,
never a pathname reconnect. Attaching/revoking changes generation, so reentrant
loss cannot report false success.

### Event-driven liveness and re-attachment

[ADR-0379](../adr/0379-make-session-liveness-event-driven-and-self-healing.md)
replaced the 100 ms daemon poll, which treated one slow reply as permanent
loss and left the 2026-10-15 session without a screen lock. Getters check the
kernel PIDFD and probe socket and the consumer's admission synchronously, and
use compositor ownership cached from `NameOwnerChanged`; they make no bus call.
Admission callbacks must therefore be cheap and make no bus call either.
Kernel loss and consumer admission still fail getters immediately; loss of the
admitted bus owner is published when the watcher delivers it. Every proof call
names a unique connection, which the daemon never reuses, so a stale cache can
only fail a call.

PIDFD and probe-socket notifiers, the compositor-name watcher and a watcher on
the selected session owner emit `revoked()` on genuine loss: peer exit, probe
hang-up, the admitted owner's release or replacement, or a display pathname
that no longer reaches the proven peer (`openConnection()` reports a transient
connect failure as -1 without detaching). The selection is retained and
`recovering()` is true: the attachment re-runs the full proof asynchronously,
including any independently expected peer, and emits `attached()` when it
succeeds. A failed or slow proof retries on the
[bounded schedule](session-resilience.md) (0.25 s doubling to 30 s,
`AttachmentRecovery` injects manual timers for tests); a new compositor owner
triggers an immediate attempt. `revoke()`, a new `attach()`, disconnection of
the selected session owner, refused admission, an expected peer's mismatch or
death retire the selection instead.

Admission withdrawal is published by event too. Consumers such as the shell
notification and clipboard observers admit only the current `Session1` owner.
The attachment matches `NameOwnerChanged` whose old owner is the selected
session owner, so whenever that owner gives up any well-known name it re-checks
admission and, when refused, retires the selection with `revoked()`. The
superseded poll found this within 100 ms; nothing polls now. A consumer whose
admission can change in any other way sees it in getters immediately and
publishes it by calling `revoke()` from its own event.

Losing the probe connection while the same compositor stays live and keeps its
name is genuine loss: in-flight work bound to that lineage (keyring prompts,
portal consent) is cancelled on `revoked()`, and the retained selection then
re-proves the same compositor and attaches a new lineage. A consumer that must
stay closed after any loss, like the shell and clipboard lock observers,
retires itself on `revoked()`.

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

The additive `sameBus(connection)` query answers true for the attachment's own
libdbus connection without a call; any other connection is compared by daemon
`GetId` replies with the explicit call bound while rechecking live attachment
admission. This is necessary for manual native requests because owner/PID
strings can collide across two independent bus daemons. A queued initial
advertisement of the already admitted owner does not revoke a freshly attached
display; actual loss of the retained owner still revokes even when it reclaims
its service name, and the reclaim then re-attaches.
