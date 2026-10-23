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
- for a `ContainerPrimary` fact only, the resolved container accent
  (`colorHex`, exact `"#RRGGBB"`; empty remains accepted from older producers);
- compositor-assigned output plus the window's virtual-desktop scope (an
  explicit all-workspaces flag, or a workspace id list);
- the task-list role: `Standalone`, `ContainerPrimary`, or `ContainerMember`;
- compositor window type (`Normal` or `NonNormal`) and client ownership
  (`Application` or the authenticated `BoundShell` panel owner);
- presentation state: `active`, `minimized`, `iconified`, and `urgent`.

`iconified` ([ADR-0203](../adr/0203-an-ordinary-window-rolls-up-to-its-icon.md))
means the window is rolled up to its icon chip: unlike a minimized window it is
still on screen, so it stays a task and is rendered as a rolled hint (a dimmed
row and the accessible-name suffix ", rolled up"). It is a schema-1 wire
boolean beside `minimized` (exact keys in both codecs), copied to the entry by
grouping, and never true for a container member.

For a `ContainerPrimary` fact, `title` is the container's own name: the
user rename or the translated default "Container". `colorHex` is its explicit
color or the current theme accent, resolved by the compositor from the same
style used by container chrome. Native application captions remain unchanged.
The existing schema-1 task fields carry both values; `colorHex` is empty for
every non-primary role and malformed colors are rejected atomically.

The color travels through `TaskWindowFact`, `TaskEntry`, `TaskListAppletRow`
and the QML row map. Both panel rows and dock tiles paint the dedicated
`qindaqt-container-symbolic` emblem in that color. Its enclosing frame,
page tabs and split panes identify a container at small sizes. The shell
embeds this same asset used by Alt+Tab, so an icon theme cannot replace it
with a generic restore-window glyph. Ordinary applications retain their own
icons. See [ADR-0429](../adr/0429-container-and-tab-identity.md).

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

KWin's `qindaqt` WindowSwitcher package consumes the same native
`skipSwitcher` projection for Alt-Tab. KWin remains the activation authority;
the package labels the representative with its container name and paints the
same container emblem and resolved accent. A read-only process-local projection
maps every grouped member's native UUID to its container identity; primary
changes therefore keep that identity, and a dissolved container immediately
falls back to native application presentation. The optional bridge preserves
native presentation when the compositor plugin is absent. Session first-run policy selects that package only
when `TabBox/LayoutName` is absent, so an explicit switcher preference is
preserved.

## Grouping and ordering

Entries are the module's canonical projection:

- grouped members never create rows; each container speaks with its primary's
  application identity, title, output, workspace scope, and minimize/active
  state, while a suppressed member's `urgent` flag still surfaces on the
  container row;
- `windowCount` and the sorted `memberWindowIds` describe collapsed membership;
- rows are ordered deterministically: application id, then standalone windows
  before collapsed containers, then task identity.

That canonical order is the default, and identical fact batches still produce
identical generations regardless of producer order. A persisted user-order
overlay ([ADR-0118](../adr/0118-user-task-order-overlay-and-panel-quick-settings.md))
takes precedence for the displayed order — which is also the keyboard
traversal order: overlayed ids come first in stored order, and every other
entry keeps canonical order behind them. Keyboard indices are renumbered
along the displayed order, so traversal is always the order the strip
presents.

Each container entry also keeps its `members`, in `memberWindowIds` order,
with their own application identity, title, and urgency. The applet
manifest's `grouping` setting chooses the view: `never` presents every window
as its own row, container members included and in displayed entry order,
while `when-crowded` (the default) and `always` present one row per container.
No crowding heuristic exists yet, so `when-crowded` currently collapses
exactly like `always`, and dock strips keep container rows in every mode.

## Request intents

The source turns a user action into a typed `TaskIntentRequest`
(`Activate`, `Minimize`, `Close`, `Raise`) evaluated against the exact generation the
caller displayed. Stale-id rejection is checked in a fixed order: malformed
request, no accepted generation, degraded source, revision mismatch
(`StaleRevision`), then unknown task id. An accepted outcome reports the
resolved entry kind, the primary window id, deterministic member list, and
displayed minimized state. Activation and raise target the primary. A request
may also name one `windowId` of the entry, as an ungrouped row does; a window
outside the entry's members is `UnknownTask`. Such a request narrows Activate
to that window, so activating a background member lets the compositor switch
its container page, and Close to that window alone, while Minimize and Raise
keep acting on the whole container. The shell
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

Urgency is both static and animated where the row renders it. While
`urgent` is projected — for example the [Terminal](../apps/terminal.md)
audible-bell path, whose Qt attention request reaches this projection through
the compositor task facts — every surface carries the text truth first: the
`!` badge inline on panel rows, and the same `!` as a static corner mark on
dock tiles (`TaskListUrgentPulse.qml`). The static marks render whenever
`urgent` holds, independent of motion, so an urgent dock tile stays visibly
distinct from a quiet one with reduced motion enabled and at every rest
frame. On top of that truth, while motion is allowed the applet's
`TaskListUrgentPulse` animation breathes the row's `urgentAttentionLevel`
between full and dimmed opacity at the QST-1 short motion duration, and
`TaskListEntryButton.qml` binds the panel badge and dock tile icon opacities
to that level. The breath is
opacity-only (dock magnification owns icon scale; layout bounds and hit
targets never move), the badge and icon remain visible at the dimmest point,
and the pulse settles back to full opacity the moment urgency clears or the
host reduced-motion policy turns on — the static marks are unaffected.

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

What one changing title may cost is bounded
([ADR-0383](../adr/0383-coalesce-compositor-change-signals-and-repaint-titles-in-the-title-band.md)).
Caption changes reach the producer at most four times a second per window,
with the final title always delivered; the producer samples at most once per
250 ms, sends the hint only when the published bytes changed, and answers
`TaskListSnapshot` from the cached, already encoded generation. The bound
shell's PID is looked up once per connection. A terminal agent's spinner title
(about 10 Hz) therefore costs about four reads a second rather than twenty.
The facts no longer depend on the latest visibility sample being valid: they
carry the last published visibility generation as their action fence, so one
stranded or half-updated window no longer turns the dock `Degraded`
([ADR-0384](../adr/0384-degrade-the-visibility-snapshot-per-window-and-rescue-stranded-windows.md)).
The program behind a Wine or Steam launcher class is resolved once per window;
its Steam name and icon arrive from a worker thread and update the facts a
moment after the window appears.

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

The controller materializes the `entryRows` list once per reprojection,
including application names and icon availability, before emitting
`stateReprojected`; `windowRows` is materialized once on its first read after a
reprojection, since only an ungrouped strip reads it. The `entryRows` and
`windowRows` getters return bounded, implicitly shared snapshots; QML sequence
length/index reads during dock layout and magnification must never repeat
desktop-entry or filesystem icon resolution. Each new
publication refreshes the metadata and revision together, including previously
missing icons. Read denial still publishes empty lists without resolving names
or icons. The focused row-snapshot regression exercises QV4 reads with 1, 12,
and 64 windows and verifies atomic freshness on the next publication.

The compiled strip feeds those snapshots through `KeyedRowModel`, a GUI-thread
presentation adapter exported by the TaskList applet module. Its `rowData`
role updates in place by `(taskId, windowId)` identity; insertions, removals,
and reorders preserve every surviving delegate. It owns at most 4,096 copied
rows, rejects duplicate/empty identities, and has no transport or authority.
DesktopControls reuses this adapter through its existing TaskList dependency.
The root QML file explicitly imports its own module under an alias for the
C++ adapter. Direct-file installed consumers therefore load the same exported
type registration as module-based consumers; implicit local QML-file discovery
is insufficient for a C++ type. The installed package gate checks the original
and relocated stage with ambient import paths removed.

Only genuinely new dock tiles fade in with the short motion token; reduced
motion is immediate. Retained rows never restart that fade, and removal retires
intent targets immediately. See [ADR-0278](../adr/0278-retain-dock-delegates-by-presentation-identity.md).

Presentation phases are the T0 projection plus the read-denial state:
`loading` (no accepted generation — cold start is Loading and the T1 producer
degrades explicitly once owner discovery resolves, so Loading cannot persist
silently), `ready`, `empty` (nothing visible in scope, even while degraded),
`degraded` (producer unavailable; the retained generation stays visible but
every intent is refused — a failed first refresh with no accepted generation
is also `degraded`, never `empty`), and `unavailable` (read capability
denied). The
strip presents at most 64 rows in displayed order — also the Tab and arrow
traversal order — and reports the exact hidden count as overflow truth. For
`grouping: "never"` the strip binds the controller's `windowRows` instead of
`entryRows`: one row per window under the same bound, with
`windowOverflowCount` as its exact overflow truth. A member row keeps its
container's `taskId` for arbitration and names its window in `windowId`, so
its Activate and Close call `activateTaskWindow` and `closeTaskWindow` while
Minimize and Raise address the whole container; member rows show no window
count and no Ungroup action. A
dock host raises the presentation bound so every row is projected and the
zone viewport scrolls instead of truncating
([Dock interactions](dock-interactions.md)). Every
intent echoes the controller's `generationRevision`, the revision of the
generation the strip is showing (see "Update cost and revision fencing"), so
the T0 arbitration refuses actions against a generation the user no longer
sees.
Context actions per row are Activate, Minimize/Unminimize, Close, Raise, and —
for container rows — Ungroup, which maps to the T1 `releaseContainer`. Close
uses the hosted Close All policy. Shell composition injects one
`DesktopEntryIconResolver` built from explicit freedesktop application roots
and one `IconAvailabilityCache` over the icon roots used by QML. Standalone rows resolve
the compositor-provided application id to the desktop entry's `Icon=` name;
container rows use `window-restore-symbolic`. Every row's `applicationName`
and the application part of its `accessibleName` come from the same
resolver's `applicationDisplayName`, so buttons, tooltips, command search, and
the active application control show `QQTerm` rather than a raw
`org.qindaqt.QQTerm` id. Both publish whether theme lookup
succeeded. Changing the active tab or tile keeps the container icon and tint. Horizontal buttons show the icon plus an elided title
inside an 84–168 by 28 logical-pixel bound; vertical buttons show only the
icon. Missing and hostile mappings use the typed application placeholder.

An opt-in panel dock host may set the compiled applet's `dockMode` property
and its bounded `dockTileSize` (24–64 logical pixels). Values from 32 through
64 are user preferences; 24–31 are transient shrink-to-fit values selected by
the panel host and are never persisted. The existing 60-logical-pixel default
therefore keeps its exact meaning. In dock mode each existing task row becomes
one icon-only tile with an icon inset by 8 logical pixels and capped at the
established 40-logical-pixel artwork size, active surface, and running
indicator; it does not fabricate pinned entries. The dock
reserves the full tile before hover magnification, while `reducedMotion`
disables both movement and scaling.
Tooltips and task context menus use their own popup window so they are not
clipped by the panel band; both are created when wanted and destroyed once
closed, so an idle strip owns no popup window (see "Popup windows are released
on close"). `dockHasLauncherGroup` is a composition-supplied
truth value: a separator appears only when both a preceding real launcher
group and at least one task row are present. The default taskbar geometry and
behavior remain unchanged.
Degraded truth is an accessible warning glyph rather than a “Limited” badge,
and loading/empty/unavailable phases use one compact phase icon without panel
text.

Dock presentation adds interaction surfaces owned by
[Dock interactions](dock-interactions.md): full-width growth with zone
scrolling, pointer magnification, drag-and-drop reorder through the fenced
user-order overlay, hover thumbnail cards over the authenticated preview
seam ([ADR-0119](../adr/0119-authenticated-window-preview-channel.md)), and
the panel right-click configuration menu.

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

The user-order overlay, reorder fencing, and the hover-preview seam add
focused rows (`qindaqt.task-list-order`, `qindaqt.task-order-persistence`,
and the dock/offscreen rows under `qindaqt.task-list-applet-*`).

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

### Update cost and revision fencing

A producer republishes the whole generation whenever any window fact changes,
and a terminal that retitles its tab ten times a second is an ordinary source
of that. The cost of one publication must be proportional to what changed, not
to the number of windows. The stability round of 2026-10-16 traced the shell's
sustained CPU to this path and fixed it in four places:

- **Row diffing.** Projected rows carry no per-snapshot field.
  `generationRevision` is a controller property
  (`TaskListAppletController::generationRevision`): the revision of the **last
  reprojection**, not the source's live revision, because between a publication
  and its reprojection the strip still shows the previous generation and a
  gesture must fence against what is shown. The strip reads it at the moment of
  a gesture (activate, minimize, close, raise, ungroup, reorder, preview) and
  echoes it into the intent. `KeyedRowModel.volatileRoles` makes the rule
  structural: a name listed there (the strip lists `generationRevision`) is
  dropped from the published row and never counts as a change, and an identity
  role can never be volatile. A title change is one `dataChanged` for one row;
  an unchanged row emits nothing
  ([ADR-0390](../adr/0390-keep-snapshot-revisions-out-of-retained-task-rows.md)).
- **Icon hints.** The controller asks whether each row's icon resolves on every
  publication; the composition answers from `IconAvailabilityCache`, a bounded
  expiring memo in front of the uncached locator, so a spinner does not walk
  the theme chain ten times a second
  ([Shell iconography](iconography.md),
  [ADR-0392](../adr/0392-memoise-icon-availability-beside-the-uncached-locator.md)).
- **Window rows.** `windowRows` is built on first read after a reprojection,
  not on every reprojection; only an ungrouped strip reads it.
- **Neighbouring controllers.** Command search, Quick Launch and the active
  application control no longer do their full work on every publication
  ([Desktop controls](desktop-controls.md),
  [ADR-0391](../adr/0391-present-shell-search-and-dock-controllers-only-while-visible.md)).

`tests/shell/task_list/tst_task_list_update_cost.cpp` (`ctest -R
'^qindaqt\.task-list-update-cost$'`) is the measurement harness. It drives the
production controller, the production icon and name resolvers over the host's
real desktop entries and icon themes, and the compiled strip with 16 windows
(12 with a desktop entry, 4 without) through 200 title-spinner publications
after 20 warm-ups, and prints one `QINDAQT_BENCH` line with thread CPU time per
update and `dataChanged` per update. It asserts only the deterministic part (one
`dataChanged` per update, no structural signal), never a wall-clock bound. On
the build host: about 82 ms and 16 touched rows per update before the change,
about 0.5 ms and 1 touched row after.

### Popup windows are released on close

A `Popup.Window` popup owns a real window, and with it a GL context and video
memory, for as long as the popup object exists, even after it closes. Measured
with the update-cost harness's strip, the first context menu opened on any tile
takes the window count from 1 to 2 and closing it leaves 2; opening one menu per
tile on a 16-window strip left 17. A tooltip and a menu per tile therefore cost
two windows for every tile ever hovered or right-clicked, which is how a
long-lived shell reached about 54 contexts and 233 MB of video memory
([ADR-0396](../adr/0396-create-per-tile-popups-on-demand-and-release-their-windows.md)).
`TaskListEntryButton` (and the Quick Launch `DockItemTile`, see
[Desktop controls](desktop-controls.md)) now creates both through a `Loader`:
the context menu (its own `TaskListEntryMenu.qml`, which acts on the tile it is
given) on `openContextMenu()` (Menu key, right click or touch context) and the
dock tooltip while it is wanted, each destroyed on the turn after it has closed, so an idle strip owns no popup window and at most one is open at a time.
Both name `parent:` explicitly, since a Loader would otherwise parent them to
itself. Rows: `qindaqt.task-list-popup-windows` (on every tile one menu window
while a menu is open and none after; the dock tooltip's window only while the
pointer is on its tile) and `qindaqt.desktop-controls-dock-retained-delegates`
(the Quick Launch tooltip's window is released after the pointer leaves); both
count `QGuiApplication::allWindows()` around real opens and closes.

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

The offscreen keyboard context-menu gate waits for the popup's actual `closed`
signal and reactivates the test host before sending the next key. The offscreen
platform leaves native focus on the hidden popup window; a real compositor
restores the parent surface. The test asserts the closed menu is destroyed with
its window, that the next open builds a fresh menu, and that it still dispatches
subsequent actions.
