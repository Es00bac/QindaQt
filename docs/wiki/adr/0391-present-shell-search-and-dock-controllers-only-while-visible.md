# ADR-0391: Present shell search and dock controllers only while visible or changed

- Status: Accepted
- Date: 2026-10-16

## Context

The task list reprojects ten or more times a second under a title spinner. Three
controllers listened to every reprojection and did their full work each time:
`CommandSearchController` re-collected and re-ranked candidates from the
launcher, global menu, windows and Settings sources although no search popup was
open; `QuickLaunchController` rebuilt every dock row; and
`ActiveApplicationController` republished its row. The launcher in particular
projects the whole application catalog on every consultation.

## Decision

Work follows visibility and inputs:

- `CommandSearchController` keeps one derived cache per source with a
  per-source staleness bitmask. A facade change only marks its source stale
  while nothing is presented; while presented, it refreshes just that source
  and notifies. `present()` and `dismiss()` are a counted lease (one controller
  serves every panel's popup); the first `present()` brings the cache up to date
  and notifies once. Any read (`results`, `resultCount`, `phaseText`,
  `rankedCandidates`, `activate`) refreshes a stale cache first, so a caller
  that never presents still reads current truth; only change signals depend on
  presentation. `CommandSearchPopup` takes the lease in `aboutToShow` and gives
  it back exactly once on close or destruction, before clearing the query.
- `QuickLaunchController` compares the running-window signature (which windows
  of which application run, and which is active) and returns before building
  any row when nothing it presents changed. It stores no snapshot revision;
  `activate()` asks the task-list facade for the revision it is showing at the
  moment of the gesture ([ADR-0390](0390-keep-snapshot-revisions-out-of-retained-task-rows.md)).
- `ActiveApplicationController` is silent unless its row, phase or
  manageability changed, and publishes `rowRevision`, which advances exactly
  when the displayed row changes. The popup compares `rowRevision`, not the
  generation, to decide that what it shows was replaced, so a spinner on another
  window no longer closes it ten times a second.

## Consequences

An idle desktop with a title spinner no longer collects, ranks or rebuilds any
of these; the measured per-update cost is in the task list update-cost
benchmark. Tests: `tst_command_search_controller` (collection counts across
presented and idle changes, lease counting, fresh reads without a lease),
`tst_quick_launch_and_active_application` (rebuild counts, silent identical
publications, revision at gesture time) and
`tst_desktop_controls_qml_active_application` (the popup survives an identical
republish and a spinner elsewhere and retires when its row changes). The
`present()`/`dismiss()` pair is part of the facade surface for any new search
surface; a surface that forgets it still works but shows results that refresh
only on read.

## Revisit when

A second consumer needs different laziness than a counted lease, or a source
can report which candidates changed so partial refresh within a source pays.
