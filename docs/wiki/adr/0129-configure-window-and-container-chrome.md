# ADR-0129: Configure window and container chrome through Appearance

- **Status:** Accepted
- **Date:** 2026-09-11
- **Owners:** Decorations, Compositor chrome, Settings Appearance
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt draws two sets of window chrome. Application windows get title bars,
frames, and buttons from the KDecoration plugin. Grouped windows additionally
get container chrome from the compositor: a shared bar with tabs, window
buttons, and container controls around member windows that keep their own
native title bars. The product owner asked for both sets to be acknowledged,
previewed, and configurable from Appearance.

Neither set was configurable. Worse, container chrome ignored the theme
entirely: `KWinHybridSession` built every container with
`ChromeStyle::qindaMacOS`, so the Bliss theme painted Luna glyph buttons on the
right of every window and macOS traffic lights on the left of every container.
The theme schema already carried `buttonPlacement`, `tabDirection`, and
`hoverGlyphs`, but nothing consumed them.

## Decision

1. **Eight arrangement keys, each defaulting to the shipped chrome.** Settings
   schema v2 gains, in the `appearance` domain:

   | Key | Tokens (default first) |
   | --- | --- |
   | `appearance.windowButtonStyle` | `theme`, `traffic-lights`, `flat`, `glyph` |
   | `appearance.windowButtonSide` | `theme`, `left`, `right` |
   | `appearance.windowButtons` | `all`, `minimize-close`, `close` |
   | `appearance.windowTitleAlignment` | `center`, `left` |
   | `appearance.containerButtonStyle` | `theme`, `traffic-lights`, `flat` |
   | `appearance.containerButtonSide` | `theme`, `left`, `right` |
   | `appearance.containerTabOrder` | `theme`, `left-to-right`, `right-to-left` |
   | `appearance.containerButtonGlyphs` | `theme`, `always`, `hover` |

   `theme` defers to the resolved theme. The schema version stays 2 because
   the change is additive, as in ADR-0028.
2. **Resolution lives in the shared painter.** `QindaQt::DecorationPainter`
   (ADR-0127) gains `ChromePreferences`, `resolveWindowChrome`, and
   `resolveContainerStyle`. The compositor publishes their results to
   decorations and container scenes, and the Settings preview paints the same
   results, so a choice cannot look different in the preview than on the
   desktop.
3. **Window arrangement.** The published decoration map carries `buttonSide`,
   `buttons`, and `titleAlignment` only when they differ from the defaults,
   so untouched chrome publishes the same map as before. Without an explicit
   side, glyph and flat buttons sit on the right and every other style on
   the left, which is exactly what every shipped theme rendered. `flat` is a
   new button style: monochrome symbols in the caption ink, always visible,
   with a hover plate, and the close action's own color on hover. The
   decoration plugin rebuilds its button group whenever the side, style, or
   visible set changes.
4. **Container arrangement follows an authored decoration block.**
   `DecorationSpec::authored` records whether a theme file declares a
   `decoration` object. An authored block sets the container side, tab
   direction, and button style (traffic lights or flat symbols). An
   unauthored theme keeps the Qinda macOS arrangement, so Pearl, Velvet,
   Smoked Plum, Dusk, and High Contrast are unchanged, and Qinda macOS
   authors exactly that arrangement. Preferences apply on top.
5. **A purpose-scoped compositor client.** `KWinChromeAppearance` reads the
   eight keys on its own Settings1 client. Settings1 rejects a whole scoped
   snapshot when any key is unknown, so an older resident service can cost
   only the arrangement, never the theme. It emits `containerStyleChanged`,
   and `KWinHybridSession::setChromeStyle` replaces the fixed macOS style.
6. **One Appearance destination for both sets.** Appearance gains a Windows
   tab. It shows an application window preview, from ADR-0127's item, and a
   container preview. `AppearanceContainerPreview` lays out a two-member
   container with the compositor's `ChromeLayoutEngine`, paints it with
   `ChromeRenderer`, and paints each member's native title bar with the
   shared decoration painter. Four glyph-light choice rows sit under each
   preview, with explanations in tooltips.

## Consequences

- Defaults reproduce the shipped chrome for every theme except Bliss
  containers. Bliss authors a Windows-style decoration block, so its
  containers now put flat buttons on the right with left-to-right tabs,
  matching its windows.
- The decoration and container renderers load in the compositor, so
  arrangement changes on a running desktop need the new plugin, which
  arrives at the next login after installation. From then on, changes apply
  live.
- The Settings Appearance route scopes the eight keys. After an installation
  that adds them, restart the resident Settings1 service before the new
  Settings application, or the route reports the service unavailable.
- `qindaqt.decoration-painter` pins default compatibility for every shipped
  theme, authored container resolution, window arrangement, flat and
  left-aligned rendering, and token round trips. Theme, appearance values,
  model, and page suites cover the flag, strict decode, draft validation,
  and the Windows destination.
