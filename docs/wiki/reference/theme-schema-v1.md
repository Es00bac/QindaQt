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
| `buttonStyle` | `symbols` or `traffic-lights`; defaults to `symbols` |
| `hoverGlyphs` | Boolean controlling whether traffic-light glyphs appear only while hovered |
| `closeColor`, `minimizeColor`, `maximizeColor` | Optional valid Qt colors |

These are semantic decoration preferences, not QML implementation details. The
compositor decoration and shell preview must consume the same map so ordinary
and grouped windows remain consistent.

The built-in catalog currently supplies Qinda Light, Qinda Dusk, Qinda Dark,
Qinda High Contrast, and Qinda macOS. Qinda macOS uses a mist-and-sage QindaQt
palette, left-side traffic lights whose `x`, `_`, and `[]` glyphs appear on
hover, and right-to-left container tabs. Future state, elevation, focus,
wallpaper, icon, and typography tokens must be added compatibly or through a
new schema version with migration tests.
