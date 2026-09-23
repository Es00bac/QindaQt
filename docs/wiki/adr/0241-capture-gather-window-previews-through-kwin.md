# ADR-0241: Capture Gather window previews through KWin's restricted interface

- **Status:** Accepted
- **Date:** 2026-09-22
- **Owners:** Shell presentation, platform integration
- **Refines:** [ADR-0119](0119-authenticated-window-preview-channel.md) for Gather only
- **Preview lifetime refined by:** [ADR-0243](0243-keep-gather-previews-by-window-identity.md)

## Context

Gather's free-window grid has a thumbnail area but displays only application
icons. The compositor plugin cannot read window textures through the exported
KWin 6.6 plugin API. KWin already has a `ScreenShot2.CaptureWindow` D-Bus
method that accepts a compositor window UUID and writes raw image bytes to a
passed file descriptor. It restricts callers by matching their executable to
an installed desktop entry whose `X-KDE-DBUS-Restricted-Interfaces` property
contains `org.kde.KWin.ScreenShot2`.

## Decision

The production shell gets that exact restricted-interface desktop entry and
owns one `KWinScreenshotPreviewPort` in its Gather composition. Gather requests
captures only for currently projected free-window tiles while its borrowed
task-list controller is available and its projection is interactive. The
request carries the compositor window UUID (a standalone row's `taskId`) and
task-generation revision. The
port checks that `org.kde.KWin.ScreenShot2` and `org.qindaqt.Compositor` have
the same D-Bus owner, calls `CaptureWindow` asynchronously, and receives the
raw image through a pipe. It checks owner continuity, UUID, dimensions,
stride, format, payload length, and a 64 MiB ceiling before constructing an
image. It aspect-fits the image to at most 1024 pixels per axis. A bounded
queue and timeout prevent one missing capture from retaining Gather forever.

The composition accepts only results for its current generation and visible
request set. It holds the small previews in memory as QML image data URLs,
clears them on close or source change, and uses the existing icon when a
capture is denied or fails. Nothing writes window pixels to persistent
storage. The planner still owns tile placement; the image fits inside the
tile's thumbnail area.

## Consequences

These are snapshots captured on open and on task-generation changes, not a
continuous video stream. The older `CompositorShell1.WindowPreview` contract
in ADR-0119 and the dock hover port remain future work; this decision does
not silently change that protocol or grant third-party applets screenshot
access. The installed desktop entry must name the exact installed shell
executable and the interface value must omit a trailing semicolon: KService
otherwise reads the semicolon into the sole list member and KWin denies the
call.
