# Theme schema v1

This page records the theme format accepted by `src/themes`. Themes are data
only: they select semantic tokens and metrics but cannot load code.

## Root object

| Field | Type | Current rule |
| --- | --- | --- |
| `schemaVersion` | integer | Required and exactly `1` |
| `id` | string | Required, non-empty, and unique within a catalog |
| `name` | string | Required and non-empty |
| `variant` | string | Required hint such as `light`, `dusk`, `dark`, or `high-contrast` |
| `fontFamily` | string | Optional UI family; defaults to `Inter` |
| `monoFontFamily` | string | Optional monospace family; defaults to `JetBrains Mono` |
| `cornerRadius` | integer | `0` through `32` logical pixels |
| `motionDuration` | integer | `0` through `1000` milliseconds |
| `blurEnabled` | boolean | Optional; defaults to `false` |
| `iconTheme` | string | Optional shell hint: 1–128 ASCII letters, digits, `.`, `_`, or `-`, without `..`; absent values choose `breeze-dark` for a dark canvas and `breeze` for a light canvas |
| `colors` | object | All required semantic colors below |
| `decoration` | object | Optional window-control and container-tab presentation contract |

## Required color tokens

Every value must be a color accepted by Qt. Schema v1 requires `canvas`,
`surface`, `surfaceRaised`, `border`, `text`, `textMuted`, `accent`,
`accentText`, and `danger`. Components consume these meanings rather than
hard-coding theme-specific palette values.

## Decoration object

| Field | Accepted values or rule |
| --- | --- |
| `buttonPlacement` | `left` or `right`; defaults to `right` |
| `tabDirection` | `left-to-right` or `right-to-left`; defaults to `left-to-right` |
| `buttonStyle` | `symbols`, `traffic-lights`, or `glyph`; defaults to `symbols` |
| `hoverGlyphs` | Boolean controlling whether traffic-light glyphs appear only while hovered |
| `closeColor`, `minimizeColor`, `maximizeColor` | Optional valid Qt colors |
| `titleBarColor`, `titleBarInactiveColor` | Optional valid Qt colors; authoring `titleBarColor` selects the Luna title-bar presentation below |
| `restoreColor` | Optional valid Qt color; colors the maximized glyph square alongside the three colors above |

All decoration fields beyond the two directions are optional presentation
hints. Absent fields keep the classic rendering: a theme that authors none
of the optional colors and keeps `buttonStyle: symbols` or
`traffic-lights` is painted exactly as before these keys existed. When
`titleBarColor` is authored, the compositor decoration paints a Luna
gradient title bar with a white Trebuchet MS caption and a deterministic
weathering pass — rust undercoat chips, speckles, and drips whose seed is
the caption hash plus the bar width, never focus — while an inactive window
paints `titleBarInactiveColor` ([ADR-0124](../adr/0124-add-qindaqt-bliss-luna-option-set.md)).
The `glyph` button style draws right-positioned outline game-console
glyphs — circle, triangle, maximized square, and cross — colored by
`minimizeColor`, `maximizeColor`, `restoreColor`, and `closeColor`, with
dash-pattern "chipped" strokes.

These are semantic decoration preferences, not QML implementation details. The
compositor decoration and shell preview must consume the same map so ordinary
and grouped windows remain consistent.

`iconTheme` is non-token metadata. `ThemeLoader` validates and retains it in
the catalog's `ThemeSpec`; shell composition consumes that already-parsed
value and never reopens the selected JSON document. It installs that theme
before panel QML and always retains `hicolor` as the final fallback. An invalid
hint rejects the catalog at startup and never becomes a path.

The built-in catalog currently supplies QindaQt Pearl, QindaQt Velvet,
QindaQt Smoked Plum, Qinda High Contrast, Qinda macOS, and QindaQt Bliss.
The default
Nightfall/Porcelain pair draws its dark surfaces from graphite and ink-blue
night tones and its action role from restrained amber; the light counterpart
uses cool porcelain surfaces with a dark burnt-amber action. The wallpaper
catalog's QindaPunk artwork keeps its green visor as an image detail rather
than a UI palette authority. Qinda macOS uses a mist-and-sage QindaQt
palette, left-side traffic lights whose `x`, `_`, and `[]` glyphs appear on
hover, and right-to-left container tabs. QindaQt Bliss is the opt-in
XP-influenced option set ([ADR-0124](../adr/0124-add-qindaqt-bliss-luna-option-set.md)):
a light Tahoma theme with squared 4-pixel corners, an XP-face canvas, white
raised surfaces, a Luna blue accent, right-side glyph window buttons, and a
worn Luna title bar. Future state, elevation, focus,
wallpaper, and typography tokens must be added compatibly or through a
new schema version with migration tests.

[QST-1](../architecture/design-tokens.md) derives the richer state, focus,
status, spacing, typography-size, motion, and elevation vocabulary without
adding stored fields. Base point size and accessibility preferences are
caller-owned inputs, not theme data. A future role that genuinely requires an
authored value still needs a compatible schema addition or a new version with
migration tests.

Every required `colors` entry accepts any valid Qt color, including alpha; v1
does not require opacity. QST-1's reduced-transparency input therefore owns a
total, deterministic opaque flattening of those existing values rather than
narrowing this schema or asking controls to compensate.

## Decoration block presence

Declaring a `decoration` object marks the theme's decoration as authored,
even when every field repeats a default. Container chrome follows an
authored block: `buttonPlacement` sets the container button side,
`tabDirection` sets the tab order, `buttonStyle` selects traffic lights
(`traffic-lights`) or flat symbols (any other style), and `hoverGlyphs` keeps
traffic-light symbols hidden until hover. A theme without the object keeps
the Qinda macOS container arrangement. Users can override both chrome sets
from Appearance
([ADR-0129](../adr/0129-configure-window-and-container-chrome.md)).

