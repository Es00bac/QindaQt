# Settings Input route

The Input route (`qindaqt-settings --page input`) changes how pointers,
tablets, keyboards, global shortcuts, and touchscreens behave. KWin and
kglobalaccel own device and shortcut behavior; Settings1 owns touch preferences; [ADR-0134](../adr/0134-input-and-shortcut-settings.md) records the
decision and the verified protocols,
[ADR-0197](../adr/0197-pen-displays-map-themselves-and-ask-once.md) records how
a pen display finds its own screen, and
[ADR-0285](../adr/0285-desk-tablets-keep-the-screens-up-and-pen-displays-turn-with-their-screen.md)
records how a desk tablet keeps the screen's up and how its areas are mapped.

## Tabs

| Tab | What it changes | Authority |
| --- | --- | --- |
| Mouse & touchpad | Pointer speed, acceleration profile, natural scrolling, left-handed, scroll speed, middle-click emulation; touchpads add tap to click, tap and drag, disable while typing, and scroll method | KWin device properties over D-Bus |
| Pen & tablet | Which screen the pen draws on; for a desk tablet its rotation (kept upright on a rotated screen), left-handed, and the part of the tablet mapped onto a part of the screen; for a pen display the screen it turns with and its calibration; pen mode, the pressure curve and tip threshold, enabling the tablet, and what the pad has | KWin device properties over D-Bus; the intent is remembered in Settings1 `input.tabletMappings` |
| Keyboard | Key repeat, delay and rate with a test field, NumLock at login, and layouts (add, remove, reorder, variant) | `qindaqt/kwininputrc [Keyboard]` and `qindaqt/kwinxkbrc [Layout]` |
| Shortcuts | Every global shortcut, searchable; change by pressing keys, conflicts named, reset, clear; custom command shortcuts | kglobalaccel compatibility during PF22–PF24 migration |
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
flag: KWin's real `org.qindaqt.KWin.InputDevice` interface has none for tapping.
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
| Map to | `outputName` (a named screen), `mapToWorkspace` (every screen), or neither (KWin's default: the active screen); then the rotation and areas that screen needs | Always |
| Screen | `outputName` | Map to is "a specific screen" |
| Rotation (desk tablet) | `orientationDBus` (KWin's orientation, through the calibration matrix), or libinput `rotation` where a future libinput offers it for tablets | A desk tablet that can turn: `supportsRotation`, or `supportsCalibrationMatrix` with `orientationDBus` |
| Left-handed (desk tablet) | `leftHanded` | A desk tablet with `supportsLeftHanded` |
| Rotation note | nothing — it says a rotated screen is being compensated, that screens disagree, or that the rotation is not known yet | A desk tablet whose note has something to say |
| Turns with ‹screen› / Display settings… (pen display) | nothing — it opens Settings → Display | A pen display |
| Area editor (desk tablet): tablet rectangle, screen rectangle, Keep proportions, Reset areas | `inputArea`, `outputArea` | A desk tablet; the tablet rectangle needs `supportsInputArea`; Keep proportions needs a physical `size` and a known screen shape |
| Fit the whole screen / Keep the tablet's proportions (pen display) | `outputArea` | A pen display; the proportions button needs a physical `size` |
| Pen mode | `tabletToolIsRelative` | The device is a tablet tool |
| Enable this tablet | `enabled`, on the pen and its pad | `supportsDisableEvents` |
| Calibrate… / Reset (pen display) | `calibrationMatrix` | A pen display with `supportsCalibrationMatrix`, mapped to a named screen |
| Soft end, Firm end | `pressureCurve` | The device is a tablet tool |
| Tip threshold | `pressureRangeMin` | `supportsPressureRange` |
| Pad summary | nothing — it states what KWin reported | The tablet has a pad |

"Map to" and the screen picker are one decision: both go through one
`applyMapping()` that clears `mapToWorkspace` before naming an output and sets
it afterwards, so the pen never spends a frame on the wrong screen.

### Pen displays and desk tablets

A tablet tool is a **desk tablet** when KWin reports `supportsInputArea`
true and a **pen display** when it reports false: libinput offers a tablet
area only to a tablet whose evdev node is not `INPUT_PROP_DIRECT`, after its
quirks database has corrected the kernel, so the flag is libinput's own
verdict. Only when KWin does not publish the flag does the ADR-0197 output
matcher decide (a screen of its own means pen display). libwacom is not used;
neither host has it (ADR-0285 says what changes if it is ever enabled).

A pen display has no rotation of its own. KWin maps its pen through its
screen's transform, so it turns with that screen; the page says which screen
and links to Displays, and the session clears any stale `orientationDBus`,
`rotation` or `leftHanded` left on it. Its screen area is drawn as the turned
screen appears and written in the panel's own frame.

### Rotation and areas on a rotated screen

KWin also turns a **desk** tablet's pen with the mapped screen's transform,
which is wrong: a monitor turned to portrait would turn the pen's directions
too. The Rotation control therefore records how the tablet lies on the desk
(upright, 90° clockwise, 180°, 90° counter-clockwise) and writes that turn
composed with the inverse of the screen's rotation, so up on the tablet stays
up on the screen. The screen's rotation comes from Display1 (a named screen,
the rotation every screen shares when the pen follows the active screen, or
none for the whole workspace). Screens rotated differently cannot all be
right; the note says so and suggests mapping to one screen. A rotation that
is not known yet is never guessed into KWin by a mapping change.

The area editor shows the tablet as it lies and the screen as it appears.
Drag inside a rectangle to move it, drag a corner to resize it, or drag on
the empty surface to draw a new one; arrow keys move it, Shift with the arrow
keys resizes it, Home fills the surface. Keep proportions shapes the screen
rectangle like the tablet rectangle's physical size, so a circle stays round.
Because KWin applies the tablet area after the orientation and the screen
area before the screen's transform, both rectangles are turned into KWin's
frames before they are written, and every rectangle is normalized first: KWin
stores an area without checking it and libinput refuses an edge past 1.0 by
even one rounding error.

What the user chose (rotation and both rectangles, as seen) is recorded in
the tablet's `input.tabletMappings` record. The session policy re-plans every
tablet from it whenever a screen turns, the mapping changes, or the record
changes, with the same planner Settings uses, so the two never disagree.

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

## Native shortcut registry migration

The PF22 fork checkpoint adds a pure KWin::Shortcuts::Registry and Store
core linked by the compositor. A binding keeps its stable component and action
identity, description, owner label, default sequences, current sequences, and
repeat policy. Registration and reassignment reject a complete sequence
already owned by another action and report that owner; a rejected edit leaves
the existing binding intact. Removing a component releases its bindings.
Locked and shortcut-inhibited contexts do not dispatch actions.

The store uses a versioned JSON document written through QSaveFile. The
legacy import boundary accepts the bounded binding records produced by a
future kglobalshortcutsrc adapter, imports only once, and preserves any
already-present native record. PF22 does not replace the running
org.kde.kglobalaccel endpoint or claim that this core is the active input
path. PF23 must connect this policy to KWin input, keep the complete client
compatibility surface, and provide actual legacy parsing before the embedded
KGlobalAccel daemon can be removed; PF24 then moves Settings and the portal to
org.qindaqt.Shortcuts1.

## Applying changes

- Choosing a screen under **Map to** both tells KWin and records the choice,
  so the session's automatic mapping never overrides it afterwards. If the
  choice cannot be recorded the row says so rather than implying it will be
  remembered.
- Pointer and tablet properties apply the moment KWin accepts them, and KWin
  persists a tablet's screen by output UUID under
  `[Libinput][<vendor>][<product>][<name>] OutputUuid=` in `qindaqt/kwininputrc`, so it
  survives re-plug and login. A refused write leaves the row showing what the
  device actually is and says why.
- Keyboard and layout writes save the file, then announce the change to the
  running KWin (`org.kde.kconfig.notify ConfigChanged` on `/kcminputrc` or
  `/kxkbrc`). If no KWin is on the bus, the status says the change applies at the
  next session.
- A shortcut change is read back from kglobalaccel. If the key already belongs
  to another action, the row names that action; "Assign anyway" moves the key.
- A set shortcut is drawn as keys (QindaTK `KeyCap`), one group of caps per
  binding with a muted "or" between them; the row's accessible name spells the
  same keys in words ("Shortcut: Meta+Space or Ctrl+Plus"). The Plus key is
  named "Plus" because KeyCap splits a sequence on `+`. "Disabled" and the
  capture prompt stay as text. This needs `dev-libs/qindatk-0.1.0-r5` or
  newer: against r4 the row fails to load with "KeyCap is not a type", so
  `qindaqt-desktop` must depend on `>=dev-libs/qindatk-0.1.0-r5`.
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
- The tablet port, its hotplug watcher, the output matcher, the mapping ledger,
  the calibration/area geometry, the pen-display classification, the rotation
  algebra, the placement planner and the Display1 rotation decorator live in
  `src/services/tablet_devices` (`QindaQt::TabletDevices`), shared with the
  session process so the screen a pen is mapped to, the way it is turned and
  the screen the Display card badges cannot disagree. The route adds
  `TabletDevicesModel`, `TabletDeviceSelection` and `TabletPlacementModel`
  (rotation, notes, areas; `tablet_placement_notes.cpp` holds its text and
  `tablet_surface.cpp` the surfaces the editor draws) over it, and the QML
  `TabletAreaEditor` / `TabletAreaCanvas` with `TabletAreaGeometry.js`.
- The keyboard ports share `announceConfigChange`, which refuses file names that
  cannot form a D-Bus object path, so relocated test files never reach a real
  desktop watcher.
- Settings Center registers `input` after Accessibility in the stable route order.

## Tests

| Row | Covers |
| --- | --- |
| `qindaqt.settings-input-pointer-port` | Device listing, capability properties, typed writes against a fake KWin |
| `qindaqt.settings-input-keyboard-config-port` | `qindaqt/kwininputrc` round trip, range refusal, the announcement on a private bus, relocated names never announced |
| `qindaqt.settings-input-keyboard-layout-port` | `qindaqt/kwinxkbrc` round trip with `Use=true`, hostile catalogs, the announcement |
| `qindaqt.settings-input-shortcut-port` | The kglobalaccel wire contract, read-back truth, command components, malformed replies |
| `qindaqt.settings-input-pointer-devices-model`, `-keyboard-models`, `-shortcuts-model` | Presentation truth and write paths over fakes |
| `qindaqt.settings-input-page` | Offscreen page: capability hiding, editors seated inside their rows, shortcut keys drawn as one KeyCap group per binding with a spelled-out accessible name, conflict capture, capture keys, keyboard navigation, unavailable notices, reachable Touch tab and real mouse edit |
| `qindaqt.settings-input-touch-model` | Ready/last-known admission, confirmed post-commit slider coalescing, refusal/conflict/uncertainty, external refresh and owner replacement over a fake Settings1 transport |
| `qindaqt.settings-input-touch-section` | Offscreen Touch section: initial notice, control gating, real slider gestures and authoritative readback |
| `qindaqt.settings-input-tablet-model` | Grouping a pen with its pad, selection surviving a refresh, deep-link selection, capability gating, the mapping write order, a refused rotation, the pen display's area helpers, reset clearing a pen display's stale rotation |
| `qindaqt.settings-input-tablet-placement` | A desk tablet's rotation against a rotated screen, both areas written in KWin's frames and presented as seen, refused areas, Keep proportions, a pen display following its screen, a rotated screen picked under Map to, an unknown rotation never guessed, the ledger driving what is shown, an unremembered change, the surfaces |
| `qindaqt.settings-input-tablet-area-editor` | Offscreen area canvas and editor: move, corner resize and draw drags, the aspect lock, keys, accessible name and description, and the editor writing a drawn rectangle in KWin's frame; any QML warning fails the row |
| `qindaqt.settings-input-tablet-page` | Offscreen Pen & tablet destination: every control for a pen display and for a desk tablet, the Display settings link, unsupported controls hidden, the empty and degraded states, the deep link, and a proof that the loaded QML plugin is this build's and not the installed one |
| `qindaqt.services-tablet-devices-port` | Tablet listing and hotplug against a fake KWin on a private bus: only tablets, flattened `(dd)`/`(dddd)` structs, typed writes, the closed writable table, `orientationDBus` as KWin's int, areas normalized on the wire |
| `qindaqt.services-tablet-devices-policy-values` | The output matcher, the ledger document and its placement members, the calibration fit and its degenerate refusals, letterboxing, normalized areas, proportional screen areas, pressure-curve validation |
| `qindaqt.services-tablet-devices-orientation` | Rotation algebra, KWin's orientation table, area turns, pen display vs desk tablet classification, the rotation KWin applies per mapping, the Display1 join |
| `qindaqt.services-tablet-devices-placement` | The planner against an independent model of the KWin + libinput pipeline for all 16 turn × rotation pairs: up stays up, areas land as drawn, fixed points, adoption, refusals, pen displays |
| `qindaqt.session-desktop-controls-tablet-orientation` | The session policy re-planning a desk tablet when its screen turns, honouring a recorded rotation, leaving the workspace and mixed rotations uncompensated, waiting for an unknown rotation, clearing a pen display's stale rotation, retrying a refused write |
