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
   active, popup, and recent list models.
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
`body` freedesktop capability. Asynchronous operation errors and client-busy
state do not yet have visible QML feedback; an optimistically hidden popup is
not restored after a later rejection.

## Surfaces and entry points

Both windows currently use the primary output, top-right anchors, overlay
layer, a 16-logical-pixel top/right margin, zero exclusive zone, on-demand
keyboard interactivity, disabled activate-on-show, and separate
`notification-popup` and `notification-center` scopes. Preferred sizes are
400 logical pixels wide for popups and 440 by 640 for the center. A pure
planner clamps both to the output's logical geometry, including a
1920x1080 mode exposed by Qt as 960x540 at 200% scaling, and rejects geometry
too small to retain usable controls.

The popup **History** button opens the center while a popup is visible. A live
clock applet toggles the center in profiles that contain it. This is not yet a
complete keyboard entry path: production panels intentionally request no
keyboard interactivity, and a custom profile may omit the clock. A dedicated
status applet or compositor/global shortcut, focus transfer, and end-to-end
assistive-technology verification remain required.

## Qualification boundary

Pure model tests cover baseline-without-replay, new and replacement popups,
urgency ordering, monotonic expiry, center suppression, popup/history bounds,
transient exclusion, and client operation preflight. Pure surface-layout tests
cover 1080p, WUXGA, 1440p, 200% logical geometry, compact clamping, and minimum
usable geometry. Offscreen QML tests instantiate cards, popup and center
surfaces, exercise active and popup delegates, verify literal plain-text body
rendering, and keep overflow plus Dismiss inside the card.

This milestone did **not** run a live or nested compositor and did not inject
input. The following remain unqualified or unimplemented:

- real layer-role mapping, placement, focus, keyboard navigation, and visual
  baselines at the reference resolutions;
- multi-output placement policy, per-output histories, and output migration;
- do-not-disturb/inhibition, lock-screen redaction, sound, and safe image/icon
  loading;
- persistent history/settings, D-Bus activation, child/bus-loss restart, and
  operation-error presentation;
- activation-token acquisition, portal routing, inline reply, and vendor
  extensions; and
- a persistent center entry point independent of a particular applet.

These limits are deliberate status statements, not final product behavior.
