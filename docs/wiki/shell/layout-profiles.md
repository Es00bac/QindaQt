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

Customization is direct rather than a global edit mode. The settings window
offers applets, panel fragments, and profiles that users drag onto highlighted
monitor edges or existing panels. The editor provides preview, undo/redo,
duplicate, reset, save-as-profile, and import/export. Every operation has a
keyboard-accessible equivalent.

Panels support every monitor edge, multiple rows, partial or full length,
above/normal/below layers, and never/dodge-active/dodge-all/maximized/intelligent
hiding. Per-monitor arrangements may differ, and profiles must survive output
remapping without losing their logical panel graph.

## Implemented logical layout planning

`src/shell_layout` now implements the pure geometry boundary between validated
schema-v1 panel values and future platform surfaces. Given an ordered logical
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
panel thickness by it; a later platform adapter performs the one required
logical-to-buffer conversion. This prevents a 32-logical-pixel panel from
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

## Built-in workflow families

QindaQt ships original layouts inspired by useful interaction patterns:

- QindaQt global-menu top bar and smart bottom shelf;
- GNOME-style top bar, overview, dash, search, and dynamic workspace option;
- Unity-style left launcher, top menu/status bar, workspace spread, and HUD;
- MATE- and XFCE-style classic menus, panels, window lists, trays, and switchers;
- NeXTSTEP-style vertical dock and compact utility areas;
- macOS-style global menu and centered smart dock, paired by default with the
  Qinda macOS mist-and-sage theme;
- Windows classic, left-start, and centered taskbar arrangements; and
- minimal, keyboard-driven, multi-row, and monitoring-focused layouts.

These profiles reproduce workflows with original QindaQt code and assets. They
do not claim extension compatibility with those desktops or copy proprietary
branding.

Profile components communicate through the public boundaries in
[Module boundaries](../architecture/module-boundaries.md). Each built-in profile
is exercised by the resolution matrix in the
[testing harness](../development/testing-harness.md).
