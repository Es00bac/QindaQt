# Settings Customize route

The installed `qindaqt-settings --page customize` route is QindaQt's direct
layout editor. It opens with a gallery of layout profiles rendered as
miniature desktops — choosing between the QindaQt, macOS-inspired,
Windows-inspired, XFCE-like, or custom arrangements is a visual decision,
not a name lookup. The selected profile is edited on a WYSIWYG monitor:
a themed screen that reproduces the live panel materials (dock vs bar
coloring, corner radius by alignment, translucency, icon chips in their
real zones) from the same design tokens the desktop uses, so the preview
follows the active QindaQt theme. An icon-tile palette and a contextual
inspector flank the monitor in wide layouts; where the Settings Center
sidebar leaves a medium-width work area, Arrange, Outline, and Details tabs
keep the preview visible instead of squeezing three columns together.
The Details inspector owns a single scroll viewport in both layouts; the
compact host gives it the available height directly.

The route is a presentation and composition boundary. It depends only on the
public `shell_customization_editor`, `shell_customization`, `profiles`, applet
manifest, Settings1 client, the public confined iconography APIs (see
[ADR-0121](../adr/0121-real-iconography-in-the-settings-customize-route.md)),
the public Settings Appearance wallpaper values/catalog
(`appearance.wallpaper`/`appearance.wallpaperMode` keys, the mode codec, and
the `qindaqt:<basename>` bundled-identity discovery) for the read-only canvas
wallpaper preview, and the public `app_appearance` theme resolution
(`AppAppearance::ApplicationAppearanceController`) plus `decoration_painter`
chrome resolver/painter (`Decoration::resolveWindowChrome`,
`Decoration::ChromePreferences`, and the member-handlebar paint functions) for
the read-only canvas contained-window decoration preview. This preview reuses
the same theme+preferences resolution and the same paint functions the live
KDecoration plugin uses, so it can never show a chrome the compositor would
not actually draw. It never imports shell surfaces, LayerShellQt, compositor
code, the shell runtime's private wallpaper resolver, a private service
implementation, or platform mutation. The editing repository remains
the sole placement and manifest acceptance authority.

## Layout gallery

Every catalog profile projects a compact panel summary (edge, alignment,
thickness, length, hide mode, applet count) alongside its name and
description. The gallery draws one card per profile: a miniature monitor
whose bars render the panel arrangement that profile would place, with the
selection marked by accent bars, a badge, and a tinted card. Selecting a
different card updates the monitor below as a named draft until Apply; the
catalog file itself is never modified until then. Tooltips carry the
catalog description and the cards expose radio-button semantics with
accessible names.

## Direct editing contract

The route is no longer the only host of the editor domain: the shell hosts the
same repository/engine/session trio for in-place customization
(Meta + right-click menus and an edit mode, see
[Production panel surfaces](../shell/panel-surfaces.md#in-place-customization-meta-right-click)).
Both write the same user store, the shell adopts either surface's Apply live,
and `qindaqt.customize-editor-live-host-parity` plus the nested
`shell.live-customization.*` rows hold the two to byte-identical profiles for
the same intents. The route keeps everything only it offers: previews, the
property panes, profile selection and the keyboard outline.

A palette drag or existing-chip drag is one `EditorSession` gesture. Arming
the payload and entering drag mode opens a provisional preview; each target
update converges the repository to that target. Release commits the preview as
exactly one durable Undo step. Escape, leaving a drag without an accepted drop,
or explicit cancellation restores the exact pre-gesture snapshot. Rejected
targets keep their reason visible and cannot partially mutate the draft.

Pointer capture belongs to the page, outside the replaceable canvas delegates.
After the platform drag threshold, pointer positions resolve panel/zone identities
in page coordinates and drive the public hover API. Release revalidates the
current target before commit; outside release, Escape and grab cancellation
roll back. This keeps a move alive when its preview reconstructs the source chip.

The canvas uses repository-projected panel rectangles, not a second geometry
solver. It selects the current primary output (falling back to the first valid
inventory entry when primary identity is unavailable), preserves that output's
logical aspect ratio, and translates global solved rectangles into the selected
output's local coordinates before scaling them. This keeps both top bars and
bottom docks visible on tall-aspect and non-zero-origin displays. Panels scoped
to a different output remain available in the Outline but do not masquerade as
members of the previewed monitor. The canvas applies only a cross-axis thickness floor (24px, the same
floor the desktop concept preview uses) so icon chips remain clickable when
the representative output is scaled down; the floor grows a bar toward the
desktop interior, so an edge-attached panel never detaches from its edge.
Applets render as icon chips carrying the same XDG glyphs the live panel
chips use, laid out in their real start/center/end zones, with hover states,
selection rings, and tooltips. Drop targets are the three profile zones on
each panel plus the remaining desktop canvas. The desktop target is the public
profile owner `@desktop`; it projects, selects, moves, duplicates, removes, and
configures `profile.desktopApplets` through the same transactional editor as
panel applets. Desktop settings never receive a synthetic panel `zone` key.
Targets light up under an active drag. Behind the panels, the screen
paints the currently configured wallpaper — the same Settings1
`appearance.wallpaper` identity and `appearance.wallpaperMode` the shell
consumes, resolved through the public Settings Appearance wallpaper catalog
and painted with the shell's scaled/centered/tiled mapping — over the themed
gradient backdrop, and a contained-window decoration preview keeps the screen
reading as a desktop. That preview paints the configured theme
(`appearance.theme` and `appearance.colorScheme` together — the same pair
`AppAppearance::ApplicationAppearanceController` and the compositor's own
equivalent controller both require to resolve a theme at all) and the
container decoration preferences (button style, side, tab order, glyphs)
through the same `Decoration::resolveWindowChrome` flattening and
member-handlebar paint functions the compositor's own KDecoration plugin
uses, so the chrome shown can never drift from what a real contained window
would render; it is never a hard-coded decorative mock. Both previews are
read-only: independent route-owned Settings1 scopes subscribe to exactly the
wallpaper keys, exactly `appearance.theme`/`appearance.colorScheme`, and
exactly `Decoration::ChromePreferences::settingsKeys()` through their own
transports, so layout editing truth, wallpaper preview truth, and window
preview truth never share request tokens. No wallpaper
(explicit empty preference), an unknown mode token, an unresolvable or
unreadable identity, a mistyped value, or unavailable Settings1 truth each
fail closed to the token gradient (wallpaper) or the resolved theme's own
default chrome (window preview); the stored preferences are never renamed or
repaired by this route. The property panes issue complete panel configuration or move
intents for display scope, edge, alignment, thickness, length, and visibility.
Display scope is deliberately the two choices the existing profile contract
can state truthfully: **Primary display** writes the current primary screen's
exact Qt/Wayland output identifier, while **All displays** writes the schema-v1
wildcard `*`. Both reuse the public panel-move intent, so the layout solver,
revision fencing, preview validation, and one-step Undo behavior remain the
same as an edge move. The route never persists the label `primary` as a
sentinel or invents a second placement solver. The panes deliberately omit
always-hidden mode
until the separate reveal-affordance work exists; creating an unrecoverable
panel from this route would violate fail-closed interaction.

Manifest `settingsSchema` fields and current values are exposed in the selected
applet pane. A declared boolean, bounded integer (both `minimum` and
`maximum` present), or closed enum/string-choice (`enum` non-empty) field
renders a real typed editor — a Switch, a Slider, or a ComboBox — through the
public `configureAppletSetting(key, value)` method, which is the one public
`ConfigureAppletSettingsIntent` this domain now owns (see
[Customization editor domain](../shell/customization-editor.md)). Any other
declared kind — a bare freeform string, a number, an object, an array, or an
unknown/absent `type` — stays the quiet read-only row it always was; the
route invents no free-text or open-ended editor. The route validates the
declared kind, numeric bounds, and enum membership itself before ever calling
the editor: an unknown key, a value of the wrong type, an out-of-range
integer, or a value outside its enum is rejected atomically with a visible
error and never reaches `UpdateAppletSettingsCommand`. A no-op — the value
already in effect, whether explicitly stored or only the schema default —
adds no Undo step. `UpdateAppletSettingsCommand` replaces the applet's whole
settings map, so the route always sends the complete current map with
exactly the one validated field changed; unrelated fields (including `zone`)
survive untouched. The route does not bypass the editor boundary by executing
engine commands or editing profile JSON directly. Duplicate and Remove remain
available through public editor intents and copy/discard the full settings
map with the rest of the applet; the applet zone selector converges on the
same public drag gesture a pointer move uses.

## Keyboard and accessibility

All palette entries, panels, zones, and applets are represented in the focusable
outline even when the scaled canvas is too small for useful pointer targets.
The palette is an icon grid: tiles carry the plugin glyph, and the name and
description live on tooltips and accessible names, so the route stays visual
without losing non-visual truth. The preview's icon chips exceed the panel
hit-target floor through the thickness floor above; the palette and outline
retain their readable names, so no label can overflow a panel just because the
representative output is scaled down.
They expose list/list-item or radio-button roles, contextual names and counts,
selected state, and visible QindaQt Controls focus treatment. Palette activation
chooses the first compatible panel/orientation/zone, or the desktop for a
desktop-only applet such as Desktop Icons, through the same gesture path as a
pointer drag. The inspector's icon buttons (edge compass, alignment glyphs,
duplicate/remove) expose button or radio semantics with their tooltips as
accessible names.

For a selected applet, Space begins or commits keyboard move mode. Ctrl+Left or
Ctrl+Right steps within a panel, Alt+Left or Alt+Right changes zone, and
Ctrl+Shift+Left or Ctrl+Shift+Right changes panel. Delete removes the selection,
Ctrl+D duplicates it, the platform Undo/Redo sequences traverse history,
Ctrl+Return applies, and Ctrl+Shift+Return discards. Announcements describe
accepted and rejected targets for assistive technology. No application
departure discards silently: dirty profile selection is rejected, while the
route Close action, the platform Quit shortcut, title-bar close, or selecting
another Settings route opens the same modal discard confirmation. Cancelling
that prompt keeps the window open and returns navigation to Customize. A pending
navigation or application-close decision is window-owned shared state, so
crossing the responsive host threshold reconstructs the prompt in the active
host; resizing cannot accept, discard, or strand the draft.

## Settings1, persistence, and failure truth

`panels.layoutProfile` is the Settings1 selection key. A confirmed snapshot
creates the route baseline and a fresh repository/editor host. Selecting a
different catalog profile is a draft until Apply. Apply first atomically writes
edited profile content through the public profiles store adapter and only then
commits a changed selection through the route-owned Settings1 client. Conflict,
uncertain result, owner loss, and failed profile storage retain dirty truth and
surface an explicit diagnostic; none is reported as success.

Undo/Redo cleanliness compares the full canonical profile with the applied
baseline. Discard rebuilds the confirmed selection from the latest successfully stored
content for that profile. Each successful content write refreshes the in-memory
catalog before the separate selection commit, including when that commit fails.
A subsequent authority snapshot or reconnect therefore cannot resurrect the
startup layout and misreport it as clean. Only one
editor coordinator lease exists: losing it makes the route read-only with an
explicit unavailable notice. Retry obtains a new Settings1 snapshot and may
rebuild after the foreign lease is released. Missing catalogs, invalid
manifests, repository failure, and Settings1 transport failure likewise fail
closed.

The production composition injects a Customize-owned snapshot provider over
the current `QGuiApplication` screen inventory. It pairs the ordered logical
output values with the exact primary `QScreen::name()` and a monotonic revision.
Primary is disabled with an accessible explanation when that identity is
missing, does not name exactly one inventory member, or is ambiguous. If the
inventory or primary changes while a draft is dirty, display-scope controls and
Apply are fenced until Discard or reload rebuilds the editor against one fresh
snapshot. No arbitrary screen is selected, and stale exact output identity can
never be written as though it were still primary. A clean editor may rebuild
immediately when the injected inventory changes.

Built and installed executables discover profiles and applet manifests from
their respective source or relocated `share/qindaqt` catalogs without relying
on a developer-tree working directory. The compiled
`QindaQt.SettingsApp.Customize` module is linked into `qindaqt-settings` and is
also installed with its QML metadata and sources in the
`SettingsAppearanceRuntime` component, alongside the profile and manifest
catalogs.

## Verification

The focused selector is:

```sh
ctest --test-dir build/dev -R '^qindaqt\.settings-customize-' \
  --output-on-failure --no-tests=error
```

The model row proves one-step pointer commit, exact cancellation, deterministic
rejection rollback, pointer/keyboard insertion convergence, Undo/Redo,
atomic profile persistence, retained applied baselines across Discard and
authority recovery, Settings1 conflict truth, and foreign-lease recovery. The
same row inserts Desktop Icons through default palette activation, projects and
selects it on the canvas, edits its typed icon-size setting, rejects an
unsupported move to a panel without mutation, and reloads the persisted
desktop applet. It also injects `DP-1` and `HDMI-A-1`, proves Primary stores only the exact
`DP-1` identity, proves `*` solves to one panel surface on each display,
reloads the strictly serialized saved profile, and rejects missing, ambiguous,
or revision-changed primary truth without saving. It also proves
`configureAppletSetting` for every supported kind (boolean, bounded integer,
enum) round-trips through Apply and the reloaded saved profile with unrelated
settings preserved; atomically rejects an unknown key, a value of the wrong
type for every kind, an out-of-range or fractional integer, an unlisted enum
choice, and a declared-but-Unsupported freeform-string key, each leaving the
settings map and Undo stack untouched; adds no Undo step for a no-op against
either an explicitly stored or a schema-default value; that Duplicate copies
the full settings map onto the new instance; and that the guard fails closed
once the selected applet is removed. The dedicated pure
`qindaqt.settings-customize-applet-setting-validation` row exercises the
kind classifier and validator directly against hostile payloads for every
declared JSON-schema kind, independent of the model/editor/QML. The dedicated
`qindaqt.settings-customize-wallpaper-preview` row proves the read-only
wallpaper projection: a configured
`qindaqt:<basename>` identity resolves to the first readable root with its
mode mapped, a saved absolute path resolves identically, a changed Settings1
revision republishes source/mode, and an empty preference, unknown identity,
unknown mode token, unresolvable path, mistyped value, or missing snapshot
each fails closed to the explicit no-wallpaper fallback without renaming the
stored preference. The
warning-fatal pointer row drives real mouse events through the production page
and editor in compact and wide modes: palette insertion, chip movement across
preview reconstruction, one-step Undo, Escape and outside-release rollback. The warning-fatal page row renders 720×720 compact and 1080×720
wide layouts with a software `QQuickView`, checks the canvas and contextual
controls, asserts the profile gallery card exists, is visible, and carries a
"layout profile" accessible name, validates accessible names for
palette/panel/zone elements, keyboard-activates the Primary/All radio controls,
verifies Primary becomes disabled with an accessible degraded explanation when
identity truth is unavailable, and proves the discard dialog is centered in the window
overlay. The same page row proves the canvas wallpaper item stays hidden on
the token fallback, appears with the configured source when wallpaper truth is
ready, and maps tiled/centered/scaled modes to the shell's fill modes. The page and pointer harnesses install the confined icon runtime over
the shipped icon theme, so both rows exercise resolved glyph rendering and the
typed placeholder path stays the icons module's own coverage. The window-lifecycle row proves a dirty title-bar close opens that
dialog, Cancel keeps the window and draft, and pending navigation plus
application-close decisions survive wide-to-compact and compact-to-wide host
reconstruction. The positive boundary row scans every owned C++ header/source
and QML file. C++ includes are closed to Qt/system headers, route-local headers,
and the public `include/` trees of the dependencies named by this route; every
other repository-relative path is rejected. Independent hostile controls
require rejection of the exact sibling Settings Center include
`src/apps/settings_center/settings_route_registry.h` and a `../` escape. The
same row also rejects shell, LayerShellQt, compositor, and D-Bus imports. The
sole direct `QDBusConnection` exception is the named route-composition source
that constructs the public Settings1 transport. The installed row reuses the
sanitized Settings package harness and proves the relocated module, the staged
confined icon module, and the
catalogs. When Qt's own QML directory also holds an installed QindaQt, whose
modules would satisfy the harness's withheld-module poisons and keep the
relocated application running until timeout, the row runs that unchanged
harness in a private bubblewrap mount namespace that hides only the host
`QindaQt` directory, and fails with an explicit diagnostic when that isolation
is unavailable.

These tests use injected transports, temporary stores, and offscreen rendering.
They do not contact a host session bus, compositor, hardware, or input device.

## Stopping point

Apply makes the profile durable and changes the Settings1 selection, and the
running desktop adopts it live: the shell reloads the catalog from its
captured directories (user store last), re-selects with the startup
precedence rules (an explicit `--profile` still outranks the saved
selection; an unknown id fails closed), refreshes the visibility inventory,
and reconciles the surface set incrementally — the same path output hotplug
uses, with no shell or compositor restart
([ADR-0122](../adr/0122-adopt-saved-layout-preferences-live.md)). The
always-hidden reveal affordance and the nested rendered matrix remain
separate later slices.

See [Customization editor domain](../shell/customization-editor.md),
[Layout profiles](../shell/layout-profiles.md),
[ADR-0043](../adr/0043-isolate-the-customization-editor-domain.md),
[ADR-0121](../adr/0121-real-iconography-in-the-settings-customize-route.md),
and [ADR-0122](../adr/0122-adopt-saved-layout-preferences-live.md).
