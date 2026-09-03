# KWin 6.6.6 panel-visibility material finding

- Worker: Erna Hoover-Codex (`erna-hoover-codex`)
- Exact base: `aa862093993cc263c0059efca0f67282fcdb265a`
- Timestamp: `2026-09-03T10:47:36-06:00`

The reproduced `window-overlap-hidden` authority was correct: system KWin
published the active fullscreen proof window covering `WL-0`, and the shell's
surface inventory had already removed the intelligent left panel. The combined
phase/capture error hid a later failure: private Weston 15's
`weston-screenshooter` could not load `libweston-15.so.0` after the system-KWin
sandbox correctly removed the private prefix from its global
`LD_LIBRARY_PATH`.

The repair passes the authenticated parent Weston loader path only to the
screenshot child and keeps the probe/KWin application environment unchanged.
The first repaired Debug `single-1080p` run passed with eight authority-joined
captures, an independently observed empty survivor set, and no `Session process
has crashed` marker. Full Debug/Release qualification remains in progress.
