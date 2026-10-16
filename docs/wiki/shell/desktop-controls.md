# Desktop controls

Desktop controls are the small panel surfaces that complete the stock profile
families: workspace switcher and tiles, show desktop, overview, active
application, command palette and HUD, system menu and status, Places, quick
launch, application tiles, and the Windows style dashboard. They are compiled
QML in `QindaQt.Shell.DesktopControls 1.0` and consume shell-owned facades.
Quick launch is the dock: pinned applications, folders, files, groups, and
the Trash, with drag and drop ([Dock items](dock-items.md)).

Each profile instance still passes manifest lookup, placement, host selection,
compiled implementation, and capability policy before its one selected panel
loader creates a control. A denied capability leaves the control unavailable
and cannot create an alternate execution path. Application tiles use the
dock's applications (group members included) and the launcher's audited
activation intent; they are distinct tile presentation from the compact Quick
Launch strip.

Workspace controls read the authenticated workspace transport through a shared
controller and include the displayed revision with every switch or show-desktop
request. The controller re-reads after a completed operation because KWin may
change a property without emitting the matching signal, and owner replacement
clears stale state and pending work. The production adapter identifies the
exact KWin unique owner and uses the configured trusted compositor PID when it
is available; the trusted QindaQt compositor peer remains the normal fallback
for sessions that do not provide that optional value.

The panel gives each selected control only the facade it needs. Dashboard
receives a small QML value view containing status, workspace, and launcher
facades. Popups use `Popup.Window` so their keyboard focus and content are not
clipped by the LayerShell panel band. All actions retain keyboard and
accessible activation paths.

System status aggregates the sound, Bluetooth, power, and network applet
facades into one indicator. Each lane is gated by System Status' own
`*.read` grant and its quick control by the matching `*.control` grant; the
borrowed facade still applies its own grants and fences. The network lane
(ADR-0258) reuses the [Network applet](network-applet.md) controller's own
glyph, summary, and accessible text, so the two views cannot disagree, and
its quick control is the Wi-Fi switch, which shows confirmed truth only. A
failed or uncertain network request marks the lane for attention.

Command palette, HUD, and overview search skip disabled results during Up/Down
navigation. Enter in the search field activates the first enabled result;
an all-disabled result list dispatches nothing. Moving above the first enabled
row returns focus to the search field.

On the Luna taskbar ([ADR-0124](../adr/0124-add-qindaqt-bliss-luna-option-set.md),
"Luna taskbar rendering") show desktop and quick launch take the `luna`
dressing: show desktop paints a white glyph beneath its fully transparent
standard button, which stays the only input and accessibility surface, and
quick launch uses Luna hover tiles and a white empty-state glyph. The
dispatcher forwards the panel-derived dressing to any desktop control that
declares `luna`; other hosts never see it.

## Popup placement

`ControlPopupFrame` does **not** own placement. It is the tokenized dressing of
`QindaQt.Controls.PanelPopup`, the one owner of panel-popup placement for every
panel surface (ADR-0221). A popup opens flush with the leading edge of the
control that owns it and directly below it, directly above it when the hosting
panel is on the bottom edge, and beside it for left and right panels, then
slides along the panel axis to stay on the output.

The complete contract — edge resolution, the window-local clamp, and why the
popup hangs off a hidden 1×1 positioner cell instead of its own control — is in
[Panel popup placement](panel-popup-placement.md). The frame adds only its
chrome, its `CloseOnPressOutsideParent` dismissal, and its width policy.

The active application control shows the focused task row's application
name. Task rows carry the desktop-entry name resolved from the compositor's
application id (see [iconography](iconography.md#desktopentryiconresolver)),
so the control, its popup heading, its accessible name, and the task list
agree; a raw reverse-DNS id is never shown.

The popup acts only on the task it was opened for. Opening records the row's
task id and displayed revision. The existing state notification then closes
the popup through `ControlPopupFrame.close()`, on the publishing turn, when any
of these happens:

- focus moves to another task;
- the same task publishes a new revision;
- no window is focused.

A Minimize or Close activation that arrives after that change dispatches
nothing, and reopening binds to the newly published row. A reprojection that
leaves the task id and revision unchanged keeps the popup open. The
controller's task-id and revision fence remains the dispatch authority; the
applet only retires stale presentation. The
`qindaqt.desktop-controls-offscreen-active-application` row proves this
sequence offscreen.

While no window is focused and the global menu presents the desktop menu
([ADR-0260](../adr/0260-show-the-file-managers-menu-when-no-application-is-active.md)),
the active application control names that menu's application, the File
Manager, with its icon, as the Finder name reads on macOS. It reads the global
menu facade's `desktopMenuShown`, `desktopMenuTitle`, and
`desktopMenuIconName` only, and only under its existing `windows.read` grant.
A focused window always wins, and with no window there are no Minimize or
Close actions, so the summary stays disabled. Without the desktop menu it
reads "Desktop" as before.

The desktop menu reaches its system and place items through this module's
controllers: System Settings through `SystemMenuController::openSettings`, the
session items through the session-actions facade that controller lends (with
the system menu's own Log out?, Restart?, and Shut down? questions), the Go
entries through `PlacesController::open`, and workspace switching and Show
Desktop through the workspace controller and its revision fence.
`DesktopControlsAccess.desktopCommands` lends the desktop menu's command
channel to the desktop-icons surfaces; no panel control reads it. See
[Global menu](global-menu.md#desktop-menu-when-no-application-is-active).

The focused checks are the desktop-controls unit, composition, offscreen QML,
workspace transport, resolver/catalog, and boundary rows. These rows prove
compiled presentation and injected-facade behavior; they do not qualify a
physical display, host compositor, or hardware service.

## Stable dock updates

Quick Launch publishes `presentationId` from each stored item kind and its
application id or path, with a fixed Trash identity. Groups use their first
stored application, which is globally unique in the dock. The compiled strip
uses the TaskList applet's public `KeyedRowModel` to update row data and move,
insert, or remove individual tiles. Running-state updates and app launches no
longer rebuild unchanged tiles or interrupt their focus/hover animations.
A group rename/reorder retains its tile; replacing its first member can replace
that group. No new dock persistence field is introduced.

New dock tiles use a short token-duration opacity fade; reduced motion makes
this immediate. Removed delegates retire immediately, and the animation never
changes layout sizes or input targets. The shared presentation contract is
[ADR-0278](../adr/0278-retain-dock-delegates-by-presentation-identity.md).

## Update cost

The task list reprojects ten or more times a second under a title spinner, and
three controllers listen to every reprojection. Their work follows visibility
and inputs, not the publication rate
([ADR-0391](../adr/0391-present-shell-search-and-dock-controllers-only-while-visible.md)):

- **Command search.** `CommandSearchController` keeps one derived cache per
  source (applications, menu actions, windows, workspaces) with a per-source
  staleness bit. While no surface presents it, a facade change only marks its
  source stale: no collection, no ranking, no signal. A surface takes a counted
  lease with `present()` when it shows results and gives it back with
  `dismiss()`; one controller serves every panel's popup, so the count matters.
  The first `present()` brings the cache up to date and notifies once; while
  presented, each facade change refreshes only the source that changed. Every
  read refreshes a stale cache first, so a caller that never presents still reads
  current truth; only the change signals depend on presentation.
  `CommandSearchPopup` takes the lease in `aboutToShow` and returns it exactly
  once on close or destruction, before clearing the query (clearing a presented
  controller would collect results nobody will see). The launcher projects its
  whole application catalog on each consultation, which is why an idle
  re-collection was the expensive part.
- **Quick Launch.** The controller compares the running-window signature (which
  windows of which application run, and which one is active) and returns before
  building any row when nothing it presents changed. It stores no snapshot
  revision: `activate()` asks the task-list facade for the revision it is
  showing at the moment of the gesture
  ([Task list](task-list.md), "Update cost and revision fencing").
- **Active application.** The controller publishes only when its row, phase or
  manageability changed, and exposes `rowRevision`, which advances exactly when
  the displayed row changes. The popup retires itself by comparing `rowRevision`
  with the value it was opened with. Comparing the generation would close the
  popup every time any other window's title changed.

Focused rows: `tst_command_search_controller` (collection counts across
presented and idle changes, the lease count, fresh reads without a lease),
`tst_quick_launch_and_active_application` (rebuild counts, silent identical
publications, the revision read at gesture time) and
`tst_desktop_controls_qml_active_application` (the popup survives an identical
republish and a spinner on another window, and retires when its own row
changes). New search surfaces must call `present()` and `dismiss()`.

## Popup windows

Dock tile tooltips are `Popup.Window` popups, and a `Popup.Window` keeps its
window (a GL context and video memory) for as long as the popup object exists.
`DockItemTile` therefore creates its tooltip through a `Loader` while it is
wanted (`dockMode`, hovered, not dragged) and destroys it once it has faded out,
so an idle dock owns no tooltip window
([ADR-0396](../adr/0396-create-per-tile-popups-on-demand-and-release-their-windows.md),
[Task list](task-list.md), "Popup windows are released on close"). The strip's
single context menu (`DockItemMenu`) was already one shared popup per strip.
