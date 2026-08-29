# Notification Live Debug/focused checkpoint

- Lead/keeper: Soren Pike
- Timestamp: 2026-08-27T17:54:26-06:00
- Exact base/worktree: `c4982697858c083828bd406f1aa56c4e942bcc10` in
  `/home/cabewse/work_SPaC3/container-wm-workers/notification-live`
- Current candidate: 70 paths after one justified pre-existing quieting-focus
  test update; compiler is stopped and released.

## Build and registration evidence

- Fresh Debug configure: exit 0.
- Fresh complete Debug build: exit 0, 1194/1194 steps.
- Registration repair reconfigure: exit 0.
- First newly discovered probe build: failed at 21/26 because Qt 6.11.1 has no
  `QJsonArray::constFirst`; the nonempty-guarded access was repaired to
  `at(0)`.
- Exact probe rebuild at `--parallel 1`: exit 0, 7/7 steps; executable present.
- Fresh discovery now has exactly six `shell.notification-live.*` rows:
  1080p, WUXGA, 1440p, scale-125, scale-150 at 240 seconds, plus race-10x at
  2400 seconds. Every row is `RUN_SERIAL=true`.
- Import-order and outer-timeout regression unit: 1/1 pass.

## Focused non-nested evidence

The first 50-test private/offscreen pass produced 48 passes and exposed two
real focus-presentation defects:

1. geometry-driven overflow delegates could temporarily refer to a stale
   action index, producing undefined-object QML errors;
2. an older quieting test asserted removed header-only `KeyNavigation` edges
   even though empty history disables Clear and natural traversal must skip it.

Repairs fail closed on a missing action, bind accessibility assertions to the
actual repeated focus-chain items, give every claimed control an explicit
Accessible role, and assert the correct natural cycle for disabled Clear. Both
focused QML tests pass after repair. The complete same 50-test selector then
passed 50/50 in 9.02 seconds, covering development input, runtime options,
driver/syntax, Settings1, lock-state, notification protocol/client/model/host,
session-supervisor, quieting bridge, notification offscreen focus/surfaces,
entry, and runtime catalog.

`git diff --check` passes. No installed nested row or host-facing process ran.

## Independent review triage

Theo Marsh's repaired-state rereview
`1787874610-theo-marsh-c1-and-import-rereview.md` is accepted: C1 has a
600-second real margin with a >=300-second guard, the tests/session KF6 import
does not widen dependencies, and the import prevents the observed silent
matrix omission. The conditional runtime claim is now additionally closed by
the fresh target build and exact six-row discovery above. Lyra Voss's narrower
C++ lifecycle review remains pending; any finding will be triaged and routed
for rereview before handoff.

Release, sanitizer, package, and all private installed nested execution remain
held until manager reassignment.
