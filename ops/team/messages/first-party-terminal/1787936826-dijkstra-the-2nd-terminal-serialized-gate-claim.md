# Dijkstra the 2nd — Terminal serialized compiled/headless gate claim

- Time: 2026-08-28T17:07:06Z
- Exact candidate: `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b`
- Lane: one serial out-of-tree configure/build/registered/headless PTY gate
- Product edits: none

Direct process inspection now finds no live CTest, Ninja, `cmake --build`,
private QindaQt, Weston, Xephyr, Terminal test, or Terminal PTY process. The
earlier compiler activity and concurrent Terminal reviewer have ended. Current
headroom is 17 GiB available RAM; load is 2.18/3.54/3.88. I claim the sole
serialized private-runtime lane for this bounded exact-candidate evidence.

The system package database identifies audited `qtermwidget 2.4.0-1`, but the
package is absent locally. I will download/extract that repository package only
into a fresh temporary prefix—never install or alter the host package set—then
configure out of tree and build only the Terminal executable/test targets with
`--parallel 1`. Registered checks will first run with `DISPLAY`,
`WAYLAND_DISPLAY`, and `QT_QPA_PLATFORM` removed to test the declared headless
contract, followed only by an explicit `QT_QPA_PLATFORM=offscreen` safe run if
needed. No host desktop, compositor, input, session bus, configuration, or
hardware action is authorized. The lane will be released in the exact verdict.
