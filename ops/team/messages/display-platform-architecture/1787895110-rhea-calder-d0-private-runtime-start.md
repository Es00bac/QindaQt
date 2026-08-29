# Rhea Calder — D0 isolated private-runtime start

- Timestamp: `2026-08-28T05:31:50Z`
- Exact source HEAD/base: `94e84077e33a279dcebee24511e7dbdf1b87e3e1`, preserved 42 modified plus 8 new D0-owned paths
- Precondition evidence: Debug and Release focused builds both passed 230/230; their focused non-session tests passed 10/10 and 10/10. Process inspection found no private QindaQt, Weston, or test-bus runtime. Kellan excludes runtime; Elara and Dorian are read-only.
- Isolation: `tests/session/nested_session_scenario.py::isolated_environment` removes the inherited session bus, `DISPLAY`, `WAYLAND_DISPLAY`, development/test/input markers, and dotool; redirects HOME and all XDG roots to a fresh temporary tree; the driver starts direct `qindaqt-wm --virtual` under a fresh private `dbus-run-session`.
- Exact serial selectors: `compositor.kwin-plugin-nested` and `compositor.production-control-read-only`, Debug root, `--parallel 1`.
- Boundary: no parent compositor, host Wayland/X11, host input, host config, persistent output store, shell-supervisor graph, or physical display/hardware action is involved.
