# Settings Customize route

The installed `qindaqt-settings --page customize` route switches between
**layout presets** and keeps the user's own ones
([ADR-0267](../adr/0267-settings-switches-layout-presets-and-editing-happens-on-the-panels.md)).
It no longer edits layouts: panels are edited where they are, on the desktop
(right-click a panel → Edit Panels, or the Meta+right-click menus; see
[Production panel surfaces](../shell/panel-surfaces.md#in-place-customization-meta-right-click)).
Every change made there is saved to the layout in use, and this page shows
it.

## Layout presets

The page is a gallery of cards, one per layout, each a miniature desktop
drawn from the layout's panel summary (edge, alignment, thickness, length,
layer) with its name; the tooltip and accessible description carry the
catalog description. **Built-in layouts** come first, in installed catalog
order, then **My presets**, the user's own, by name. The current layout is
marked by a badge shape, a border and a tint, and exposed as the checked
radio button; the default layout (`macos-inspired`,
[ADR-0263](../adr/0263-the-mac-style-layout-is-the-default.md)) carries a
**Default** badge. Badges are words, never colour alone.

Clicking a card (or Space/Enter on it) switches the desktop to that layout by
committing the Settings1 selection `panels.layoutProfile`; the running shell
adopts it live ([ADR-0122](../adr/0122-adopt-saved-layout-preferences-live.md)).
There is no draft and no Apply.

Where a preset comes from decides what it offers. The catalog is the
installed profile directories (low-to-high precedence, the same ones the
shell reads) plus the writable user store
(`<data>/qindaqt/profiles`), read last so a store copy with an installed
layout's id shadows it
([Layout profiles](../shell/layout-profiles.md#shell-startup-selection-and-catalog-precedence)):

| Kind | What it is | Offers |
| --- | --- | --- |
| Built-in | Only an installed profile has the id | Switch |
| Edited built-in (**Modified**) | The store holds a copy of an installed id, written by panel edits | Switch, **Restore original**, **Save as new preset** |
| Own preset | Only the store has the id | Switch, **Rename**, **Duplicate**, **Delete** |

**Save current layout as preset…** copies the applied layout into My
presets. The route re-reads the store first, so the copy includes every edit
made on the panels, even one that landed a moment ago. Saving does not
switch: the desktop stays on the layout in use.

## Your own presets

A new preset's id is `user-` plus an ASCII slug of its name (`user-preset`
when the name has none), with `-2`, `-3`… added until it is free among the
presets and the store's files. The prefix means an own preset can never take
an installed layout's id and silently shadow it. The name is trimmed and must
be 1–64 printable characters (well-formed UTF-16) and unique among all
presets, built-ins included, ignoring case; the name dialog validates every
keystroke and only offers Save for a name the model accepts. At most 50 own
presets can exist; edited built-ins do not count.

- **Rename** rewrites the preset under the same id and file, so a selection
  naming it stays valid. It re-reads the store first; only a panel edit that
  lands between that read and the write can be overwritten.
- **Duplicate** copies an own preset as "‹name› copy", then "‹name› copy 2"…,
  shortening the name to make room for the suffix.
- **Delete** asks first. Deleting a preset that is not in use removes its
  file. Deleting the one in use first switches the desktop to the default
  layout (or, without it, the first remaining built-in) and removes the file
  only once that switch is confirmed, so neither Settings1 nor the shell ever
  names a missing layout; if the switch fails, the preset stays and the page
  says it was not deleted.

## Edited built-ins

Editing panels while a built-in layout is in use saves a user copy under the
same id (ADR-0213). Its card shows **Modified** with **Restore original**
(asks first, then removes the copy so the installed layout shows through; the
shell adopts it through its store watcher) and **Save as new preset** (keeps
the edits as an own preset, prefilled "‹name› (edited)"). A store copy also
shadows future updates of that built-in, which Restore original undoes.

## Switching and Settings1 truth

A switch is pending from the commit until a same-owner, same-epoch snapshot
at the commit's reported revision confirms the requested value; only then
does the page say "Switched to …". A same-lineage snapshot older than that
revision is stale and keeps the switch pending, bounded by a four-second
deadline. A refused commit, a conflict, owner or epoch replacement, a lost
reply, or an unavailable Settings1 each end the switch with an explicit
message, and nothing is replayed. While a switch is pending every card and
store action is disabled; the Settings client allows one write at a time, so
the auto-hide delay waits too.

A selection naming a layout that is no longer installed (deleted outside
Settings) leaves the page usable: no card is current, the status says the
desktop uses the default, and any card can be chosen. A catalog that fails to
load (a malformed file anywhere, reported with its diagnostic) or an
unavailable Settings1 shows the degraded notice with Retry, which re-reads the
catalog and Settings1; the auto-hide delay control stays on the page.

The route watches the user store (the directory and each profile file, or
its parent before the first save, the same watch the shell keeps) and
re-reads the catalog 250 ms after a change, so a panel edit made with the
page open shows up as **Modified** without reopening it.

## Panel options

The global `panels.autoHideDelayMs` preference (0–5000 ms) stays here: it is
not part of any layout, so switching presets never resets it. The control
shows a saved value only after an exact typed Settings1 snapshot and enables
only when the owner admits a write and no preset switch is pending. Pointer
and keyboard slider gestures submit their final value once. A commit reply
remains pending until a same-owner, same-epoch snapshot reaches its reported
revision and confirms the requested value; stale readback triggers a bounded
refresh, then an uncertainty notice rather than a replay. Refusal,
conflicting external values, owner replacement and unavailable Settings1
leave the last confirmed value and an explicit status visible. The
[panel visibility runtime](../shell/panel-visibility.md#production-integration)
consumes the same key.

The customization chord (`shell.customization.chord`) has an active consumer
but still no Settings control ([Settings1 catalog](../handbook/catalog/settings.md)).

## Where editing went

Before the editor was removed, each of its capabilities was checked against
the panels; the six that had no panel-side path were added there.

| Old Settings editor | On the desktop now |
| --- | --- |
| Add an applet to a panel zone | Edit bar "Add applet…" per zone; panel menu Add applet ▸ |
| Move an applet within a zone, across zones and panels | Edit-mode drag (ADR-0266); applet menu Move to start/center/end, Move left/right, Move to panel ▸ |
| Remove an applet | Applet menu Remove |
| Duplicate an applet | Applet menu **Duplicate** (added) |
| Applet settings (switches, closed choices, bounded numbers) | Applet menu "‹applet› settings ▸" |
| Panel edge, alignment, length, visibility | Panel ▸ Edge, Alignment, Length, Auto-hide |
| Panel thickness 20–192 px | Panel ▸ Size, now 20–192 (was 16–160) |
| Panel on the primary display or all displays | Panel ▸ **Displays** ▸ This display only / All displays (added) |
| Add or remove the desktop icons | Desktop menu Desktop icons ▸ **Show/Hide desktop icons** (added) |
| Desktop icons' settings, icon size included | Desktop menu Desktop icons ▸ (icon size added) |
| Undo, redo | Panel menu Undo/Redo, edit bar Undo, desktop menu Undo |
| Apply/Discard a draft | Not needed: every panel edit is saved at once and undoable |
| Wallpaper and window previews | The live desktop is the preview |

Keyboard-only editing is the one gap: the old outline and Space move mode
have no panel-side equivalent yet. Panel menus are keyboard-navigable once
open, but opening them and focusing a panel still need the pointer.

## Keyboard and accessibility

Cards are focusable radio buttons named "‹name› layout preset" (", modified"
for an edited built-in) with the description as accessible description and
the current layout checked; Space and Enter switch. Card actions are icon
buttons whose tooltips are their accessible names ("Rename “Work”…"). Each
section is a named list. Errors are alert messages; confirmations and the
current layout are plain text. The name and confirmation dialogs are modal,
centred in the window overlay, focus the name field on open, and accept Enter.

## Boundary

The route is a presentation and composition boundary. It depends on the
public profiles values, loader and user store, and the Settings1 client. It
never imports shell surfaces, LayerShellQt, compositor code or a private
service implementation; `customize_route_composition.cpp` is the one source
that constructs the Settings1 D-Bus transport. The library still builds
`RepositoryCustomizeEditorHost`, which nothing in the page uses: it is the
independent reference composition the shell's live host is proven
byte-identical to ([Customization editor domain](../shell/customization-editor.md#live-host-in-the-shell)).
The page also keeps the confined `QindaQt.Shell.Icons` glyphs for its action
buttons ([ADR-0121](../adr/0121-real-iconography-in-the-settings-customize-route.md)).

The compiled `QindaQt.SettingsApp.Customize` module is linked into
`qindaqt-settings` and installed with its QML metadata and sources in the
`SettingsAppearanceRuntime` component, beside the profile catalog (and the
applet catalog, which the packaged route check still expects). Built and
installed executables discover the installed catalogs from their source or
relocated `share/qindaqt` without relying on a developer-tree working
directory.

## Verification

```sh
ctest --test-dir build/dev -R '^qindaqt\.settings-customize-' \
  --output-on-failure --no-tests=error
```

- `qindaqt.settings-customize-presets` runs the production model over a
  temporary installed catalog and user store and an in-process Settings1
  double: built-ins before own presets with their provenance, a confirmed
  switch through a stale readback, refusal and an unanswered commit reported
  and never replayed, Save current layout including a panel edit written
  after the page loaded, id derivation and collisions, every name rule and
  the 50-preset bound, rename/duplicate/delete of own presets only,
  Modified/Restore and Save as new preset, deleting the active preset
  (switch first, file removed only after confirmation, and kept when the
  switch fails), a missing selection that stays usable, and the default
  preset against the Settings1 schema.
- `qindaqt.settings-customize-page` (warning-fatal, offscreen) renders both
  sections, the text badges, each kind's actions, switching by pointer,
  Space and Enter, the name dialog's validation and store failure, delete
  and restore confirmation, busy and degraded states, and the auto-hide delay
  slider's final-intent commits.
- `qindaqt.settings-customize-window-lifecycle` constructs the page inside
  the real `Main.qml` in both responsive hosts and proves closing the window
  and leaving the route never prompt.
- `qindaqt.settings-customize-panel-delay` covers the delay's confirmed
  write, stale readback, refusal, owner loss and a preset switch around it.
- `qindaqt.settings-customize-boundary` (plus its poison row) scans every
  route source for non-public includes and shell, compositor or bus imports;
  `qindaqt.settings-customize-installed-route` proves the staged module,
  icons and catalogs through the Settings package harness and the route
  construction witness
  ([ADR-0250](../adr/0250-require-active-loader-witness-for-every-settings-route.md)).
- The panel side of the parity table is covered by
  `qindaqt.shell-live-customization-controller`,
  `qindaqt.live-customization-offscreen` and
  `qindaqt.desktop-surface-customize-menu`.

These tests use injected transports, temporary stores and offscreen
rendering. They do not contact a host session bus, compositor or hardware.

See [Layout profiles](../shell/layout-profiles.md),
[Customization editor domain](../shell/customization-editor.md),
[ADR-0122](../adr/0122-adopt-saved-layout-preferences-live.md) and
[ADR-0267](../adr/0267-settings-switches-layout-presets-and-editing-happens-on-the-panels.md).
