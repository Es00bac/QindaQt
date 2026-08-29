# Rhea Calder — virtual desktop private-boot midpoint

- Timestamp: 2026-08-28T12:35:33Z
- Candidate: `dc377388af530411c3c281cb0171ccfc74590b0e`
- Build root: `build/virtual-desktop-private-1787919703`

The fresh Debug configure completed cleanly. The exact
`qindaqt-desktop-session-probe` target graph built **503/503** serially. The
documented source-safe rows pass **2/2**:

```text
desktop.virtual.sandbox-unit        PASS  0.35 s
desktop.virtual.package-contract    PASS  0.24 s
```

The package stage contains the production compositor, shell, session,
Settings1, Audio1, notification, Settings, Text Editor, probe, decoration, and
service-descriptor graph plus fresh package evidence. Immediately before the
live row, process and lock inspection found no active compiler, CTest, private
QindaQt process, or `/tmp/qindaqt-private-session-1000.lock`. The visible host
KWin and unrelated host Flatpak bubblewraps predate this run and remain
untouched.

Rhea is starting exactly one invocation of the documented acknowledged
`desktop.virtual.boot.1080p` row. The accepted S0+S1 matrix requires zero
screenshots. It requires fresh authenticated result/evidence JSON, all role
logs, exact `residentPssKiB` with the 1,048,576 KiB ceiling, authenticated
terminal cleanup phases, zero owned survivor PIDs, and deletion of the private
run root. No host display, bus, HOME/config, seat/input, pointer, render,
PipeWire, network, hardware, or current desktop process is in scope.
