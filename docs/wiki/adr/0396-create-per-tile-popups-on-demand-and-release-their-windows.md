# ADR-0396: Create per-tile popups on demand and release their windows on close

- Status: Accepted
- Date: 2026-10-16

## Context

Every task-list tile carried a `Popup.Window` tooltip and a `Popup.Window`
context menu, and every Quick Launch tile a `Popup.Window` tooltip, so that
popups are not clipped by the panel band. A `Popup.Window` owns a real window,
and with it a GL context and video memory, for as long as the popup *object*
exists: closing it hides the window and keeps it. Measured on the task-list
strip with the update-cost harness, opening one tile's menu takes the window
count from 1 to 2 and closing it leaves 2, and opening one menu on each of 16
tiles left 17 windows. The stability review counted about 54 GL contexts and
233 MB of video memory in a long-lived shell; per-tile popups kept after use are
the mechanism that grows with hovering and right-clicking.

## Decision

A per-tile popup is created when it is wanted and destroyed when it has closed:

- The task-list context menu, now its own `TaskListEntryMenu.qml` acting on the
  tile it is given, is built by a `Loader` on `openContextMenu()` (the Menu key,
  right click and touch context all go through it) and unloaded by a
  zero-interval timer started from the menu's `onClosed`, so it is never destroyed
  from inside its own signal.
- The dock tooltips (task list and Quick Launch) are built while a `tooltipWanted`
  binding holds and stay until they have closed and are no longer wanted, so they
  keep their exit transition; a tooltip that never opened (the pointer left
  during its delay) is dropped at once. The loaded state is a plain property
  moved by those events, not a binding on the loaded item, which would be a
  binding loop. The tooltip's `visible` is bound in `Component.onCompleted`, not
  given as an initial value: a popup created already visible opens at once and
  skips its `delay`, which both defeats the hover delay and opens the window under
  the pointer.
- Each names `parent:` explicitly, because a Loader would otherwise parent the
  popup to itself and a tooltip positions against its parent.
- Strip-level popups that exist once per strip (the hover preview card, the
  Quick Launch item menu) are unchanged: one window per strip is the budget.

## Consequences

An idle strip owns no popup window and at most one is open at a time. Opening a
popup builds an object (a menu of a handful of items), a cost paid per user
gesture rather than per update. A test that held a menu object across closes
now asserts the opposite (a closed menu is gone, the next open builds a new one):
`qindaqt.task-list-applet-qml-keyboard-offscreen`. The new
`qindaqt.task-list-popup-windows` and the Quick Launch row in
`qindaqt.desktop-controls-dock-retained-delegates` count
`QGuiApplication::allWindows()` before, during and after real opens and closes
and end the tooltips with a real pointer exit. The task-list rows place the strip
low in the window: the offscreen platform clamps a tooltip that would open above
the screen onto its tile, where it takes the pointer's hover away.

Other `Popup.Window` users that exist once per surface (panel menus, applet
popups) are not changed here and still keep their window after first use.

## Revisit when

Qt releases a `Popup.Window`'s window on close (then the Loaders can go), or the
remaining one-per-surface popups show up in a new measurement.
