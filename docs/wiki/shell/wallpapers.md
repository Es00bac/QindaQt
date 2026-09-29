# Wallpapers

The shell paints one noninteractive background surface per output
([ADR-0078](../adr/0078-own-wallpaper-surfaces-in-the-shell.md)). Each surface
chooses its own picture from the confirmed Settings1 preferences, so different
displays and virtual desktops can show different wallpapers
([ADR-0286](../adr/0286-per-display-and-per-desktop-wallpapers.md)).
[Appearance Settings](../apps/appearance-settings.md) edits the choices; the
[visual identity](visual-identity.md) page describes the bundled artwork.

## Preferences

| Key | Meaning |
| --- | --- |
| `appearance.wallpaper` | The everywhere wallpaper: `qindaqt:<name>`, an absolute path, or empty for no wallpaper. Every output shows it unless a more specific choice applies. |
| `appearance.wallpaperMode` | `scaled`, `centered` or `tiled`, for every output. |
| `appearance.wallpaperAssignments` | Per-display and per-desktop choices (object, default `{}`). |

The choices value is `{"version": 1, "assignments": [...]}` with records
`{"display", "desktop", "wallpaper"}`. `display` is an
[ADR-0017](../adr/0017-persistent-output-identity.md) stable output id or empty
for every display; `desktop` is a compositor virtual-desktop id or empty for
every desktop; a record never has both empty. `wallpaper` uses the forms of
`appearance.wallpaper`, and empty means no wallpaper for that scope. Limits: 64
records, 128-byte display ids, 128-unit desktop ids, 2,048-byte wallpaper
values, one record per scope. `QindaQt::Services::WallpaperAssignments`
(`src/services/wallpaper_assignments`) is the only codec; it rejects any other
shape as a whole and encodes no choices as `{}`.

## Which picture an output shows

For an output whose stable id is D on the current desktop K:

1. the choice for D on K;
2. else the choice for D on every desktop;
3. else the choice for K on every display;
4. else `appearance.wallpaper`.

A display's own choice outranks a desktop-wide one, so a portrait monitor keeps
its picture when a desktop has a landscape one. When D or K is unknown — Display1
has not answered, the display is an ambiguous ADR-0017 twin, the display was
replaced, or the desktop was removed — that level is skipped. Nothing is ever
deleted: a saved choice applies again when its display or desktop returns.
The Appearance preview uses the same resolver, so it shows what the desktop
paints.

## Shell runtime

`WallpaperController` keeps one background window per `QScreen` and resolves
each window's source through a borrowed `WallpaperSelectionSource`:

- **Choices** come from a purpose-scoped Settings1 client for
  `appearance.wallpaperAssignments` alone. A Settings1 peer without the key
  (for example mid-upgrade) costs only per-display pictures, never the shared
  shell preference scope. An unreadable value is ignored with one warning.
- **Display identity** comes from a read-only public Display1 client. The
  join is Display1's connector name to `QScreen::name()`, the compositor output
  name the notification output authority also relies on. Ambiguous twins are
  not addressable.
- **The current desktop** comes from the desktop controls' workspace
  controller, borrowed rather than a second compositor binding. Without a
  workspace applet granted `windows.read`, desktop scopes are not applied.

A surface starts with the everywhere wallpaper and never waits for a service.
When its source changes, the new image loads on a second layer and fades in
over the old one for the theme's `motionDuration`; the fade starts only once
the image has loaded or failed, so the fallback color never flashes. With
`accessibility.reducedMotion` the swap is instant. Both layers sit under one
item, so the shortcut note card ([ADR-0084](../adr/0084-own-the-desktop-shortcut-note-on-wallpaper-surfaces.md))
stays on top.

## Bundled artwork

Bundled wallpapers are `data/wallpapers/<name>.png`, installed to
`qindaqt/wallpapers` in the data directories and named in the chooser from the
file name. Every image has a `### <name>` section in `data/wallpapers/ARTWORK.md`
with its provenance. The owner collection (Horizon Arc, Light Waves, Valley
Sunrise, Neon Harbor, Aurora Plain) was added beside the original six without
changing them or the default selection.

`tools/import-wallpapers LIST.json` adds more. The list is a JSON array of
`{"source", "title", "description", "credit"}` objects (credit defaults to
"Jarrod C, AI-assisted"). The tool:

- accepts only landscape images at least 1,536 px wide;
- skips an image whose content matches a bundled file, a recorded source hash,
  or an earlier entry;
- refuses to replace an existing bundled name (the name is the title's slug);
- copies an image that is already an 8-bit RGB 1672 × 941 PNG byte for byte,
  and otherwise converts it to that format with a centered crop;
- appends a credit section with the title, credit and source SHA-256.

`--dry-run` reports the plan; `--check` validates the bundle without an image
library. Choosing which images may ship remains a provenance decision for the
owner: third-party packs and images of unclear origin stay out until cleared.

## Tests

- `qindaqt.services-wallpaper-assignments`: precedence, fallbacks, edits,
  bounds, the strict codec, the Settings1 size bound, the schema key.
- `qindaqt.shell-wallpaper-controller`: Settings1 integration on a private bus,
  and per-display/per-desktop switching, fallbacks and cross-fade timing with a
  fake selection source.
- `qindaqt.appearance-values`, `qindaqt.appearance-settings-model`: tolerant
  route decode, scoped draft edits, Apply order, Revert, unreadable values.
- `qindaqt.appearance-wallpaper-scopes-page`: the scope picker, where picks
  land, saved-choice removal, accessible names, unplugged-selection fallback.
- `qindaqt.wallpapers-bundle` and `qindaqt.dev-harness`: the bundle check and
  the import tool.
