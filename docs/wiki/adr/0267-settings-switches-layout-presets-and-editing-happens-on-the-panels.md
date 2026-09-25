# ADR-0267: Settings switches layout presets; editing happens on the panels

- **Status:** Accepted
- **Date:** 2026-09-24
- **Owners:** Settings Customize route, shell live customization, profiles
- **Supersedes:** In part, [ADR-0213](0213-host-the-customization-editor-live-in-the-shell.md):
  the Settings Customize route as a second editing surface ("one editing
  policy, two surfaces"). The shell is now the only one; everything else in
  ADR-0213 stands.
- **Superseded by:** None

## Context

Plan W15, in Jarrod's words: "The settings WYSIWYG panel editor sucks. All
panel/layout customization happens by editing the panels directly. The
settings should just switch between layout presets, which should allow user
presets to be saved as well."

Settings → Customize edited the selected layout profile on a scaled monitor
canvas with an applet palette, a keyboard outline and property panes, as a
draft applied with Apply. Since ADR-0213 and ADR-0266 the panels edit
themselves: Meta+right-click menus, a plain right-click "Edit Panels", drags
within and between panels and displays, and an "Add applet…" picker, each
edit one editor gesture and one save of the user profile. Two editors for
one layout meant two places to learn, a draft that could conflict with live
panel edits, and a Settings page nobody liked.

Profiles already had what presets need: installed (stock) profiles plus a
writable user store read last, so a user copy with a built-in's id shadows
it (docs/wiki/shell/layout-profiles.md), and the confirmed Settings1
selection `panels.layoutProfile` that the shell adopts live (ADR-0122).

Before removing the editor, every capability it had was checked against the
panels (the table is in the W15 worker record and in
[Settings Customize route](../apps/customize-settings.md#where-editing-went)).
Six had no panel-side path: duplicating an applet, choosing one display or
all displays for a panel, thicknesses above 160 px (the panel spin box
stopped at 160 while the engine accepts 192), and adding, removing or
resizing the desktop icons.

## Decision

1. **The page is a preset gallery.** One card per layout, drawn from its
   panel summary: installed layouts first, then the user's own presets.
   Clicking a card commits `panels.layoutProfile` and reports the switch only
   after a same-owner, same-epoch snapshot at the commit's revision confirms
   it; a stale readback waits (bounded), a refusal or an uncertain write is
   reported and never replayed. There is no draft and no Apply.
2. **Presets are profiles in the user store.** "Save current layout as
   preset…" re-reads the store and copies the applied layout (every panel
   edit included) under a new id derived from the name with a `user-`
   prefix, so it can never take an installed profile's id. Own presets can
   be renamed (same id and file, so a selection naming one stays valid),
   duplicated and deleted. Names are trimmed, 1–64 printable characters,
   unique among all presets ignoring case; at most 50 own presets.
   Provenance is by source: a profile only the store has is the user's own;
   a store copy of an installed id is an **edited built-in**.
3. **Edited built-ins show Modified** with **Restore original** (remove the
   store copy; the installed layout shows through and the shell adopts it
   through its store watcher) and **Save as new preset**. Delete and Restore
   both ask first.
4. **Deleting the active preset switches first.** The model commits the
   default layout (`macos-inspired`, ADR-0263; else the first remaining
   built-in) and deletes the file only once that switch is confirmed, so
   neither Settings1 nor the shell ever names a missing layout. A failed
   switch keeps the file and says so.
5. **Non-layout options stay.** The global auto-hide delay
   (`panels.autoHideDelayMs`) keeps its confirmed-write control on the page.
   The customization chord (`shell.customization.chord`) never had a
   Settings control and still has none.
6. **The panels gain what only the editor had** (all appended after the
   existing menu entries so no keyboard position moves): applet menu entry 8
   "Duplicate"; Panel ▸ Displays ▸ "This display only" / "All displays"
   (`configurePanel(…, "output", …)`, the exact output the panel was clicked
   on or `*`); the Size spin box spans the engine's 20–192 px; the desktop
   menu's Desktop icons ▸ gains Show/Hide and the bounded-number rows (icon
   size), and resolves the layout's own desktop-icons instance id. The
   desktop surface now finds the desktop-icons settings by plugin, not by
   the instance id `desktop-icons`, so a re-added instance keeps its
   settings.
7. **The editor code leaves the route.** Canvas, palette, outline, property
   panes, pointer gestures, action bar, the wallpaper and window previews,
   the output inventory provider and the applet-setting validator are
   removed with their tests. `RepositoryCustomizeEditorHost` stays, unused by
   the page, as the independent reference composition the live host is
   proven byte-identical to (`qindaqt.customize-editor-live-host-parity`,
   `qindaqt-customize-parity-tool`).
8. **The profiles store gains `remove(id)`**: it deletes exactly
   `<store>/<id>.json` for a valid id and nothing else (NotFound when
   absent, RemoveFailed keeps the file).

## Consequences

- One editor, on the panels. The profile schema, catalog precedence, the
  Settings1 keys and the editor domain are unchanged; no key or persisted
  field is added.
- The Customize model has no `dirty` state; it publishes `dirty` as a
  constant false so the Settings Center's Customize departure fence
  (SettingsRouteHost, Main.qml) never engages. Removing that fence is a
  Settings Center cleanup, not done here.
- The page no longer reads applet manifests; the Settings component still
  installs the applet catalog beside the profiles (the packaged route check
  expects both).
- Keyboard-only layout editing regressed: the old route's outline and move
  mode have no panel-side equivalent yet. Panel menus are keyboard-navigable
  once open, but opening them and focusing a panel still need the pointer
  (ADR-0266 gives panels keyboard focus only after a click). Chip focus
  traversal in edit mode is the follow-up.
- A rename or a copy re-reads the store first; a panel edit landing between
  that read and the write can be overwritten by the rename (milliseconds).
- Tests: `qindaqt.settings-customize-presets` (provenance and order,
  confirmed switching, refusal and uncertainty, save/rename/duplicate/delete
  with name and count bounds, Modified/Restore, active-preset deletion and
  its failure, a missing selection, the default against the schema),
  `qindaqt.settings-customize-page` and
  `qindaqt.settings-customize-window-lifecycle` (warning-fatal page and
  Main.qml hosts), `qindaqt.settings-customize-panel-delay`,
  `qindaqt.profile-formats` (store removal), the Settings Center route
  construction witness (ADR-0250) and search rows, and on the panel side
  `qindaqt.shell-live-customization-controller`,
  `qindaqt.live-customization-offscreen` and
  `qindaqt.desktop-surface-customize-menu`.

## Revisit when

- Layout editing needs something the panel menus cannot express (then it
  goes to the panels, not back to Settings).
- Presets need to travel between machines (import/export), or built-in
  layouts start changing often enough that silently shadowed upgrades of
  edited built-ins matter.
