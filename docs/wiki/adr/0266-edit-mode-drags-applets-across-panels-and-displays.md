# ADR-0266: Edit mode drags applets across panels and displays

- **Status:** Accepted
- **Date:** 2026-09-24
- **Owners:** Shell runtime (live customization), panel surfaces
- **Supersedes:** In part, [ADR-0213](0213-host-the-customization-editor-live-in-the-shell.md):
  how edit-mode drags resolve their targets, and its "Esc is not offered"
  clause. Every other ADR-0213 decision stands.
- **Superseded by:** None

## Context

Plan W14: "in edit mode, they stop working normally, and I can just drag and
drop them", within a panel, between panels, and onto another display. The
live shell already had an edit mode (ADR-0213) with a drag protocol on
`LiveCustomizationController` (arm and begin, hover, drop and Apply,
cancel), yet the user was moving the clipboard with seventeen "Move right"
entries. Reading the path end to end showed why:

- edit mode was reachable only from the Meta+right-click menus and
  Meta+Shift+E, never from a plain right click;
- applets kept working in edit mode, so presses went to them (and to the
  dock strip's own tile drag, ADR-0265) instead of the edit drag, and a zone
  `Flickable` could steal the drag outright;
- the Done/Undo bar covered the rightmost applets;
- a drag onto another panel was resolved by the source surface from solved
  geometry, by thirds of the other panel and always appending, only on the
  source's own output; a release over the desktop still committed the last
  accepted target; nothing showed where a drop would land; and Escape did
  nothing because panels never take keyboard focus.

Each panel is its own layer-shell surface. While a button is held, KWin keeps
the pointer's implicit grab on the surface that took the press (pointer
focus does not change while buttons are down), so that surface keeps
receiving motion in its own coordinates over other panels and other outputs.
Solved panel geometry is global and outputs never overlap.

Options considered for crossing surfaces:

- **A Wayland drag and drop with a typed payload** (`QDrag`, a MIME type, a
  `DropArea` on every panel). Rejected: it adds a data-device session, a drag
  icon surface and a DnD focus model for a drag that never leaves one
  process, and it would share the dock's DnD path (ADR-0265) that edit mode
  must stay out of.
- **A shell-owned full-output overlay while dragging.** Rejected: one more
  surface per output, stacked above the panels, with input-region juggling.
- **The source surface resolves other panels itself** (what existed).
  Rejected: it cannot know another surface's chips, so every cross-panel drop
  is a guess.
- **A global drag point that every surface resolves.** Chosen.

## Decision

1. **Entering and leaving.** "Edit Panels" is offered in every panel's plain
   right-click menu (a toggle that reads "Done Editing Panels" while
   editing), in every desktop context-menu style, and as a checkable View ▸
   Edit Panels in the desktop menu (one more ADR-0260 command,
   `EditPanels`, routed to the live customization controller's toggle).
   Meta+Shift+E and the Meta+right-click entries stay. Done, Escape, the
   menus or the shortcut leave.
2. **Applets are inert in edit mode.** A shield covers each applet chip and
   takes every press, wheel and hover: nothing beneath sees input, including
   the dock strip's tile drag, drag-off-to-remove and grouping (ADR-0265
   gestures are live only outside edit mode; edit-mode drags only inside it).
   A left drag anywhere on the chip moves the whole applet; a right click
   opens its customize menu. Zones do not flick in edit mode.
3. **Global drag point.** The surface that owns the drag publishes the
   pointer in global logical coordinates
   (`LiveCustomizationController::trackDragPoint`: its solved origin plus the
   local point). Every panel surface resolves a point inside itself to a
   target on `dragPointChanged` (nearest zone along the main axis, halfway
   boundaries so empty zones are reachable, a dock's shelf staying its centre
   zone; the applet the drop lands before) and hovers it. A point over no
   panel surface hovers the empty target first, which the editor rejects, so
   a release there cancels. Surfaces without a known global geometry (no
   output id) resolve only themselves.
4. **Preview.** The accepted hovered target is published as `dropTarget`;
   the zone it names slides its chips aside from the insertion point by one
   gap, asks for that extra extent so the panel makes room, and paints a
   marker in the gap. Presentation only: the profile still changes once, on
   drop, through the editor transaction (ADR-0213: one gesture, one Apply,
   the same undo, persistence and escrow).
5. **The bar.** Add applet… (per zone, the catalog's palette for the panel's
   orientation, inserting through the `InsertApplet` intent), Undo and Done
   sit at the trailing end of each panel's painted material in a stretch the
   zones leave free; a dock widens its centred shelf by that stretch, so the
   bar is always on the shelf and inside its input mask.
6. **Escape.** While edit mode is on, every panel surface asks for on-demand
   keyboard interactivity (a click on a panel focuses it); leaving edit mode
   returns them to none. Escape on a panel cancels an open drag, else leaves
   edit mode. No other key is taken.

## Consequences

- No persisted key, schema or engine command is added; the editor domain and
  its parity with the Settings route are unchanged.
- Cross-panel and cross-display drops land exactly where the target panel's
  marker shows. A drag over a panel whose surface is not mapped (an
  auto-hidden panel) leaves the last target in place until the pointer
  reaches a mapped panel or empty desktop.
- The implicit-grab behaviour is a compositor fact the design depends on; the
  nested `shell.live-customization.editmode.*` rows exercise it.
- While editing, clicking a panel moves keyboard focus to it, so the active
  application window deactivates until it is clicked again.
- The zones give the bar its stretch only while editing, so applets shift by
  that much in edit mode.
- Tests: `qindaqt.live-customization-edit-mode-offscreen` (Edit Panels in the
  plain menu, inert applets, bar placement, zone and anchor resolution, a
  drag within the panel, onto a second surface, and off every panel, the gap
  and marker, the picker), `qindaqt.shell-live-customization-controller`
  (global lookup, off-target clearing, self anchors), `qindaqt.shell-panel-edit-keyboard`,
  the desktop menu model and targets rows, and
  `qindaqt.desktop-surface-customize-menu`.

## Revisit when

- The compositor stops keeping the implicit pointer grab across surfaces, or
  panels move into one surface per output.
- Edit mode needs to accept drops from other processes (then a typed
  Wayland drag and drop becomes the right transport).
