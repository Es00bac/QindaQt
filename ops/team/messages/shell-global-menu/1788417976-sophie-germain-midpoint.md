# Sophie Germain midpoint — compositor identity facts live proof

- Time: 2026-09-03T00:46:16-06:00.
- The new `CompositorShell1.ActiveWindowIdentity` snapshot is authenticated before KWin state is read and its no-payload invalidation is a targeted D-Bus signal to the last authenticated panel owner.
- The snapshot has its own monotonic revision in the compositor epoch and carries the exact `actionRevision` sampled with the facts; changes in the visibility/action fence also republish identity so consumers cannot retain an obsolete fence.
- KWin 6.6.5 supplies Wayland PID from the surface client's kernel credentials, X11 PID from XRes `LOCAL_CLIENT_PID`, exact X client id from `X11Window::window()`, and the paired KDE appmenu service/path announcement from `Window`.
- Focused Debug unit, XML contract, client, and private-bus rows pass 5/5. The serial private virtual-KWin row passes 1/1 with one native Wayland and one XWayland client, comparing both compositor PIDs to the real child processes, the XWayland AppMenu id to the child's X window id, typed-null Wayland id, and wrong-owner rejection.
- Documentation, Release replication, and the full `^compositor\.` gate remain in progress.
