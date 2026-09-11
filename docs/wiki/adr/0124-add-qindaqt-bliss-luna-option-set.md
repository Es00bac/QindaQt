# ADR-0124: Add the QindaQt Bliss Luna option set

- **Status:** Accepted
- **Date:** 2026-09-10
- **Owners:** Themes, Compositor decorations, Shell presentation
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt's shipped identity is deliberately original: the Pearl, Velvet,
Smoked Plum, High Contrast, and macOS palettes ([ADR-0109](0109-use-pearl-and-smoked-plum-app-materials.md))
pair original colors with restrained chrome, and Qinda macOS is the only
theme that repositions window controls. Users keep asking for one more
reference point: the early-2000s Luna-era desktop — blue title bars, a
compact squared light palette, a bottom taskbar with a distinct start
button, and window buttons shaped like game-console glyphs. Reproducing
that feel with original code and assets is exactly what the stock profiles
already do for the GNOME, Unity, MATE, XFCE, NeXTSTEP, macOS, and Windows
workflow families.

The constraints are structural. Themes are data only
([Theme schema v1](../reference/theme-schema-v1.md)); window chrome is
projected from the selected theme through one palette map so ordinary and
grouped windows stay consistent
([ADR-0081](0081-project-confirmed-appearance-into-window-chrome.md)); and
the built-in contrast, catalog, and icon-coverage gates enumerate every
shipped theme. Any addition must therefore be additive schema data plus
opt-in rendering paths that leave the five existing themes byte-identical.

## Decision

1. **Additive option data; no default changes.** Theme schema v1 keeps its
   version and required fields. `decoration.buttonStyle` gains the enum
   value `glyph`, and three optional decoration colors are recognized:
   `titleBarColor`, `titleBarInactiveColor`, and `restoreColor`. Every new
   field is optional; when they are absent the decoration renders exactly
   as before, so QindaQt Pearl, Velvet, Smoked Plum, High Contrast, and
   macOS remain byte-identical. Nothing about the default theme pair, any
   existing stock profile, or any other default changes.
2. **The QindaQt Bliss theme** (`data/themes/qinda-bliss.json`) is a light
   variant with `fontFamily` Tahoma, `cornerRadius` 4, an XP-face canvas
   (`#ECE9D8`), white raised surfaces, and a Luna blue accent (`#245EDC`).
   Its decoration authors `buttonStyle: "glyph"`, the Luna title colors,
   and the glyph colors: `closeColor`, `minimizeColor`, and
   `maximizeColor` carry console-glyph colors, and `restoreColor` colors
   the maximized square.
3. **Worn Luna title bar.** When a theme authors `titleBarColor`, the
   compositor decoration paints a Luna gradient title bar with a white
   Trebuchet MS caption instead of the plain flat bar, and runs a
   procedural weathering pass (`paintWornLunaTitle`): rust undercoat chips
   along the edges, speckles, and drips. The weathering is deterministic —
   the seed is the caption hash plus the title-bar width, never focus — so
   repaints never flicker while every window weathers differently. An
   inactive window paints `titleBarInactiveColor`.
4. **Glyph buttons.** The `glyph` button style places right-positioned
   outline game-console glyphs: circle (`minimizeColor`), triangle
   (`maximizeColor`), square (`restoreColor`, shown when maximized), and
   cross (`closeColor`), stroked with a dash pattern that reads as chipped
   paint. The shell preview consumes the same decoration map, so ordinary
   and grouped windows keep one chrome truth.
5. **Panel and applet dressing are opt-in.** `PanelContent` derives a
   `lunaMode` from the panel id `bliss-taskbar` — the same derivation
   precedent as `dockMode` — selecting an opaque Luna gradient panel
   material with a gloss line; a copied or renamed panel drops the
   treatment with its id. The task-list, clock, and quick-launch manifests
   gain an optional `presentation` settings enum (`standard` or `luna`,
   default `standard`); Luna dressing — white captioned clock text, Luna
   gradient task buttons, hover tints — applies only to instances that opt
   in, so every existing profile's panels render exactly as before.
6. **The qinda-bliss profile** ships one bottom `bliss-taskbar` panel —
   start-menu, quick-launch (luna), task-list (luna), status-notifier,
   notification-center, clock (luna), and show-desktop — with
   `defaultTheme` `qinda-bliss`. It deliberately carries **no clipboard
   slot**: the XP taskbar it evokes has no utility chip there, an explicit
   exception to the stock convention of one clipboard slot beside the
   notification center. The stock-profile invariants assert one resolved
   clipboard per profile *except* qinda-bliss, and exactly one resolved
   menu slot that is either a launcher or the start-menu.
7. **Start Menu applet.** A new `qindaqt.applets.start-menu` built-in
   renders a green Luna start button and a two-column start panel popup —
   launcher sections left, places and system actions right — under the
   existing `applications.launch` capability and no new one.
8. **File-manager session default.** `SessionDefaults::ensure` seeds
   `mimeapps.list` with `inode/directory=org.qindaqt.FileManager.desktop`
   only when the user has no `inode/directory` default. It is a
   seed-missing default, never an override, and a failed seed is reported
   and never blocks session start; `xdg-open` and portal
   "open containing folder" flows then resolve to the QindaQt File
   Manager, whose desktop entry already declares the MIME type.
9. **Companion wallpaper.** `qinda-bliss.png` joins the bundled wallpaper
   catalog as the option set's paired artwork, an original rolling-hills
   homage generated for this change.

## Consequences

- Counts move from five to six themes, ten to eleven stock profiles, and
  twenty-four to twenty-six applet manifests; the built-in data gates
  (theme catalog, WCAG contrast, derivation benchmark, appearance preview
  inventory, shell icon coverage) cover the new inventories.
- The five original themes and every existing stock profile keep their
  exact previous rendering: the new decoration paths run only for authored
  data, the Luna material only for the `bliss-taskbar` id, and the Luna
  applet dressing only for `presentation: "luna"` instances.
- The worn-Luna painter is deterministic by construction; focus changes
  recolor the caption and buttons but never re-roll the wear pattern.
- The Bliss profile's desktop icons ride the separate desktop-zone hosting
  decision in [ADR-0125](0125-host-desktop-zone-applets.md).
