# ADR-0104: Serialize production shell rendering

- Status: Accepted
- Date: 2026-09-07

## Context

The installed shell crashed in Qt 6.11.1's threaded scenegraph loop while
`QWaylandWindow::beginFrame()` acquired invalid lock storage. The GUI thread
was synchronizing a window expose event. This supports investigation of a
platform-window lifetime race or memory corruption; the exact corrupting or
freeing operation is not established. Unrelated clients failing to connect to
Wayland do not establish that this shell's compositor had already died.

## Decision

Default only the production `qindaqt-shell` process to Qt Quick's `basic`
render loop before constructing QGuiApplication. An explicit nonempty
`QSG_RENDER_LOOP` remains authoritative for diagnosis. Qt documents this as a
supported single-threaded loop: window lifecycle and scenegraph rendering run
on the GUI thread. Keep normal Qt scenegraph ownership rules intact.

This is an interim mitigation for the observed cross-thread crash path, not a
claim that the underlying memory fault has been found or repaired. It does not
change the compositor or application render loops, and it does not replace the
separate session-supervisor recovery policy.

## Consequences

Panel rendering no longer runs in a separate render thread by default. Long GUI
work can delay panel frames, so nested profile/output and popup interaction
qualification remains required before installation. Diagnostic runs can set
`QSG_RENDER_LOOP=threaded`; `qt.scenegraph.general` logging verifies the selected
loop. Future removal requires evidence that the original lifetime fault is
fixed, plus repeated window show/hide/reconfigure qualification.

See [Applet runtime](../shell/applet-runtime.md) and
[Qt Quick scenegraph rendering](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html).
