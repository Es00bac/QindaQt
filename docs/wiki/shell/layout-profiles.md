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
