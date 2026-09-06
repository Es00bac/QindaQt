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
- compositor window type (`Normal` or `NonNormal`) and client ownership
  (`Application` or the authenticated `BoundShell` panel owner);
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

## Native task identity qualification

The compositor's development workflow checks the native boundary as well as
the shell projection. After a two-window dock, the authenticated development
`Windows()` observation must expose one member with both `skipTaskbar` and
`skipSwitcher` clear; every other member of the container, including inactive
pages, has both flags set. The same assertion is made after page activation,
page detachment, singleton unwrapping, and a second dock. Releasing a
container restores every detached member's original native task identity and
geometry before the container disappears.

This is an executable private-session proof: run the registered development
compositor workflow through `qindaqt-session-probe` and inspect its JSON
result. It uses the authenticated compositor `Windows()` surface, so it does
not scrape a panel or depend on host desktop state. A physical panel should
therefore show one group row while the group exists, and one standalone row
per member after detach or release.

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
(`Activate`, `Minimize`, `Close`, `Raise`) evaluated against the exact generation the
caller displayed. Stale-id rejection is checked in a fixed order: malformed
request, no accepted generation, degraded source, revision mismatch
(`StaleRevision`), then unknown task id. An accepted outcome reports the
resolved entry kind, the primary window id, deterministic member list, and
displayed minimized state. Activation and raise target the primary. The shell
composition maps Minimize to unminimize when that fenced state was minimized;
container minimize/unminimize and Close All visit every member in canonical
order. Ungroup remains an explicit `releaseContainer` action, while dismissing
the context menu is Cancel. Only shell composition may touch real windows.

## Scope filtering

`TaskListScope` restricts rows per output and workspace; an empty field means
no restriction on that axis. Container rows follow their primary's placement,
and all-workspaces entries participate in every workspace scope. Before either
axis is considered, T0 excludes every non-normal role and every window owned
by the bound shell client. Panels, notification surfaces, and shell popups
therefore cannot become tasks even if a compositor backend reports one as an
otherwise normal managed window.

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
authenticated `CompositorShell1` task-fact authority documented in the
[compositor protocol reference](../reference/compositor-control-v1.md). It binds
the exact current unique owner of `org.qindaqt.Compositor` on an injected bus
connection (the same owner-binding pattern as the integrated compositor output
authority), never the replaceable well-known name and never the host
compositor outside a real session.

Initial owner discovery has three distinct states: unresolved remains Loading,
a resolved empty owner publishes one unavailable observation and becomes
Degraded, and a resolved unique owner starts the first read. Thus a compositor
that is absent at cold start cannot leave the task list silently Loading.

The adapter reads only schema-1 `TaskListSnapshot()` and observes only the
directed `TaskListSnapshotChanged` hint. The payload atomically carries window,
output, workspace, task role, window type, bound-shell ownership, state, and
container-lineage facts plus the action fence; T1 never joins `Windows`,
`Outputs`, `ShellVisibilitySnapshot`, or
`Containers`. Each reply is bounded and fully decoded before publication.

The producer retains `(owner, epoch, revision, payload)` lineage. Under one
owner, an epoch change, revision regression, or changed bytes at an equal
revision is foreign truth and rejects fail-closed. A coherent snapshot
publishes one T0 generation and makes the source Ready. Owner replacement
starts a new lineage, clears the old source generation, and publishes Degraded
truth; late owner/token replies and reads raced by invalidation are discarded.
Transient same-owner transport failures retain the last generation as
Degraded and consume the finite retry schedule once. After it is exhausted,
only a new invalidation or owner edge can trigger another read; there is no
polling loop. Every failed read, owner loss/replacement, and
`stop()` emits `stateChanged`; stop also clears the owner so mutation admission
fails before bus traffic.

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

## Window operations and atomic fact contract

Window-level `activate`, `minimize`, `unminimize`, `close`, and `raise` exist
today on the authenticated `org.qindaqt.CompositorShell1` surface
([ADR-0061](../adr/0061-authenticate-shell-window-actions-by-panel-owner.md)),
not on Compositor1, and are consumed through the published exact-owner
`src/shell_window_actions_client` with its action generation `(epoch,
revision)` as fence. `src/shell/runtime/tasklistappletcomposition.*` routes
accepted Task List window intents through that client. It borrows the exact
instance already owned by `ShellRuntimeApplication` for authenticated identity
and window actions, so Task List never creates a second bus connection or
owner binding. The router requires the facts authority and action client to
name the same unique owner, a Ready displayed revision, and a valid window
generation. It serializes the whole task operation and maps each real reply to
one terminal applet result. Timeout, transport loss, or owner change after
dispatch is `Uncertain` and is never replayed; unbound or mismatched owners are
`Unavailable` before bus traffic. The older Compositor1 operation adapter keeps
its stable `compositor-window-*-unavailable` fallback codes for standalone
consumers and owns only the container operations.

No new Compositor1 window operation is requested, and the task-list modules
must not grow their own identity or action reader. The authenticated
active-window identity
([ADR-0063](../adr/0063-project-authenticated-active-window-identity.md))
remains a separate concern served by that same shell-owned client.

The compositor now supplies the missing read contract on that same authenticated
object. `TaskListSnapshot` includes every managed window's identity,
application ID and label, title, task role, normal/non-normal type,
application/bound-shell ownership, active/minimized/maximized/
fullscreen/demands-attention flags, output and workspace scope, and container
ID. Referenced outputs, workspaces, and container revision/authority entries
travel in the same generation. The snapshot also carries the window-action
fence, so a listed UUID and its mutation generation are sampled together.
Authentication precedes sampling, hostile values reject the complete
candidate, and the no-argument change hint is coalesced and directed only to
the bound shell owner
([ADR-0073](../adr/0073-publish-atomic-authenticated-task-facts.md)).

Container page activation/detach for `hybrid-process` groups remains outside
the older Compositor1 transaction bridge. This does not prevent ordinary task
activation, whole-container minimize/restore, Close All, or raise through the
authenticated window-action policy.

The hosted policy selects Close All for the Close action, maps Ungroup to
`releaseContainer`, and treats context-menu dismissal as Cancel. Close All and
container minimize/unminimize send one authenticated request per canonical
member but expose one outer pending operation and exactly one terminal result.

## Applet presentation

`src/shell/task_list/applet/` owns the registered panel presentation slice: the
pure bounded strip projection, the shell-private `TaskListAppletController`,
its injected operation seam, and the compiled `QindaQt.Shell.TaskList` 1.0
module. The slice is a **registered and hosted built-in**: the
manifest (`data/applets/task-list.json`), the audited registry entry
(`qindaqt.applets.task-list`), and the policy decisions resolve a profile
instance to `ready`, and the production shell/preview dispatchers render that
entry on horizontal and vertical panels.

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
Context actions per row are Activate, Minimize/Unminimize, Close, Raise, and —
for container rows — Ungroup, which maps to the T1 `releaseContainer`. Close
uses the hosted Close All policy. Shell composition injects one
`DesktopEntryIconResolver` built from explicit freedesktop application roots
and one `IconThemeLocator` over the icon roots used by QML. Each row resolves
the compositor-provided application id to the desktop entry's `Icon=` name and
publishes whether theme lookup succeeded. Horizontal buttons show the icon plus an elided title
inside an 84–168 by 28 logical-pixel bound; vertical buttons show only the
icon. Missing and hostile mappings use the typed application placeholder.

An opt-in panel dock host may set the compiled applet's `dockMode` property
and its bounded `dockTileSize` (56–64 logical pixels; 60 is the profile
default). In that mode each existing task row becomes one icon-only tile with
a 40-pixel icon, active surface, and running indicator; it does not fabricate
pinned entries. The dock reserves the full tile before a hover lift and small
magnification, while `reducedMotion` disables both movement and scaling.
Tooltips and task context menus use their own popup window so they are not
clipped by the panel band. `dockHasLauncherGroup` is a composition-supplied
truth value: a separator appears only when both a preceding real launcher
group and at least one task row are present. The default taskbar geometry and
behavior remain unchanged.
Degraded truth is an accessible warning glyph rather than a “Limited” badge,
and loading/empty/unavailable phases use one compact phase icon without panel
text.

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

Window-level activate, minimize/unminimize, close, and raise route through the
published exact-owner `src/shell_window_actions_client`
([ADR-0061](../adr/0061-authenticate-shell-window-actions-by-panel-owner.md));
the applet presents refused or uncertain outcomes truthfully as feedback.
The authenticated active-window identity
([ADR-0063](../adr/0063-project-authenticated-active-window-identity.md))
remains a separate single-client concern the applet never touches.

Focused rows are selected with `ctest -R '^qindaqt\.task-list-applet-'`:
pure projection bounds/overflow, controller fencing over fake seams (cold
start, capability gates, stale/foreign lineage, synchronous-completion
attribution, owner loss), bridge dispatch over the real adapter with a fake
transport, the production composition over a private bus (all five action
methods, grouped sequencing, owner-loss uncertainty and stale-truth clearing),
fatal-warning-clean offscreen QML state/keyboard and horizontal/vertical
dispatcher rows, a static boundary poison probe, and a relocated
installed-package/source-poison proof of the `TaskListAppletRuntime` component.
No focused row contacts a host bus, display, or compositor.

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
described in the previous section. T3 composes those slices into the production
shell, hosts the ninth built-in in both dispatchers, places an exact Task List
instance in task-oriented stock profiles, and packages its compiled module in
every shell-carrying component. The current reader publishes Ready only from
the authenticated atomic task-fact contract. It re-degrades on malformed or
incoherent data or owner loss and clears old-owner truth on replacement.
Private-bus transport and composition rows, offscreen keyboard/accessibility
rows, and contained boot jointly qualify the boundary.

`ShellDevelopment1.taskList` reports the live hosted applet's phase,
generation, visible window count, and per-button application ID, icon name,
and `iconResolved` truth. Its `panelApplets` array records normalized profile
instances. The interactive 1080p row requires exactly Settings and Text Editor,
resolved icons for both, no bound-shell task, one ready launcher, and one task
list on the smart shelf. Legacy `application-launcher` aliases normalize to
`launcher`; a legacy task-list alias normalizes only when no canonical task
list exists and is otherwise dropped as redundant. The independent
Compositor1 inventories remain diagnostic inputs and must not be substituted
or joined by shell code.
