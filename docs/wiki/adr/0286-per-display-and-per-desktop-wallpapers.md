# ADR-0286: Per-display and per-desktop wallpapers

- **Status:** Accepted
- **Date:** 2026-09-28
- **Owners:** Shell (wallpaper surfaces), Appearance Settings route, Settings
  service (schema)
- **Supersedes:** None (extends [ADR-0078](0078-own-wallpaper-surfaces-in-the-shell.md);
  [ADR-0228](0228-bundled-wallpapers-resolve-beyond-png.md) and
  [ADR-0279](0279-keep-custom-wallpapers-in-a-user-folder.md) are unchanged)
- **Superseded by:** None

## Context

ADR-0078 paints one confirmed `appearance.wallpaper` on every output. The owner
asked for "a way to set different wallpapers for different displays/desktops",
and for the wallpapers they made themselves to become bundled defaults.

A display must keep its picture across docks, port changes and unplugging, so
it cannot be keyed by connector name or by KWin's runtime output handle.
[ADR-0017](0017-persistent-output-identity.md) already derives a
privacy-preserving stable output id that Display1 publishes. Virtual desktops
have compositor ids (KWin UUIDs) that the shell's workspace module reads.
Settings1 commits one key per transaction, and a scoped snapshot fails as a
whole when it names a key the service does not know.

Options considered:

- **Keys per display** (`appearance.wallpaper.<id>`): rejected; Settings1 keys
  are schema-declared, not dynamic.
- **Replace `appearance.wallpaper` with a structured value**: rejected; every
  saved single wallpaper and every peer that reads it would need a migration.
- **Connector names**: rejected; they change across docks and ports.
- **One additive object key beside the existing wallpaper**: chosen, following
  ADR-0280's additive-key rule and ADR-0265's one-structured-value pattern.

## Decision

1. **`appearance.wallpaperAssignments`** (schema v2, `object`, default `{}`)
   holds `{"version": 1, "assignments": [{"display", "desktop", "wallpaper"}]}`.
   `display` is an ADR-0017 stable id or empty for every display; `desktop` is
   a compositor desktop id or empty for every desktop; both empty is not
   allowed (that is `appearance.wallpaper`). `wallpaper` takes the forms of
   `appearance.wallpaper`: `qindaqt:<name>`, an absolute path, or empty for no
   wallpaper there. At most 64 records, ids of 128 bytes/units, wallpaper
   values of 2,048 UTF-8 bytes (so the largest value fits Settings1's aggregate
   bound), one record per scope, canonical order by display then desktop. An
   unknown field, another version, a duplicate scope or any bound violation
   rejects the whole value. No choices is stored as `{}`.
2. **One codec and one precedence.** `QindaQt::Services::WallpaperAssignments`
   (`src/services/wallpaper_assignments`, Qt Core only) is the only reader and
   writer of the shape and owns the rule for one output: the display-and-desktop
   choice, then the display's choice, then the desktop's choice on every
   display, then `appearance.wallpaper`. A display's own picture outranks a
   desktop-wide one so a portrait or odd-sized monitor keeps its image. An
   explicit empty choice means no wallpaper, not "inherit".
3. **Fallbacks never delete.** An unknown or unplugged display, an ambiguous
   ADR-0017 twin, an unknown or removed desktop, or a service that has not
   answered simply skips that level. Saved choices stay stored and apply again
   when the display or desktop returns. Nothing in the shell writes.
4. **Shell.** Each background surface (ADR-0078) resolves its own picture. A
   `SessionWallpaperSelection` owns a purpose-scoped Settings1 client for the
   new key alone (a Settings1 peer without the key costs only per-display
   pictures, never the shared shell scope), a read-only public Display1 client
   that joins `QScreen::name()` to stable ids through Display1's connector name,
   and borrows the desktop controls' workspace truth for the current desktop.
   A changed picture cross-fades over the theme's `motionDuration`; with
   `accessibility.reducedMotion` it swaps instantly. The fade starts only after
   the new image has an outcome, and "no wallpaper" fades like a picture.
   Startup never waits for Display1 or the compositor: outputs show the
   everywhere wallpaper until their scope is known.
5. **Appearance Settings.** The Wallpaper destination picks a scope — all
   displays or one display from a miniature of the arrangement, and all
   desktops or one desktop — then the gallery, **No wallpaper** and **Add
   image…** edit that scope in the one route draft. **Stop using a separate
   wallpaper here** removes a scope's choice; a list shows every saved choice,
   naming displays that are not connected and desktops that no longer exist,
   each with **Remove**. Apply writes `appearance.wallpaper` before the new key
   (scoped-key order), Revert restores both. An unreadable confirmed value keeps
   the route usable, is reported, and is replaced by the next explicit choice;
   unrelated Apply never touches it. The route reads displays from the public
   Display1 client the Settings process already runs and desktop names from a
   read-only workspace controller its composition root builds when the shell's
   workspace module is part of the build.
6. **Bundled defaults.** The owner's five penguin-and-duck images (Horizon
   Arc, Light Waves, Valley Sunrise, Neon Harbor, Aurora Plain; 1672 × 941
   PNG, credited "Jarrod C, AI-assisted") join the six existing wallpapers,
   which are unchanged, as is the default selection. `tools/import-wallpapers`
   takes a JSON list, accepts only landscape images at least 1,536 px wide,
   skips content duplicates, never replaces a bundled name, copies images that
   already have the bundled format byte for byte, converts others, and records
   title, credit and source SHA-256 in `data/wallpapers/ARTWORK.md`; `--check`
   validates the bundle.

## Consequences

- Existing single wallpapers keep working with no migration or write; the
  default layer and all older readers are unchanged.
- The shell gains a read-only Display1 client and the Settings executable a
  read-only workspace controller; neither can mutate displays or desktops.
  [Module boundaries](../architecture/module-boundaries.md) records both.
- The join relies on Display1's connector name equalling the compositor output
  name Qt reports as `QScreen::name()`, the same assumption the notification
  output authority makes. Live multi-monitor confirmation is still required.
- Per-desktop pictures need workspace truth: with no workspace applet granted
  `windows.read`, or a build without the shell, desktop scopes are simply not
  offered or applied, and display scopes still work.
- The fit mode stays global (`appearance.wallpaperMode`); a per-scope fit
  would be an additive field in a new format version.
- The bundle grows by 10,735,378 bytes (22,593,707 bytes of artwork in total).
- Tests: `qindaqt.services-wallpaper-assignments`,
  `qindaqt.shell-wallpaper-controller`, `qindaqt.appearance-settings-model`,
  `qindaqt.appearance-values`, `qindaqt.appearance-wallpaper-scopes-page`,
  `qindaqt.wallpapers-bundle`, and `qindaqt.dev-harness`. See
  [Wallpapers](../shell/wallpapers.md).

## Revisit when

Revisit if KWin gains per-output virtual desktops, if Display1 stops
publishing connector names, if per-scope fit or slideshow rotation is wanted,
or if a second consumer needs the owner-collection import outside the source
tree.
