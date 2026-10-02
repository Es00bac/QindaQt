# Screenshot

**Screenshot** (`qindaqt-screenshot`, desktop entry `org.qindaqt.Screenshot`)
is QindaQt's own screenshot and screen-recording tool. It replaced Spectacle
([ADR-0289](../adr/0289-native-screenshot-and-record-tool.md)). The window is
QindaTK, themed by the desktop's tokens (light, dark and the Corner Bar
themes) through `QindaQtTheme`.

## Taking a screenshot

| Area | What happens |
| --- | --- |
| Rectangular region | The whole desktop freezes; drag across any screen. Arrows move the box (Shift: 10 px), Alt+arrows resize it, Enter captures (the whole screen when nothing is selected), Esc or a right click cancels. |
| All screens | Every output, at each output's native resolution. |
| Current screen | The output KWin considers active. |
| Active window | The window that was active before Screenshot hid itself. |
| Window under the pointer | After the delay, click the window you want; Esc cancels. |

Options: a delay of 0, 3, 5 or 10 seconds, the mouse pointer, and (window
modes) the title bar and borders. Screenshot's own windows are never in a
capture.

The result window offers **Copy**, **Save**, **Save As…**, **Open**, **Show in
folder**, **New screenshot** and **Discard** (Ctrl+C, Ctrl+S, Ctrl+Shift+S,
Ctrl+N). Save goes to the configured folder, by default
`XDG_PICTURES_DIR/Screenshots/Screenshot_YYYY-MM-DD_HH-MM-SS.png`, and never
replaces a file: a taken name gets `-2`, `-3`, … A save posts a notification
with Open, Copy and Show in folder.

## Keys

| Keys | What they do |
| --- | --- |
| `Print`, `Meta+Shift+Print` | Capture a region |
| `Shift+Print` | Capture every screen |
| `Alt+Print` | Capture the active window |
| `Meta+Alt+R` | Start or stop an OBS recording |

They belong to desktop-controls' shortcut set and are listed (and can be
changed) in **Settings → Input → Shortcuts**. `Meta+Shift+S` and
`Meta+Shift+R` stay the container split and group-resize keys.

## Recording

The **Record** tab starts and stops an OBS recording of the current OBS scene
through the desktop's one OBS client, with the elapsed time OBS reports
(including while paused). When OBS is not running, not set up, or refuses
QindaQt's password, the tab says so in the OBS applet's words with one action
to open **Settings → Streaming**. When OBS finishes a file, the tool offers
Open, Show in folder and Copy path and, by preference, notifies. Recording a
region or a single window is a follow-up slice.

## Preferences

**Settings → Streaming → Screenshots and recording** holds the six
`services.screenshot*` Settings1 keys (see the
[settings catalog](../handbook/catalog/settings.md)): save folder, file-name
pattern (`{date}`, `{time}`, `{mode}`), the area and delay new captures start
with, whether shortcut captures open the result window (off: saved and
announced directly), and what happens when a recording stops.

## Command line

```text
qindaqt-screenshot                       # the window
qindaqt-screenshot --region              # capture, then the result window
qindaqt-screenshot --fullscreen --copy   # windowless: clipboard only
qindaqt-screenshot --active --save [path]
qindaqt-screenshot --window --delay 5 --pointer --no-decorations --save
qindaqt-screenshot --record-toggle
```

`--save` prints the saved path; a folder gets the default name inside it and
an existing file is never replaced. Exit status: 0 done, 1 failed, 2 bad
arguments, 3 cancelled.

## Structure

| Part | Responsibility |
| --- | --- |
| `capture/` | App command-line and save-without-overwrite policy; compatibility wrappers for public request/decoder/geometry |
| `platform/` | Compatibility wrapper for public restricted capture port; app notifications, ext-data-control clipboard publishing, file-manager and Settings hand-offs |
| `record/` | `RecordController` state machine and the OBS connection gate |
| `ui/` | Capture flow, result actions, and the image store QML reads |
| `app/` | The composition root and process residency |
| `src/services/compositor_capture` | [Public shared capture request/decoder/geometry/transport](../architecture/compositor-capture.md), also consumed by the portal candidate |
| `src/services/screenshot_preferences` | Settings1 preferences and file naming, shared with Settings |

KWin authorizes the tool by its installed desktop entry, so a build-tree
binary is refused; test it installed.

## Tests

| Row | Covers |
| --- | --- |
| `qindaqt.apps-screenshot-policy` | Request builder per mode, command-line parsing and refusals, file-name patterns, never-overwrite saves |
| `qindaqt.apps-screenshot-geometry` | ScreenShot2 payload validation (formats, stride, truncation), error sentences, drag/nudge/crop geometry across outputs and scales |
| `qindaqt.apps-screenshot-flow` | Hide → delay → capture sequencing, frozen-workspace overlays, crop, cancel and failure over a fake port |
| `qindaqt.apps-screenshot-record` | The OBS record state machine over the client's transport seam, paused recordings, saved-file reporting, refusals, the connection gate |
| `qindaqt.apps-screenshot-preferences` | Schema mirroring, defaults, invalid stored values, Settings1 write/readback |
| `qindaqt.apps-screenshot-ui` | The QindaTK window offscreen with fatal warnings: options, save twice without overwrite, a region by mouse and keyboard, the record page without OBS |
| `qindaqt.settings-streaming-page` | The Screenshots and recording section binding confirmed values |
| `qindaqt.session-desktop-controls-shortcut-set` | The four new actions, their keys, and never taking the container keys |
| `desktop.daily-controls.live` | Print in a private nested session produces a decoded capture from the installed tool |
