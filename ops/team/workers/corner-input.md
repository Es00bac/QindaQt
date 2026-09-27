# Corner input worker

- Status: waiting — native positive and negative gates verified; final exact candidate awaits review
- Base: 46816db4
- Worktree: /home/cabewse/work_space/container-wm-corner-input
- Owned paths: compositor/patches, focused native input tests and documentation

## Updates

- 2026-09-27T18:51:00Z — Claimed isolated patch audit and real pointer-through-window proof; manager confirmed installed KWin on both machines lacks required patch.
- 2026-09-27T18:54:00Z — Audited pinned KWin hitTest routing and unchanged production patch; manager is building missing packaged patch. Added test-only upstream two-window pointer regression and isolated runner. Metadata/patch applicability and 418-document checker pass; native execution awaits retained package build.
- 2026-09-27T18:56:00Z — Manager requested worker slot release while KWin builds; candidate is pushed and static review is clean. Native gate remains required and will resume on explicit follow-up.
- 2026-09-27T19:10:00Z — Native compile found protected Window predicate; changed test to public workspace move/resize owner invariant, retaining exact upper geometry and actual lower-client delivery assertions. Manager reruns native gate.
- 2026-09-27T19:11:00Z — Removed redundant qRound from integral buffer geometry top; avoids Qt6 ambiguous overload.
- 2026-09-27T19:15:00Z — Moved client pointer creation before mapped-window waits so the Wayland bind reaches the server before injected motion; removed redundant qRound on integral top margin. Manager reruns actual positive gate.
- 2026-09-27T19:20:00Z — Native real-window proof passes patched KWin (3/3 rows,116ms); exact same executable with retained unpatched6.6.6 library fails cutout assertion (2pass/1expectedfail,118ms). Added explicit private Wayland sync after motion/buttons; both lower-client button events observed and upper frame unchanged.
