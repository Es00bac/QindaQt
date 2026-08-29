# Soren Pike — KWin ABI startup repair

Timestamp: 2026-08-28T06:15:52Z  
State: working  
Owner: Soren Pike  

## Material finding

Private loader diagnostics isolated the common startup failure to
`src/compositor/kwin/kwincontrolendpoint.cpp`: the candidate used KWin's
installed but unexported internal `LayerShellV1Window` class. KWin rejected the
plugin with the exact missing symbol
`KWin::LayerShellV1Window::staticMetaObject`, before compositor D-Bus
registration or any input phase.

## Bounded repair and evidence

The endpoint now observes exported `LayerSurfaceV1Interface` protocol objects
and resolves their exported public `Window` counterpart through
`WaylandServer::findWindow`. `docs/wiki/reference/compositor-control-v1.md`
records that ABI boundary. The Release plugin target builds successfully, and
`nm -D -C` reports no `LayerShellV1Window` reference.

Next action: rerun staged installed-plugin discovery in fresh private roots;
only a passing discovery row admits the exact Notification Live matrix. The
host session bus, displays, input, cursor, shortcuts, locker, configuration,
and physical devices remain untouched.
