# Corner input worker

- Status: working — audit Corner Bar KWin cutout and add native input evidence
- Base: 46816db4
- Worktree: /home/cabewse/work_space/container-wm-corner-input
- Owned paths: compositor/patches, focused native input tests and documentation

## Updates

- 2026-09-27T18:51:00Z — Claimed isolated patch audit and real pointer-through-window proof; manager confirmed installed KWin on both machines lacks required patch.
- 2026-09-27T18:54:00Z — Audited pinned KWin hitTest routing and unchanged production patch; manager is building missing packaged patch. Added test-only upstream two-window pointer regression and isolated runner. Metadata/patch applicability and 418-document checker pass; native execution awaits retained package build.
