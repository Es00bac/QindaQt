# ADR-0390: Keep snapshot revisions out of retained task-list rows

- Status: Accepted
- Date: 2026-10-16

## Context

[ADR-0278](0278-retain-dock-delegates-by-presentation-identity.md) made the
strip retain a delegate per `(taskId, windowId)` and update it in place with
`dataChanged`. Every projected row also carried `generationRevision`, the task
list source revision it was projected from, and that revision advances for every
publication. `KeyedRowModel` compared whole rows, so every publication changed
every row: a terminal tab that retitles itself ten times a second made a
16-window strip emit 16 `dataChanged` per update and re-evaluate every delegate
binding, which together with uncached icon probing
([ADR-0392](0392-memoise-icon-availability-beside-the-uncached-locator.md)) kept
the shell near 80 ms of CPU per update. The 2026-10-16 stability review traced
the shell's sustained CPU to exactly this.

## Decision

A revision describes the snapshot, not the row, so it is published once, by the
facade that owns the snapshot:

- `TaskListAppletController::generationRevision` (NOTIFY `stateReprojected`) is
  the revision of the **last reprojection**, never the source's live revision.
  Between a publication and its reprojection the strip still shows the
  previous generation, and a gesture then must fence against what is shown.
  Strip QML reads it at the moment of a gesture and echoes it into the intent;
  the T0 stale-id arbitration is unchanged.
- Projected rows no longer contain `generationRevision`.
- `KeyedRowModel` gains `volatileRoles`: fields named there are dropped from the
  published `rowData` and never count as a change. Identity roles can never be
  volatile (a name that overlaps `identityRoles` is ignored). The strip declares
  `volatileRoles: ["generationRevision"]` so a producer that still emits the
  field cannot reintroduce the churn.
- `windowRows` is built on first read after a reprojection instead of on every
  reprojection; only an ungrouped strip reads it.

## Consequences

A title change reaches exactly one row as exactly one `dataChanged`; an
unchanged row emits nothing; insert, remove and reorder behave as before.
The benchmark in `tests/shell/task_list/tst_task_list_update_cost.cpp` measures
200 title updates on 16 windows over the host's real desktop entries and icon
themes: about 82 ms of CPU and 16 touched rows per update before, about 0.6 ms
and 1 touched row after.

Compatibility: `entry.generationRevision` is gone from first-party QML.
Anything that fenced from a retained row must read
`access.generationRevision`; a retained row would be stale by design. Rows from
other facades are not affected. The dock retained-delegate test now asserts the
absence of the field and that a gesture fences with the controller revision.

## Revisit when

A source starts publishing per-task revisions. A per-row revision that changes
only when that row changes would be a legitimate row role and would need no
volatile treatment.
