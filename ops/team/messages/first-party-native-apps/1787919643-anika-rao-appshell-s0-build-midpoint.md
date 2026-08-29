# Anika Rao AppShell S0 exact build midpoint

- Time: 2026-08-28T12:20:43Z
- Exact source base: `1b4e2846e40d31d79ffb03db2229c07ff9bca271`
- Branch/worktree: `worker/appshell-s0` at
  `/home/cabewse/work_SPaC3/container-wm-workers/appshell-s0`
- Runtime lane after this checkpoint: released to Rhea Calder

The manager's two demonstrated blockers are repaired. `ApplicationCoordinator`
is an explicit Qt QML module source, the generated registrar includes its real
public declaration and compiled, and the coordinator remains derivable as
required by Qt's creatable QML element wrapper. The installed-consumer row no
longer invokes the repository-wide install target: it creates a clean stage
from the exact built AppShell, Tokens, and Controls modules plus AppShell public
headers, clears ambient QML import paths, recompiles/runs the C++ consumer, and
loads the QML consumer. That row now passes in 3.79 seconds.

Exact serial build passed. Exact `^qindaqt\.app-shell-` CTest completed in 4.05
seconds with 4/5 PASS: action registry, coordinator, source policy, installed
consumer. The remaining offscreen row exposed a real source contract defect:
Qt warns that its `Accessible` attached property cannot attach to
`ApplicationWindow`; QWindow accessibility correctly exposes the native title,
not the ignored application-name attachment. I am correcting the QML/test/wiki
contract so the native title identifies the window and the item-derived page
surface carries application name/degraded description. This is source/static
work only until Rhea releases the private runtime lane.

No host desktop/session/input/configuration, compositor, service, or physical
device was touched.
