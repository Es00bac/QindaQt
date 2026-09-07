# Desktop controls architecture

This page records the accepted architecture for the daily desktop essentials:
media-key volume and brightness with visible feedback, the Print screenshot
action, the session-started polkit authentication agent, and the configurable
idle display-off policy. Its current maturity is **EXECUTABLE (focused
evidence)**: the resident process, supervisor startup, Settings Power section,
and focused tests are implemented and green in Debug builds. Live physical
key, brightness hardware, Spectacle launch, and real idle/DPMS behavior on an
installed desktop remain later installed-session evidence owned by the
integration gate.

The durable choice of a separate supervised process over shell or compositor
integration is [ADR-0100](../adr/0100-own-desktop-essentials-in-a-session-process.md).

## Scope and authority map

| Concern | Truth authority | QindaQt owner |
| --- | --- | --- |
| Volume/mute media keys | KGlobalAccel (compositor-provided) | `qindaqt-desktop-controls` shortcut set |
| Volume and mute mutation | resident `Audio1` | public `AudioClient` on the default output |
| Brightness media keys | KGlobalAccel | same shortcut set |
| Internal-panel brightness write | kernel backlight sysfs | public `SysfsBacklightSource` write primitive (ADR-0060) |
| Print screenshot | `org.kde.KWin.ScreenShot2` (Spectacle) | detached `spectacle -b -r` launch |
| Visible media-key feedback | resident notification host | `org.freedesktop.Notifications` with per-category replaces-id |
| Idle observation | KIdleTime over ext-idle-notify-v1 | `IdleTracker` seam (inhibitor-aware) |
| Display power off/on | org-kde-kwin-dpms on KWin | `DpmsController` seam on a private Wayland connection |
| Idle display-off preference | Settings1 `power.idleDisplayOffMinutes` | purpose-scoped provider + Settings Power section |
| Polkit authentication UI | polkit daemon | optional supervisor child, distribution agent binary |

Nothing here modifies the compositor, the Power1 v1 wire protocol, or the
screen-lock preference. Display-off is display power only; locking remains the
separate screen-lock surface.

## Process and module boundary

One bounded `qindaqt-desktop-controls` process, started by the
`qindaqt-session` supervisor as an optional one-restart child after the
shell (the compositor's `org.kde.kglobalaccel` and the session bus are
therefore already alive). Absence of the executable is skipped, never fatal;
the session remains usable without it.

The `src/session/desktop_controls` module keeps each concern behind an
injected seam so focused tests need no compositor, bus, or hardware:

| Piece | Cohesive responsibility |
| --- | --- |
| `DesktopShortcutSet` | one QAction per media key with stable ids, registered through the `ShortcutRegistrar` seam (production: KGlobalAccel Autoloading; user remapping survives restarts) |
| `VolumeKeyController` | ±5% steps and mute toggle on the snapshot's default output, capability-gated, optimistic feedback, honest unavailable reasons |
| `BrightnessKeyController` | firmware > platform > raw device preference, one-sixteenth-of-maximum steps clamped to `[1, maximum]`, typed read-only truth |
| `ScreenshotLauncher` | sibling-then-PATH resolution, detached launch, honest failure feedback |
| `FreedesktopFeedbackNotifier` | one replaceable notification per feedback category, bounded text, fail-quiet on host loss |
| `IdleDisplayPolicy` | arms the configured idle timeout, requests DPMS off on reach and on on resume, passive when disabled or DPMS is unavailable |
| `Settings1IdlePreferences` | purpose-scoped Settings1 read of `power.idleDisplayOffMinutes` with the documented default when truth is absent |

Production adapters (`KGlobalAccelRegistrar`, `KIdleTimeTracker`,
`KWaylandDpmsController`) live behind a build gate on the KF6/KWayland CMake
configs; the library and its tests build without them.

## Preference contract

`power.idleDisplayOffMinutes` is an integer Settings1 key in the `power`
domain: `-1` (or any value ≤ 0) means the display never turns off, positive
values are clamped to 1..240 minutes, and the documented default is 10. The
Settings Power route writes only this key through its own purpose-scoped
client; the resident process reads it through the same seam. A transiently
absent Settings1 owner keeps the documented default rather than silently
disabling the policy. The screen-lock preference (`kscreenlockerrc` Daemon
group) is untouched and independent.

## Inhibition and availability boundary

Idle inhibition is honored on exactly one path today, and the boundary is an
explicit non-claim rather than a completion statement:

- **Native Wayland idle inhibitors (`zwp_idle_inhibitor_v1`) are honored.**
  KIdleTime 6.27's Wayland backend creates `get_idle_notification` objects,
  which the ext-idle-notify-v1 protocol requires to stay non-idle while a
  visible surface inhibitor is active. A full-screen video or game holding
  the native inhibitor therefore suppresses the timeout itself; no DPMS
  request is made while it is inhibited.
- **`org.freedesktop.ScreenSaver.Inhibit` and XDG portal `Inhibit` (flag 8)
  are NOT consumed by this slice.** KIdleTime has no D-Bus inhibition path,
  and the display-off request goes directly to org-kde-kwin-dpms, so a client
  that inhibits only through those D-Bus APIs can still be blanked. Until a
  suppression path or runtime evidence lands (tracked as the next idle gate),
  this desktop must not be claimed to "respect video/game inhibitors" in
  general — only the native Wayland path is claimed.
- Legacy `org_kde_kwin_idle` compositors (KIdleTime's fallback) carry no
  ext-idle inhibitor semantics in the protocol, so inhibitor protection is
  not claimed there either.

A DPMS controller that is unavailable at policy start (late global bind,
hotplug) cannot permanently disarm the policy: the controller reports an
availability change when the first output becomes controllable, and the
policy re-applies the current preference then — an equal Settings1 snapshot
never needs to be re-sent.

## Verification and non-claims

Focused executable evidence covers: shortcut ids/defaults/dispatch and
binding-change reporting; volume stepping bounds, mute toggling, capability
gating, rejected-operation honesty, and uncertain-operation quietness; sysfs
fixture brightness stepping, clamping, device preference, and typed read-only
failure; idle-preference mapping including never/clamps; policy arming, DPMS
off on timeout, on on resume, disabled and unavailable-DPMS passivity,
re-arming on preference change, and recovery when DPMS availability appears
after start (including with an equal, suppressed snapshot); screenshot
resolution and detached launch against a fixture helper; notifier wire shape
and replaces-id reuse against a private `dbus-run-session` fake; supervisor
optional-child startup, one-restart budget, and skip-on-absence; and the
Settings Power model and page behavior for the new section.

These rows use fixtures, fake transports, and private buses only. They do not
claim a physical media key, a real backlight write, a live Spectacle capture,
a polkit prompt on installed packages, or real idle/DPMS cycling on the host
desktop; those belong to the installed-session verification gate. The nested
compositor inhibition matrix (native inhibitor suppresses the timeout;
inhibitor release re-enables it; portal/ScreenSaver-only behavior recorded)
is the requested next nested-lane gate and has not been run in this slice.
