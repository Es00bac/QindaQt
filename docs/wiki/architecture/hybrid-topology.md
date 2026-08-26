# Hybrid topology coordination

The Hybrid topology module owns session-wide container membership and the
atomic boundary between the pure container model and compositor scene state.
It is a Qt Core library in `src/hybrid`; it does not import KWin, Qt Quick,
decorations, input handling, persistence, or D-Bus.

The production `KWinHybridSession` keeps this topology in the compositor
process and composes the pure coordinator with constraint, input, chrome, and
KWin adapters. It is deliberately separate from the milestone-2 per-container
D-Bus bridge. Production gestures never enable that externally callable
mutation surface; [ADR-0004](../adr/0004-process-local-hybrid-topology.md)
records the process boundary. Shared chrome now follows the scene-item decision
in [ADR-0005](../adr/0005-scene-resident-hybrid-chrome.md).

## Published topology

`WindowTopology` is a revisioned value containing:

- a deterministic, ID-keyed map of live `WindowContainer` values; and
- the set of known independent windows that may participate in docking.

Every known window has exactly one placement: independent or owned by one
container. A published container has at least two members. Empty and singleton
containers are transaction staging states only; normalization removes them and
returns a singleton survivor to the independent set before validation or
publication. Page and layout-node IDs remain scoped to their container, while
container and window IDs are session-wide.

Construction validates imported topology. The repository exposes immutable
snapshots and has no general mutable accessor. Snapshot references are valid
only until the next successful command because publication swaps in the fully
prepared candidate.

The production D-Bus endpoint mirrors this process-local authority for
observation only. `Containers` identifies each entry with
`authority: "hybrid-process"` and the current nonzero session revision;
`Snapshot` returns the same schema-1 container value and revision. The public
surface does not acquire mutation authority, and Hybrid commits do not emit the
older bridge's `ContainerCommitted` signal. Exact encodings are in the
[Compositor1 reference](../reference/compositor-control-v1.md).

## Semantic commands

`TopologyCommand` is a closed variant. Callers submit intent rather than a list
of low-level tree operations.

| Command | Atomic effect |
| --- | --- |
| `AddIndependentWindow` | Adds one newly managed, unowned client to the docking inventory. |
| `ForgetWindow` | Removes a closed/unmanaged client from independent placement or its container without retaining the dead ID. |
| `DockIndependentWindows` | Removes two independent windows and creates one split container. |
| `InsertIndependentWindow` | Inserts one independent window into an existing container as a new page or as a split relative to a member. |
| `GroupIndependentWindowsAsPages` | Removes two independent windows and creates one two-page container. |
| `RegroupMemberWithIndependent` | Detaches one owned member and combines it with an independent window in a new split or paged container while normalizing the source. |
| `MergeContainers` | Inserts all source pages, in order, at a target page index and removes the source container. The target active page is preserved. |
| `MovePage` | Moves one complete page between different containers at a final target index. The page's whole tree and every page/node/window ID survive. |
| `DetachPage` | Removes one page from a multi-page container. A leaf page becomes an independent window; a split page becomes a new one-page container with its complete tree. |
| `MoveMemberToPage` | Extracts one member, preserving its leaf ID, into a new page immediately after a stable target page in the same container. |
| `RegroupPageWithIndependent` | Creates a new two-page container with an independent target first and one complete source page second. The source page and tree IDs survive. |
| `MoveMember` | Detaches one source member and inserts it in another container as a page or split. Its leaf ID is preserved. |
| `ReorderPage` | Moves a page to an explicit final index. |
| `ActivatePage` | Selects a different existing page without changing logical page order. |
| `ResizeSplit` | Applies an adapter-computed finite divider ratio to an existing split. |
| `ReorderMembers` | Swaps two leaf payloads without changing stable node IDs or split geometry. |
| `ReparentMember` | Detaches and reinserts a member relative to another member in the same container, preserving the moved leaf ID and creating one requested split. |
| `DetachMember` | Returns a member to independent placement and normalizes its source. |
| `ReleaseContainer` | Returns every member to independent placement and removes the container. |

Commands reject duplicate or empty newly mapped IDs, stale source ownership,
unknown IDs, same-source/target moves, invalid indices or ratios, structural
collisions, and no-op reorder, reparent, activation, or resize requests.
`DetachPage` rejects the only page because that page already is the whole
container. A split page requires a unique new container ID; a leaf page ignores
the unused ID and becomes independent. Removing an active page selects the page
that shifts into its index, or the preceding last page. Moving or adding a page
does not replace the target container's ID-based active page. A detached split
page is the active page of its new one-page container. A
`RegroupPageWithIndependent` result makes the newly created independent page
active because it is inserted first; the transferred source page follows it.
Rejected commands do not create a scene transaction or advance the revision.
Chrome tab clicks and divider drags use `ActivatePage` and `ResizeSplit`; the
compositor adapter converts divider pixels and size constraints into a ratio
before submitting the command.

`HybridInteractionRuntime` translates one committed pointer intent to exactly
one semantic command. Edge docking maps to a split orientation and insertion
side; tab docking maps to page insertion or grouping. A tab is a complete page,
not its representative member: a cross-container tab drop uses `MovePage`, an
empty drop uses `DetachPage`, and a tab dropped on an independent window's tab
target uses `RegroupPageWithIndependent`. Within one container a tab drop after
another page uses `ReorderPage`; a member-title drop on another page uses
`MoveMemberToPage` and extracts only that member. Tab-to-edge drops are
intentionally rejected because no typed subtree-as-split command exists.
Every tab source carries its stable page ID; a representative that no longer
belongs to that page is stale and rejects. Dropping onto the same page or an
already-effective reorder is `NoChange`, not a revision. `MovePage` itself is
cross-container only.

This is in addition to independent-window insertion/grouping, member regrouping,
cross-container member moves, and same-container tree reparenting. Structural
IDs are derived from the next topology revision, so a failed scene transaction
can be retried with the same candidate IDs. Begin, update, and cancel intents
may drive previews but never mutate topology.

Client lifecycle uses the same scene transaction and revision path as user
commands. A newly mapped client enters through `AddIndependentWindow` rather
than an out-of-band repository edit. `ForgetWindow` removes an independent or
grouped client; when pruning exposes a singleton, only the live survivor becomes
independent. The dead client is present in the before snapshot and absent from
the candidate, so a scene adapter must tolerate a before-only object that KWin
has already begun destroying.

## Pointer and keyboard intent paths

Pointer input reaches Hybrid policy through three deliberately separate paths:

- A compositor-side `HybridChromePointerRouter` owns ordinary left-button
  controls, tabs, dividers, outer-title moves, and outer-edge resizes. It uses
  Qt's configured drag threshold and leaves modified presses untouched.
- A plain left-button drag on a preserved member title falls through to its
  native KDecoration/KWin move. When KWin begins the interactive move, member
  policy atomically detaches that member; KWin then continues the same move with
  the restored independent size and pointer anchor through the final drop.
- Exact `Meta+Shift+Left` acquires the compositor input grab after an
  eight-logical-pixel threshold. It can start from an independent title as well
  as grouped member or shared chrome and supplies the explicit docking and
  rearrangement path. Merely containing those modifiers is insufficient, so
  unrelated chords and ordinary client input pass through.

The consuming KWin filter is installed at Decoration order, after lock-screen,
global-shortcut, effect, and Popup policy but before native KDecoration handles
the same order. This makes an outside press dismiss a popup without also
reaching shared chrome. A press that successfully starts a QindaQt grab and its
subsequent events are consumed; all other events return to KWin unchanged.
Begin and update intents may show the input-transparent, `outputOnly` dock
preview, but only a commit intent may execute a topology command. Dropping a
grouped member outside every target on the exact-modifier path detaches it. The
same drop for an independent window cancels because there is no group to leave.
Ordinary native member-title movement does not pass through the shared chrome
router or infer a docking target.

Both ordinary shared-chrome input and exact-modifier target discovery consult
the live bottom-to-top KWin stack at the pointer position. A scene hit is valid
only when its current member anchor is paintable and no eligible native input
owner above that anchor covers the point. The resolver stops at the first real
owner even when it is an internal window, popup, dialog, or other non-topology
window; it never tunnels to a manageable client below. Exact docking may ignore
only its dragged source so that source can target chrome beneath itself.

An ordinary right-button press and matching release on `OuterTitleDrag` emits
one stable-ID context-menu request. Member-title right clicks remain native.
The nonblocking group menu offers layer, workspace, activity, pin, and output
commands and revalidates the active representative when each command is
dispatched.

Thirteen autoloading KGlobalAccel actions cover interactive grabs and
coordinate-free semantic commands for KWin's active managed window:

| Default | Interaction after entry |
| --- | --- |
| `Meta+Shift+D` | Dock: arrows choose an edge target, `T` chooses a tab target, and `D` explicitly selects detach for a grouped member. |
| `Meta+Ctrl+Shift+D` | Begin page docking from the active tab. `D`+`Enter` detaches; a directional or `T` target plus `Enter` moves or regroups the complete page. |
| `Meta+Shift+M` | Move the complete group in ten-logical-pixel arrow-key steps. |
| `Meta+Shift+S` | Adjust the first split in deterministic active-page preorder with arrow-key steps. |
| `Meta+Shift+R` | Resize the complete group from its bottom-right edges with arrow-key steps. |
| `Meta+Ctrl+PageDown` / `Meta+Ctrl+PageUp` | Activate the next/previous page, wrapping in stable logical order. |
| `Meta+Ctrl+Shift+PageDown` / `Meta+Ctrl+Shift+PageUp` | Reorder the active page forward/backward. |
| `Meta+Ctrl+Shift+Q` | Open the active group's Close All/Ungroup/Cancel policy. |
| `Meta+Ctrl+Shift+N` | Minimize the complete active group. |
| `Meta+Ctrl+Shift+X` / `Meta+Ctrl+Shift+U` | Maximize/restore the complete active group. |

`Enter` commits and `Escape` cancels each interactive grab. Displacement is
cumulative from one stable baseline, matching pointer placement and divider
semantics; unrelated keys and releases pass through. Entry validates the active
KWin member, group, committed layout, maximized state, and selected split before
acquiring a grab. Direct page and window-action shortcuts dispatch immediately.
All actions use autoloading so saved reassignment or disabled shortcuts remain
user policy. Page activation/reorder, page docking, and group window actions
resolve to stable-ID `HybridSemanticRequest` values. The virtual accessibility
tree dispatches those same requests rather than maintaining a second policy.

Pointer target selection examines the topmost eligible KWin input owner on the
current desktop/activity and returns no native target unless that owner is a
manageable normal window. For a valid window, the center 40% of each axis is
the tab target; otherwise the nearest edge wins. Keyboard edge selection ranks
directional manageable windows by forward distance and perpendicular distance.
Stale source ownership or a target that vanished before commit rejects without
advancing the topology revision.

## Candidate and scene transaction

`TopologyCoordinator::execute()` is synchronous and follows one publication
order:

1. copy the repository topology;
2. apply the typed command only to that candidate;
3. normalize all empty or singleton containers;
4. assign exactly the next revision and validate global ownership;
5. create a fresh scene transaction and call `prepare()`;
6. call `commit()` while the repository still exposes the old revision; and
7. publish with a no-fail value swap.

If preparation or commit fails, the coordinator calls `rollback()` and leaves
the repository unchanged. A scene adapter must restore everything it staged or
applied when rollback is called. It must not publish model state, retain
snapshot references after a call, or throw through the coordinator boundary.
This ordering lets an adapter plan all KWin changes against a valid candidate
without exposing model state that the real scene did not accept.

The coordinator borrows its repository and transaction factory; both outlive
it. Calls and callbacks are serialized on the compositor owner thread.
Reentrant execution is rejected. Revision exhaustion and a factory that cannot
produce a transaction are explicit, non-mutating errors.

## KWin scene publication and direct reflow

The production scene transaction captures every affected live window before
mutation, solves every page, validates every desired state, and stages copied
restore/layout maps. Its commit then applies member state in stable window-ID
order, applies focus once, atomically finalizes registry ownership plus target
frames, and swaps the staged maps. A cross-container move therefore has one
owner finalization rather than an observable release followed by acquisition.
If any application or finalization fails, rollback reapplies earlier live
states in reverse order and restores the original focus; the coordinator keeps
the old topology and revision.

Outer move, outer resize, maximize, and restore do not change tree structure or
advance the topology revision. Their narrower reflow path validates that the
borrowed container snapshot still matches live ownership, re-solves every page
for the requested outer frame, prevalidates all member states, and publishes a
copied committed layout only after live state and target-frame finalization.
Failure rolls back member state and focus and leaves the prior committed layout
visible. Cancelling move or resize reflows the original baseline. A maximized
container must be restored before it can move or resize.

The committed active-page solution is the only geometry input accepted by the
chrome-plan builder. This prevents painted hit regions from drifting away from
the scene frames they control. Constraint and restore details are in
[Hybrid constraints](hybrid-constraints.md); rendering and drag-region details
are in [Hybrid chrome](hybrid-chrome.md).

## Whole-group context adoption

Output, desktop/workspace membership, activity membership, Keep Above, and
Keep Below are one grouped context. KWin signal adapters observe every member
and queue one canonical source per container for the next event-loop turn;
they do not propagate synchronous intermediate states. A queued source remains
valid only while both the topology and live ownership still place it in that
container, including when an earlier stack/chrome refresh reconnects observers.

The scene factory captures every member and the exact current KWin focus token,
then re-solves all pages and applies the source context to the whole group. An
output change maps the complete committed outer frame between the old and new
placement areas before solving. State application, KWin postcondition checks,
ownership/target-frame finalization, and committed-layout publication form one
transaction; the independent restore snapshots are never rewritten. Failure
restores every pre-event frame and the previous canonical context, attempting
all members even after one recovery call fails, and restores focus even when it
belongs to a dialog outside topology. Session policy releases the group if
adoption cannot be made atomic. If release also fails, persistent chrome
quarantine blocks rendering and pointer access across later synchronization
until the group disappears or a subsequent atomic adoption proves it coherent.

The outer-title context menu intentionally mutates only the current task
representative. Its Keep Above/Below, pin/workspace, activity, and Move to
Output actions therefore share this same queued adoption path instead of
maintaining a second group-state implementation. Destination IDs and the
representative are read live and revalidated because topology, virtual
desktops, activities, or outputs may change while the menu is open.

## Collapsed native identity

Each published container has exactly one native taskbar and switcher identity.
A pure policy chooses the active KWin member when it belongs to the active page,
otherwise the first active-page leaf in deterministic tree order. That real
client supplies the current caption/icon entry. Every other member, including
all inactive-page leaves, is skipped by both native surfaces and remains
minimized when its page is inactive.

The KWin adapter observes activation, minimize, unminimize, and skip-policy
changes without admitting dialogs or transients into topology. An external
activation or unminimize of an inactive member re-hides it before atomically
committing `ActivatePage`; only the newly active page is exposed afterward.
Native minimize of any visible member routes to whole-container minimize, and
restore reveals only active-page members. Scene callbacks are suppressed during
owned transitions to avoid reentrant minimize loops. The original independent
minimized/taskbar/switcher bits remain in schema-v2 restore state and return
exactly on detach, page transfer, singleton normalization, close recovery,
rollback, release, or plugin unload.

## Lifecycle and teardown

Mapped and closed clients use the same topology transaction as gestures. A
transaction that replans other groups, adds or forgets a client, or releases
several groups leaves an untouched, active, non-minimized independent window
active. Preparation failure does not clear focus before the transaction has
captured a focus token. Add/Forget processing also refuses to steal focus from
an unaffected independent client. A native member detach marks its transition
before calling KWin so a synchronous registry refresh cannot reenter stale
focus state; a redocked client may detach again later. Chrome lifecycle events
are separate from topology: KWin stack and activation changes plus registry
output-inventory changes coalesce into one next-turn republish, and large
window-signal bursts collapse to one reconciliation. Each scene `ImageItem` is
anchored to the group's topmost active-page KWin member; registry output events
resample committed geometry and live device pixel ratio. That publication path
does not itself remap an output or reflow a container; an individual member
context change uses the atomic adoption path above.

Non-popup dialog/transient windows are never inserted into the topology. When
their transient chain or main-window list resolves to a grouped member, the
transient adapter follows that owner by a stable user-adjustable offset and
copies its output, desktops, and activities while preserving transient
stacking. It deliberately never raises during context following. Group stacking
alone keeps associated transient subtrees above the complete contiguous member
block, preventing KWin's ancestor-raising behavior from splitting the group or
moving it above an unrelated active client. Window map/removal and owner
geometry/context changes refresh the association. Admission rejects every
popup, transient, and dialog even when KWin also reports a normal window type,
and scene focus capture/rollback accepts opaque UUIDs for non-topology windows.

KWin compositor reinitialization destroys and recreates the entire scene while
topology and client windows remain live. Direct pre-teardown handlers for
`aboutToToggleCompositing` and `aboutToDestroy` mark scene publication
unavailable, invalidate chrome grabs, and destroy every QindaQt `ImageItem`
before KWin destroys its parent `WindowItem`; stack and accessibility caches
clear at the same boundary. Synchronization remains suspended until
`compositingToggled(true)`, after client items exist, then republishes fresh
items from the unchanged topology and committed layouts. `sceneCreated` is too
early for this purpose. The nested session requests this transition through
development-only `ReinitializeCompositingForTest`; production rejects that
method with `control-disabled` before consulting compositor availability.

Plugin shutdown disconnects queued lifecycle synchronization, stops shortcut
and input grabs, clears preview/placement state, and restores any temporary
member focus mode. It then retries normal release in stable container-ID order
while task, member, transient, scene, and registry observers remain alive. If a
group still cannot release, a bounded emergency scene recovery restores the
captured independent states and clears ownership without publishing another
soon-to-be-destroyed topology. Fallback use and incomplete recovery are logged;
the second shutdown phase cannot replay stale focus geometry over an already
restored independent frame. Accessibility roots and other Hybrid collaborators
are destroyed only after recovery, followed by any older D-Bus-bridge container
and the public D-Bus object/service.

## Verification and qualification boundary

Focused tests under `tests/hybrid` cover every command, mapped/closed client
lifecycle, independent insertion and grouping, member regrouping and
same-container reparenting, global duplicate ownership, normalization, leaf-ID
preservation, deterministic page insertion, candidate validation, revision
progression, pre-scene rejection, scene unavailability, prepare rollback,
commit rollback, and publication ordering. Compositor tests cover every edge
and tab translation, deterministic retry IDs, KWin-platform transaction
rollback, copied committed layouts, direct reflow rollback, atomic context
adoption and external-mutation recovery, placement policy, chrome translation,
anchor-aware exposure, popup/decoration input ordering, outer-title context-menu
dispatch, preview lifecycle, compositor scene teardown/rebuild, and
input/shortcut seams.

The live nested pointer workflow starts three painted Wayland probes, uses exact
`Meta+Shift+Left` to create a process-local split, and reads its nonzero public
revision and schema-1 snapshot. While that same group is live, the workflow
reinitializes KWin compositing and requires an observed inactive-to-active
transition, the same container and topology revision, and one visible anchored
scene item before continuing. It also proves that an unrelated covering window
blocks shared input, a popup-dismiss press does not fall through, and a
normal-type transient dialog remains outside topology with focus preserved. It
then starts a plain native KDecoration title drag. The member detaches at KWin's
interactive-move start, retains its original independent size, follows the
pointer to the drop, and agrees with its target frame there; the sibling returns
to its exact original current and target frames, every owner clears, and the
topology revision advances. This is not an "exact original frame" assertion for
the dragged window because the native move intentionally changes its position.

A companion workflow creates a Hybrid-owned split, dynamically unloads the
plugin, requires the plugin, QindaQt service, and Hybrid authority to disappear,
adds an inactive page, and proves one task/switcher primary while switching
pages. Native member minimize must collapse the complete group; restore must
reveal only the active page. After unload, KWin must report the exact independent
frames, minimized bits, and task/switcher flags before a surviving client is
retitled and resized. The latest broad qualification results are recorded only
in the [testing harness](../development/testing-harness.md).

The virtual KWin seat in the qualified environment did not admit the host
`dotool` uinput devices. The harness keeps the one `dotool` process alive,
records that concrete admission failure, and falls back to a development-only
keyboard/pointer `KWin::InputDevice`. Events from that device traverse KWin's
normal input spy/filter/controller chain; production never constructs it and
rejects `InjectTestInput` before parsing. This proves compositor input routing,
not a physical input device. The exact distinction is maintained in the
[testing harness](../development/testing-harness.md).

Focused tests cover keyboard grabs, all 13 shortcut dispatches, page/tree
transfers, scene-resident chrome and ordinary router ownership, virtual
accessibility trees, collapsed native task identity, native-member focus
actions, transient following, bounded shutdown recovery, lifecycle focus
preservation, and offscreen preview/chrome rendering. The live pointer workflow
is functional input evidence, not a screenshot assertion. The completed
Debug/Release, focused, bridge-only, sanitizer/stress, documentation, and audit
results are maintained in the
[testing harness](../development/testing-harness.md). Hybrid interaction is
complete.
Physical input, DRM/KMS and GPU coverage, cross-output mixed-DPI migration,
hotplug/rotation/lid policy,
persisted login-session topology, and the performance budget remain later
Platform or Release gates.

The per-container tree invariants remain owned by
[Window containers](window-containers.md). Cross-module dependency direction is
defined by [Module boundaries](module-boundaries.md).
