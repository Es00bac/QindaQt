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
| Idle screensaver program | the saver package itself (the installed `x11-misc` savers, discovered from their desktop entries) | `ScreensaverLauncher`, started only while idle and unlocked |
| Idle screensaver preference | Settings1 `power.screensaver` / `power.screensaverMinutes` | purpose-scoped provider + [Screen saver route](../apps/screensaver-settings.md) |
| Low/critical battery warning level | UPower `WarningLevel` (via resident `Power1`'s `composite.warning`) | `BatteryNotificationPolicy`, edge-triggered on the resident notification host |
| Tablet screen mapping and hotplug | KWin `org.kde.KWin.InputDevice` / `InputDeviceManager` | `TabletMappingPolicy` over the shared `QindaQt::TabletDevices` port |
| Remembered tablet mapping decisions | Settings1 `input.tabletMappings` | purpose-scoped `Settings1TabletMappings` + Settings Pen & tablet destination |
| Pen display announcement and its actions | resident notification host | `TabletArrivalNotifier` with `ActionInvoked` routing |
| Polkit authentication UI | polkit daemon | optional supervisor child, distribution agent binary |

Nothing here modifies the compositor, the Power1 v1 wire protocol, or the
screen-lock preference. Display-off is display power only; locking remains the
separate screen-lock surface, and the screensaver is decoration in front of
neither ([ADR-0215](../adr/0215-the-idle-screensaver-is-decoration-not-a-lock.md)).

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
| `TabletMappingPolicy` | maps a tablet tool to its own screen once, re-maps when the screen arrives after the tablet, and never overrides a recorded user choice |
| `Settings1TabletMappings` (shared library) | purpose-scoped Settings1 read/write of `input.tabletMappings`, used by **both** the session policy and the Settings route so a choice made in Settings is not re-decided a moment later; stays unloaded until a real document arrives |
| `TabletArrivalNotifier` | one replaceable announcement per device group with the `setup` / `internal` / `dismiss` actions, ignoring every `ActionInvoked` that is not its own |
| `TabletRouteLauncher` | the `--page input --destination tablet --select <group>` deep link, resolved sibling-first like `ScreenshotLauncher` |

The production process keeps `KGlobalAccelRegistrar` for volume, mute,
microphone mute, and airplane mode.
Spectacle owns the installed Print action, preserving its own user remapping and
capture-mode choices. PowerDevil owns monitor-brightness shortcut registration
and the idle display-off policy; QindaQt only observes its documented public
brightness signal and binds its idle preference. The retained KIdleTime,
DPMS, and sysfs classes are migration seams for focused tests and are not
instantiated by the resident process.

## Tablet mapping contract

A pen display is a screen and a tablet at once, and KWin maps a tablet tool
with an empty `outputName` to the active output — which is why a pen plugged
into the laptop drew on the laptop's panel.
[ADR-0197](../adr/0197-pen-displays-map-themselves-and-ask-once.md) records the
decision; the operational shape is:

- The policy reconciles on start, on `deviceAdded` / `deviceRemoved`, when the
  process's screens change, and when the ledger changes. It writes only
  properties whose value differs.
- An output is a tablet's own screen when its EDID manufacturer is a
  display-tablet vendor or its model shares a distinctive word with the
  tablet's name. An internal panel (`eDP`/`LVDS`/`DSI`) is never a match, and
  two equally good candidates are ambiguous: nothing is written.
- `input.tabletMappings` is one Settings1 `object` whose members are tablet
  identities — `vendor:product:name`, the same triple KWin keys its own
  per-device configuration on. **Never KWin's `deviceGroupId`**, which hashes
  the libinput device group's pointer address and changes on every re-plug;
  a record keyed on that could never be found again. Records written under
  the old key are dropped when the ledger is read. A member carries the choice, the output name, whether the
  user made the choice, and whether the tablet has been announced. Settings1
  rejects a whole snapshot on one unknown key (ADR-0126), so this key has its
  own client, separate from the idle-preference client, and needs a restart of
  the resident settings service before it is accepted.
- One announcement per device group. A re-plug of a known tablet is silent;
  the one exception is a mapping that actually changed, which is the
  USB-before-HDMI case.
- `--no-tablet-policy` disables the whole feature for a session, the same way
  `--no-idle-policy` disables idle display-off.

Output identity comes from `QScreen`: KWin fills a Wayland output's make and
model from the EDID and Qt republishes them, with `name()` being the connector
`outputName` takes. The process needs no Display1 client for one string per
output.

## Preference contract

`power.idleDisplayOffMinutes` is an integer Settings1 key in the `power`
domain: `-1` (or any value ≤ 0) means the display never turns off, positive
values are clamped to 1..240 minutes, and the documented default is 10. The
Settings Power route writes only this key through its own purpose-scoped
client; the resident process reads it through the same seam. A transiently
absent Settings1 owner keeps the documented default rather than silently
disabling the policy. The screen-lock preference (`kscreenlockerrc` Daemon
group) is untouched and independent.

`power.screensaver` (a token: the reserved `none`/`blank`, or a
discovered saver's program name) and `power.screensaverMinutes` (1..240,
default 5) are a second, separate scope in the same `power` domain, read by
`Settings1ScreensaverPreferences` and written only by the
[Screen saver route](../apps/screensaver-settings.md)
([ADR-0226](../adr/0226-configure-the-screen-saver.md)). Which tokens exist is
discovered at runtime by `DesktopEntryScreensaverCatalog` from the installed
savers' `.desktop` entries — an entry attests its purpose (a "screensaver"
mention in Keywords, GenericName, Comment, or an action name) and proves the
launch contract (an action running the entry's own program with
`--screensaver` or `--all-screens`) — scanning only the system application
directories, never the user-writable one. A token the catalog does not know
reads back as `none`, so persistence can never supply a program name to
`QProcess`; the catalog supplies the fixed per-saver arguments (every output,
telemetry and sound off) that discovery itself cannot infer. An absent
Settings1 owner leaves the saver off rather than assuming a default, because
starting an unchosen program is worse than starting nothing.

`ScreensaverLauncher` (in `production/`, alongside the KGlobalAccel registrar,
because it needs KIdleTime) arms one idle timeout from that preference,
resolves the chosen token to its catalog entry — program and fixed arguments —
only at launch (a saver whose package was removed between snapshot and idle
simply does not start), and stops it on resume, on a preference change, and
on `org.freedesktop.ScreenSaver.ActiveChanged`. The reserved `blank` token
arms nothing here: it is a lock-screen appearance the greeter paints, not a
process. Stopping on lock is not the end
of the picture: the locker's greeter draws the same saver itself as its
wallpaper plugin (ADR-0216), so exactly one thing renders it at a time and no
screensaver process is ever shown above the lock screen. It relaunches a saver that
exits while the session is still idle — an output topology change ends one —
but stops after three exits inside five seconds, and never retries a program
that failed to start at all.

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
boundaries; screensaver preference mapping (an unconfirmed owner starting
nothing, an unknown token never becoming a program name, clamped minutes, and
one signal per real change) and catalog discovery rules (attestation, the
launch contract, system directories only, reserved-token refusal); the retained screenshot launcher helper against a fixture;
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
