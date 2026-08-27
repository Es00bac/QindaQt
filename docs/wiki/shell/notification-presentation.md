# Notification presentation

QindaQt's production shell now presents authenticated notifications in a
bounded popup stack and an active/recent notification center. This page owns
the shell behavior and its current limitations. Submission, resident service
policy, and the private transport are documented in
[Notification service](../architecture/notifications-service.md); the exact
wire contract is in
[Notification presentation protocol 1](../reference/notification-presentation-v1.md).

## Runtime path

The production path is:

1. `qindaqt-session` generates one presentation token and sends independent
   copies to the host and shell through inherited one-shot descriptors.
2. The host publishes snapshots through its private, token-bound D-Bus object.
3. The owner-bound asynchronous shell client authenticates and accepts only a
   coherent owner, epoch, and monotonic revision lineage.
4. `NotificationPresentationController` projects that snapshot into separate
   active, popup, and recent list models using an injected shell-owned
   interruption policy.
5. The shell window controller maps popup and center QML as nonexclusive
   LayerShellQt overlay surfaces.

The client, model, and surfaces are constructed only when the shell receives a
valid inherited descriptor. A standalone shell or host does not silently open
the private presentation path. The shell links the public client and model; it
never links the notification-service implementation.

## Model behavior and bounds

The first accepted snapshot for an owner/epoch becomes a baseline. Its entries
appear in Active, but are not replayed as new popups. New or replaced entries
after the baseline enter the popup model in urgency-descending, newest-first
order. Popup deadlines use a monotonic clock and default to four seconds for
low urgency, six seconds for normal urgency, and ten seconds for critical
urgency.

At most eight popup entries are retained and at most three are visible. Opening
the center clears the current popup stack. Updates received while the center is
open update Active directly and are not replayed after the center closes.

When an observed active item disappears, a non-transient copy is prepended to
Recent. Recent is in memory, capped at 100 entries, and excludes transient
notifications. It survives a host reconnect only while the same shell process
continues. It is lost on shell restart and can miss removals that occur while
the client is disconnected. **Clear history** changes only this local model; it
does not close active notifications or modify application state.

## Do Not Disturb

Do Not Disturb is an implemented, session-volatile popup policy. It starts off
for every new production-shell lifetime. Enabling it immediately filters low-
and normal-urgency entries from the current popup stack and suppresses later
low/normal popups. Critical urgency (`2`) explicitly bypasses the filter;
unknown in-process urgency values fail closed while the policy is enabled.

The Active model continues to reflect the authenticated host snapshot, and
disappearing non-transient items still enter Recent. The policy never dismisses,
closes, or removes a notification from the host. Disabling Do Not Disturb does
not replay entries received or filtered while it was active because the
controller preserves its prior-snapshot baseline. Only a later new or replaced
entry is reconsidered. Consequently, a replacement promoted to critical may
appear while Do Not Disturb is on, while one demoted from critical is removed
from the popup projection immediately.

The notification center exposes the writable, tab-focusable control with an
accessible description of the critical bypass. The panel applet receives only
a read-only policy flag: it shows a moon indicator and includes Do Not Disturb
in its accessible open/close label, but cannot change the policy. Persistence,
schedules, per-application exceptions, inhibition integration, and lock-screen
policy are not implemented. Future persistence must arrive through
`org.qindaqt.Settings1`; it must not be added to the policy module or private
notification wire. The boundary is accepted in
[ADR-0010](../adr/0010-inject-shell-notification-interruption-policy.md).

## Cards and operations

Cards render application name, summary, and body as plain text. Markup is not
interpreted. The current identity is an application-name initial; image paths,
inline images, and icon files are not loaded. Critical urgency receives a
distinct border, and the card exposes alert metadata and tab-focusable Qt
Quick Controls.

Two primary actions fit directly on a card. Additional protocol-valid actions
remain available through a bounded-width **More** menu, and long labels are
elided visually while retaining their accessible names. **Dismiss** remains
reachable regardless of whether an application supplied actions. Popup close
only hides that local popup; Dismiss and application actions go through the
authenticated client and host.

Action invocation currently supplies an empty activation token. The action may
run, but focus transfer is not promised, and the host still advertises only the
`body` freedesktop capability.

Only one dismiss or action request may be outstanding. While it is outstanding,
the card controls are disabled and both notification surfaces may show a plain-
text **Working…** status. Starting an operation pauses popup expiry. The popup is
removed only after a valid success reply; a rejection retains the card, renews
its complete urgency-specific display interval, and leaves its controls
available for retry.

The client accepts an operation result only when its exact fields and ID match,
`revisionBefore` is no older than the snapshot that initiated the request, and
`revisionAfter` does not regress. Dismissal and non-resident actions must advance
the revision. A resident action may legitimately preserve it because the
notification remains active. Timeout, malformed reply, and ordinary remote
failure reject the operation and trigger an authoritative snapshot fetch. If a
snapshot request is already in flight, the client marks it dirty and fetches
again afterward so a reply predating the operation cannot become the final
state. An authorization failure instead discards the binding and re-registers.

An owner change publishes the new unavailable/authenticating lineage before it
rejects the old-owner operation. Replies from the former owner or operation
token are ignored. Presented remote errors are trimmed, replace NUL or malformed
UTF-16 with safe replacement characters, and are capped at 512 UTF-16 code units
including a truncation ellipsis. They render as plain text and clear after eight
seconds or the next successful operation.

## Surfaces and entry points

Both windows currently use the primary output, top-right anchors, overlay
layer, a 16-logical-pixel top/right margin, zero exclusive zone, on-demand
keyboard interactivity, and separate `notification-popup` and
`notification-center` scopes. Popup stacks always disable activate-on-show so
an incoming notification cannot steal focus. The center alone requests
activate-on-show when mapped. Preferred sizes are
400 logical pixels wide for popups and 440 by 640 for the center. Popup height
is a 38-logical-pixel header plus at most three 146-logical-pixel cards. The
runtime keeps the 38-pixel header-only surface mapped while an operation is
busy or rejected even when no popup card remains. A pure planner clamps both to
the output's logical geometry, including a
1920x1080 mode exposed by Qt as 960x540 at 200% scaling, and rejects geometry
too small to retain usable controls. Popups retain a 240-logical-pixel minimum
usable width. The center requires 384 logical pixels so its Do Not Disturb,
History, and Close header controls remain usable; a compact 400x300 output
therefore produces a clamped 384x284 center.

The popup **History** button opens the center while a popup is visible. A
dedicated notification-center applet now appears exactly once in each of the
ten stock profiles. Its manifest requests no capabilities; the production
renderer receives only a shell-owned facade that requests a center toggle and
mirrors open state plus read-only Do Not Disturb state. Notification records,
dismiss/action operations, interruption-policy mutation, and the presentation
controller remain outside the applet boundary. A custom profile
may remove or omit this entry. If the shell starts without the supervisor's
authenticated presentation descriptor, the facade is absent and the button is
disabled.

Once the authenticated presentation client has started, the shell also owns a
stable `qindaqt_toggle_notification_center` action with default `Meta+N` and
registers it through KF6 GlobalAccel using autoloading semantics. This entry
path does not depend on the selected profile and preserves a user's remapped or
disabled binding. KGlobalAccel setter acceptance and an observable active
binding are tracked separately: acceptance is not proof that the service is
active, while an empty binding may be intentional disablement, a conflict, or
service absence and is not forcibly reclaimed. The boundary is recorded in
[ADR-0009](../adr/0009-use-kglobalaccel-for-shell-shortcuts.md).

The center seeds its close button as the initial focus target only after the
window becomes active and no item already owns focus. A window-scoped Escape
shortcut closes it without changing the global binding. These are structural
keyboard paths, not yet a qualified live keyboard experience: no isolated
nested-Wayland run has proved that the compositor accepts activation, that
focus traversal works end to end, or that GlobalAccel remapping, conflicts, and
assistive-technology operation behave correctly.

## Qualification boundary

Pure model tests cover baseline-without-replay, new and replacement popups,
urgency ordering, monotonic expiry, center suppression, popup/history bounds,
transient exclusion, serialized operations, success-only removal, rejection
retention and renewal, bounded error lifetime, immediate Do Not Disturb
filtering, the critical bypass, urgency-changing replacements, Active/Recent
retention, no replay on disable, service-owner/epoch rebaseline, and rejection
of an in-flight operation after its popup becomes suppressed. Focused policy
tests cover default-off session lifetime, change notification, and total
urgency admission. Client
tests cover resident equal-revision success, advancing-operation validation,
timeout/malformed/remote-error recovery, owner replacement, and stale replies.
Pure surface-
layout tests cover 1080p, WUXGA, 1440p, 200% logical geometry, compact clamping,
the zero-popup 38-pixel status plan, the center's 384-pixel minimum usable
width and compact 400x300 result, and minimum usable geometry. Offscreen QML
tests instantiate cards, popup and center surfaces, exercise active and popup
delegates, verify literal plain-text body and operation-error rendering, disable
controls while busy, and keep overflow plus Dismiss inside the card.

The notification-center entry tests use an injected registrar to prove the
facade's center-toggle/open-state and read-only Do Not Disturb boundary, stable
action identity and `Meta+N` default, dispatch, setter-request status, and
active-binding state changes without touching the developer's shortcut
registry. A second offscreen QML
test proves the applet's disabled fallback, accessibility label changes, narrow
toggle call, read-only Do Not Disturb state/indicator, and compiled-entry-point
dispatch without compositor or pointer input. The notification-surface
offscreen test proves the center's writable, accessible Do Not Disturb control,
explicit bidirectional header focus chain, compact 384x284 busy/error geometry,
window-scoped Escape route, and focusable initial target without activating a
real surface. Catalog, resolver, and profile tests prove the empty capability
request, audited registry entry, and exactly one instance in every stock
profile.

This milestone did **not** run a live or nested compositor and did not inject
input. The following remain unqualified or unimplemented:

- real layer-role mapping, placement, compositor activation acceptance, focus
  traversal, global-shortcut dispatch/remapping, and visual baselines at the
  reference resolutions;
- multi-output placement policy, per-output histories, and output migration;
- Do Not Disturb persistence/scheduling/inhibition, lock-screen redaction,
  sound, and safe image/icon loading;
- persistent history and settings, D-Bus activation, and child/bus-loss restart;
- activation-token acquisition, portal routing, inline reply, and vendor
  extensions.

These limits are deliberate status statements, not final product behavior.
