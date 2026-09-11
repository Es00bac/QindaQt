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

On the Luna taskbar ([ADR-0124](../adr/0124-add-qindaqt-bliss-luna-option-set.md),
"Luna taskbar rendering") show desktop and quick launch take the `luna`
dressing: show desktop paints a white glyph beneath its fully transparent
standard button, which stays the only input and accessibility surface, and
quick launch uses Luna hover tiles and a white empty-state glyph. The
dispatcher forwards the panel-derived dressing to any desktop control that
declares `luna`; other hosts never see it.

## Popup placement

`ControlPopupFrame` owns placement for every control popup built on it. A
popup opens flush with the left edge of the control that owns it and directly
below it, directly above it when the hosting panel is on the bottom edge, and
beside it for left and right panels. The edge comes from the hosting
`RuntimePanel` model; hosts without one (the desktop surface, previews, tests)
open away from the nearer horizontal output edge, and `panelEdge` overrides
both. The popup then slides along the panel axis to stay on the output. The
anchor position is window-local because a Wayland layer-shell client never
learns where its window sits, so a clamp can be late for an offset panel but
never wrong; the compositor's popup positioner remains the backstop.

The active application control shows the focused task row's application
name. Task rows carry the desktop-entry name resolved from the compositor's
application id (see [iconography](iconography.md#desktopentryiconresolver)),
so the control, its popup heading, its accessible name, and the task list
agree; a raw reverse-DNS id is never shown.

The focused checks are the desktop-controls unit, composition, offscreen QML,
workspace transport, resolver/catalog, and boundary rows. These rows prove
compiled presentation and injected-facade behavior; they do not qualify a
physical display, host compositor, or hardware service.
