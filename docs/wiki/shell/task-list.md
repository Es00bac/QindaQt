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
`Degraded` (the facts producer is unavailable; any last generation stays
visible, but every intent is refused until a fresh publish succeeds). A failed
first refresh is also `Degraded`, distinguishing known unavailability from a
read that is still loading. An empty
selection presents `Empty` even while degraded. Every presented row carries a
1-based keyboard index in canonical order and a deterministic accessible name
composed as application, title, window count for grouped containers, then the
state suffixes `active`, `minimized`, and `urgent` in that order.

## Public facts producer boundary

`src/shell/task_list/producer/` is the shell-side facts producer over the
public [Compositor1](../reference/compositor-control-v1.md) authority. It binds
the exact current unique owner of `org.qindaqt.Compositor` on an injected bus
connection (the same owner-binding pattern as the integrated compositor output
authority), never the replaceable well-known name and never the host
compositor outside a real session.

Initial owner discovery has three distinct states: unresolved remains Loading,
a resolved empty owner publishes one unavailable observation and becomes
Degraded, and a resolved unique owner starts the first read. Thus a compositor
that is absent at cold start cannot leave the task list silently Loading.

The adapter reads only schema-2 `Windows()` and observes only
`WindowsChanged`. It never reads or combines `ShellVisibilitySnapshot`: that
method is an independent panel-visibility inventory, and its canonical
contract explicitly says clients must not combine it with `Windows()`. An
equal epoch/revision fence does not override that rule. `Containers()` is also
not joined, because its per-container revisions do not form one atomic
generation with `Windows()`.

Each `Windows()` reply is bounded and fully decoded under the exact owner. The
producer retains `(owner, epoch, revision, payload)` solely as window-inventory
lineage. Under one owner, an epoch change, revision regression, or changed
bytes at an equal revision is foreign truth and rejects fail-closed. Owner
replacement starts a new lineage; late owner/token replies and reads raced by
`WindowsChanged` are discarded. Transient transport failures use a bounded
retry schedule, while the known protocol gap does not poll.

Compositor1 1.1 has no single payload carrying every T0 fact. `Windows()` lacks
output/workspace scope, application display name, urgency, and an atomic
container-revision/authority join. Consequently this adapter deliberately
does not publish a generation and exposes no container lineage: it marks the
source `Degraded`, retaining any previously injected generation, until a
coherent public task-list inventory exists. Inventing placeholders or joining
the panel snapshot would turn absence into false UI truth. Every failed read,
owner loss/replacement, and `stop()` emits `stateChanged`; stop also clears the
owner so mutation admission fails before bus traffic.

## Operation adapter

`src/shell/task_list/operations/` executes intent through the public mutation
surface. Every admission is fenced atomically — before any bus traffic —
against an injected read-only authority's exact unique owner, accepted
generation revision
(`StaleGeneration` on mismatch, `SourceNotReady` while Loading/Degraded), and
the container lineage of the accepted generation (`UnknownContainer`).
Requests are serialized: one in flight, a second request is rejected `Busy`
rather than queued, because a queued intent would act on a generation the user
no longer sees. Each admitted request gets one monotonic token from the
transport that owns pending calls, so a late reply from a destroyed adapter
instance can never settle a reconstructed adapter sharing that transport. No
global allocator or process singleton is involved. A transaction is submitted
exactly once, and timeouts, malformed replies, or owner loss in flight finish
as `Uncertain` and are never resubmitted.

Replies settle only on the canonical reply lineage: a `Submit` reply must echo
the protocol (major 1, minor at most 1), the exact `transactionId` and
`containerId` of the submitted transaction, a known `status`, and a canonical
`revision`; `committed` additionally requires the fenced container revision
advanced by exactly one. A reply
missing or contradicting any echo finishes `Uncertain`
(`reply-lineage-mismatch`), because the transaction may have committed.
Successful `DockWindows` replies carry compositor-generated ids, so they are
validated for protocol, id presence, and the protocol-fixed revision `"1"`;
any other parseable revision is uncertain lineage. `ReleaseContainer`
replies
carry only `status` and `failure` on the wire and are matched by the
exact-owner pending-call binding alone.

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

## Window operations and remaining protocol gaps

Window-level `activate`, `minimize`, `unminimize`, `close`, and `raise` exist
today on the authenticated `org.qindaqt.CompositorShell1` surface
([ADR-0061](../adr/0061-authenticate-shell-window-actions-by-panel-owner.md)),
not on Compositor1, and are consumed through the published exact-owner
`src/shell_window_actions_client` with the displayed
`ShellVisibilitySnapshot` generation `(epoch, revision)` as fence. The
task-list adapter therefore finishes window-level T0 intents as `Unavailable`
(codes `compositor-window-*-unavailable` /
`compositor-container-*-unavailable`) only until the later shell composition
lane routes accepted intents through that client; no new Compositor1 window
operation is requested, and this module must not grow its own identity or
action reader — the authenticated active-window identity
([ADR-0063](../adr/0063-project-authenticated-active-window-identity.md)) is a
separate single-client concern.

The existing authenticated client closes the ordinary window-action gap, but
Compositor1 1.1 still lacks the following task-list facts; each is a candidate
compositor lane, not a shell workaround:

1. One atomic task-list inventory containing window identity/title/app ID,
   output/workspace scope, collapsed container role plus revision/authority,
   activation/minimized/urgent state, and its own owner/epoch/revision lineage.
   Extending `Windows()` with those fields or adding a dedicated method are
   both compositor-owned choices; the shell must not assemble the generation
   from independent inventories.
2. An application display name and `urgent`/demands-attention fact in that
   coherent inventory.
3. Mutation authority for `hybrid-process` containers, if task-list page
   activation/detach should cover production groups.

Container close policy (Close All / Ungroup / Cancel) stays with the later
shell composition lane; its Ungroup arm maps to `releaseContainer`.

## Applet presentation

`src/shell/task_list/applet/` owns the registered panel presentation slice: the
pure bounded strip projection, the shell-private `TaskListAppletController`,
its injected operation seam, and the compiled `QindaQt.Shell.TaskList` 1.0
module. The slice is a **registered built-in, deliberately not hosted**: the
manifest (`data/applets/task-list.json`), the audited registry entry
(`qindaqt.applets.task-list`), and the policy decisions resolve a profile
instance to `ready`, but production-shell dispatcher composition
(`src/shell/runtime`, `src/shell/qml`) remains the later hosting lane.

The controller composes the accepted T0/T1 boundaries over injected seams and
owns no bus connection of its own:

- it borrows the composition-owned `TaskListSource` for the accepted
  generation and stale-id intent arbitration;
- it observes the producer through the read-only `TaskListOperationAuthority`
  (the production authority is the T1 facts producer), so owner loss or
  replacement reprojects immediately; and
- it dispatches through the injected `TaskListAppletOperationPort`, whose
  production implementation (`TaskListAppletOperationBridge`) forwards to the
  T1 operation adapter unchanged. The port may finish a fenced rejection
  synchronously inside the dispatch call; the controller buffers results
  emitted mid-dispatch and attributes them strictly by token, drops results
  for unknown tokens, and keeps one pending marker per task until the exactly
  one terminal result arrives — state changes never clear a pending marker.
  A dock dispatch names two tasks, so its single token marks **both**
  participants pending and its terminal result releases both; neither side
  may attract a second mutation while the dock is in flight.

The manifest requests `windows.read`, `windows.activate`, and
`windows.manage`; the installed policy grants them to the audited package
through the audited-builtin trust default and explicitly denies `windows.read`
and `windows.activate` to third-party packages (the wildcard already denies
`windows.manage`). The composing shell passes the evaluated grants to the
controller at construction, immutable afterwards. `windows.read` denial
withholds all observation (phase `unavailable`, no rows, no dispatch);
`windows.activate` and `windows.manage` gate their intents independently with
pre-dispatch refusals and user feedback.

Presentation phases are the T0 projection plus the read-denial state:
`loading` (no accepted generation — cold start is Loading and the T1 producer
degrades explicitly once owner discovery resolves, so Loading cannot persist
silently), `ready`, `empty` (nothing visible in scope, even while degraded),
`degraded` (producer unavailable; the retained generation stays visible but
every intent is refused — a failed first refresh with no accepted generation
is also `degraded`, never `empty`), and `unavailable` (read capability
denied). The
strip presents at most 64 rows in canonical order — also the Tab and arrow
traversal order — and reports the exact hidden count as overflow truth. Every
row carries its generation revision and echoes it into each intent, so the T0
arbitration refuses actions against a generation the user no longer sees.
Context actions per row are Activate, Minimize, Close, and — for container
rows — Ungroup, which maps to the T1 `releaseContainer`; container close
policy (Close All / Ungroup / Cancel) itself stays with the later shell
composition lane. Rows show a typed one-letter icon placeholder derived from
the application identity: no freedesktop/QIcon seam exists in the tree yet,
and inventing one here would duplicate launcher's future authority.

The compiled module follows the first-party presentation rule from
[Module boundaries](../architecture/module-boundaries.md): both QML files
import `QindaQt.Controls 1.0` explicitly (labels, the dismiss button, and the
row focus ring are Controls primitives) and resolve every remaining color,
spacing, radius, and type metric from the read-only QST-1 `QindaQt.Tokens`
singleton. The applet owns no palette, theme map, or fallback colors — a
boundary probe rejects hex literals and missing imports — and the row's
context menu uses the QQC2 style palette because Controls ships no menu
primitive yet. The composing shell publishes the theme through the same
Tokens facade seam the offscreen rows exercise.

Window-level activate/minimize/close still finish `Unavailable` through the T1
adapter until the composition lane routes them through the published exact-owner
`src/shell_window_actions_client` ([ADR-0061](../adr/0061-authenticate-shell-window-actions-by-panel-owner.md));
the applet presents that outcome truthfully as feedback instead of hiding it.
The authenticated active-window identity
([ADR-0063](../adr/0063-project-authenticated-active-window-identity.md))
remains a separate single-client concern the applet never touches.

Focused rows are selected with `ctest -R '^qindaqt\.task-list-applet-'`:
pure projection bounds/overflow, controller fencing over fake seams (cold
start, capability gates, stale/foreign lineage, synchronous-completion
attribution, owner loss), bridge dispatch over the real adapter with a fake
transport, fatal-warning-clean offscreen QML state/keyboard rows, a static
boundary poison probe, and a relocated installed-package proof of the
`TaskListAppletRuntime` component. No row contacts a host bus, display, or
compositor, and no nested session is claimed.

## Current implementation

The source/static slice at `src/shell/task_list` implements the values,
validation, grouping, filtering, presentation projection, and intent
arbitration described above. The T1 slice adds the exact-owner, fail-closed
public facts reader and the operation adapter, both covered by hostile unit
rows and a private-bus
transport row in `tests/shell/task_list` (see the
[testing harness](../development/testing-harness.md)). The T2 slice adds the
registered applet controller, compiled `QindaQt.Shell.TaskList` presentation,
manifest/policy/registry entry, and `TaskListAppletRuntime` install component
described in the previous section. These slices are
registered in the combined source/test build but are deliberately not
instantiated by the production shell. The current reader intentionally cannot
publish Ready from Compositor1 1.1; the coherent inventory is a compositor
prerequisite. Composing the published exact-owner
`src/shell_window_actions_client` behind accepted window intents, production
dispatcher hosting of the registered applet, and installed nested
keyboard/accessibility qualification remain later
shell slices and are not claimed here.
