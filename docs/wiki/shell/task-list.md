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

## Production facts producer

`src/shell/task_list/producer/` is the shell-side facts producer over the
public [Compositor1](../reference/compositor-control-v1.md) authority. It binds
the exact current unique owner of `org.qindaqt.Compositor` on an injected bus
connection (the same owner-binding pattern as the integrated compositor output
authority), never the replaceable well-known name and never the host
compositor outside a real session.

One refresh joins three reads under that owner: `Windows()` (identity,
activation, minimized, taskbar policy, container membership), `Containers()`
(container revisions and authority), and `ShellVisibilitySnapshot()` (the sole
public output/workspace scope authority — `Windows()` carries neither). All
three `WindowsChanged`, `ContainerCommitted`, and `ShellVisibilityChanged`
signals are treated as invalidation hints only. An invalidation racing the
three reads fences the whole refresh: the complete set is discarded and
re-read once after a fixed-leading debounce, so a published generation never
mixes two compositor states. There is no polling beyond that debounce and a
bounded retry backoff after failures.

Classification deliberately reuses the compositor's collapsed native identity
instead of fanning out per-container `Snapshot` reads (which could never be
coherent with the `Windows()` read they would supplement): exactly one member
of each container carries `skipTaskbar == false`, and that member becomes the
`ContainerPrimary`. A container with zero or two such members, a window naming
an unknown container, a container with no published member, or a published
window missing from the scope snapshot each reject the whole batch: the source
is marked `Degraded` and the last accepted generation stays visible. A
standalone window with `skipTaskbar == true` opted out of task lists and
produces no fact. Suppressed members copy the primary's output/workspace
scope, because the primary's placement describes the whole container.

Two honest substitutions hold until the protocol grows (see below):
`applicationName` falls back to the application id, and every fact's `urgent`
flag is `false`. Neither is fabricated state: both are the absence of a wire
field.

## Operation adapter

`src/shell/task_list/operations/` executes intent through the public mutation
surface. Every admission is fenced atomically — before any bus traffic —
against the producer's exact unique owner, the accepted generation revision
(`StaleGeneration` on mismatch, `SourceNotReady` while Loading/Degraded), and
the container lineage of the accepted generation (`UnknownContainer`).
Requests are serialized: one in flight, a second request is rejected `Busy`
rather than queued, because a queued intent would act on a generation the user
no longer sees. Each admitted request gets one monotonic token; a transaction
is submitted exactly once, and timeouts, malformed replies, or owner loss in
flight finish as `Uncertain` and are never resubmitted.

The operations the protocol admits are wired through:

- `activateContainerPage` and `detachWindow` build a Compositor1 `Submit`
  transaction with `expectedRevision` set to the container revision of the
  accepted generation;
- `releaseContainer` calls `ReleaseContainer`;
- `dockWindows` calls `DockWindows` after validating orientation, position,
  and ratio.

`Submit` and `ReleaseContainer` mutate `control-bridge` containers only, so
the adapter rejects them for `hybrid-process` containers
(`UnsupportedAuthority`) instead of issuing a call the compositor must refuse;
process-local Hybrid topology remains the compositor's own authority
([hybrid topology](../architecture/hybrid-topology.md)).

## Protocol extension requests

Compositor1 1.1 does not expose the following; each is a candidate compositor
lane, not a shell workaround:

1. Window-level `activate`, `minimize`, and `close` operations for ordinary
   windows and for whole containers (the adapter finishes these T0 intents as
   `Unavailable` with codes `compositor-window-*-unavailable` /
   `compositor-container-*-unavailable`).
2. Per-window `urgent`/demands-attention state in `Windows()` (the producer
   currently publishes `urgent: false` for every fact).
3. An application display name in `Windows()` (the producer currently
   publishes `applicationName == applicationId`).
4. Mutation authority for `hybrid-process` containers, if task-list page
   activation/detach should cover production groups.

Container close policy (Close All / Ungroup / Cancel) stays with the later
shell composition lane; its Ungroup arm maps to `releaseContainer`.

## Current implementation

The source/static slice at `src/shell/task_list` implements the values,
validation, grouping, filtering, presentation projection, and intent
arbitration described above. The T1 slice adds the production facts producer
and the operation adapter, both covered by hostile unit rows and a private-bus
transport row in `tests/shell/task_list` (see the
[testing harness](../development/testing-harness.md)). Both slices are
registered in the combined source/test build but are deliberately not
instantiated by the production shell; shell composition (including composition
of the published exact-owner `src/shell_window_actions_client` behind accepted
window intents), QST-1 presentation, and installed keyboard/accessibility
qualification remain later slices and are not claimed here.
