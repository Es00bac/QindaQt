# Notification service

QindaQt implements a lean notification model, a separate freedesktop D-Bus
adapter, a resident Core/DBus host executable, and an authenticated private
shell-presentation protocol with an owner-bound asynchronous client. The
essential-session supervisor provisions its token through inherited one-shot
descriptors. A separate bounded shell model now drives initial production
popup and active/recent-center surfaces, with an injected shell-side
interruption policy controlling popup admission. This is an implemented
vertical slice, not a complete notification subsystem. The service dependency
decision is recorded in
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

The resident host now has an optional `org.qindaqt.NotificationPresentation1`
object on the same well-known owner as the freedesktop object. A 256-bit token
handshake binds one unique D-Bus presenter. Only that sender may fetch complete
snapshots, dismiss an item, invoke an action, or release the binding. The host
watches unique-name disappearance so a restarted shell may authenticate. It
sends change signals only to the bound destination, with epoch/revision but no
notification content.

Snapshots use an exact schema version, canonical restart UUID, monotonic
revision, ascending nonzero IDs, bounded text/actions/count, and strict value
types. They deliberately omit authenticated producer identity and inline image
bytes. The shared decoder handles both process-local variants and nested
QtDBus arguments with bounded collection parsing. Full method, field, error,
and restart behavior is in the
[presentation protocol reference](../reference/notification-presentation-v1.md).

The corresponding owner-bound asynchronous client is implemented as a separate
library. It subscribes to the exact unique owner before authenticating, rejects
late-owner replies and regressing or malformed snapshots, coalesces targeted
invalidations, enforces request timeouts, retries with bounded backoff, and
validates dismiss/action requests against its accepted snapshot. Operations are
serialized. Results must match the requested ID and initiating lineage;
dismiss/non-resident results advance the revision, while only a resident action
may preserve it. A timeout, malformed result, or non-authorization remote
failure rejects visibly and forces an authoritative snapshot, including a
second fetch when an older request was already in flight. Authorization loss
restarts registration. Owner replacement publishes its new lineage before
rejecting the interrupted operation, and stale old-owner replies are ignored.

The production shell composes the client only when a descriptor token was
supplied. The adapter stays presentation-neutral; a separate presentation-model
library projects only client public values into active, popup, and in-memory
history models. It pauses popup expiry during the one outstanding operation,
removes the originating popup only on success, and retains plus renews it after
rejection. Remote text is normalized and capped to 512 UTF-16 code units before
the model exposes it, and the error clears after eight seconds. QML consumes
those public models and status values, never the service implementation
library. The first owner/epoch snapshot is baselined without replaying older
items as new popups. Detailed UI behavior and limits are in
[Notification presentation](../shell/notification-presentation.md).

Do Not Disturb remains entirely on the shell side of this boundary. Settings1
persists and publishes the preference; a scoped shell bridge applies confirmed
values to the focused interruption policy. Low/normal popups are suppressed,
critical urgency bypasses, Active/Recent remain, and disabling does not replay.
Neither the host nor private notification protocol gains a setting or field.
See [ADR-0012](../adr/0012-persist-notification-quieting-through-settings1.md).

Lock privacy is a separate, higher-priority shell policy and likewise does not
change the presentation wire. The session supervisor supplies KWin's direct
kernel-parent PID alongside the token descriptor. A focused asynchronous
observer accepts lock state only when `org.qindaqt.Compositor`,
`org.freedesktop.ScreenSaver`, and `org.kde.screensaver` share one exact unique
owner whose bus-daemon PID matches that provisioned value. State is private by
default and during every unknown, locking, locked, owner-change, or failure
condition. Denial clears all public projections and operations; unlock consumes
the current snapshot as a fresh no-replay baseline. Critical urgency cannot
bypass this boundary. See
[ADR-0011](../adr/0011-gate-notifications-on-authenticated-lock-state.md).

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
post-start service restart policy is not yet implemented.

The executable is built and installable. A production-shell build makes
`qindaqt-session` the launcher's default `--exit-with-session` process. That
supervisor generates one token in memory, sends independent copies to the host
and first shell over bounded inherited pipes, arms a race-closed parent-death
witness, captures its direct KWin parent PID, and gives both children their own
supervisor-death witnesses. If the first shell exits, the host and its active
model remain resident while the supervisor starts exactly one replacement with
a fresh one-shot descriptor containing the retained token and the same KWin
PID. The host releases the former presenter's unique-name binding, so the
replacement must authenticate normally. Restart failure or another shell exit
ends the session; notification-host restart remains intentionally
unimplemented. See
[ADR-0019](../adr/0019-restart-the-production-shell-once.md).

Only the descriptor number and non-secret PID appear in shell arguments; the
token never enters argv, environment, a persistent file, a signal, or
diagnostics. A standalone host still receives no token, so its private object
is absent. Reads accept one exact record and time out after two seconds. The
default freedesktop capability remains `body`: the initial action controls do
not yet provide activation tokens or a screen-reader-qualified accessibility
path. The installed nested workflow qualifies real compositor keyboard focus
and QML Accessible roles/names without claiming a screen-reader session. Busy,
confirmed persistence rejection, and uncertain-operation presentation are
qualified through the production Settings1 DND transaction; no notification-
host mutation seam exists solely to synthesize a rejected action.

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

Presentation tests add strict codec/token rejection and a private-bus workflow
with a wrong-token peer, one authenticated presenter, a denied second peer,
directed revision delivery, bounded snapshot decode, authorized action and
dismiss paths, explicit release, disconnect handoff, and complete standard-name
rollback when the private object path cannot register.

Client tests add deterministic owner changes, late replies, malformed and
regressing snapshots, invalidation bursts, timeout/backoff, reauthentication,
operation validation, resident action results without a revision advance,
uncertain-result authoritative recovery, serialized busy state, owner-change
interruption, and release. A second private-bus workflow runs the real Qt client
transport against two successive resident hosts and proves targeted updates,
action-token forwarding, loss, new-owner authentication, and new epoch
acceptance. Descriptor and supervisor tests prove bounded one-shot reads,
secret-free arguments, two-child startup, resident-host PID continuity across
one shell PID replacement, fresh-launch failure teardown, exhausted-budget
shutdown, coupled parent death, and second-child startup rollback without
opening a display.

Presentation-model tests cover baseline-without-replay, new and replacement
popups, urgency ordering, monotonic expiry, center-open suppression, popup and
history caps, transient exclusion, operation preflight, busy serialization,
success-only removal, rejection retry preservation, bounded error lifetime,
and injected Do Not Disturb filtering without Active/Recent loss or replay.
Focused interruption-policy tests cover the default lifetime, change signals,
the exact critical bypass, unknown-urgency rejection while enabled, and
state-dependent admission. Privacy-policy/model tests cover default denial,
complete projection and status clearing, critical suppression, transport-free
operation rejection, in-flight outcome suppression, and fresh unlock
baselining. Lock-state tests add three-owner/PID authentication, stale-reply and
owner-change fencing, early locking, active transitions, double inactive
confirmation, bounded object retry, stop/restart behavior, and a real
asynchronous Qt transport against a private `dbus-daemon` with separate KDE
and freedesktop interfaces. The transport-loss case terminates only that
isolated daemon after the monitor reaches `Unlocked` and proves immediate
presentation revocation without an explicit monitor stop. Runtime-option and
supervisor tests require the
token descriptor and compositor PID as one validated argument bundle.
Pure logical-screen planning tests cover 1080p, WUXGA, 1440p, 200% scale,
compact clamping, a zero-popup 38-logical-pixel status surface, and minimum
usable dimensions. Offscreen QML tests exercise active and popup delegates,
literal plain-text body/error rendering, busy control disabling, and bounded
action/Dismiss placement. These focused rows open no display; the separately
registered `shell.notification-live.*` matrix stages the installed production
stack on a private bus and nested virtual compositor. It proves exact
KGlobalAccel dispatch/remapping, compositor-owned notification
roles/output/geometry, center activation, full forward/reverse keyboard focus,
Settings1 failure/restart behavior, the resident-host/single-shell-restart
contract, and actual nested KScreenLocker privacy without touching the host
desktop. Its development-only read-only observability is specified by
[ADR-0020](../adr/0020-authenticate-private-live-evidence.md).

The following are not yet implemented:

- complete accessibility-tree and screen-reader behavior;
- popup-safe icon/image loading and activation-token acquisition;
- post-start notification-host restart policy beyond fail-closed session exit;
- sound, persistent disk history, Do Not Disturb scheduling/inhibition, and
  physical desktop interaction qualification;
- physical-seat/real-desktop lock transition, multi-seat/session-switching,
  alternative-locker, and suspend/resume qualification, plus any future
  least-authority lock-screen presenter;
- multi-output notification placement and physical mixed-output proof;
- portal routing and third-party `notify-send` qualification; and
- inline reply/vendor extensions.

These omissions keep the roadmap honest: the supervised production shell has
a nested-qualified popup/center path, but notifications are not yet a complete
daily-use desktop feature.
