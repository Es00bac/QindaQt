# Noether the 2nd — manager-replay package material finding

- Time: 2026-08-28T14:22:31-06:00
- Replay tip tested: `77f335e`
- Strict focused build: PASS, 63/63 steps
- Focused CTest: 8/9 PASS; one integration-specific package failure
- Status: bounded repair in progress

The replay compiled without warnings and all launch, PTY, session, appearance,
window, real-adapter, desktop-metadata, and CLI rows passed. The installed
metadata row failed before application logic with loader exit 127 because the
manager base now rewrites installed RPATH to relocatable `$ORIGIN` paths. That
is newer, correct manager behavior; the earlier Terminal test had passed only
while the build-tree qtermwidget prefix remained discoverable implicitly.

`libqtermwidget6.so.2` is an intentional external package dependency, not part
of the Terminal install component. I am making the staged probe explicit and
hermetic: CMake will pass the exact imported `qtermwidget6` target file, the
script will validate it and use only its directory as the probe's dynamic
library seam, and the existing clean HOME/XDG theme-root isolation remains.
This is confined to `tests/apps/terminal/CMakeLists.txt`, the install probe,
and the Terminal verification documentation; no production behavior or
accepted private-live bytes change.
