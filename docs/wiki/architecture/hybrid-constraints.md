# Hybrid constraints and restore state

`src/hybrid_constraints` owns the platform-neutral geometry rules for a tiled
page and the lossless value used to return a member window to independent
placement. It depends on the pure container tree in `src/core`; it does not
query KWin, screens, themes, decorations, or input state.

## Constraint solution

`ConstraintSolver::solve()` accepts one immutable layout tree, an outer frame,
per-member size constraints, and resolved chrome/divider metrics. A successful
result contains:

- a gap-free tile frame and a bounded client frame for every member;
- the frame, preferred ratio, and effective ratio for every split;
- the content frame remaining after shared chrome insets;
- deterministic integer-pixel divider placement; and
- an explicit overflow report when the available frame cannot satisfy all
  recursive minimum sizes.

Minimum, maximum, and fixed sizes are distinct. Minimum sizes influence split
allocation. Maximum and fixed sizes can make a client frame smaller than its
tile, in which case the client is centered without breaking the gap-free tile
partition. A valid solve may therefore report minimum-size overflow; malformed
trees, frames, metrics, or constraints fail without returning a partial plan.

Preferred ratios remain the user's durable layout intent. Effective ratios
describe the actual allocation after minimum constraints and deterministic
rounding. An adapter must not write an effective ratio back into the topology
merely because a small outer frame temporarily constrained it.

The solver is stateless, retains no references, and is safe on any thread that
owns its input values. Missing member constraints mean unbounded clients;
extra registry entries are ignored so an adapter may pass one session-wide
constraint snapshot.

## Independent-window restoration

`WindowRestoreState` schema version 2 captures all state that grouping may
temporarily replace:

| Value | Restoration purpose |
| --- | --- |
| Independent geometry | Base frame applied before special window modes |
| Minimized flag | Preserve hidden state without losing the member's tile |
| Horizontal/vertical maximize axes | Restore full or single-axis maximize |
| Quick-tile edges | Restore compositor quick-tile placement |
| Fullscreen flag | Return to the original fullscreen policy |
| Output ID | Resolve the prior output without retaining an output pointer |
| Desktop and activity IDs | Restore workspace membership losslessly |
| Keep-above and keep-below flags | Restore stacking policy |
| Focused flag | Restore focus only when the live transaction still permits it |
| Skip-taskbar and skip-switcher flags | Restore each client's independent native task/switcher visibility after the group temporarily collapses to one entry |

The value is copyable, validates contradictory or malformed combinations, and
round-trips through versioned JSON. The v2 reader accepts v1 values and defaults
their absent task/switcher flags to `false`; a malformed v2 value is rejected.
It deliberately stores stable IDs rather than platform objects. JSON support
defines a process-neutral transaction value; it does not make ephemeral focus
suitable for login-session persistence.

## KWin adapter contract

The KWin scene adapter captures a restore value before a window first enters a
container and keeps that original value while the member moves or containers
merge. Candidate preparation solves every page of every candidate container
before touching live windows. It retains only the active page's copied solution
for chrome, while inactive pages keep their planned target frames and are
minimized.

When a group is first created, the stationary target is its geometry and policy
anchor. While grouped, members use the anchor's output, desktops, activities,
keep-above, and keep-below policy; their maximize axes, quick-tile state, and
fullscreen mode are cleared before tiled geometry is applied. The original
values remain untouched in the restore map. The shared grouped context may
subsequently change through KWin or the outer-title menu, but those changes
still do not rewrite any member's independent restore snapshot. Existing focus
is retained when it belongs to the visible active page; otherwise the adapter
selects one visible member and applies focus only after every other state
change. Focus rollback tokens are opaque KWin window UUIDs resolved against the
complete workspace, so an active dialog or other non-topology window can be
preserved even though the managed registry deliberately excludes it.

Release applies each original state in a defined order: clear temporary
minimize/fullscreen/tile/maximize state, resolve and apply output plus workspace
placement, replace stacking and task/switcher policy, apply the independent
geometry, then restore maximize or quick tile, fullscreen, minimize, and finally
eligible focus. A missing output or desktop is a preflight failure rather than
an implicit remap. Output hotplug remapping belongs to Platform services.

Rollback restores the exact pre-transaction live values. A `ForgetWindow`
candidate can contain an ID only in its before snapshot after KWin has already
started destroying the object; the adapter skips that dead object while still
normalizing and restoring every surviving member. Neither a constraint plan nor
a restore map may retain `KWin::Window` or output references.

## Group placement and window-state policy

Tree-preserving outer move and resize use a direct reflow transaction rather
than manufacturing a topology revision. The adapter verifies that the current
container members exactly match live ownership, captures member state and size
constraints, solves every page for the requested frame, validates every desired
state, and stages the new active-page layout. If state application, focus, or
atomic target-frame finalization fails, it reverses already applied members and
focus and keeps the old committed layout.

Current group-wide controls have these semantics:

- **Minimize** minimizes every member and hides shared chrome. Native minimize
  on any visible grouped member routes to the same whole-container action, so a
  tiled leaf cannot leave a layout hole. Restoring exposes only active-page
  members; inactive pages remain minimized and excluded.
- **Maximize** saves the current outer frame in process memory and reflows the
  complete group to KWin's maximize work area. **Restore** reflows to that saved
  frame. Failure preserves whichever side of the transition was previously
  valid.
- **Close** opens one nonblocking prompt for the container. **Close All** copies
  member IDs before requesting close, **Ungroup** performs one atomic
  `ReleaseContainer`, and **Cancel** changes nothing. The asynchronous decision
  revalidates that the container still exists; cancel is the default and escape
  action.
- Closing one member through its own decoration uses `ForgetWindow`; the dead
  member is pruned and a singleton survivor is restored to independent state.

## Whole-group output and workspace context

The grouped output, desktop/workspace set, activity set, and keep-above or
keep-below layer are one atomic scene value. The KWin context manager observes
those five signal families on every member but never applies inline. It queues
at most one source per container for the next event-loop turn, so coupled KWin
changes such as enabling Keep Above while Keep Below is cleared are adopted
only after their final, valid state exists. Pending sources survive an earlier
chrome resubscription only while both topology membership and live registry
ownership still agree.

At drain time the source member is revalidated and becomes the canonical
context for every page and member. The scene factory captures all members and
the exact current focus token, solves every page, validates each desired state,
applies output/workspace/activity/layer uniformly, verifies KWin accepted that
context, atomically finalizes all target frames, and only then swaps the copied
committed layout. A change of output maps the complete committed outer frame
from the old placement area to the new one using KWin's relative-center rule,
scales that relative center to the new area, and keeps an originally contained
frame inside the destination before solving. This is a group move between
known outputs, not output creation, mode setting, hotplug recovery, or a claim
of qualified mixed-DPI migration.

Planning, application, postcondition, or finalization failure restores every
member's complete pre-event frame and prior canonical context and restores the
opaque focus token. Recovery attempts all members even after one restore call
fails, so the externally changed source cannot remain alone on a different
output, workspace, activity, or layer. If atomic adoption still fails, session
policy releases the container through the normal independent-state path; if
release also fails, the container enters a process-lifetime chrome quarantine.
That quarantine survives ordinary chrome synchronization and compositor scene
recreation, blocks both visibility and pointer addressing, and clears only when
a successfully reconciled topology no longer contains the container or a later
atomic context adoption explicitly proves the surviving group coherent.

An unmodified right click on the shared outer title exposes Keep Above, Keep
Below, pin-to-all-workspaces, individual workspaces, all or individual
activities, and Move to Output actions. The menu rebuilds from live stable IDs
for every opening and revalidates the representative and destination at
dispatch. It mutates exactly one active representative; the queued transaction
above is the only path that propagates the result to the rest of the group.

While grouped, a pure task-identity policy selects the active KWin member when
it belongs to the active page, otherwise the first active-page leaf in stable
tree order. That real client supplies the group's native caption/icon entry;
every other member is skipped by both taskbar and switcher. An activation or
unminimize attempt on an inactive-page member re-hides it before committing
`ActivatePage`, then republishes exactly one primary for the new page. Dialogs
and transients remain outside topology and retain their native policy.

Member decoration actions use a separate temporary focus baseline:

- only an active-page member may enter maximize or fullscreen focus mode;
- maximize clears the real KWin maximize bit, fills the committed outer frame,
  hides peers/shared chrome, and exposes a process-local restore glyph property
  to `QindaDecoration`; fullscreen uses KWin fullscreen while applying the same
  peer/chrome visibility policy;
- a second maximize on the focused member or fullscreen exit restores the exact
  group baseline; another member cannot take focus mode concurrently;
- minimizing the focused member first restores its temporary presentation and
  then routes through whole-container minimize; close restores surviving
  baseline members; native title drag restores the temporary focus presentation
  as its atomic detach commits; and
- shutdown restores any remaining focus baseline before `ReleaseContainer`
  reapplies each member's independent `WindowRestoreState`.

Non-popup dialogs/transients associated with grouped members retain floating
geometry outside the topology. Their adapter preserves a stable owner-relative
offset (updated after user/client movement), follows owner output, desktops, and
activities, and never raises during context following. The separate group
stacking policy places associated transient subtrees above the complete
contiguous member block and preserves the group's rank relative to unrelated
windows; raising from the context adapter would let KWin raise transient
ancestors, split that member block, and steal the outside stack rank.

The maximize restore frame, group-minimized marker, member focus baseline, and
transient associations are process-lifetime compositor state, not a login-session
persistence format. Persisted topology/session restore and output-hotplug
recovery are still beyond this slice.

## Diagnostics and qualification

The scene factory exposes copied committed layout and overflow values only to
process-local collaborators. They are not public D-Bus pointers or mutable
views. `Capabilities.hybrid` reports runtime readiness and topology counts, but
does not currently export per-container constraints or overflow.

Focused tests under `tests/hybrid_constraints` cover normal, fixed/maximum,
nested, fractional, overflow, and malformed solves plus validation, v1
compatibility, v2 JSON round-trips, and value semantics for restore state.
Compositor fake-platform
tests cover all restore fields, inactive-page minimization, focus movement,
dead-member normalization, cross-container single-finalization, overflow,
copied layout publication, close-choice dispatch, and rollback of both
structural transitions and direct reflow.

The nested native pointer detach requires the dragged member's original size to
survive while its position follows the pointer and its target frame agrees at
drop; the sibling's original current and target frames and independent ownership
must restore exactly. The Hybrid plugin-unload workflow independently requires
original KWin frames, minimized state, and task/switcher flags after the QindaQt
service is gone, then proves the surviving clients remain usable. Current
qualification results are recorded in the
[testing harness](../development/testing-harness.md). Full restore-field
coverage remains the fake-platform contract; the live workflow does not claim
to have induced every
maximize, quick-tile, fullscreen, workspace, activity, stacking, and focus
combination.
Physical GPU/output, mixed-DPI migration, and hotplug recovery remain later
gates.

Topology publication order is defined by
[Hybrid topology coordination](hybrid-topology.md). Shared presentation and hit
regions are defined by [Hybrid container chrome](hybrid-chrome.md).
