# Cora Vale Controls S2 focused behavior repair checkpoint

- **Timestamp:** 2026-08-27T23:09:02Z
- **Branch/worktree:** `worker/controls-s2` at
  `/home/cabewse/work_SPaC3/container-wm-workers/controls-s2`
- **Exact HEAD:** `a083a20af14a2d7b9e954735a2d659c475a536b2`
- **Compiler state:** stopped; Notification owns the lane

After the async registration and bare-probe repairs, the exact focused build
completed its remaining 19/19 Ninja steps and exited 0. It linked the Controls
module/plugin, behavior and visual executables, and both PSS probes. The
Controls qmllint command exited 0 but reported one actionable unsupported
TextField implicit-content property, so lint-clean was not claimed.

The exact focused nonvisual selector:

```sh
ctest --test-dir build/controls-debug \
  -R '^qindaqt\.controls-(behavior|source-policy|pss-measurement)$' \
  --output-on-failure
```

ran 3 CTests: source policy passed, PSS measurement passed, behavior failed
(2/3 CTests pass, command exit 8). Behavior reached product surfaces and
reported 12 QtTest passes / 6 failures. The failures were retained as evidence
rather than weakened:

- FormRow relied on GridLayout auto-placement while also assigning a dynamic
  error column, causing cell collisions and a zero-height editor host; compact
  height consequently failed.
- TextField referenced an implicit-content property absent from its public QML
  type description.
- ThemeCard's partial-map predicate returned `undefined`, leaving prior Boolean
  truth and allowing transient invalid role access.
- StateCard read the derived `alert` binding inside `onStatusChanged` before it
  settled, so warning/information transitions could announce with the prior
  state's urgency.
- two tests expected Return activation even though native Button's proven
  supported activation here is Space; ordinary and StateCard Space paths had
  passed.
- QColor equality retained different representation metadata despite equal
  rendered ARGB values.

Bounded source-only repairs now assign explicit FormRow rows/columns, derive a
positive TextField implicit height through its supported content item, make
ThemeCard validation total and guard all invalid role access, derive StateCard
urgency directly from the new status and expose the exact QML politeness
mapping, use supported Space activation, and compare disabled QColor semantics
after RGB conversion. The controls wiki records the public politeness mapping.

Current static evidence only: `git diff --check` exit 0; direct Controls source
policy exit 0 with all 14 QML files. No compiler process remains. Focused
requalification, visuals, package install, Release, broad, and docs gates are
still unclaimed and will not begin until the manager explicitly releases the
compiler again.
