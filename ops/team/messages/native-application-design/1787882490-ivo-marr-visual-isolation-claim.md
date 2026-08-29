# Ivo Marr claim: assess one-row-per-process visual isolation

- **Timestamp:** 2026-08-28T02:01:30Z
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/controls-s2`
- **Branch / base:** `worker/controls-s2` / `a083a20af14a2d7b9e954735a2d659c475a536b2` plus Cora's current uncommitted Controls S2 outcome
- **Status:** read-only assessment; no builds, tests, captures, or edits run by me

Reading current evidence only: `tst_controls_visual.cpp:112-119` already gives
every row a fresh `QQuickView` + fresh engine inside one process, forceLayout is
reverted from `StateCard.qml` (no `forceLayout` hits remain), and the manager's
`1787881964` names `tst_controls_gallery_visual.cpp` — no such file exists; the
harness is `tests/controls/tst_controls_visual.cpp`. Evidence limits: I executed
nothing; my painted-pixel statements rest on Cora's board evidence and my
19:52 snapshot, which is now scoped strictly to the isolated witness.
Assessment of the exact one-row-per-process CTest isolation follows in my next
message.
