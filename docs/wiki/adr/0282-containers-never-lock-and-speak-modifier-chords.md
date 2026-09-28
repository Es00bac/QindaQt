# ADR-0282: Containers never lock up, and speak one set of modifier chords

- **Status:** Accepted
- **Date:** 2026-09-28
- **Owners:** Compositor window containers (placement, hybrid input, member policy); desktop surface
- **Supersedes:** None (reverses the "refuse while maximized" rule of
  [Window containers](../architecture/window-containers.md) and
  [Hybrid constraints](../architecture/hybrid-constraints.md))
- **Superseded by:** None

## Context

The owner reported that a container sometimes "becomes difficult to move",
and that moving, rolling up or iconifying only worked again after every window
was dragged out of it. The live session log on qinda-top showed one container
refusing roll-up 27 times with "restore a maximized container before shading
it". Reading the placement, input and member-policy code found four separate
ways a container stopped answering:

1. **Maximize was a lock.** A maximized container refused every title drag
   ("restore a maximized container before moving it"), every resize and every
   roll-up, silently from the user's point of view. Every other desktop
   restores a maximized window when its title is dragged.
2. **A lost gesture end poisoned placement.** A move or resize whose Commit or
   Cancel never reached `HybridContainerPlacementController` left its baseline
   behind, and every later Begin was refused with "container already has an
   active placement drag" until the container dissolved (its windows dragged
   out) and `forgetContainer()` cleared it. One proven path: the session
   refused a Commit when member focus could not be restored and returned
   without telling placement anything. The input controller also had no
   lost-release guard, unlike the chrome router.
3. **A member maximize hid the container's only title.** Member focus mode
   hides the shared chrome, so the zoomed member's own title bar is the group's
   only title. Its wheel did nothing (the chrome router owns handlebar wheel
   only while chrome is visible), so the group could not be rolled up, and the
   one gesture left on that title -- a native drag -- detaches the member.
4. **A member could move alone.** If a native move of a grouped member did not
   detach it (a refused transaction, a transition in progress), KWin carried the
   member away while topology, chrome and the committed layout still placed it
   in its tile.

The owner also asked for the modifier chords to work on containers rather than
on the single member KWin sees: Meta + left moves a container, Meta + Shift +
left moves one window in or out, Meta + right resizes, Meta + wheel rolls up and
down, the pen speaks the same chords (tip = left, barrel button = right), and a
maximized container must roll up.

## Decision

**Maximize is left by acting on the frame, never a lock.** A title-bar move
Begin on a maximized, unrolled container restores it to its restore size under
the press point (the press keeps its fraction of the title width) and continues
the drag; Cancel re-maximizes with the original restore frame. A resize Begin
leaves maximize and starts from the maximized frame; Cancel re-maximizes. A
maximized container rolls up and stays maximized: its strip starts at the
maximized top-left, nothing is reflowed while rolled up (ADR-0099), and unroll
returns to the current maximize area. Moving a rolled-up maximized strip leaves
maximize, so unroll restores the restore size where the strip was put; restore
while rolled up only re-targets the unroll. An explicit maximize while rolled up
is still refused. A keyboard group move restores first.

**Every gesture ends.** A placement Begin supersedes an abandoned gesture for
the same container instead of refusing it (the abandoned one keeps the frame it
last applied). When the session refuses a Commit it delivers a Cancel instead.
`InteractionController` ends a gesture whose button is no longer held, like the
chrome router, and a pending modifier click that never began sends nothing
downstream. A container move, resize or divider gesture leaves that container's
member focus at Begin, before its first reflow, rather than at Commit, where the
restore would replay the pre-drag frames over the moved layout.

**A zoomed member's title is the group's title for roll-up.** A wheel away from
the user on a grouped member's native title while its container chrome is hidden
rolls up the whole container (`titleWheelRoute`), leaving member focus first.

**A member never moves alone.** A native move of a member that did not detach is
cancelled, using the same "owned member" predicate as the native-resize veto
(ADR-0117).

**One modifier setting, four chords.** The window-management modifier is the
docking chord without Shift (`windowManagement.dockingModifier`: Meta by default,
Alt, Control, or Off, which disables all of them):

- modifier + Shift + left drag: the existing docking gesture on the one window
  under the pointer (ADR-0085);
- modifier + left drag over a container (member window, title, tab): moves the
  whole container; over an independent window the press is left to KWin's own
  modifier move;
- modifier + right drag over a container: resizes it from the nearest corner or
  side (KWin's thirds rule); over an ordinary independent window it starts
  KWin's own interactive resize, because QindaQt seeds `CommandAll3=Nothing` for
  the shell's Meta + right-click customization on panels and the desktop, which
  keep that chord;
- modifier + wheel over a window, container chrome or icon chip: one notch (120
  units, accumulated for high-resolution wheels and touchpads) away from the
  user rolls the container up -- or rolls an independent window to its icon --
  and towards the user rolls it down, once per scroll gesture, idempotently.
  This is judged in the early (GlobalShortcut-order) filter, so it wins over a
  plain-wheel tab-strip scroll and over KWin's own Meta + wheel axis shortcuts
  (zoom) wherever something is under the pointer; over the bare desktop or a
  panel KWin keeps Meta + wheel.

A stylus reaches the same routing as the mouse (container chrome, icon chips,
then the controller) through `TabletPointerTranslator`. Amended 2026-09-28 at
the owner's request, the pen has its own chords, with M the window-management
modifier (Meta by default):

| Pen | Held | Does |
| --- | --- | --- |
| Tip | M | Moves the window, or its whole container |
| Tip | M + Ctrl | Resizes the window or container from the nearest corner |
| Eraser | M (or M + Shift) | Docks: moves one window into, out of or within a container |
| Tip | M + Shift | Docks, like the mouse chord |
| Tip or eraser | nothing | Belongs to the application (drawing, erasing) |

The lower barrel button (BTN_STYLUS) remains the right button and the upper one
the middle button. A tablet press is consumed only when a router claimed it, so
drawing, erasing and the barrel's plain right-click are unchanged; a plain pen
press on chrome QindaQt draws (tabs, container buttons, icon chips) works like a
mouse press. The release repeats the chord the press chose, even if the keys
were let go first, and leaving proximity releases whatever the pen still held.
A native move the pen drives that the late-Shift takeover adopts keeps the pen's
position and ends on the pen's lift; before this amendment the lift went
nowhere and the dock preview stayed until Escape.

One device drives a gesture at a time. While the pen holds a claimed gesture,
or a finger drives a picked-up title or a pressed piece of chrome, mouse and
touchpad events bypass QindaQt's routers and reach KWin and the client as
usual: their buttonless motion would otherwise read as the gesture's lost
release and cancel it, and their click would commit the pen's or finger's drop
at the mouse cursor. A claimed pen press owns every pen event until its lift,
even when a router settles at once, so a client never sees a lift or motion
without its press. Likewise the chrome touch policy swallows, until it lifts, a
second finger whose down it consumed without letting it join the gesture; the
live log showed 210 "Detected a touch move that never has been down" from
exactly that leak.

**A held finger picks a window up by its title.** Touch has no modifiers, so
a finger held still (the touch policy's long press, 500 ms within its 8 px
slop) on a window's own title bar picks the window up into the same dock drag
as modifier + Shift + left drag: the same drop targets highlight, a drop on a
window, container edge or tab strip docks it, a grouped window dropped on empty
space detaches, and an independent one dropped there stays where it was. Until
the long press fires every event of the sequence stays KWin's, so a tap and an
ordinary title drag are unchanged; moving past the slop, a second finger or an
early lift disarms it. The takeover happens only when KWin read the landing as
a title press (not a title-bar button) and has not started its own move; it
releases KWin's recorded title press, consumes the finger's motion, and passes
the lift on so KWin's decoration filter clears its touch-press id (swallowing
it would make KWin ignore every later title touch). A second finger or a
seat-wide cancel ends the pick-up. The policy is `TouchTitlePickup`.

**Standard desktop icons are applet settings.** The desktop-icons applet shows
optional Home, Documents, Downloads, Pictures, Videos, Music, Trash and Computer
icons, one boolean desktop-icons setting each (Home and Trash on), before the
Desktop folder's entries. They are placed, moved and snapped like every icon,
open in File Manager through `FileBoundary`, and are never renamed, cut, copied
or trashed; hiding one is its setting. The Trash icon follows its contents
(`user-trash` / `user-trash-full`), and icons or files dropped on it move to the
home Trash through File Manager's identity-checked mutation authority. There is
no Network icon: File Manager has no folder to open for its Network place.

## Consequences

- The only remaining ways to leave a container unmovable are genuine scene
  failures, and those are now reported on the gesture that hit them instead of
  poisoning every later one.
- `HybridContainerPlacementController` gained a third translation unit
  (`hybridcontainermaximize.cpp`); the placement, rescue, shade and new maximize
  suites share one fixture and must link it.
- The chord rules are pure (`containerchords.h`, `wheelrollchord.h`,
  `tabletpointertranslator.h`, `hybridtitlewheelroute.h`) and unit tested; the
  KWin adapters only move events in and out of them. Real-device behaviour (a
  Wacom barrel button, a touchpad's scroll stop events, a real touchscreen's
  long-press pick-up, KWin's own resize
  following a pen) still needs a live session to confirm.
- Meta + wheel no longer zooms while the pointer is over a window.
- Every chord follows the one existing Windows-settings choice; there is no
  separate key binding per chord.

## Revisit when

- Users need the move, dock, resize and wheel chords on different modifiers.
- KWin starts delivering tablet tools only as emulated pointer events (the
  tablet path would become redundant), or changes the filter order the wheel
  chord relies on.
- Member focus stops hiding shared chrome, which would let a zoomed group keep
  its own title row instead of borrowing the member's.
