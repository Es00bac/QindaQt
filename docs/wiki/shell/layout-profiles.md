# Layout profiles

A layout profile describes how the QindaQt shell is arranged and behaves. It is
independent from a theme, which describes visual tokens, and from user settings,
which describe machine- or account-specific choices. Users may combine any
compatible profile and theme.

## Profile contract

`QindaQt.LayoutProfile 1` is declarative data. A profile has a stable identifier,
schema version, display name, and definitions for:

- panels, docks, edge, monitor scope, length, rows, alignment, margins, layer,
  exclusive zone, opacity, and hiding policy;
- ordered applet instances and their profile-level defaults;
- launcher, task-list, menu, overview, workspace, and window-button behavior;
- shortcut mappings and optional window-management defaults; and
- capabilities required from the current QindaQt version.

The foundation loader currently implements the deliberately smaller
[profile schema v1](../reference/profile-schema-v1.md): workflow hints plus
edge, layer, hiding, alignment, row, thickness, length, output, and ordered
applet data. The remaining fields above are accepted product requirements, not
yet implemented persistence fields.

Profiles contain no executable code and cannot request undeclared applet
capabilities. Built-in data is immutable; user edits are saved as derived user
profiles so upgrades do not overwrite customization. Unsupported schema majors
are rejected with a useful error, and supported older versions migrate through
tested transformations.

Schema v1 loading is strict: present fields are never type-coerced, defaults
apply only to absent optional fields, duplicate JSON keys and excessive nesting
are rejected, and programmatic candidates must contain only losslessly
persistable JSON settings values. Applet instance IDs are unique across the
complete profile, so a move between panels preserves one identity while a
duplicate receives a new ID. The exact accepted values and structured error
contract are maintained in the
[profile schema reference](../reference/profile-schema-v1.md) and
[ADR-0006](../adr/0006-profile-global-applet-identity.md).

## Apply and edit behavior

Applying a profile is a transaction: validate, stage a live preview, commit, or
roll back. It does not require logout. A failure on one output must not leave a
partially applied layout on the others.

Customization is direct. On the desktop itself, panels are edited where they
are: Meta+right-click menus, and panel edit mode, where applets are dragged
within and between panels and displays
([ADR-0266](../adr/0266-edit-mode-drags-applets-across-panels-and-displays.md)).
The settings window does not edit layouts: Settings → Customize switches
between layout presets, saves the applied layout as the user's own preset,
and renames, duplicates, deletes or restores them
([ADR-0267](../adr/0267-settings-switches-layout-presets-and-editing-happens-on-the-panels.md)).
The panel editor provides undo/redo and duplicate; reset is Restore original
in Settings and save-as-profile is Save current layout as preset.
Import/export is not implemented yet, and keyboard-only editing on the
panels is an open gap (the menus are keyboard-navigable once open).

Panels support every monitor edge, multiple rows, partial or full length,
above/normal/below layers, and never/dodge-active/dodge-all/maximized/intelligent
hiding. Per-monitor arrangements may differ, and profiles must survive output
remapping without losing their logical panel graph.

## Implemented logical layout planning

`src/shell_layout` implements the pure geometry boundary between validated
schema-v1 panel values and platform surfaces. Given an ordered logical
output inventory, the planner:

- expands `output: "*"` once per output in inventory order while a named selector
  expands only on the exact output;
- treats `(panel ID, output ID)` as an expanded surface identity and rejects any
  duplicate pair, missing or duplicate output, malformed geometry or scale, and
  over-constrained arrangement before returning any layout;
- places top, bottom, left, and right surfaces in profile order, stacking
  surfaces on the same edge inward by `rows * thickness`; top and bottom stacks
  own their complete cross-edge bands, so side panels occupy only the vertical
  lane remaining between both stacks;
- gives `start`, `center`, and `end` panels their rounded fractional edge length,
  while `fill` intentionally owns the complete edge regardless of `length`; and
- rejects any cross-edge collision and returns both deterministic,
  pairwise-disjoint surface rectangles and the exclusive work area for each
  output.

All input and result rectangles are desktop-logical coordinates. Output scale
must be finite and positive, but the planner never multiplies coordinates or
panel thickness by it; Qt's Wayland platform performs the one required
logical-to-buffer conversion after `shell_surface` passes the logical values
through unchanged. This prevents a 32-logical-pixel panel from
becoming 40 logical pixels merely because it is placed on a 125% output.
Intermediate depths, offsets, extents, and inclusive rectangle endpoints use
checked wide arithmetic. A valid rectangle ending exactly at `INT_MAX` is
representable; any truly unrepresentable expansion fails atomically instead of
wrapping a Qt coordinate.

Until schema v1 gains an explicit exclusive-zone field, `normal` and `above`
surfaces reserve work area, while `below` and `overlay` surfaces do not. Every
surface still participates in same-edge stacking so panels cannot obscure one
another. Consequently, if a non-reserving surface precedes a reserving surface,
the reservation reaches the latter surface's actual inner boundary; a
non-reserving surface after the deepest reserving surface does not enlarge the
work area. Hide-mode animation and dynamic reservation changes remain runtime
shell work and do not alter this static plan.

Focused tests cover all edges and alignments, row depth, stacking and layer
semantics, cross-edge corner ownership, arithmetic boundaries, 1920x1080,
1920x1200 WUXGA, 2560x1440, and a negative-coordinate 1080p plus 125%-scaled
1440p logical arrangement.

The production conversion, reconciliation, LayerShellQt mapping, and current
runtime limitations are specified in
[Production panel surfaces](panel-surfaces.md). The preview QML geometry is a
single-canvas visual fixture and is not part of this contract.

## Implemented editing transactions

`src/shell_customization` implements the pure transaction boundary used by the
future settings window and shell surface controller. It starts from a validated
schema-v1 profile, a supplied logical output inventory, and an immutable copy
of the session's validated applet-manifest catalog. It then publishes retained,
immutable snapshots carrying the normalized profile, complete solved layout,
optimistic revision, and preview status. The output inventory belongs to one
editor session; the shell's live host supplies the current logical outputs and
rebuilds its session when they change. Repository reads and commands are confined
to one editor thread. Invalid initial profiles or manifest catalogs leave the
repository non-ready with no published snapshot or committed profile; the
initialization error and supplied initial revision remain available at the
command boundary without disguising an unsolved value as usable state.

One move-only coordinator lease may edit a repository at a time. Releasing and
reacquiring that lease preserves the repository-owned durable history and any
active preview. Typed commands add, remove, reorder, move, and configure panels;
insert an applet by instance and plugin ID; move or reorder applets within or
across panels; remove, duplicate, and update applet instances; and operate undo,
redo, begin-preview, commit-preview, and cancel-preview. Future pointer and
keyboard adapters must emit the same typed command for an equivalent edit.
Panel IDs and applet-instance IDs remain stable, applet-instance IDs are
profile-global, and stale revisions, duplicate IDs, self-anchors, or unknown
drop anchors are rejected before publication.

The repository also exposes a copied transaction status containing preview
activity/dirty state and the currently applicable undo/redo availability. The
exclusive coordinator can evaluate a revision-bound command without publishing
a snapshot or consuming history. Evaluation and execution share the same
preflight, manifest-placement, strict profile round trip, complete output solve,
and no-change checks. The settings canvas can therefore highlight only targets
that the current revision would accept without duplicating placement policy;
acceptance reserves nothing and must be discarded after any revision change.

Applet creation and duplication, panel insertion, panel-orientation changes,
and applet orientation/zone changes are checked against the copied manifest
catalog. Top and bottom panels are horizontal; left and right panels are
vertical. An absent applet `settings.zone` is equivalent to `start`. Legacy
applets without an available manifest may still be reordered, removed, moved
between equivalent placements, or receive placement-neutral settings changes;
an operation that needs a new compatibility decision fails explicitly instead
of guessing.

Every candidate passes typed profile validation, a strict schema-v1
serialization/load round trip, and a complete `shell_layout` solve before
publication. Any returned command failure therefore leaves the snapshot,
revision, preview, and history unchanged. Preview edits are visible but
provisional: cancel restores the exact pre-preview profile in one revision,
while commit collapses the whole preview into one durable undo step. Undo and
redo work within an active preview, and the final available revision is
reserved so an active preview can always commit or cancel without overflow.
This transaction layer adds no persistence fields; accepted values remain
profile schema v1.

This module does not load applet entry points, construct shell surfaces, render
drop targets, persist user profiles, or provide settings presentation. The
shell's live host composes it through the
[customization editor domain](customization-editor.md) for the panel menus and
edit mode; the [Settings Customize route](../apps/customize-settings.md) only
switches and saves presets. The production surface host consumes the same
solver contract from its selected profile but does not subscribe to
provisional editor snapshots; the reveal affordance remains an outstanding
part of the Shell and customization milestone.

## Shell startup selection and catalog precedence

The production shell adopts the confirmed Settings1 `panels.layoutProfile`
selection at startup ([ADR-0074](../adr/0074-compose-shell-preferences-through-settings1.md))
and reconciles a newly applied Customize profile live
([ADR-0122](../adr/0122-adopt-saved-layout-preferences-live.md)).
Before the initial surface plan it performs one bounded read of the scoped
Settings1 snapshot; an explicit `--profile` outranks the saved selection, and
the `macos-inspired` profile (menu bar and dock) is the fallback when the
service is unavailable or the saved profile has been deleted or renamed (a
diagnostic names the dropped selection). It is also the Settings1 default for
a new user, and the live-adoption fallback when neither the saved nor the
running layout survives a reload
([ADR-0263](../adr/0263-the-mac-style-layout-is-the-default.md)). Only an
explicit unknown `--profile` fails startup.
After a panel edit saves profile content, or Settings confirms a new
selection or adds, restores or removes a user-store copy, the running shell
reloads the same catalog precedence and incrementally reconciles the
resulting surface set without a compositor or shell restart. Startup still
uses the bounded initial read and precedence rules above.

Profile catalogs merge low-to-high precedence with the writable user store
last, so user-saved profiles override — and partial user catalogs no longer
shadow — the installed stock profiles. The source tree participates only for
the genuine build-tree executable. Explicit `--profile-dir` and
`QINDAQT_PROFILE_DIR` remain isolated single-directory overrides. The shell and
the Settings Customize route share this precedence contract; the route also
keeps the two sides apart, so a profile only the user store has is the user's
own preset and a store copy of an installed id is an edited ("Modified")
built-in ([ADR-0267](../adr/0267-settings-switches-layout-presets-and-editing-happens-on-the-panels.md)).

## Built-in workflow families

QindaQt ships **one stock profile per distinct feel** — eleven of them. Each
one is a different idea about where the shell lives, not a reshuffle of the
same bar, and each pairs with its own default theme
([ADR-0223](../adr/0223-one-stock-profile-per-distinct-feel.md),
[ADR-0268](../adr/0268-familiar-desktop-experiences-are-layout-and-theme-pairs.md)):

| Profile | Feel | Panels | Default theme |
| --- | --- | --- | --- |
| `qindaqt` | The signature layout: three edges, three jobs | top 26 px ledger (focused window + its menu + clock), left 52 px shelf (windows and containers), bottom-right 34 px instrument strip on `overlay`/`dodge-active` | `qinda-dark` |
| `qinda-bliss` | Classic Taskbar (XP-like) | one bottom 40 px taskbar: start menu, quick launch, task buttons, tray, clock, show desktop, plus desktop icons | `qinda-bliss` (Qinda Classic Blue) |
| `macos-inspired` | Menu and Dock | 24 px global menu bar — the thinnest stock panel — with the system menu at its far left, plus a centred 72 px dock that hides intelligently and always begins with the File Manager and ends with the Trash; desktop icons on the **right** with the mac context menu | `qinda-macos` (Qinda Mist) |
| `gnome-inspired` | Overview | one 38 px top bar, clock **centred**, no task bar at all: windows live in the overview | `qinda-slate` |
| `unity-inspired` | Command rail | 28 px top bar with the focused window's name, its menu and the HUD, plus a 64 px left rail that hides behind any window | `qinda-dusk` |
| `xfce-inspired` | Compact and flexible | 34 px top panel with Applications and Places menus and **ungrouped** window buttons, plus a small bottom launcher dock that ducks the active window | `qinda-light` |
| `nextstep-inspired` | Workspace Dock (NeXT-like) | one 72 px column of square tiles down the **right** edge, 62 % of it, with the system tile on top and no horizontal bar anywhere; a minimized window waits as its tile | `qinda-graphite` |
| `windows-modern` | Centered Taskbar (Windows 11-like) | one 48 px bottom bar whose start button and glyph-only task tiles stay centred, with the **modern** start panel (search, pinned grid, All apps; no advertising, promoted apps, web results or account prompts); widgets left, status right | `qinda-daylight` |
| `beos-inspired` | Corner Bar (BeOS-like) | one 32 px bar across the right half of the top edge: applications menu, a button per application, status, tray, clock; with its theme, yellow title tabs that roll up on double-click | `qinda-marigold` |
| `win31-inspired` | Program Groups (Windows 3.1-like) | one 34 px top bar: applications menu and the dock's groups as program groups, status, tray, clock; no task bar, and with its theme a minimized window becomes an icon on the desktop | `qinda-classic-grey` |
| `minimal` | Nothing until you ask | one always-hidden 32 px strip, a third of the width, holding the command palette and four glyphs: no clock, no task buttons, no workspace switcher | `qinda-glass-dark` |

Panel thickness, edge, alignment, length, layer, and hide mode all differ
between them, and every applet instance carries an explicit `zone`. A profile
declares only settings the runtime actually honours — `zone`, `dockMode`,
`presentation`, `grouping`, the quick-launch `items` slice, and the
desktop-icons keys — so a stock layout never ships a value that reads as
configuration and does nothing.

Panel thickness also respects each applet's declared minimum cross extent. The
`launcher` applet asks for 32 logical pixels, so every panel hosting one is at
least that thick; `macos-inspired` reaches 24 px precisely because its menu bar
hosts no launcher.

The network appears in every profile that shows service status: the
`qindaqt` instrument strip places the standalone Network applet beside
Bluetooth and Power, and the `gnome-inspired`, `macos-inspired`,
`unity-inspired`, and `windows-modern` profiles, which place no standalone
Bluetooth or Power applet either, show it as the network lane of their System
Status instance ([Network applet](network-applet.md)).

Only layouts that resolve a global-menu applet own the AppMenu registrar;
every other layout keeps application menus inside their windows, and a live
layout switch moves the registrar with it
([ADR-0130](../adr/0130-window-attached-menus-without-a-global-menu.md)).
Among the stock layouts only Menu and Dock, QindaQt and Command Rail show the
global menu.

## Familiar desktop experiences

Six layouts are familiar desktops for people switching to QindaQt: Menu and
Dock (Mac-like), Classic Taskbar (XP-like), Centered Taskbar (Windows
11-like), Corner Bar (BeOS-like), Program Groups (Windows 3.1-like) and
Workspace Dock (NeXT-like). Each experience is the profile, the theme its
`defaultTheme` names (which carries the W19 button style), whether the global
menu shows, and the File Manager arrangement it expects
([ADR-0268](../adr/0268-familiar-desktop-experiences-are-layout-and-theme-pairs.md)).
They are original: names, colors, glyphs and shapes are QindaQt's own, and no
name a user reads is another vendor's; ids stay as they were.

| Layout | Theme | Buttons | Title bar | File Manager |
| --- | --- | --- | --- | --- |
| Menu and Dock | Qinda Mist | traffic lights, left | theme surface | `finder` |
| Classic Taskbar | Qinda Classic Blue | `blue-tiles`, right | clean blue | `explorer` |
| Centered Taskbar | Qinda Daylight | `wide`, right | theme surface, 8 px corners | `explorer` |
| Corner Bar | Qinda Marigold | `tab`, left | clean yellow tab; double-click rolls up | `finder` |
| Program Groups | Qinda Classic Grey | `bevel`, right | clean navy; minimize rolls up to an icon | `explorer` |
| Workspace Dock | Qinda Graphite | `bold`, right | clean black | `finder` |

- **Title-bar behaviour lives in the theme.** A theme's `decoration` block may
  author `titleDoubleClick`, `minimizeAction` and `titleWear`
  ([theme schema](../reference/theme-schema-v1.md)); the Appearance
  double-click option still overrides. Iconify and roll-up are the existing
  roll-up to the window's icon ([ADR-0203](../adr/0203-an-ordinary-window-rolls-up-to-its-icon.md)).
- **The pairing is advisory.** Choosing a layout does not change the saved
  theme ([ADR-0074](../adr/0074-compose-shell-preferences-through-settings1.md));
  the profile's `defaultTheme` applies when no theme is saved, and the
  handbook names each layout's theme.
- **The File Manager hint.** `workflow.fileManager` names the arrangement
  (`finder`, `explorer`, `commander`) the File Manager's style setting uses
  as its default until the user picks one; nothing reads it until that
  setting lands (plan W11s).
- **The Mac-style dock's ends.** Its dock holds a permanent File Manager tile
  first and a permanent Trash tile last around the stored items
  ([Dock items](dock-items.md#permanent-ends)).

These profiles reproduce workflows with original QindaQt code and assets. They
do not claim extension compatibility with those desktops or copy proprietary
branding.

## Centered dock presentation

The QindaQt smart shelf and macOS-inspired dock use one canonical `task-list`
instance, the launcher trigger, and the compiled `quick-launch` dock items
([Dock items](dock-items.md)) in
their center zone; the macOS-inspired dock splits the quick-launch items into
a permanent File Manager end, the stored items, and a permanent Trash end
after the task strip (ADR-0268). Their existing applet presentation setting `dockMode: true`
selects dock treatment after profile resolution; it is not a schema field and
does not alter task-list, launcher, or pin persistence. A copied or renamed
center-bottom panel retains the treatment when its applets retain that setting;
the `dock` and `smart-shelf` IDs remain legacy fallbacks only.

Both stock dock panels use the `overlay` layer. Reservation follows the layer
(see [Panel visibility](panel-visibility.md)): `normal` and `above` panels
reserve work area while visible, `overlay` and `below` never do. An
auto-hiding dock on a reserving layer would push maximized windows up on
every reveal, so a dock that hides must stay on `overlay`.

Dock tiles receive a 60-logical-pixel presentation budget inside a 72-pixel
surface with token spacing for a bottom gap and hover allowance. Dock panels
solve at full edge length: the painted rounded shelf hugs its occupied center
content while it fits, grows to the full output width as dock content grows,
and overflow beyond full width scrolls inside the center zone instead of
truncating rows (see [Dock interactions](dock-interactions.md)). If a
customization places dock content in a start or end zone, the material
expands to cover that real content rather than leaving a clickable unpainted
control. Reduced motion disables dock lift in the owning
applets; reduced transparency and high contrast select opaque material from
the published QST accessibility projection.

## Worn Luna taskbar material

The Classic Taskbar profile's (`qinda-bliss`) one bottom panel carries the id
`bliss-taskbar`;
`PanelContent` derives its `lunaMode` from that panel id — the same
presentation-derivation precedent as the dock `dockMode` setting — and
selects an opaque Luna gradient material with a gloss line instead of the
token material. As with the dock derivation, a copied or renamed panel keeps
or loses the treatment together with its id.

`lunaMode` travels through `PanelAppletRow` to every `AppletChip`, so applets
on the Luna bar sit directly on the gradient with no raised chip; hover and an
open notification center show a translucent white tint. Behind the end zone a
full-height notification-area well, a lighter blue band with a dark leading
seam, frames the tray, notification button, clock, and show-desktop control.
Hosted applets take their Luna path from either `lunaMode` or their own
`presentation: "luna"` setting (declared by the task-list, clock, and
quick-launch manifests): white glyphs, a plain white Tahoma clock, Luna task
buttons, and the real `notifications` glyph. Applets outside a Luna bar and
without that setting, in this or any other profile, keep the standard
presentation ([ADR-0124](../adr/0124-add-qindaqt-bliss-luna-option-set.md),
"Luna taskbar rendering"). The Bliss task list also sets `grouping: "never"`,
so every window, container members included, gets its own button
([Task list](task-list.md#grouping-and-ordering)).

The Bliss profile is also a deliberate stock-profile exception: it ships
**no clipboard slot**, because the XP taskbar it reproduces has no utility
chip there. Every other stock profile keeps exactly one clipboard instance
beside its notification center, and the stock-profile invariants assert one
resolved clipboard per profile except qinda-bliss and exactly one resolved
menu slot that is either a launcher or the start-menu applet.

Every one of the eleven shipped profiles contains exactly one
`notification-center` applet instance. This is a stock default, not a schema
requirement: customization may remove it, and imported or user-created profiles
may omit it. When the authenticated notification presentation runtime is
available, the shell-owned `Meta+N` action remains the layout-independent entry
path; its user mapping is owned by KGlobalAccel rather than profile shortcut
data. See [Notification presentation](notification-presentation.md) for the
current focus and live-qualification limits.

Profile components communicate through the public boundaries in
[Module boundaries](../architecture/module-boundaries.md). Each built-in profile
is exercised by the resolution matrix in the
[testing harness](../development/testing-harness.md).

## Audited preset equivalences

Every preset resolves its menu slot to the compiled application launcher — or,
in the Bliss profile, to the compiled start-menu applet — at that layout's own
menu position, and its tray slot to the compiled status notifier. The
workspace-dock clock uses the compiled clock implementation. These
substitutions provide the shared supported behavior; they do not claim separate
classic-menu renderers or a tile-specific clock.

Profiles never reference a plugin that has no manifest. `unity-inspired` named
`application-launcher` and `grouped-task-list`, and the centred Windows preset
named `centered-task-list`; none of the three had a manifest, so those
instances silently resolved to nothing and the layouts showed a duplicated or
missing control. **All three now exist** (ADR-0224) as manifests over the
existing launcher and task-list implementations — the same aliasing precedent
as `system-tray.json` — each carrying its own name, zones, sizing and default
tile shape. `qindaqt.applet-runtime-resolution` resolves every stock instance
in its own placement.

## Tile shapes and start-menu variants

Two applets present more than one way, selected per instance from the profile:

- `task-list` takes `presentation`: `standard` panel rows with titles, `luna`
  for the Bliss dressing, `centered` for the Windows-11 taskbar's glyph-only
  tile with an underline that widens while the task is focused, and `rail` for
  the Unity-style square tile that marks its leading edge. `dockMode` always
  wins over it, because a dock tile is already glyph-only and owns its own
  magnification envelope. `centered-task-list` and `grouped-task-list` default
  to `centered` and `rail` respectively.
- `start-menu` takes `variant`: `luna` (the default, and what Bliss uses) is
  the worn XP two-column panel with the green start pill; `modern` is the
  Windows 11 centred card — a search field, a five-column pinned grid, the
  complete program list behind an **All apps** toggle, and a session footer —
  opened by a glyph-only square button. An unrecognised value renders Luna, so
  a typo never produces an empty panel. Exactly one panel is built per
  instance: a `Popup` creates its content item whether or not it ever opens,
  so a Luna taskbar must not pay for the modern panel's grid and list.

Unlike the Luna dressing, the modern panel is tokenized. Windows 11's start
menu has no fixed period palette to reproduce — it follows the system accent
and light/dark mode — so following QST-1 is the faithful choice as well as the
maintainable one.

Other unresolved semantic controls remain explicit capability gaps until their
real implementations are integrated.
