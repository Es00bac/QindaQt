# Task list source model

The task list shows one row per visible task: a standalone window, or a whole
QindaQt container collapsed to its primary active-page member (see
[Window containers](../architecture/window-containers.md)). The
`src/shell/task_list` module is a pure injected-facts boundary, mirroring the
[panel visibility policy](panel-visibility.md): it consumes immutable window
values supplied by shell composition, and it neither inspects KWin objects,
mutates windows, nor performs activation, minimize, or close itself.

## Injected facts contract

One publish supplies every window as an immutable `TaskWindowFact` value
copied from one coherent compositor/hybrid generation:

- unique window identity, application identity and display name, and window
  title;
- compositor-assigned output plus the window's virtual-desktop scope (an
  explicit all-workspaces flag, or a workspace id list);
- the task-list role: `Standalone`, `ContainerPrimary`, or `ContainerMember`;
- presentation state: `active`, `minimized`, and `urgent`.

The producer classifies containers before publishing: exactly one primary
represents each container, and suppressed members reference it by container id.
The module validates the batch and rejects it atomically on the first fault —
duplicate or empty identities, role/container-id conflicts, an orphaned member,
two primaries for one container, more than one active window, an active
minimized window, active or minimized state on a suppressed member, a
standalone window id colliding with a container id, or any bound violation
(4,096 windows, 64 workspaces per window, 512-character identities). A rejected
batch never replaces the retained generation; shell presentation keeps showing
the last coherent task list.

## Grouping and ordering

Entries are the module's canonical projection:

- grouped members never create rows; each container speaks with its primary's
  application identity, title, output, workspace scope, and minimize/active
  state, while a suppressed member's `urgent` flag still surfaces on the
  container row;
- `windowCount` and the sorted `memberWindowIds` describe collapsed membership;
- rows are ordered deterministically: application id, then standalone windows
  before collapsed containers, then task identity.

That canonical order is also the keyboard traversal order, so identical fact
batches always produce identical generations regardless of producer order.

## Request intents

The source turns a user action into a typed `TaskIntentRequest`
(`Activate`, `Minimize`, `Close`) evaluated against the exact generation the
caller displayed. Stale-id rejection is checked in a fixed order: malformed
request, no accepted generation, degraded source, revision mismatch
(`StaleRevision`), then unknown task id. An accepted outcome reports the
resolved entry kind, the primary window id, and the deterministic member list —
activation targets the primary, while container close/minimize policy (Close
All, Ungroup, Cancel) remains with the shell adapter, which is the only
component allowed to touch real windows.

## Scope filtering

`TaskListScope` restricts rows per output and workspace; an empty field means
no restriction on that axis. Container rows follow their primary's placement,
and all-workspaces entries participate in every workspace scope.

## Presentation

`TaskListPresentationModel::project()` maps source status plus generation plus
scope to one of four states: `Loading` (no accepted generation),
`Ready`, `Empty` (an accepted generation with nothing visible in scope), and
`Degraded` (the facts producer is unavailable; the last generation stays
visible, but every intent is refused until a fresh publish succeeds). An empty
selection presents `Empty` even while degraded. Every presented row carries a
1-based keyboard index in canonical order and a deterministic accessible name
composed as application, title, window count for grouped containers, then the
state suffixes `active`, `minimized`, and `urgent` in that order.

## Current implementation

The source/static slice at `src/shell/task_list` implements the values,
validation, grouping, filtering, presentation projection, and intent
arbitration described above, with hostile coverage in
`tests/shell/task_list`. It is deliberately not wired into the shell build,
consumes no transport, and claims no runtime qualification. Remaining work for
later slices: a shell-side facts producer fed from public compositor/hybrid
snapshots, the window-operation adapter behind the intent boundary, QST-1
presentation, and installed keyboard/accessibility qualification per the
[testing harness](../development/testing-harness.md).
