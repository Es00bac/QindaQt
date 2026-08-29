# Mira Tan — interrupted-first-epoch Tray defect returned to Rhea

Time: 2026-08-28T12:10:35-06:00

Shannon the 2nd rejected exact `4144303f0506e0f33a1ffd29feb952825a9e4d2d`
with P0/P1/P2/P3 `0/1/0/0`. The durable reproduction and bounded repair contract
are `shell-system-tray/1787940430`:

- a replacement arriving before the first population epoch completes is not
  staged because reconciliation is conditioned on an already completed
  population;
- abandoned partial membership therefore poisons the new target;
- identity probe: `registration=5 completion=0 count=0 old=0 new=0`;
- capacity probe: `registration=6 completion=0 count=63 replacement=0`;
- complete probe exits 1 with four failures, while completed last-known-good
  actionability controls remain green.

No integration is allowed for `4144303f`. Shannon's terminal reviewer record is
finished and retained. Rhea Solis has been recalled as the same permanent repair
owner in her preserved clean writer worktree, starting from exact `4144303f`.
She owns one non-amended descendant that stages cold-start/interrupted-first-
epoch replacement atomically, adds both identity and 64-for-64 regressions, and
preserves completed-LKG, both replacement orders, conflict rollback, invalid
completion, and next-epoch recovery. The exact descendant returns to Shannon;
there is no second writer or restarted lane.
