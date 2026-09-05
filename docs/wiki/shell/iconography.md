# Shell iconography

The shell's shared iconography lives in `src/shell/icons` (namespace
`QindaQt::Shell::Icons`, target `QindaQt::ShellIcons`, compiled QML module
`QindaQt.Shell.Icons 1.0`). It gives every shell surface one confined XDG
icon-theme resolution path so applets render real icons instead of text
placeholders. The durable decision is
[ADR-0071](../adr/0071-shell-iconography-confined-xdg-icon-themes.md). The
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
- Bounded resources: 256 KiB index ceiling, 128 directories per index, 8
  levels of `Inherits` recursion with a cycle guard, a 16-entry flattened
  theme chain, a 64-entry parsed-index cache, a 64-entry provider image LRU,
  a 64 KiB per-desktop-entry ceiling, and a 1,024-entry scan bound. Oversized
  or hostile inputs contribute nothing instead of growing shell memory.
- The module performs no network access and no filesystem writes, links no
  D-Bus, KWin, LayerShellQt, or shell-runtime targets, and reuses the
  launcher's public pure desktop-entry parser rather than reparsing
  documents itself.

## IconThemeLocator

`IconThemeLocator(iconRoots, themeNames)` implements the XDG Icon Theme
Specification lookup over the injected roots:

1. The theme chain is the injected names in order, each expanded by its
   `Inherits=` parents (cycle-guarded, depth-capped), with `hicolor` always
   last.
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
through QSvgRenderer at the exact device size; raster sources are
dimension-checked before and after decode (2,048-pixel ceiling) and smoothly
scaled. `color` applies only with `symbolic=1` and only in the documented
`#rrggbb` form: the symbolic SVG renders, then every painted pixel's RGB is
replaced by the token color while its alpha shape is preserved. An
unresolved, refused, or undecodable name returns a deterministic neutral
placeholder image — never a null image and never a warning, so
`QT_FATAL_WARNINGS=1` consumers stay clean on hostile input.

## QML `Icon` element

`QindaQt.Shell.Icons 1.0` exports `Icon`:

| Property | Contract |
| --- | --- |
| `name` | XDG icon name |
| `size` | Logical pixel edge, clamped to [1, 512] |
| `color` | Recolor target for symbolic SVGs; default `transparent` means no recolor |
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

## Wiring plan (later lane)

A concurrent lane owns the shell runtime and applets. The wiring lane will:

1. Call `IconRuntime::install` from the shell composition root with roots
   from `IconRuntime::freedesktopIconRoots(dataHome, dataDirs)` and the
   application roots from `freedesktopApplicationRoots`.
2. Take the theme name from the shell theme JSON's optional `iconTheme` key
   (default `breeze`; `hicolor` is always appended last by the locator).
   That schema addition belongs to the wiring lane.
3. Replace applet label/letter placeholders with the `Icon` element and feed
   the task list through `DesktopEntryIconResolver::iconNameForAppId`.

## Focused tests

```sh
ctest --test-dir build/dev \
  -R '^qindaqt\.shell-icons-' --output-on-failure --no-tests=error
```

| Test | Scope |
| --- | --- |
| `qindaqt.shell-icons-locator` | Generated theme roots: exact/threshold/scalable matching, scale-aware directories, inherits chains with cycle guard and depth cap, hicolor-last ordering, deterministic root order, `-symbolic` preference and fallback, unthemed root hits, hostile names, `../` and symlink-escape refusal, oversized-index refusal, index-cache bound. |
| `qindaqt.shell-icons-resolver` | Generated application roots: exact/nested id mapping, first-root precedence, app-id normalizations, hidden/NoDisplay/malformed/oversized/wrong-Type entries skipped, hostile `Icon=` values refused, symlink escape refused, empty and missing roots, deterministic rescan. |
| `qindaqt.shell-icons-provider` | Offscreen, fatal warnings: raster and SVG rendering at device size, symbolic recolor pixel assertions, placeholder determinism and non-emptiness, size/scale clamping, hostile URL ids, LRU cache bound. |
| `qindaqt.shell-icons-qml-offscreen` | The compiled `Icon` element through the real `IconRuntime` seam: resolved rendering, typed fallback glyph, accessible names, warning-free under `QT_FATAL_WARNINGS=1`. |

All rows run offscreen or headless with the host display and bus variables
unset; fixtures are generated beneath the build directory. No row contacts a
host bus, display, compositor, network, or hardware.

## Non-claims

This lane does not wire icons into any applet, the shell runtime, or the
task list (a later lane owns those paths), does not add the theme JSON
`iconTheme` key, does not render pixmap-payload icons (the status-notifier
renderer keeps that wire concern), and makes no nested-session or physical
display claims.
