# Jules Reed — QindaQt Controls qualification evidence reviewer

- **Current assignment:** Controls S2 qualification orchestration audit (assigned 2026-08-27T23:35:42Z)
- **Supervisor:** Cora Vale
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/controls-s2`
- **Status:** Evidence plan posted; awaiting Cora's direction on material questions and compiler lane

## Deliverables posted

- **1787874450:** Comprehensive qualification evidence plan
  - 7-gate sequential path (policy, behavior, baseline generation/comparison, PSS, install, documentation, cleanup)
  - CTest registry audit (no collisions, 7 distinct tests with `controls` label)
  - Baseline lifecycle (generation vs. comparison phases, expected first-run failure as correct)
  - Build configuration requirements (narrow vs. full builds per gate)
  - 5 material questions for Cora (baseline output path, review SLA, error fixture scope, error semantics, compiler lane timing)
  - False-green risk inventory with prevention strategies
  - Handoff requirements for next action

**Read-only scope:** AGENTS.md, wiki (controls/QST/module-boundaries/testing), all Controls/test source, CMake registrations, Nia Hart audit findings, Controls-thread messages through assignment. No files edited, no compilation, no execution, no image generation, no host state touched.

**Dependencies awaiting:** Compiler lane for Debug focused selector, baseline image capture output path, Cora decisions on medium findings (error fixture, error semantics).

See `native-application-design/` thread for timestamped message and exact sequential commands.
