# Settings Input route

The Input route (`qindaqt-settings --page input`) changes how pointers,
keyboards, and global shortcuts behave. KWin and kglobalaccel stay the
authorities; [ADR-0134](../adr/0134-input-and-shortcut-settings.md) records the
decision and the verified protocols.

## Tabs

| Tab | What it changes | Authority |
| --- | --- | --- |
| Mouse & touchpad | Pointer speed, acceleration profile, natural scrolling, left-handed, scroll speed, middle-click emulation; touchpads add tap to click, tap and drag, disable while typing, and scroll method | KWin device properties over D-Bus |
| Keyboard | Key repeat, delay and rate with a test field, NumLock at login, and layouts (add, remove, reorder, variant) | `kcminputrc [Keyboard]` and `kxkbrc [Layout]` |
| Shortcuts | Every global shortcut, searchable; change by pressing keys, conflicts named, reset, clear; custom command shortcuts | kglobalaccel |

Rows a device does not support are hidden rather than disabled. When KWin or
kglobalaccel is unreachable, the tab shows an unavailable notice instead of
controls.

## Applying changes

- Pointer properties apply the moment KWin accepts them.
- Keyboard and layout writes save the file, then announce the change to the
  running KWin (`org.kde.kconfig.notify ConfigChanged` on `/kcminputrc` or
  `/kxkbrc`). If no KWin is on the bus, the status says the change applies at the
  next session.
- A shortcut change is read back from kglobalaccel. If the key already belongs
  to another action, the row names that action; "Assign anyway" moves the key.
- In a capture control, Escape cancels, Backspace clears, and Tab leaves the
  control without capturing.
- A custom command shortcut creates `~/.local/share/kglobalaccel/qindaqt-custom-<name>.desktop`
  and binds its launch action. Removing it deletes that file and the binding.

## Module shape

- `src/apps/settings/input` holds the four ports (pointer devices, keyboard
  configuration, keyboard layouts, shortcuts), their models, the
  `InputRouteComposition` QML singleton that builds the production adapters,
  and the QML pages.
- The keyboard ports share `announceConfigChange`, which refuses file names that
  cannot form a D-Bus object path, so relocated test files never reach a real
  desktop watcher.
- Settings Center registers `input` as the last route.

## Tests

| Row | Covers |
| --- | --- |
| `qindaqt.settings-input-pointer-port` | Device listing, capability properties, typed writes against a fake KWin |
| `qindaqt.settings-input-keyboard-config-port` | `kcminputrc` round trip, range refusal, the announcement on a private bus, relocated names never announced |
| `qindaqt.settings-input-keyboard-layout-port` | `kxkbrc` round trip with `Use=true`, hostile catalogs, the announcement |
| `qindaqt.settings-input-shortcut-port` | The kglobalaccel wire contract, read-back truth, command components, malformed replies |
| `qindaqt.settings-input-pointer-devices-model`, `-keyboard-models`, `-shortcuts-model` | Presentation truth and write paths over fakes |
| `qindaqt.settings-input-page` | Offscreen page: capability hiding, editors seated inside their rows, conflict capture, capture keys, keyboard navigation, unavailable notices |
