# Rhea Calder — D0 isolated runtime pass

- Timestamp: `2026-08-28T05:35:55Z`
- Source state: base/HEAD `94e84077e33a279dcebee24511e7dbdf1b87e3e1`; one bounded repair in new D0-owned `tests/session/compositoroutputworkflow.cpp` makes queued `OutputsChanged` delivery part of inventory convergence instead of checking it immediately afterward.
- Debug: `compositor.kwin-plugin-nested` and `compositor.production-control-read-only` pass 2/2, serial, 2.97 s.
- Release: the same selectors pass 2/2, serial, 2.71 s.
- Executable output evidence: development mode adds then removes its owned virtual output; output generation advances exactly twice; exactly one `OutputsChanged` is received per mutation; `Outputs` and `ShellVisibilitySnapshot` converge to the same generation and name set. Production mode exposes no development-output capability, returns byte-identical `control-disabled` rejection for hostile and valid add requests, leaves inventory unchanged, and emits no output signal.
- Isolation/cleanup: private bus plus direct `--virtual`; no inherited host bus, `DISPLAY`, `WAYLAND_DISPLAY`, HOME/XDG, dotool, or input path. No D0 launcher/probe/private KWin/test bus process or current-run temporary directory remains. Two `/tmp/qindaqt-nested-test-*` roots discovered by name have Aug 26 mtimes and predate this Aug 28 run; they were not touched.
- Boundary: deterministic KWin 6.6.5 VirtualBackend evidence only; no physical DRM/GPU/connector/monitor/lid/hotplug claim.
