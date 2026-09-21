# Desktop controls

Desktop controls are the small panel surfaces that complete the stock profile
families: workspace switcher and tiles, show desktop, overview, active
application, command palette and HUD, system menu and status, Places, quick
launch, application tiles, and the Windows style dashboard. They are compiled
QML in `QindaQt.Shell.DesktopControls 1.0` and consume shell-owned facades.

Each profile instance still passes manifest lookup, placement, host selection,
compiled implementation, and capability policy before its one selected panel
loader creates a control. A denied capability leaves the control unavailable
and cannot create an alternate execution path. Application tiles use the
launcher facade's bounded pinned projection and its audited activation intent;
they are distinct tile presentation from the compact Quick Launch strip.

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

The focused checks are the desktop-controls unit, composition, offscreen QML,
workspace transport, resolver/catalog, and boundary rows. These rows prove
compiled presentation and injected-facade behavior; they do not qualify a
physical display, host compositor, or hardware service.
