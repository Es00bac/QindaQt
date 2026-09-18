# Desktop controls architecture

This page records the accepted architecture for the daily desktop essentials:
media-key volume and brightness with visible feedback, the Print screenshot
action, the session-started polkit authentication agent, and the configurable
idle display-off policy. Its current maturity is **EXECUTABLE (focused
evidence)**: the resident process, supervisor startup, Settings Power section,
and focused tests are implemented and green in Debug builds. The private
installed-session row passed real `VolumeUp`, one production-shell feedback
popup, and a decoded Spectacle region capture. The real KDE polkit agent also
presented an authentication dialog in installed QindaQt session 39, corroborated
by the user; terminating the unapproved request removed the dialog and left
the session healthy. No credential was entered or privileged command run.
Private PowerDevil inhibition/display-off evidence is recorded below. Installed
Settings1 now returns the ten-minute idle preference, matching the active AC
PowerDevil profile exactly (enabled, 600 seconds). The obsolete Settings1 owner
needed restarting after installation. Physical brightness hardware coverage
remains outside this observed evidence.

The durable choice of a separate supervised process over shell or compositor
integration is [ADR-0100](../adr/0100-own-desktop-essentials-in-a-session-process.md).

## Scope and authority map

| Concern | Truth authority | QindaQt owner |
| --- | --- | --- |
| Volume/mute media keys | KGlobalAccel (compositor-provided) | `qindaqt-desktop-controls` shortcut set |
| Volume and mute mutation | resident `Audio1` | public `AudioClient` on the default output |
| Microphone mute media key | KGlobalAccel (compositor-provided) | `qindaqt-desktop-controls` shortcut set |
| Microphone mute mutation | resident `Audio1` | public `AudioClient` on the default input |
| Airplane-mode (Wi-Fi/WWAN) media key | KGlobalAccel (compositor-provided) | `qindaqt-desktop-controls` shortcut set |
| Wi-Fi/WWAN radio enable | resident `Network1` | public `NetworkClient::setRadio` |
| Brightness media keys | PowerDevil 6.6.6 `ScreenBrightnessAgent` | PowerDevil's registered shortcuts |
| Internal/external brightness mutation | PowerDevil `org.kde.ScreenBrightness` | session-owned PowerDevil |
| Print screenshot | Spectacle desktop action | installed Spectacle owns Print and its capture UI |
| Visible media-key feedback | resident notification host | `org.freedesktop.Notifications` with per-category replaces-id |
| Brightness key feedback | PowerDevil `BrightnessChanged` with `(internal)` / `brightness_key` | `PowerDevilBrightnessFeedbackObserver` and existing notifier |
| Idle observation and display power | PowerDevil 6.6.6 policy agent | session-owned PowerDevil idle adapter and binding |
| Idle display-off preference | Settings1 `power.idleDisplayOffMinutes` | purpose-scoped provider + Settings Power section |
| Low/critical battery warning level | UPower `WarningLevel` (via resident `Power1`'s `composite.warning`) | `BatteryNotificationPolicy`, edge-triggered on the resident notification host |
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
| `MicMuteKeyController` | mute toggle on the snapshot's default input, mirrors `VolumeKeyController`'s mute path exactly |
| `AirplaneModeKeyController` | toggles the Wi-Fi radio over the public `NetworkClient`; see [Airplane mode and microphone mute](#airplane-mode-and-microphone-mute) |
| `PowerDevilBrightnessFeedbackObserver` | filters PowerDevil keyboard feedback, reads the public per-display maximum, and emits normalized notifier feedback |
| `BrightnessKeyController` | retained sysfs fixture seam for migration coverage; not instantiated or registered by production |
| `ScreenshotLauncher` | retained launch-fixture seam; it is not instantiated by the resident process because Spectacle owns Print |
| `FreedesktopFeedbackNotifier` | one replaceable notification per feedback category, bounded text, fail-quiet on host loss |
| `PowerDevilIdlePreferencesBinding` | coalesces Settings1 preferences and applies them through the session-owned PowerDevil adapter |
| `Settings1IdlePreferences` | purpose-scoped Settings1 read of `power.idleDisplayOffMinutes` with the documented default when truth is absent |
| `BatteryNotificationPolicy` | edge-triggered low/critical/action battery notifications from `PowerClient::snapshotChanged`; see [Battery notifications](#battery-notifications) |

The production process keeps `KGlobalAccelRegistrar` for volume, mute,
microphone mute, and airplane mode.
Spectacle owns the installed Print action, preserving its own user remapping and
capture-mode choices. PowerDevil owns monitor-brightness shortcut registration
and the idle display-off policy; QindaQt only observes its documented public
brightness signal and binds its idle preference. The retained KIdleTime,
DPMS, and sysfs classes are migration seams for focused tests and are not
instantiated by the resident process.

## Preference contract

`power.idleDisplayOffMinutes` is an integer Settings1 key in the `power`
domain: `-1` (or any value ≤ 0) means the display never turns off, positive
values are clamped to 1..240 minutes, and the documented default is 10. The
Settings Power route writes only this key through its own purpose-scoped
client; the resident process reads it through the same seam. A transiently
absent Settings1 owner keeps the documented default rather than silently
disabling the policy. The screen-lock preference (`kscreenlockerrc` Daemon
group) is untouched and independent.

## Battery notifications

`BatteryNotificationPolicy` connects to the resident `Power1` `PowerClient`
(the same public client the Settings Power route and applet use) and reacts
to `snapshot.composite.warning`, UPower's own `WarningLevel` for the
coalesced system battery. QindaQt names no percentage thresholds itself —
`WarningLevel` is already computed upstream from `UPower.conf`'s
`PercentageLow`/`PercentageCritical`/`PercentageAction` (20/5/2 on
`qinda-top`, confirmed live).

- Fires **once per crossing** into `Low`, `Critical`, or `Action` (an
  ordinal comparison against the last notified level), never once per
  snapshot tick. A level at or below the last notified one is not a new
  crossing.
- A level below `Low` (charging, plugged in, or the battery becoming
  temporarily unreported) resets the edge, so the next discharge into `Low`
  notifies again.
- `composite.present == false` (no battery at all, e.g. a desktop machine)
  never notifies and keeps the edge reset.
- Delivered through `FreedesktopFeedbackNotifier::showBattery`, its own
  replaces-id category (never collides with the volume/brightness/notice
  OSD popups): `expire_timeout = 0` (stays until dismissed, unlike the
  1.2 s key-feedback flash) and the freedesktop `urgency` hint (`1` for
  `Low`, `2`/critical for `Critical` and `Action`), so a notification host
  or do-not-disturb policy does not auto-dismiss or hide a battery warning
  the way it may a transient one.

## Airplane mode and microphone mute

`MicMuteKeyController` (`XF86AudioMicMute` → `Qt::Key_MicMute`) mirrors
`VolumeKeyController::toggleMute()` exactly, against the snapshot's default
*input* device instead of its default output.

`AirplaneModeKeyController` (`XF86WLAN` → `Qt::Key_WLAN`) reads the current
Wi-Fi radio's `softwareEnabled` state from the public `NetworkClient`'s
snapshot and calls `setRadio(Wifi, !current)`. **Only Wi-Fi is toggled,
even when a WWAN (cellular) radio is present.** `NetworkClient` (like every
other resident client in this codebase) admits one operation at a time; a
first implementation that issued `setRadio(Wwan, ...)` immediately after
`setRadio(Wifi, ...)` in the same synchronous call found the second request
silently rejected as busy every time (caught by
`presentWwanRadioIsNeverToggled`, a test written expecting the opposite and
failing honestly instead). Sequencing a WWAN follow-up correctly would mean
waiting for the Wi-Fi operation's `operationFinished` signal before
dispatching the second request — real state-machine complexity (what
happens to a second key press while that follow-up is pending, what the
combined feedback signal reports) that this controller does not take on.

**`XF86RFKill` has no Qt key mapping and is not wired.** Qt's public `Key`
enum has `Key_WLAN` but no `Key_RFKill`/`Key_Flight`/`Key_Airplane`
constant, and the installed `libQt6Gui.so`'s compiled keysym table has no
entry for the `XF86RFKill` X11 keysym at all (confirmed by binary string
search; `XF86WLAN`, by contrast, is present as both a distinct XKB symbol
and a mapped Qt key). A physical key whose hardware scancode maps to
`KEY_RFKILL` rather than `KEY_WLAN` (both exist as separate entries in
`/usr/share/X11/xkb/symbols/inet`, so this is keyboard-model dependent)
produces no Qt key event through `QShortcut`/`QAction`/KGlobalAccel the way
every other media key in this codebase does, and is a known, undemonstrated
gap rather than a silently claimed capability. Which keysym `qinda-top`'s
own physical airplane-mode key actually sends needs a hands-on
**(user)** press-and-observe check.

**Bluetooth is not toggled by either key.** A conventional "airplane mode"
often also disables Bluetooth, which would mean composing a second public
client (`BluetoothClient::setAdapterPower`, itself needing adapter-handle
enumeration from its own snapshot). Left out to keep
`AirplaneModeKeyController` to one client, matching every other controller
in this module; a bounded follow-up if wanted.

## Inhibition and availability boundary

Idle inhibition is delegated to PowerDevil's policy authority:

- **Native Wayland idle inhibitors (`zwp_idle_inhibitor_v1`) are honored** by
  PowerDevil's idle policy through the KDE idle policy path.
- **Portal Idle and legacy `org.freedesktop.ScreenSaver.Inhibit` are honored**
  by PowerDevil's PolicyAgent when the session-owned PowerDevil service is
  present. QindaQt does not inspect either inhibition source or issue a
  competing DPMS request.
- If the PowerDevil owner is absent, QindaQt does not provide idle display-off;
  the display remains on. The installed-session gate must exercise fullscreen
  video/game playback through native, portal, and ScreenSaver inhibition with
  the real release-matched PowerDevil owner.

A DPMS controller that is unavailable at policy start (late global bind,
hotplug) cannot permanently disarm the policy: the controller reports an
availability change when the first output becomes controllable, and the
policy re-applies the current preference then — an equal Settings1 snapshot
never needs to be re-sent.

## Verification and non-claims

Focused executable evidence covers: volume/mute/mic-mute/airplane-mode/
screenshot shortcut ids and dispatch; microphone mute toggling against a
fake `AudioClient` input snapshot (capability-gated, unsupported/no-default
reasons, rejected/uncertain operation handling); airplane-mode radio
toggling against a fake `NetworkClient` transport (Wi-Fi toggling in both
directions, a present or absent WWAN radio never generating a second
request, missing-snapshot and missing-radio unavailable reasons, rejected/
uncertain operation handling); PowerDevil brightness feedback filtering,
range normalization, and
owner absence; sysfs fixture brightness stepping remains migration coverage;
idle-preference mapping and PowerDevil binding coalescing/recovery/failure
boundaries; the retained screenshot launcher helper against a fixture;
notifier wire shape and replaces-id reuse against a private
`dbus-run-session` fake; supervisor optional-child startup, one-restart budget,
and skip-on-absence; the Settings Power model and page behavior for the
new section; and `BatteryNotificationPolicy`'s crossing-edge logic (fires
once into Low, replaces with Critical and raises urgency, resets on recovery,
never fires with no battery present) against a fake `PowerClient` transport
and a private-bus notification fake. The live UPower thresholds cited above
(`PercentageLow=20.0`, `PercentageCritical=5.0`, `PercentageAction=2.0`) were
read from `/etc/UPower/UPower.conf` on `qinda-top` as a read-only `ssh`
probe; no notification was triggered against the live session, and no
package was installed to verify end-to-end delivery there.

Focused rows use fixtures, fake transports, and private buses only. The separate
`desktop.daily-controls.live` installed row accepts only a manager-granted private
namespace: it verifies the real `VolumeUp` global shortcut changes Audio1’s
private PipeWire default output and makes exactly one production shell feedback popup (the existing read-only shell
evidence reports counts but not notification text), then waits for Spectacle's
actual capture surface, completes a region selection, and requires its normal
`Print` action to save a decoded, non-uniform image in a disposable private
output directory. Because KWin 6.6.6's screenshot plugin requires an EGL
backend, this private capture row requests llvmpipe OpenGL and verifies KWin's
public compositing type before Print. The row configures only that disposable
Spectacle profile; it never changes an installed user's capture-mode or save preferences. It does not claim a
physical media key, a real PowerDevil brightness operation, a polkit prompt on
installed packages, or real idle/display cycling on the host desktop. The nested
inhibition matrix (native, portal, and ScreenSaver inhibition suppresses
PowerDevil display-off and release re-enables it) remains required with the real
service owner.
