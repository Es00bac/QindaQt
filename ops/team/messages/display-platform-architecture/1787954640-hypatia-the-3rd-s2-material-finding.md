# Hypatia the 3rd — S2 material finding and ADR-0049 boundary

- Timestamp: 2026-08-28T16:04:00-06:00
- Owner: Hypatia the 3rd
- Exact base: `ce6b3124cf6de7213c194e11d109593aec1f6b0d`

The currently qualified S1 topology cannot produce the requested screenshot:
KWin 6.6.5's `org.kde.KWin.ScreenShot2` plug-in requires an EGL backend, while
S1 intentionally starts the virtual backend with QPainter. This is a backend
contract, not a transient timeout. Exact upstream inspection used the official
`v6.6.5` source and the installed binaries.

The manager has allocated ADR-0049 to this lane. I am implementing one additive
S2 row without changing S1: Weston 15 headless/pixman is the private parent,
KWin `--windowed` is its child, and the child continues to own every QindaQt
surface. The only synthetic interaction is an exact Meta+N sequence through
the scenario-gated `org.qindaqt.Compositor1.InjectTestInput` development device.
`weston-screenshooter` may contact only the parent private socket; its output
must validate as a fresh non-uniform 1920x1080 PNG in the approved build-local
evidence archive. Aggregate PSS still covers only production QindaQt roles,
and teardown must authenticate zero survivors.

The topology/output/process/readiness validators now accept a topology argument
so the new row can require KWin's exact `WL-<ordinal>` naming while every S1
call retains its original `Virtual-<ordinal>` default. I am adding the capture
validator, probe action, isolated runtime branch, focused unit tests, one exact
CTest row, ADR-0049, and testing-harness documentation next.
