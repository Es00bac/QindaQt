# ADR-0289: A native screenshot and record tool replaces Spectacle

- **Status:** Accepted
- **Date:** 2026-09-28
- **Owners:** First-party applications, session desktop controls
- **Supersedes:** [ADR-0107](0107-delegate-print-to-spectacle.md)
- **Superseded by:** None

## Context

ADR-0107 delegated Print to an installed Spectacle. Spectacle is a Plasma
application, it crashed at logout on 2026-09-28, and it is one of the Plasma
dependencies the plasma-free plan (M4) removes. The owner's rule for every
replacement is that it is a native QindaQt tool designed for the desktop, not
a port of the KDE one: QindaTK presentation themed by the desktop tokens,
Settings1 preferences shown in Settings, the desktop's notification service,
its global-shortcut path, and its one obs-websocket client for recording.

KWin already exposes the pixels through the restricted
`org.kde.KWin.ScreenShot2` interface that Gather uses (ADR-0241). The
container compositor already owns `Meta+Shift+S` (adjust split) and
`Meta+Shift+R` (resize group), so those two requested keys are not free.

## Decision

**`qindaqt-screenshot` (`src/apps/screenshot`) is the desktop's screenshot and
screen-recording tool, and desktop-controls owns Print again.**

- **Capture:** the tool calls ScreenShot2 itself. Its installed desktop entry
  `org.qindaqt.Screenshot` names the exact configured executable and lists
  `X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2` (no trailing
  semicolon, as ADR-0241 found). The port talks only to a ScreenShot2 owner
  that is also `org.kde.KWin`, reads raw pixels from its own pipe, validates
  shape, format and length before building an image (bounded at 512 MiB), and
  always requests native resolution with `hide-caller-windows`. Modes: region,
  all screens (`CaptureWorkspace`), current screen (`CaptureActiveScreen`),
  active window, and the window under the pointer (`CaptureInteractive`,
  picked with a click); delay, pointer and window decorations are options.
- **Region:** the tool captures the whole workspace first and shows it frozen
  in one full-screen overlay per output; the selection is one global logical
  rectangle, so a drag may cross outputs. Arrows move it, Alt+arrows resize
  it, Enter captures (the whole output when nothing is selected), Esc
  cancels. The crop happens locally from the frozen image, so the overlay can
  never appear in the result. Selection policy lives in C++ where rows pin it.
- **Result:** a QindaTK window with Copy, Save (never overwrites: collision
  suffixes plus an exclusive create), Save As, Open, Show in folder and New
  screenshot. A save posts a notification through
  `org.freedesktop.Notifications` (QindaQt's notification host) with Open,
  Copy and Show in folder; the buttons act on that saved file.
- **Clipboard:** copies go through the clipboard service's own
  ext-data-control adapter, because KWin accepts a normal selection only from
  the focused client and a windowless launch or notification button has no
  focus. The adapter gained an additive `publishedSelectionLive()` query; the
  tool stays resident while it still serves its selection, and falls back to
  QClipboard where data control is unavailable.
- **Command line:** `--region | --fullscreen | --current-screen | --active |
  --window`, optional `--delay`, `--pointer`, `--no-decorations`, and `--copy`
  / `--save [path]` for a windowless launch; `--record-toggle`. Exit status 0
  done, 1 failed, 2 bad arguments, 3 cancelled. A windowless launch exits once
  it owes the user nothing: no notification with live buttons (bounded to ten
  minutes) and no clipboard selection to serve.
- **Recording:** a Record tab and `--record-toggle` start and stop OBS through
  the shared `QindaQt::ObsClient` (ADR-0201) behind the same start gate as the
  OBS applet: a confirmed Settings1 baseline with auto-connect, OBS's active
  config on the chosen port, and a password already in the keyring
  (ADR-0248). `RecordController` derives idle/starting/recording/paused/
  stopping from OBS's snapshot plus its one outstanding request, treats a
  paused recording as recording, reports refusals in OBS's words and
  unconfirmed requests as unconfirmed, and uses the applet's own unavailable
  sentences (`projectApplet`). The client's snapshot gained
  `lastRecordingPath`, from the STOPPED event or the StopRecord reply, so the
  tool can offer Open, Show in folder and Copy path and notify. It records the
  current OBS scene.
- **Shortcuts:** desktop-controls' existing KGlobalAccel set launches the tool:
  Print and `Meta+Shift+Print` capture a region, `Shift+Print` every screen,
  `Alt+Print` the active window, and `Meta+Alt+R` toggles recording. They are
  appended after the original eight actions so stable indices and persisted
  bindings stay put, and Settings → Input → Shortcuts lists them because it
  lists every KGlobalAccel component.
- **Preferences:** six `services.screenshot*` Settings1 keys in schema v2 —
  save folder, file-name pattern (`{date}`, `{time}`, `{mode}`), default mode,
  delay, whether shortcut captures show the result window, and what happens
  when a recording stops. `screenshot_preferences` owns validation and the
  Settings1 write contract (one write in flight, published only after
  readback, never replayed). They appear as the "Screenshots and recording"
  section of Settings → Streaming, which already holds recording; the tool's
  Preferences button opens that route.
- **Packaging:** the desktop package installs the tool, its desktop entry and
  the `org.qindaqt.Screenshot` mark (the icon catalog's camera inside selection
  corners), and QindaQt itself no longer depends on `kde-plasma/spectacle`;
  `tools/check-release-contract` now fails if the desktop ebuild names it.
- **Shell hook:** none beyond the shortcuts in this slice. The OBS applet keeps
  its own controller over the same client; routing its Recording toggle
  through `RecordController` needs the controller lifted out of the app first.

## Consequences

- A user who still has Spectacle installed has two claimants for Print in
  KGlobalAccel; whichever registered first keeps it until Spectacle is removed
  or rebound. Settings → Input → Shortcuts shows the conflict.
- The two requested keys `Meta+Shift+S` and `Meta+Shift+R` stay with the
  container compositor; the defaults above avoid a silent double binding.
  Moving them is a separate, owner-approved change.
- Capture authorization depends on the installed desktop entry, so a
  development binary run from a build tree is refused by KWin by design.
- The tool owns a second pixel-reading ScreenShot2 client beside Gather's;
  both validate the same way, and a later slice may share one decoder.
- Region or single-window *recording*, annotation, and the
  xdg-desktop-portal Screenshot backend are follow-up slices. Wayland capture
  sources are chosen through the portal, so recording a picked region is not
  improvised through OBS here.

## Revisit when

- KWin changes ScreenShot2's authorization or payload, or offers a
  window-at-point capture.
- The owner rebinds the container keys and wants `Meta+Shift+S` /
  `Meta+Shift+R` for the tool.
- The OBS applet should share `RecordController`, or OBS gains a cleanly
  scriptable region/window source.
