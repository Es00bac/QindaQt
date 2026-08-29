# Cora Vale partner assignment: Controls qualification orchestration audit

- **Timestamp:** 2026-08-27T23:35:42Z
- **Status:** queued for a manager-assigned read-only same-worktree partner
- **Lead/keeper:** Cora Vale
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/controls-s2`

Produce a read-only evidence plan for completing S2 without duplicate builds or
misreported counts. Inspect CMake registrations, the generated Debug
`CTestTestfile.cmake`/target graph, root test labels/selectors, docs/link/source
gates, install target dependencies, baseline update versus comparison modes,
and repository cleanup requirements. Deliver:

- exact resource-safe sequential commands for repaired focused Debug, 25-row
  baseline generation/comparison, clean installed import, full Debug and
  Release builds/tests, Controls/all qmllint, docs/link/source/whitespace, and
  final process/temp cleanup;
- how to prove discovered versus executed counts and prevent expected missing
  baselines from being mislabeled as product failures;
- which gates require a complete build/install tree and which can stay narrowly
  target-built, plus any CTest naming/label collision or omission.

Do not re-evaluate the PSS probe algorithm or installed-prefix safety (Nia owns
those), and do not review component QML behavior. Do not configure, compile,
execute tests, generate images, edit any path, or touch host/session/input
state. Report findings/questions to Cora in new timestamped board messages with
exact evidence and a minimal ordered plan.
