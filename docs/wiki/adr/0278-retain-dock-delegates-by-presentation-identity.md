# ADR-0278: Retain dock delegates by presentation identity

- Status: Accepted
- Date: 2026-09-27

## Context

Task-list and Quick Launch Repeaters consumed replacement QVariant lists. A
new task revision, title, focus, or running indicator could destroy every tile
and recreate it, losing hover/focus and replaying animations. Separately,
task-list property getters rebuilt all row metadata for every QML sequence
read, multiplying filesystem icon resolution with the number of windows.

## Decision

The task-list controller publishes bounded entry/window row snapshots once
per reprojection. Property reads return those values without resolving icons
or application metadata. The transport-free `KeyedRowModel` presentation
adapter in the existing TaskList applet module reconciles snapshots by stable
identity using Qt insertion, removal, movement, and data-change signals.
DesktopControls uses that same public presentation adapter through its existing
dependency on the TaskList applet module. It adds no new authority or storage.

Task identities combine `taskId` and `windowId`, retaining distinct members in
ungrouped views. Quick Launch publishes an identity from the stored item kind
and application id or path; Trash has its own fixed identity. A group's first
stored application anchors its identity because applications are globally
unique in the dock. Renaming or moving a group and changing its running state
retain the tile. Replacing that first member may replace the affected group;
this does not introduce a persisted UUID or change the dock schema.

Only newly inserted dock delegates receive a short token-duration opacity
fade. Retained delegates keep animation state; reduced motion makes insertion
immediate and stops an in-progress fade. Removal retires the actionable
delegate immediately. Layout dimensions and input targets are never animated;
existing paint-only magnification and drag transforms remain authoritative.

Launcher persistence retains display values during a validated commit's
same-owner `Authenticating` refresh, using the confirmed snapshot owner and
`SettingsClient::currentOwner()` to fence that gap. It subscribes directly to
owner changes, so a replacement while already Authenticating revokes values
immediately. Edits remain disabled until Ready; transport loss, malformed
resync, and confirmed empty values still clear presentation. This narrows the
previous blanket clearing policy that emptied every dock on recent-app writes.
The Settings client continues validating epoch/revision continuity.

## Consequences

The adapter owns bounded copied presentation values on the GUI thread, emits
no model reset for valid updates, and clears invalid, duplicate-identity, or
oversized snapshots. At most 4,096 rows are retained, matching the task-list
ceiling. It does not implement domain sorting, persistence, or intent fencing.
Those remain in the controllers. Snapshot and model regressions exercise
1,000 repeated reads/updates with 64 rows, verify unchanged resolver/delegate
counts, and cover insertion, removal, reorder, and refreshed row data.

See [Task list](../shell/task-list.md),
[Desktop controls](../shell/desktop-controls.md), and
[Shell iconography](../shell/iconography.md).

The decomposition review keeps diffing in the small C++ adapter rather than
adding list mutation policy to the existing TaskListApplet QML composer. That
composer remains responsible for strip layout, gestures, and preview wiring;
its existing size requires review, and this change keeps it below the 600
nonblank-line ceiling. Preview selection follows the retained task/member
identity when rows move and closes when that identity disappears.
