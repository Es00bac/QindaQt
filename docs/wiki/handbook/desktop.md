# Desktop experience

QindaQt's central interaction is a hybrid window container. It groups related
windows without requiring those applications to implement their own tabs or
splits. Conventional independent windows remain available throughout.

## Grouped work

A container has an outer frame and ordered pages. Each page contains a tree of
horizontal or vertical splits ending in tiles, and each tile owns one member
window. Top-level tabs select pages. Moving the group moves the shared layout;
member title bars preserve local identity and detach affordances.

Docking, detaching, page selection, split reorganization, group actions, and
keyboard commands pass through atomic topology operations. Invalid candidates
must not appear as half-applied layouts. Empty branches collapse and singleton
structures normalize. Client size constraints restrict divider movement; an
impossible arrangement has a recoverable overflow state. Dialogs follow their
owning member, and a member exiting must not destroy peers' state.

Task and switcher presentation collapses a group to one primary active-page
member. Activating an inactive-page member selects the page before displaying
it. Independent window state is restored when a member leaves the group. The
[container model](../architecture/window-containers.md), [Hybrid topology](../architecture/hybrid-topology.md),
[constraints](../architecture/hybrid-constraints.md), and [chrome](../architecture/hybrid-chrome.md)
provide exact invariants and current qualification limits. Model serialization
is not evidence that full login-time application restoration is implemented.

## Panels and everyday controls

Production panels are real LayerShellQt surfaces. Profiles describe edge,
length, alignment, row count, layer, output selection, and applet placement.
Logical geometry is solved before surface publication. Visibility policy
combines window overlap, hiding mode, reveal, and hold state with reservation
intent. The shell has safe-visible recovery when it cannot establish reliable
compositor state.

| Feature | User purpose and boundary |
| --- | --- |
| Launcher | Discover installed desktop entries, search/categories, and request bounded application launches. |
| Task list | Present authenticated window facts and request activation or window actions; grouped windows share an identity. |
| Global menu | Present the active application's exported menu while retaining application authority over actions. Missing or stale exports clear safely. |
| Status tray | Present registered status-notifier items and bounded menus/actions through owner-checked adapters. |
| Clock | Show locale-aware time through the audited built-in applet. |
| Notifications | Show bounded popups and Active/Recent state behind authenticated lock privacy. |
| Audio, power, Bluetooth, clipboard | Expose focused service-client controls with explicit availability, pending state, and capability policy. |
| Iconography | Resolve application and shell icons through the confined theme lookup boundary. |

The feature catalog records the exact delivered breadth of each item. An applet
manifest declares a contract; it does not alone prove production hosting.
See [applet runtime](../shell/applet-runtime.md), [panel surfaces](../shell/panel-surfaces.md),
[visibility](../shell/panel-visibility.md), and the [complete documentation
catalog](catalog/reading.md) for each component's focused page.

## Notifications and interruptions

`Meta+N` is the shell-owned notification-center action. Do Not Disturb suppresses
low/normal banners while preserving critical behavior and center state; disabling
quieting does not replay suppressed banners. Settings1 owns confirmed preference
persistence. Authenticated lock state gates presentation: unknown or locked
state removes access to ordinary notification projections. See
[notification presentation](../shell/notification-presentation.md) for exact
default, focus, restart, shortcut, and privacy behavior.

The desktop's lock/session actions are distinct from notification privacy.
A privacy gate protects content; it does not implement a replacement locker.
See [platform services](platform.md) and [privacy](privacy.md).

Continue with [customization](customization.md), [applications](applications.md),
or the [handbook index](index.md).
