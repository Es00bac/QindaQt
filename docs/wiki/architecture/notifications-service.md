# Notification service

QindaQt implements a lean notification model, a separate freedesktop D-Bus
adapter, and a resident Core/DBus host executable. This is the implemented
service foundation for the Shell/customization milestone; popup/history
presentation and session supervision remain explicit next slices. The
dependency decision is recorded in
[ADR-0008](../adr/0008-lean-notification-service.md).

## Domain contract

`QindaQt::Notifications` is single-thread confined and receives an injected
monotonic clock and output backend. A trusted construction-time revision seed
allows exhaustion to be qualified without wrapping a 64-bit counter; request
clients cannot influence it and it restores no notification entries. The model
publishes immutable shared snapshots in ascending notification-ID order. Each
successful model mutation increments one revision; a rejection changes no
snapshot, generated-ID sequence, retained-byte count, or backend event stream.

Typed operations cover:

- new and replacement submissions;
- application-owned close, trusted user dismissal, and exact close reasons;
- resident and non-resident action behavior with optional activation tokens;
- default, critical, requested, capped, and never-expire timeouts; and
- one batched expiration pass for all items due at the same clock reading.

The service returns the next monotonic expiration deadline. The resident host
owns a replace-on-arm one-shot scheduler and calls `expireDue()`; the domain
never starts an ambient event source. Backend callbacks may inspect the
already-published snapshot but cannot reenter mutation.

## Admission and memory bounds

Requests must contain well-formed UTF-16 without embedded NUL characters and
fit per-field UTF-8 byte limits. Action keys are unique and bounded. RGB/RGBA
image data must use eight-bit channels, consistent row stride, exact payload
size, bounded dimensions, and the aggregate image cap.

The configured active-notification count and aggregate retained payload have
hard upper ceilings. Independent per-source ceilings default to 64 active
notifications and 32 MiB retained payload, compared with global defaults of
256 and 64 MiB. A source is the authenticated unique D-Bus sender name, never
an application-supplied field. This prevents one sender from filling every
persistent slot or consuming the complete payload budget, preserving headroom
for peers. Replacement accounts for the old entry before both global and
per-source admission and therefore remains available at a count ceiling. A
private ledger keeps one bounded record per source with active entries, so
quota checks do not rescan active entries. Each accepted entry's payload byte
count is retained separately, so replacement and every removal path also avoid
re-encoding retained text or traversing image bytes.

Server-generated IDs advance monotonically and skip any currently active
client-supplied `replaces_id`. Arbitrary explicit IDs are not retained in a
global lifetime history: the protocol invalidates an ID when its notification
closes, and retaining unbounded sparse caller values would let one source deny
future explicit IDs to every peer. This distinction preserves generated-ID
non-reuse without turning closed attacker-chosen values into permanent state.

QindaQt deliberately does not discard notifications when their sender
disconnects: a notification may remain useful after its short-lived producer
exits. Per-source accounting remains attached to retained entries until close,
dismissal, action-close, or expiry. A hostile process can reconnect under
multiple unique names, so the global bounds remain the ultimate memory limit;
bus policy and supervisor abuse controls are a later defense layer. Revision
exhaustion, a regressing clock, malformed data (including an invalid typed
urgency value), ownership violations, and backend reentrancy all fail without
partial state.

## D-Bus adapter

`QindaQt::NotificationDBus` implements specification 1.3 at
`/org/freedesktop/Notifications`. Registration is explicit and reports service
or object ownership failures. The caller's unique bus name is injected as the
source identity; it is never accepted from a payload field.

The adapter supports `GetCapabilities`, `GetServerInformation`, `Notify`, and
`CloseNotification`, plus `NotificationClosed`, `ActionInvoked`, and
`ActivationToken` signals. It decodes the standard urgency, category, desktop
entry, image path/data, sound, resident, transient, action-icon, and
suppress-sound hints while ignoring unknown hints. Missing-notification close
uses the specification's empty D-Bus error body.

Default capability advertisement is intentionally only `body`. The session
host may add `actions` when it composes an accessible action presenter. It must
not advertise markup, hyperlinks, inline images, sound, or persistence merely
because the model can retain related hint data.

## Presentation transport boundary

The standard freedesktop object is an application-to-server protocol. It does
not provide a shell-facing snapshot feed or trusted user-interaction methods.
The injected `NotificationBackend` is deliberately process-local: it lets the
resident host compose an adapter and lets tests observe publications, but it
does not authorize linking notification implementation libraries into
`qindaqt-shell`.

Before popup/history presentation is implemented, QindaQt must add a versioned
private IPC adapter in the resident host and a public service client in the
shell. The protocol must carry immutable snapshot revisions, resynchronization,
dismissal, and action invocation, with bounded messages and explicit peer
authentication, errors, timeouts, backpressure, disconnect, and host-restart
semantics. The adapter remains presentation-neutral; QML consumes only the
client's public values.

## Resident host

`QindaQt::NotificationHost` composes the standard server with an injected
deadline scheduler. `qindaqt-notification-host` is a `QCoreApplication`; it is
not embedded in the shell or compositor. Startup validates the complete D-Bus
well-known-name grammar before consulting the bus and distinguishes an invalid
service name, invalid policy, unavailable or failed bus query, existing owner,
registration failure, and initial scheduling failure. Registration and initial
scheduling are fail-closed: failure releases both the object path and
well-known name.

Every model publication reconciles the one absolute expiration deadline.
Rearming replaces the old callback, a persistent/no-deadline model cancels it,
an early event-loop wake rearms the remainder, and stop/destruction cancels the
callback before releasing D-Bus ownership. Post-start scheduler or expiration
failures disarm the deadline and remain visible through a typed runtime state;
session-supervisor restart policy is not yet implemented.

The executable is built and installable but is not yet automatically started
by `qindaqt-wm`, a D-Bus activation file, or a user service. Its default
capability remains `body` because no accessible action presenter is composed.

## Qualification and remaining work

Focused tests cover submission/admission, thousands of sparse explicit-ID
lifecycles without lifetime-state exhaustion, active-ID collision avoidance,
generated-ID monotonicity, replacement/ownership, immutable snapshots,
global and per-source count/payload limits, typed urgency validation, timeout
and clock failures, quota recovery through close/action/expiry, seeded 64-bit revision
exhaustion, and callback reentrancy. A real private `dbus-daemon` test uses
independent callers to verify identity, protocol methods, malformed-call
errors, publications, and standard signals.

Host tests add real private-bus ownership conflict/rollback/release behavior,
including object-path reuse after initial scheduler failure; one-shot deadline
rearm/cancel/early-fire behavior; post-start arm and expiration failure states;
typed malformed-name, invalid-policy, and
disconnected-bus failures; and QTimer replacement cancellation observed beyond
the superseded deadline plus destruction cancellation.

The following are not yet implemented:

- shell popup, notification center, keyboard/accessibility presentation, and
  do-not-disturb/inhibition policy;
- the versioned private resident-host adapter and shell client required for
  snapshot publication, resynchronization, dismissal, and actions;
- automatic session supervision, D-Bus activation, and post-start bus-loss
  recovery;
- sound, disk history, settings persistence, lock-screen redaction, and restart
  migration;
- portal routing and third-party `notify-send` qualification; and
- inline reply/vendor extensions.

These omissions keep the roadmap honest: protocol, bounded state, resident
ownership, and expiration exist, but notifications are not yet a complete
user-visible desktop feature.
