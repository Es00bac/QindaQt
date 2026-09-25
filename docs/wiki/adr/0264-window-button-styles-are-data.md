# ADR-0264: Window button styles are data, with title-bar options

- **Status:** Accepted
- **Date:** 2026-09-24
- **Owners:** Decorations, Compositor chrome, Settings Appearance
- **Supersedes:** None (extends [ADR-0127](0127-preview-window-chrome-and-toolkit-through-one-painter.md), [ADR-0129](0129-configure-window-and-container-chrome.md) and [ADR-0207](0207-decoration-themes-are-their-own-documents.md))
- **Superseded by:** None

## Context

Window buttons had four looks (`symbols`, `traffic-lights`, `glyph`, `flat`)
drawn by three hand-written painters, and containers drew theirs with a
fourth, simpler painter in the compositor's chrome renderer. The product owner
asked for about ten more looks, from glossy gel lights and raised bevels to
blue tiles, wide flat cells, a tab title and touch-sized buttons, without ten
more painters; for the console glyph set's close cross, drawn with a dashed
"worn" pen, to read as a cross again (keeping the triangle for maximize and
the square for restore); and for title-bar options on windows and containers:
button size and spacing, title-bar height, corner radius, title weight, the
application icon, a roll-up button, and what a title double-click does.

Two facts shaped the design. KWin 6.6 has no native window shade: its
`Window` class and window operations no longer know shading, so the only
roll-up an ordinary window has is the compositor's roll-up to its icon
([ADR-0203](0203-an-ordinary-window-rolls-up-to-its-icon.md)). And KWin runs
its own title double-click (`TitlebarDoubleClickCommand`, maximize by default)
unless the decoration accepts the press.

## Decision

1. **A style is a row of data.** `DecorationButtonStyle`
   (`decoration_button_style.h`) describes a plate shape (none, circle,
   rounded square, square, pill, dot), a fill (solid, gradient, glass,
   outline, bevel), where the plate color comes from (action colors, the
   title surface, or the style's own faces), a hover treatment (tint, plate,
   halo), a glyph family (classic, lines, console) and ink, glyph reveal,
   cell size, aspect, spacing, edge inset, the side the style prefers, an
   optional own title height, and whether the title is a tab. One painter,
   `paintStyledButton`, draws every row. The shipped styles are rows too and
   reproduce their old pixels; a frozen copy of the old painters in the tests
   pins that. Fifteen names exist: the four shipped ones plus `gel`, `bevel`,
   `blue-tiles`, `wide`, `tab`, `bold`, `minimal`, `pills`, `dots`,
   `outline` and `chunky`. All artwork is drawn from shapes and our own
   colors; the Windows-like looks (`blue-tiles`, `wide`) use their own
   palette, shapes and glyphs, and no style name is another vendor's mark.
2. **The glyph set's strokes are solid.** The dashed pen is gone from the
   console glyphs; the triangle still means maximize and the square restore.
3. **One name list.** `Themes::DecorationThemeTokens::buttonStyles()` is the
   list the theme and decoration-document loaders validate, so themes and
   documents may name any style. The painter keeps one row per name (tests
   pin the match), and an unknown name still paints the traffic lights.
4. **Containers use the same painter through their style.**
   `HybridChrome::ChromeStyle` gains a named style, a `ChromeButtonPainter`,
   cell and gap overrides, and a title double-click action. The decoration
   painter supplies `paintContainerButton` as that painter, so hybrid chrome
   still does not depend on the decoration painter and a container button
   matches a window button. The Settings preview's container map carries the
   style by name and rebinds the painter on read. Pre-W19 names a theme
   authors keep the container plates they always had; the user's choice of
   any style other than lights and flat, and any W19 name, paints named
   buttons.
5. **Eleven option keys join the arrangement scope** (schema v2, additive):

   | Key | Tokens (default first) |
   | --- | --- |
   | `appearance.windowButtonSize`, `appearance.containerButtonSize` | `theme`, `small`, `large` |
   | `appearance.windowButtonSpacing`, `appearance.containerButtonSpacing` | `theme`, `tight`, `roomy` |
   | `appearance.windowTitleHeight` | `theme`, `compact`, `tall` |
   | `appearance.windowCornerRadius` | `theme`, `square`, `small`, `large` |
   | `appearance.windowTitleWeight` | `theme`, `regular`, `bold` |
   | `appearance.windowAppIcon`, `appearance.windowRollUpButton` | `hidden`, `shown` |
   | `appearance.windowTitleDoubleClick` | `theme`, `maximize`, `roll-up`, `minimize` |
   | `appearance.containerTitleDoubleClick` | `none`, `maximize`, `roll-up`, `minimize` |

   Both button-style keys accept every style but `symbols` (which windows
   paint exactly like the lights). The keys are read like the ADR-0129 keys:
   `ChromePreferences` tokens, the compositor's arrangement client, the
   Appearance route's scope. Window options resolve into `DecorationChrome`
   (published to the decoration and omitted at their defaults); container
   options resolve into `ChromeStyle`. A container bar's height is part of
   the container layout contract (placement, shade strip, solver), so
   containers get no height option; their tab labels are not a caption, so
   no title weight or icon either; and every container already has a roll-up
   control.
6. **Existing actions only.** The roll-up button and a roll-up double-click
   on a window emit `QindaDecoration::qindaqtRollUpRequested()` from the
   event loop; `KWinChromeAppearance` relays it by window id and the plugin
   calls `KWinHybridSession::iconifyWindow`, the ADR-0203 roll-up. Maximize
   and minimize are KDecoration's `requestToggleMaximization` and
   `requestMinimize`. The decoration runs a chosen double-click itself and
   accepts the second press, so KWin neither repeats its own command nor
   starts a move; `theme` leaves KWin's command in charge. On a container,
   the pointer router reports a double-click on the unshaded title row, and
   the session runs the container's own maximize/restore or minimize window
   action or its wheel roll-up; `none` keeps the row inert, as it shipped.

## Consequences

- Every default reproduces the shipped chrome: untouched preferences publish
  a byte-identical decoration map and container style, and the shipped styles
  paint their old pixels (apart from the solid glyph strokes).
- A new look is a new row plus its name in the shared lists; the Appearance
  menu, the two `ChromePreferences` token lists and `schema-v2.json` must
  agree, which `qindaqt.appearance-values` checks against the schema.
- The eleven keys ride the arrangement client (ADR-0129): an older resident
  Settings1 service without them rejects the whole arrangement snapshot, so
  after installation the service must restart with the new schema, as for the
  original eight keys.
- The tab style fills only its tab; the strip beside it is transparent but
  still belongs to the window for input, and the shadow still follows the
  full frame.
- Container buttons of a named style are drawn by a painter the style
  carries; a `ChromeStyle` built by hand without one keeps the built-in
  plates.
- `decoration_painter.cpp` and `qindadecoration.cpp` were split (the worn
  Luna texture and the decoration's pointer input moved to their own files)
  and the container half of `chrome_preferences.cpp` moved to
  `container_chrome_style.cpp`, keeping each file under the review size.

## Verification

- `qindaqt.decoration-button-styles`: the shipped styles against the frozen
  old painters in 32 states at 1x and 2x; solid console strokes and the
  triangle/square meanings; every style at 1x and 2x on light and dark
  chrome, deterministic, visible and hover-responsive; the tab title; the
  application icon and title weight; containers painting through the named
  style, including from the preview's map.
- `qindaqt.decoration-title-options`: name lists, strict tokens and round
  trips, option resolution and the decoration map, style metrics and option
  layout, roll-up placement and its absence from handlebars, container style
  resolution.
- `qindaqt.decoration-painter` (key count), `qindaqt.theme-formats` and
  `qindaqt.decoration-theme-formats` (every style name loads),
  `hybrid-chrome.layout` and `hybrid-chrome.renderer` (cell and gap
  overrides, the painter hook), `compositor.hybrid-chrome-pointer-router` (title-row double-click),
  `qindaqt.appearance-values` (schema agreement, strict decode) and
  `qindaqt.appearance-window-decoration-page` (menus and option rows drive
  the draft).
- Not covered by unit tests: the live KWin double-click and roll-up relay,
  which need a nested session.

## Revisit when

KWin regains native shading, or a theme wants to author the title-bar
options itself (they are preferences today; decoration documents do not carry
them).

[ADR-0268](0268-familiar-desktop-experiences-are-layout-and-theme-pairs.md)
lets a theme author the title double-click, a minimize that rolls up, and a
clean title bar; `theme` for `appearance.windowTitleDoubleClick` now means the
theme's action, else KWin's.
