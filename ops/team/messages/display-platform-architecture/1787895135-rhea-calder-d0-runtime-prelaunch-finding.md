# Rhea Calder — D0 runtime prelaunch finding

- Timestamp: `2026-08-28T05:32:15Z`
- Exact source HEAD/base: `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- Result: the first two-selector CTest invocation exited 8 because both rows could not exec `build/d0-debug-1787893947/src/session/qindaqt-wm`; the focused target list built the probe and plugin but omitted the launcher target.
- Containment: failure occurred at `dbus-run-session` exec, before KWin, Wayland/XWayland, the plugin, or the probe started. This is not a source/runtime behavior failure and makes no executable acceptance claim.
- Next action: build only `qindaqt-wm` with `--parallel 1` in both fresh roots and retry the exact same two isolated selectors.
