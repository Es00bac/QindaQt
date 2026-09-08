# QindaQt icon theme

`data/icons/QindaQt` is QindaQt's source-controlled freedesktop icon theme.
It is an asset catalog only: icon lookup remains owned by the confined shell
icon runtime described in [Shell iconography](iconography.md). The theme does
not add a runtime dependency, alter icon-root ordering, or bypass the XDG
resolver's mandatory `hicolor` fallback.

## Contract

- The public theme name is `QindaQt`; `index.theme` declares scalable app,
  action, device, place, mimetype, category, and status directories, each with
  a `Context=` entry (`Applications`, `Actions`, `Devices`, `Places`,
  `MimeTypes`, `Categories`, `Status`), and inherits `hicolor`. QindaQt never
  ships or installs its own `hicolor` index; the inherited theme is always the
  system's, so `hicolor` fallback stays whatever the host provides.
- Every asset is SVG with a `64 × 64` view box and is suitable for the
  resolver's scalable size range. Plain names supply the color app
  treatment; matching `-symbolic` names supply real silhouettes (stroked
  outlines or `evenodd` cutouts, never a same-alpha shape painted over another
  to fake a hole) for the runtime's token recolor path, which replaces every
  opaque pixel's RGB and keeps its alpha (`source-in`).
- The catalog is an explicit, closed name table: every canonical icon has its
  own hand-authored geometry, and a name resolves only if it is a canonical
  entry or a declared alias of one with genuinely equivalent semantics (a
  battery-level or wifi-signal count is real data driving shared geometry, not
  a fallback). A name outside the catalog is a defect to fix there, not a case
  to special-case in the generator; the generator refuses to draw a
  placeholder or group-level fallback shape for it. A third-party name outside
  the catalog entirely follows the existing resolver chain to `hicolor` and
  system themes; it is never mapped to a pretend brand icon.

## Visual language

The palette is the icon-side spelling of the Pearl / Smoked Plum identity in
[Visual identity](visual-identity.md): smoked plum (`#211D27`), pearl
(`#FAF5F0`), and apricot (`#EAB391`), joined by heather (`#9C86AA`), violet
(`#B9A2CC`), jade (`#70BFA5`), and apricot (`#E8AE84`).

| Color | Icon role |
| --- | --- |
| Heather (legacy `BLUE` generator constant) | Body of every first-party application and shell built-in in the `apps` group (launcher Q, Settings dial, File Manager folder, clock, notifications bell, executable), the `preferences-system-*` family, and neutral action frames; the palette samplers (application grid, theme dots, color dial) show amber, blue, violet, and apricot only |
| Apricot (legacy `AMBER` generator constant) | At most one accent per first-party icon, placed where it meets the surface rather than on top of a blue body: the launcher Q tail, the Settings dial ticks, the Text Editor pen, the Terminal prompt, and the Welcome (`help-about`) question mark |
| Porcelain | Light bodies (Text Editor page) and marks painted on a blue or ink body |
| Jade | Semantic success or positive state only: battery charge, `emblem-ok`/`dialog-ok`, `security-high`, upload/update arrows, media glyphs; never a brand or application color |
| Apricot | Warning, off, and destructive overlays (`dialog-warning`, `dialog-error`, offline slashes, muted audio), place folders, and small status badges |
| Violet | Audio, input, and storage devices, sleep, and media MIME types |

Keep small on-body marks pearl or ink so they remain recognizable without hue. Application and
category icons use rounded forms with restrained layering. Actions, status,
devices, and places retain clear silhouettes so they remain recognizable after
symbolic recoloring on both light and dark surfaces.

## Maintenance and validation

The catalog is split by module so each stays small and reviewable:

| Module | Owns |
| --- | --- |
| `tools/qinda_icon_shapes.py` | Palette constants, the `Icon` type, stroke/fill/ring/rect primitives, and paths shared verbatim by multiple icons (bell, folder, trash, bluetooth, document) |
| `tools/qinda_icon_catalog_apps.py` | Launcher, first-party app, and `preferences-*` glyphs |
| `tools/qinda_icon_catalog_actions.py` | Edits, window controls, navigation, dialogs, session actions |
| `tools/qinda_icon_catalog_devices.py` | Audio, battery, input, storage, and network-radio devices |
| `tools/qinda_icon_catalog_places.py` | Folders, trash, hosts |
| `tools/qinda_icon_catalog_mimetypes.py` | MIME-type document glyphs |
| `tools/qinda_icon_catalog_categories_status.py` | Application categories and status/emblem icons |
| `tools/qinda_icon_catalog.py` | Merges the modules above into one `CANON`/`ALIASES` table; raises at import time on a duplicate or dangling alias |
| `tools/generate_qinda_icon_theme.py` | Writes the catalog and `index.theme` from that table |
| `tools/validate_qinda_icon_theme.py` | Structural and semantic checks (below) |

When adding a name, give it its own geometry in the module for its semantic
group; only declare it an alias of an existing canonical icon when the two
names mean the exact same object. Add the plain and symbolic forms together.

Validation requires Python 3 with Pillow and the `rsvg-convert` executable;
the XML command below also requires `xmllint`. These are artwork verification
tools, not dependencies of the installed icon theme.

Validate a checkout without building the desktop:

```sh
python3 tools/generate_qinda_icon_theme.py
python3 tools/validate_qinda_icon_theme.py
find data/icons/QindaQt -name '*.svg' -print0 | xargs -0 -n1 xmllint --noout
```

`validate_qinda_icon_theme.py` checks the freedesktop contract (index fields,
per-directory `Context=`), that every catalog name has both SVG forms on disk
and nothing stray is left over, that every SVG is well-formed with the
`64 × 64` view box, that the literal built-in names the shell icon-coverage
audit pinned are still present, that no two *canonical* (non-alias) icons
render identical artwork -- the check that would have caught the withdrawn
candidate's generic per-group fallback shapes -- and that the palette still
matches the packaged identity: ink, porcelain, and amber equal the
`qinda-dark` canvas and accent and `qinda-light` surface roles in
`data/themes`, and no `apps` color artwork uses jade.

Render representative SVGs with `rsvg-convert` for visual review. Generated
contact sheets are local review artifacts and are not committed.

Palette and wallpaper guidance live in [Visual identity](visual-identity.md).

## Application consumption

Controls supplies `QindaQt::Controls::applicationIcon(name)` and `Qinda.Icon`
(`name`, optional `color`, item width/height). Lookup is GUI-thread confined;
returned QIcon values own their engine. Invalid/path-like names and unknown
names return null. Qt's selected icon theme is consulted with an embedded
catalog fallback, so an app does not import shell private headers or rely on
ambient icon installation. A valid QML color preserves alpha and tints the
silhouette. Symbolic names should be used for command glyphs. Composition
selects the validated theme's iconTheme through public Qt APIs.
