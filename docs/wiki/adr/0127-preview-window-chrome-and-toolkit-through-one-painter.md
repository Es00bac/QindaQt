# ADR-0127: Preview window chrome and the Qt toolkit through one painter

- **Status:** Accepted
- **Date:** 2026-09-11
- **Owners:** Decorations, Settings Appearance, Controls
- **Supersedes:** None
- **Superseded by:** None

## Context

The Appearance route let a user pick a theme without ever showing what a
window would look like. Title bars, buttons, and frame were painted only by
the KDecoration plugin, whose painting code was bound to a live
`DecoratedWindow`; the palette ordinary Qt applications receive (ADR-0115)
appeared only as a row of swatches; and nothing rendered the Fusion style
those applications actually use. Every other desktop environment previews
these three things together, and the product owner asked for exactly that:
window decorations, colors, and Qt styles previewed as they will look.

Two further findings shaped the decision. First, the decoration already
follows theme changes live: the compositor publishes the flattened chrome
map to every decoration and the plugin repaints, so "apply" was never the
problem, only visibility. Second, the destination switcher on the route was
three `Button`s reading as buttons, and the route explained itself in
paragraphs; the owner wants glyphs, previews, and tooltips instead of a page
that reads like a book.

## Decision

1. **One renderer for window chrome.** `src/decoration_painter` is a
   KDecoration-free static library (`QindaQt::DecorationPainter`) holding
   `DecorationChrome`, the theme → chrome derivation the compositor used to
   own, button layout, color rules, and the title, frame, button, and
   caption painters. The KDecoration plugin gathers live window state and
   calls these functions; the compositor's `decorationPaletteProperties` is
   `DecorationChrome::toVariantMap()`; `qindaqt_decoration_visuals` keeps
   only the nine-patch shadow. The painter is the contract: a preview cannot
   drift from what the compositor draws because both call the same code.
2. **A preview window in the Appearance route.** `AppearanceWindowPreview`
   (a `QQuickPaintedItem` in `QindaQt.SettingsApp.Appearance`) paints the
   previewed theme's canvas, an inactive window behind an active one, both
   through the shared painter, and the active client area through the real
   Fusion `QStyle` with the palette from `nativeAppearance`. The route model
   publishes `previewChrome`, `previewToolkitPalette`, `previewToolkitFont`,
   and `previewCanvasColor` for the draft's resolved theme, so a theme card,
   scheme, or font change shows before Apply. Settings therefore hosts the
   widgets application class; no widget window is ever created, and the
   item degrades to a flat palette rendering without one (headless tests).
3. **Tabs are tabs.** `QindaQt.Controls` gains `TabBar`, `TabButton`, and
   `ToolTip`. The route's Themes, Wallpaper, and Fonts destinations are a
   tab strip with glyphs from the confined icon provider, an accent
   indicator on a shared rule, and tooltips carrying the explanation. The
   page's purpose sentence moves to the heading's accessible description,
   the color-scheme helper becomes a tooltip, and the swatch card shrinks to
   a labeled row; the sidebar shows each route's glyph and explains it on
   hover.

## Consequences

- Choosing a theme shows the decoration and the toolkit as they will look,
  including the worn Luna chrome and glyph buttons of the Bliss set.
- The decoration plugin's paint path is now testable without KWin:
  `qindaqt.decoration-painter` pins the chrome round trip, the button
  layout, focus and authoring color rules, and deterministic rendering.
- `qindaqt_settings_appearance_qml` links Qt Widgets and the painter; it
  still links no route model, so the shared module carries no second copy
  of settings code. The painter's `POSITION_INDEPENDENT_CODE` allows the
  shared-module link.
- The Controls source policy counts nineteen public files. The gallery and
  its visual baselines are unchanged; tab semantics are covered by the
  behavior scene.
- Title height and corner radius remain the painter's constants (24 and
  10) rather than `theme.cornerRadius`; honoring the theme radius in the
  live decoration is a separate change and would then flow to the preview
  automatically.
