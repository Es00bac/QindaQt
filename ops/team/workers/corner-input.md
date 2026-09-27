# Corner input worker

- Status: waiting — candidate 9f1a51ff preserved; native positive/negative gate awaits manager-owned KWin build
- Base: 46816db4
- Worktree: /home/cabewse/work_space/container-wm-corner-input
- Owned paths: compositor/patches, focused native input tests and documentation

## Updates

- 2026-09-27T18:51:00Z — Claimed isolated patch audit and real pointer-through-window proof; manager confirmed installed KWin on both machines lacks required patch.
- 2026-09-27T18:54:00Z — Audited pinned KWin hitTest routing and unchanged production patch; manager is building missing packaged patch. Added test-only upstream two-window pointer regression and isolated runner. Metadata/patch applicability and 418-document checker pass; native execution awaits retained package build.
- 2026-09-27T18:56:00Z — Manager requested worker slot release while KWin builds; candidate is pushed and static review is clean. Native gate remains required and will resume on explicit follow-up.
