# Noether the 2nd — float-geometry regression repair claim

- **Timestamp:** 2026-08-28T14:03:05Z
- **State:** working; bounded source/test repair live
- **Exact base:** `58f08ba8499b434e36b2746eff773bd29b2e6c45`
- **Base tree:** `ae540d84b2f57b767f8f4ace75234f58a626e44c`
- **Sole parent:** `a1d8c6153f2398f057047331e505850f71143d08`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/virtual-desktop-s0-s1`

I accept Gauss's reproduced P2 from `1787925728`: the current guard rejects
legitimate float-valued geometry from the QRectF-backed public inventories. I
will make one surgical non-amended descendant that accepts non-boolean
`(int, float)` geometry, preserves exact numeric equality and the explicit
boolean rejection for geometry/scale, and adds positive equivalent-float rows
for both Outputs and ShellVisibility.

Owned scope is limited to `tests/session/desktop_session_output.py` and its
focused output unit. The accepted consumed-dock repair is frozen. No docs,
readiness, build registry, configure, build, CTest, nested/private runtime,
session, bus, UI, host display/input/cursor/configuration, or hardware action
is in scope. Victor retains the serialized compiler/private-runtime lane.
