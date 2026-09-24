# Settings Input route

The Input route (`qindaqt-settings --page input`) changes how pointers,
tablets, keyboards, global shortcuts, and touchscreens behave. KWin and
kglobalaccel own device and shortcut behavior; Settings1 owns touch preferences; [ADR-0134](../adr/0134-input-and-shortcut-settings.md) records the
decision and the verified protocols, and
[ADR-0197](../adr/0197-pen-displays-map-themselves-and-ask-once.md) records how
a pen display finds its own screen.

## Tabs

| Tab | What it changes | Authority |
| --- | --- | --- |
| Mouse & touchpad | Pointer speed, acceleration profile, natural scrolling, left-handed, scroll speed, middle-click emulation; touchpads add tap to click, tap and drag, disable while typing, and scroll method | KWin device properties over D-Bus |
| Pen & tablet | Which screen the pen draws on, the mapped area, rotation and left-handed, pen mode, calibration, the pressure curve and tip threshold, enabling the tablet, and what the pad has | KWin device properties over D-Bus |
| Keyboard | Key repeat, delay and rate with a test field, NumLock at login, and layouts (add, remove, reorder, variant) | `kcminputrc [Keyboard]` and `kxkbrc [Layout]` |
| Shortcuts | Every global shortcut, searchable; change by pressing keys, conflicts named, reset, clear; custom command shortcuts | kglobalaccel |
| Touch | Touchscreen on or off (a real stop of every touch device at the compositor seat), how long a finger is held for the menu, the on-screen keyboard, and what a swipe from each screen edge opens | Settings1 `input.touch.*` ([ADR-0205](../adr/0205-touch-edges-and-touch-preferences-belong-to-the-compositor.md)) |

Rows a device does not support are hidden rather than disabled. When KWin or
kglobalaccel is unreachable, the tab shows an unavailable notice instead of
controls.

The Mouse & touchpad tab keeps the selected KWin device by its ID when the
inventory reorders. A removed device, or a replaced KWin owner, clears its
selection and capability rows immediately; a surviving selected device keeps
its identity. While the tab is visible, the model refreshes from KWin on its
property-change feed and a bounded periodic inventory check so hotplug and
external edits appear without reopening Settings. D-Bus reads and writes run
off the UI thread. A control stays disabled while a write is pending, and its
shown value comes from KWin readback, including after refusal; a failed
multi-property profile or scroll-method write restores earlier writes before
readback. Each D-Bus transaction addresses the captured unique KWin owner for every
read, write, rollback, and readback, so a replacement KWin cannot receive a
stale edit. Late replies from a previous selection or owner cannot repaint
the current device.

Tap-to-click and tap-and-drag availability come from KWin's `tapFingerCount`
integer property (0 = the device cannot tap), not from a boolean `supports*`
flag: KWin's real `org.kde.KWin.InputDevice` interface has none for tapping.
An earlier version of `kwin_pointer_device_port.cpp` read
`supportsTapToClick`/`supportsTapAndDrag`/`defaultTapToClick` names that KWin
has never exposed, which left both rows (and the Touchpad section header)
permanently hidden regardless of hardware — confirmed against the live
`qinda-top` touchpad (`event4`, `tapFingerCount == 3`, `tapToClick == true`).
Every other `supports*`/`default*` pair follows the pattern
`supports<Prop>`/`<prop>EnabledByDefault`, which the earlier version also got
wrong for several rows; see the `AGENT-CONTRACT` in `pointer_device_port.h`
for the confirmed name list before adding a new capability row.
## Pen & tablet

One row per tablet, not per device: a pen and its pad share a vendor, a
product and a base name, and are one entry. That triple — the same one KWin
keys its own per-device configuration on — is also what a remembered mapping
is stored under, so a choice survives unplugging the tablet. KWin's
`deviceGroupId` is deliberately not used for this: it hashes a pointer
address and changes on every re-plug. A tablet is listed when KWin reports
`tabletTool` or `tabletPad` on it.

| Control | What it writes | Shown when |
| --- | --- | --- |
| Map to | `outputName` (a named screen), `mapToWorkspace` (every screen), or neither (KWin's default: the active screen) | Always |
| Screen | `outputName` | Map to is "a specific screen" |
| Fit the whole screen / Keep the tablet's proportions | `outputArea` | The device reports a mapped rectangle; the proportions button also needs a physical `size` |
| Rotation, Left-handed | `rotation`, `leftHanded` | `supportsRotation`, `supportsLeftHanded` |
| Pen mode | `tabletToolIsRelative` | The device is a tablet tool |
| Enable this tablet | `enabled`, on the pen and its pad | `supportsDisableEvents` |
| Calibrate… / Reset | `calibrationMatrix` | `supportsCalibrationMatrix`, and the tablet is mapped to a named screen |
| Soft end, Firm end | `pressureCurve` | The device is a tablet tool |
| Tip threshold | `pressureRangeMin` | `supportsPressureRange` |
| Pad summary | nothing — it states what KWin reported | The tablet has a pad |

"Map to" and the screen picker are one decision: both go through one
`applyMapping()` that clears `mapToWorkspace` before naming an output and sets
it afterwards, so the pen never spends a frame on the wrong screen.

The calibration wizard opens a full-screen window **on the screen the pen is
mapped to**, resets the tablet to its own calibration first, draws four
crosshairs, and fits the four measured points onto them. It measures the
stylus rather than the cursor, so a stray touchpad tap cannot become a
sample. A tablet that follows the active screen has no fixed surface to
calibrate against, so the button is unavailable until a screen is chosen. Measurements that are collinear or coincident are refused rather than
written — a collapsed matrix would lose the pen entirely.

KWin reads exactly two control points of the pressure curve and always adds
`(1, 1)` as the end point, so "Soft end" and "Firm end" are those two points.
A curve KWin could not read is refused before it reaches the wire. Pad
buttons, rings and strips arrive as ordinary keys; KWin publishes their counts
and no binding interface, so the pad section names the hardware and sends the
user to Shortcuts.

## Opening a destination directly

`qindaqt-settings --page input --destination tablet --select <deviceGroupId>`
opens the Pen & tablet destination with one tablet selected. Both values are
opaque to the Settings process: an unknown destination keeps the route's
default and an unconnected device id opens the destination anyway, so a stale
link from an old notification never costs the user the route. The pen-display
notification and the Display card's "Pen & tablet settings…" both use this.
## Touch

`InputTouchSection.qml` binds `TouchSettingsModel`, which rides its own
purpose-scoped Settings1 client over `input.touch.enabled`, `longPressMs`
(200–1500 ms), `onScreenKeyboard` (`auto`, `off`) and
`edgeLeft/Top/Right/Bottom` (`none`, `overview`, `notifications`,
`task-switcher`); `input.touch.mode` stays unscoped and unshown until a
consumer exists. Each accepted edit is one user-value write. The rows use the confirmed
snapshot as their authoritative value. The first missing snapshot hides
controls; if Settings1 later becomes unavailable or degraded, the last-known
values remain visible but disabled and the notice identifies their stale
authority. All new edits require a current Ready snapshot and no unrelated
write in progress. A hold-time slider gesture may replace its own pending
final value even while an Applied reply is awaiting its confirming read; that
same-owner, same-epoch movement is queued only in memory. It sends at most one
additional write after the first success has been confirmed by a fresh
snapshot at or beyond the result revision.
A rejected, conflicted, uncertain, or interrupted write drops that pending
value without replay. Exact Settings1 owner replacement retires the pending
gesture immediately, including an Authenticating-to-Authenticating transition
that does not change the client's state. Refusal and uncertainty messages
survive unchanged refreshes and external edits until the user begins a new
accepted edit.
With the touchscreen switched off only the switch remains. The compositor plugin
consumes the same keys live (the touchscreen switch stops every touch
device at the seat, thresholds, edge reservations, the keyboard mode);
`mode` is stored ahead of its application consumers (ADR-0205,
decision 4). The Input page's Touch destination loads this section from the same
purpose-scoped composition; its unavailable notice is visible before the
first snapshot and the tab is reachable by mouse or keyboard.

## Applying changes

- Choosing a screen under **Map to** both tells KWin and records the choice,
  so the session's automatic mapping never overrides it afterwards. If the
  choice cannot be recorded the row says so rather than implying it will be
  remembered.
- Pointer and tablet properties apply the moment KWin accepts them, and KWin
  persists a tablet's screen by output UUID under
  `[Libinput][<vendor>][<product>][<name>] OutputUuid=` in `kcminputrc`, so it
  survives re-plug and login. A refused write leaves the row showing what the
  device actually is and says why.
- Keyboard and layout writes save the file, then announce the change to the
  running KWin (`org.kde.kconfig.notify ConfigChanged` on `/kcminputrc` or
  `/kxkbrc`). If no KWin is on the bus, the status says the change applies at the
  next session.
- A shortcut change is read back from kglobalaccel. If the key already belongs
  to another action, the row names that action; "Assign anyway" moves the key.
- In a capture control, Escape cancels, Backspace clears, and Tab leaves the
  control without capturing. While capture is active, the control claims
  `ShortcutOverride` for keys it records or consumes, and the Input route
  reports capture activity to the Settings shell. The shell disables its
  global shortcuts until capture ends, so Ctrl+K, Ctrl+digit, and Alt+Left are
  saved as shortcut data without opening search or changing routes.
- A custom command shortcut creates `~/.local/share/kglobalaccel/qindaqt-custom-<name>.desktop`
  and binds its launch action. Removing it deletes that file and the binding.

## Module shape

- `src/apps/settings/input` holds the four ports (pointer devices, keyboard
  configuration, keyboard layouts, shortcuts), their models, the
  `InputRouteComposition` QML singleton that builds the production adapters,
  and the QML pages.
- The tablet port, its hotplug watcher, the output matcher, the mapping ledger
  and the calibration/area geometry live in `src/services/tablet_devices`
  (`QindaQt::TabletDevices`), shared with the session process so the screen a
  pen is mapped to and the screen the Display card badges cannot disagree. The
  route adds only `TabletDevicesModel` and `TabletDeviceSelection` over it.
- The keyboard ports share `announceConfigChange`, which refuses file names that
  cannot form a D-Bus object path, so relocated test files never reach a real
  desktop watcher.
- Settings Center registers `input` after Accessibility in the stable route order.

## Tests

| Row | Covers |
| --- | --- |
| `qindaqt.settings-input-pointer-port` | Device listing, capability properties, typed writes against a fake KWin |
| `qindaqt.settings-input-keyboard-config-port` | `kcminputrc` round trip, range refusal, the announcement on a private bus, relocated names never announced |
| `qindaqt.settings-input-keyboard-layout-port` | `kxkbrc` round trip with `Use=true`, hostile catalogs, the announcement |
| `qindaqt.settings-input-shortcut-port` | The kglobalaccel wire contract, read-back truth, command components, malformed replies |
| `qindaqt.settings-input-pointer-devices-model`, `-keyboard-models`, `-shortcuts-model` | Presentation truth and write paths over fakes |
| `qindaqt.settings-input-page` | Offscreen page: capability hiding, editors seated inside their rows, conflict capture, capture keys, keyboard navigation, unavailable notices, reachable Touch tab and real mouse edit |
| `qindaqt.settings-input-touch-model` | Ready/last-known admission, confirmed post-commit slider coalescing, refusal/conflict/uncertainty, external refresh and owner replacement over a fake Settings1 transport |
| `qindaqt.settings-input-touch-section` | Offscreen Touch section: initial notice, control gating, real slider gestures and authoritative readback |
| `qindaqt.settings-input-tablet-model` | Grouping a pen with its pad, selection surviving a refresh, deep-link selection, capability gating, the mapping write order, a refused write, the area helpers, reset |
| `qindaqt.settings-input-tablet-page` | Offscreen Pen & tablet destination: every control for a capable tablet, unsupported controls hidden, the empty and degraded states, the deep link, and a proof that the loaded QML plugin is this build's and not the installed one |
| `qindaqt.services-tablet-devices-port` | Tablet listing and hotplug against a fake KWin on a private bus: only tablets, flattened `(dd)`/`(dddd)` structs, typed writes, the closed writable table |
| `qindaqt.services-tablet-devices-policy-values` | The output matcher, the ledger document, the calibration fit and its degenerate refusals, letterboxing, pressure-curve validation |
