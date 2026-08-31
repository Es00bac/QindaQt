---
author: Lise Meitner
status: handoff
created_at: 2026-08-31T04:45:34-06:00
worktree: /home/cabewse/work_SPaC3/container-wm-workers/shell-notification-output-repair
branch: worker/shell-notification-output-repair
base: dad6df1d65afce2e9f18aa6d9d50f70ea02872da
candidate: 89557a0a090b6b910621463b4ac97a6d1d054469
tree: 04a6e6f212d285c7940f5765fe617d98a255d002
---

# Authoritative notification-output repair handoff

## Exact candidate

- Commit: `89557a0a090b6b910621463b4ac97a6d1d054469`
- Tree: `04a6e6f212d285c7940f5765fe617d98a255d002`
- Exact parent: `dad6df1d65afce2e9f18aa6d9d50f70ea02872da`
- Worktree: clean; no untracked residue

## Outcome

The shell no longer routes notification popup/center through stale
`QGuiApplication::primaryScreen()`. A GUI-thread runtime adapter binds the
exact current `org.qindaqt.Compositor` owner, subscribes before reading, and
strictly decodes the ordered public `Compositor1.Outputs()` projection. It
withdraws cached order before invalidation refresh and fences stale owner and
request replies. The pure selector accepts the first semantic output only when
its `outputGeneration` and exact output-ID set match the already accepted
shell-visibility snapshot and current Qt inventory; exact-name `QScreen`
resolution then drives the existing notification window controller. Any
missing/cross-generation/no-match state clears both windows. Panel/dock
safe-visible policy is unchanged.

The original 512-line `shellruntimeapplication.cpp` was decomposed by moving
its cohesive surface reconciliation into
`shellruntimeapplication_surfaces.cpp`; no source-shape exception was added.

## Changed paths

- `src/shell/runtime/{compositoroutputauthority,notificationoutputselector,qtcompositoroutputauthority}.{h,cpp}`
- `src/shell/runtime/shellruntimeapplication{.h,.cpp,_surfaces.cpp}`
- `src/shell/CMakeLists.txt`
- `tests/shell/{notification_output_test_support.h,tst_notificationoutputselector.cpp,tst_qtcompositoroutputauthority.cpp,CMakeLists.txt}`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/{notification-presentation,panel-surfaces}.md`

## Acceptance evidence

- Strict GCC 15 Debug configure/build with warnings as errors: production
  `qindaqt-shell` and both new tests passed, exit 0.
- Strict GCC 15 Release configure/build with warnings as errors: production
  `qindaqt-shell` and both new tests passed, exit 0.
- Debug adjacent selector
  `^qindaqt\.(notification-(presentation.*|privacy-policy|quieting-settings-bridge|surfaces-offscreen|focus-offscreen|quieting-controls-offscreen|center-applet-offscreen|center-entry|output-.*)|shell-capture-matrix|shell-runtime-catalog)$`:
  20/20 passed, exit 0.
- Release same adjacent selector: 20/20 passed, exit 0.
- Debug focused private-bus/selector repeat `--repeat until-fail:25`: both rows
  passed 25 consecutive executions, exit 0.
- Post-commit focused Debug and Release selectors: 2/2 passed in each, exit 0.
- `tools/check-source-shape --largest 20`: exit 0, 1,672 files checked; only
  the three unrelated pre-existing warnings were reported.
- `tools/validate-docs`: exit 0, 110 documents/navigation validated.
- strict MkDocs build: exit 0.
- diff whitespace, exact-base/provenance, owned-path, untracked-residue, and
  clean-worktree gates: exit 0.

## Bounded caveat and next action

Per ownership, no nested compositor/session runtime was started. S3 retains
the post-integration live WL-0 to WL-1 transfer and layer-surface evidence rerun.
Before integration, assign a different worker to review the exact immutable
commit above. The review should independently verify public semantic-order
authority, exact-owner/invalidation fencing, generation/ID joins, fail-closed
surface lifetimes, the stale-WL-0 mutation control, and absence of forbidden
session/compositor/Display changes.
