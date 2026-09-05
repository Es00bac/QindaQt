# Shell iconography

The shell's shared iconography lives in `src/shell/icons` (namespace
`QindaQt::Shell::Icons`, target `QindaQt::ShellIcons`, compiled QML module
`QindaQt.Shell.Icons 1.0`). It gives every shell surface one confined XDG
icon-theme resolution path so applets render real icons instead of text
placeholders. The durable decision is
[ADR-0072](../adr/0072-shell-iconography-confined-xdg-icon-themes.md). The
status-notifier tray keeps its own deliberately narrower locator/renderer in
`src/shell/status_notifier/icon`; this module generalizes those confinement
rules without modifying that module.

## Confinement contract

- All filesystem reach is through injected, ordered roots. Production
  composition uses `XDG_DATA_HOME/icons` then each `XDG_DATA_DIRS/icons`
  (resolved from an explicit environment snapshot, never read ambiently);
  `~/.icons` is never added unless explicitly injected, matching the XDG
  icon-theme specification's deprecation.
- Every candidate path — theme indexes and icon files alike — is
  canonicalized and must remain beneath its canonical injected root. `../`
  directory declarations and symlink escapes fail closed.
- Icon names are a bounded ASCII grammar (`[A-Za-z0-9._-]`, no `..`, at most
  128 UTF-8 bytes). Hostile names (`../`, absolute paths, embedded NUL,
  spaces, oversized) resolve to "unresolved".
- Bounded resources: a 1,024-byte provider request-id ceiling (longer ids
  are refused before parsing and never reach the cache, whose keys are the
  parsed, bounded request tuple — name, size, scale, color, symbolic — so
  retained key bytes stay bounded regardless of the request's spelling), a
  256 KiB index ceiling, 128 directories per index, 8 levels of `Inherits`
  recursion with a cycle guard, a 16-entry flattened theme chain, a 64-entry
  parsed-index cache, a 64-entry provider image LRU, a 256 KiB SVG source
  payload ceiling (`kMaxSvgSourceBytes`), a 64 KiB per-desktop-entry ceiling,
  and a 1,024-entry scan bound. Oversized or hostile inputs contribute
  nothing instead of growing shell memory.
- The module performs no network access and no filesystem writes, links no
  D-Bus, KWin, LayerShellQt, or shell-runtime targets, and reuses the
  launcher's public pure desktop-entry parser rather than reparsing
  documents itself.

## IconThemeLocator

`IconThemeLocator(iconRoots, themeNames)` implements the XDG Icon Theme
Specification lookup over the injected roots:

1. The theme chain is the injected names in order, each expanded by its
   `Inherits=` parents (cycle-guarded, depth-capped), with `hicolor` always
   last — when the 16-entry chain cap is already full, the deepest entry
   yields so the mandated fallback is never lost and the cap is never
   exceeded.
2. Within one theme, an exact `Size`/`Scale` match (Fixed), `Threshold`
   window, or `Scalable` MinSize/MaxSize match wins in root-then-declared
   order; otherwise the smallest specification distance wins with the same
   tie order. `Scale=` directories answer only matching requested scales in
   the exact pass; distances compare device pixels.
3. With `symbolic` requested, `<name>-symbolic` is tried through the whole
   chain before the plain name.
4. Unthemed `<root>/<name>.<ext>` hits (png, svg, xpm in fixed order) are
   the final fallback.

Lookups are deterministic: identical inputs and fixtures always return the
identical canonical path. An instance is confined to its owning thread; the
provider and the QML seam hold separate instances.

## DesktopEntryIconResolver

`DesktopEntryIconResolver(applicationRoots)` eagerly scans the injected
`applications/` roots (bounded depth 4, sorted relative paths within a root,
roots in injected order, first claim wins an id). Documents parse through
the launcher's public `DesktopEntryParser`. An entry contributes its `Icon=`
name only when it parses, is not hidden (`Hidden`/`NoDisplay`), and the value
is a plain icon name — absolute paths and traversal values are refused.
`iconNameForAppId` resolves a compositor app id by exact id, case-insensitive
id, then case-insensitive reverse-DNS tail (`org.kde.dolphin` answers
`dolphin`), with a trailing `.desktop` ignored. This is the seam the task
list will use to show window icons from the compositor's app id.

## IconImageProvider and the `image://qindaqt-icon/` URL scheme

`IconRuntime::install(QQmlEngine&, iconRoots, themeNames)` is the
composition seam the shell composition root will call. It installs the
provider (engine-owned) and the per-engine `IconLookup` QML singleton. A
second install on one engine is refused; without installation every lookup
fails closed to the placeholder.

The URL scheme is:

```
image://qindaqt-icon/<name>?size=<px>&scale=<f>&color=<#rrggbb>&symbolic=1
```

All query items are optional. `size` clamps to [1, 512] logical pixels
(default: 32 or the QML `sourceSize`), `scale` to [1, 4]. SVG icons render
through QSvgRenderer at the exact device size with a 256 KiB source payload
ceiling (`kMaxSvgSourceBytes`); raster sources are dimension-checked before
and after decode (2,048-pixel ceiling) and smoothly scaled. `color` applies
only with `symbolic=1` and only in the documented opaque `#rrggbb` form —
an alpha-carrying color (serialized `#aarrggbb`) is refused and the symbolic
icon keeps its own pixels: the symbolic SVG renders, then every painted
pixel's RGB is replaced by the token color while its alpha shape is
preserved. An id over 1,024 UTF-8 bytes is refused before parsing and before
any cache access, and the 64-entry image LRU is keyed on the parsed, bounded
request tuple rather than the raw id, so hostile spellings cannot grow shell
memory. An unresolved, refused, or undecodable name returns a deterministic
neutral placeholder image — never a null image and never a warning, so
`QT_FATAL_WARNINGS=1` consumers stay clean on hostile input.

## QML `Icon` element

`QindaQt.Shell.Icons 1.0` exports `Icon`:

| Property | Contract |
| --- | --- |
| `name` | XDG icon name; names outside the bounded grammar (over 128 bytes or outside `[A-Za-z0-9._-]`, or containing `..`) are treated as unresolved and never reach the URL layer |
| `size` | Logical pixel edge, clamped to [1, 512] |
| `color` | Recolor target for symbolic SVGs; default `transparent` means no recolor. Only fully opaque colors recolor — a semi-transparent color is not a recolor target and the symbolic icon keeps its own pixels |
| `symbolic` | Prefer `<name>-symbolic` and permit recoloring |
| `fallbackText` | Accessible name and placeholder glyph source |
| `resolved` (readonly) | The confined lookup result, via the `IconLookup` singleton |

A resolved name renders through the provider at the window's device pixel
ratio (clamped to [1, 4]). An unresolved name — including every name when no
`IconRuntime` is installed, as in previews — renders the typed placeholder:
the first letter of `fallbackText` on a rounded tile colored exclusively
from QST-1 tokens (`Tokens.bg.highest`, `Tokens.outline.divider`,
`Tokens.fg.muted`); no color is hard-coded. `Accessible.name` is
`fallbackText`, falling back to the icon `name`; the role is Graphic when
resolved and StaticText for the placeholder. Composition must publish a
complete QST-1 generation before creating `Icon` instances, exactly as for
`QindaQt.Controls`.

## Production wiring

The production shell and `qindaqt-shell-preview` install `IconRuntime` after
QST-1 publication and before constructing panel QML. Shell composition derives
icon and application roots from one explicit freedesktop data-root snapshot.
The theme loader validates and retains the selected document's optional
`iconTheme`; shell composition consumes that catalog value without a second
filesystem parse. Absent hints choose `breeze-dark` for dark canvases and
`breeze` for light canvases, while the locator always appends `hicolor`.
Invalid hints reject the catalog before icon installation.

Panel summaries use the compiled `Icon` element and preserve their prior
accessible names on the surrounding buttons. Task-list composition owns one
`DesktopEntryIconResolver` and projects its result from the compositor's
application id into each row, then checks the name through an
`IconThemeLocator` over the production roots. The `DesktopVirtual` component
stages the four first-party desktop entries under `share/applications`; its
sandbox exports `/opt/qindaqt/share:/usr/share` as `XDG_DATA_DIRS`, so staged
metadata and system Breeze icon roots participate in the same confined lookup.
Interactive evidence rejects a first-party task button whose `iconResolved`
flag is false. Missing, hostile, or uninstalled icons still render the typed
QST placeholder for non-qualified applications; presentation never substitutes
a text label in-panel.

## Focused tests

```sh
ctest --test-dir build/dev \
  -R '^qindaqt\.shell-icons-' --output-on-failure --no-tests=error
```

| Test | Scope |
| --- | --- |
| `qindaqt.shell-icons-locator` | Generated theme roots: exact/threshold/scalable matching, scale-aware directories, inherits chains with cycle guard and depth cap, hicolor-last ordering including when the chain cap is full, deterministic root order, `-symbolic` preference and fallback, unthemed root hits, hostile names, `../` and symlink-escape refusal, oversized-index refusal, index-cache bound. |
| `qindaqt.shell-icons-resolver` | Generated application roots: exact/nested id mapping, first-root precedence, app-id normalizations, hidden/NoDisplay/malformed/oversized/wrong-Type entries skipped, hostile `Icon=` values refused, symlink escape refused, empty and missing roots, deterministic rescan. |
| `qindaqt.shell-icons-provider` | Offscreen, fatal warnings: raster and SVG rendering at device size, symbolic recolor pixel assertions, placeholder determinism and non-emptiness, size/scale clamping, hostile URL ids, over-long-id refusal before cache access, canonical cache-key sharing, hostile-id flood cache-key-byte and RSS bounds, LRU cache bound. |
| `qindaqt.shell-icons-qml-offscreen` | The compiled `Icon` element through the real `IconRuntime` seam: resolved rendering, typed fallback glyph, accessible names, warning-free under `QT_FATAL_WARNINGS=1`. |
| `qindaqt.shell-icon-runtime-configuration` | Catalog-retained selected-theme hint, light/dark default, hostile-hint refusal, an oversized theme proving no second file-size/parser policy, schema-valid punctuation, and explicit XDG root ordering/fallback. |
| `qindaqt.shell-icon-coverage` | Complete-inventory guard requiring every literal built-in summary name to resolve in the pinned Breeze/Breeze-dark intersection fixture, plus an `iconTheme` field for every built-in theme. |

All rows run offscreen or headless with the host display and bus variables
unset; fixtures are generated beneath the build directory. No row contacts a
host bus, display, compositor, network, or hardware.

## Non-claims

This module does not render pixmap-payload icons (the status-notifier
renderer keeps that wire concern), and makes no nested-session or physical
display claims.
