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

The focused checks are the desktop-controls unit, composition, offscreen QML,
workspace transport, resolver/catalog, and boundary rows. These rows prove
compiled presentation and injected-facade behavior; they do not qualify a
physical display, host compositor, or hardware service.
