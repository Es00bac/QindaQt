# ADR-0105: Delegate idle display-off to PowerDevil

- **Status:** Accepted
- **Date:** 2026-09-07
- **Owners:** Session and desktop controls
- **Supersedes:** None
- **Superseded by:** None

## Context

Wayland `ext-idle-notify-v1` protects native idle-inhibit surfaces, but portal
Idle and legacy `org.freedesktop.ScreenSaver.Inhibit` requests reach KDE's
PowerDevil policy agent. A separate QindaQt KIdleTime-to-DPMS loop cannot see
those policy-agent requests and can blank a playing video or game when
PowerDevil is absent. KScreenLocker does not publish a supported read-only
inhibition query. PowerDevil 6.6.6 already owns the profile settings, idle
timer, DPMS action, and `ChangeScreenSettings` inhibition policy.

## Decision

PowerDevil is the sole owner of idle display-off. QindaQt exposes a small
`PowerDevilIdleAdapter` that accepts an enabled flag and a minute timeout,
updates only PowerDevil's documented `powerdevilrc` Display keys for the AC,
Battery, and LowBattery profiles, and calls the documented
`org.kde.Solid.PowerManagement.refreshStatus` method after a successful KConfig
sync. The adapter reports exact service availability and asynchronous apply
errors and refreshes once when the real service owner returns.

The session composition owns the installed PowerDevil process and starts and
stops it with the session. The adapter does not assume a systemd user unit,
create a fake Solid service, read private KScreenLocker state, inspect D-Bus
traffic, run a competing idle timer, or issue DPMS requests. When the service
owner is absent, new preference writes fail closed and leave the configuration
unchanged.

## Brightness feedback compatibility

PowerDevil 6.6.6 is also the sole owner of monitor-brightness media-key
registration. QindaQt does not register duplicate `XF86MonBrightnessUp` or
`XF86MonBrightnessDown` actions and does not write `/sys/class/backlight` from
the resident desktop-controls process. Its
`PowerDevilBrightnessFeedbackObserver` listens to the public
`org.kde.ScreenBrightness.BrightnessChanged` signal, accepts only the pinned
PowerDevil keyboard source `(internal)` with context `brightness_key`, reads
the public per-display `MaxBrightness` property, and normalizes the reported
value before forwarding it to the existing feedback notifier. Automatic dim,
external clients, and uncontextualized changes are ignored so they do not
produce duplicate key feedback.

The source name and context filter are a PowerDevil 6.6.6 compatibility
contract. An upgrade must recheck the installed PowerDevil source and public
D-Bus XML, then rerun the private-bus key-vs-auto-dim, owner-loss, and range
tests before changing the filter or claiming compatibility.

## Consequences

- Portal and legacy ScreenSaver inhibition share PowerDevil's policy gate with
  the idle display-off action, so video/game behavior has one owner.
- Profile changes apply to all three standard PowerDevil profiles while
  preserving every unrelated KConfig entry.
- A daemon restart is visible as unavailable until the exact owner returns;
  the adapter then requests one supported configuration refresh.
- The session must package and supervise the release-matched PowerDevil 6.6.x
  executable. Without its owner, idle display-off remains unavailable rather
  than silently bypassing inhibitors.
- Focused tests use a private D-Bus and temporary config; nested session tests
  remain required for real portal, legacy ScreenSaver, and playback scenarios.

## Revisit when

Revisit if PowerDevil changes the `powerdevilrc` profile keys or
`refreshStatus` contract, changes the ScreenBrightness source/context or
public range properties, publishes a newer stable idle-policy API, or QindaQt
adopts a different single owner that demonstrably consumes both portal and
legacy ScreenSaver inhibition.
