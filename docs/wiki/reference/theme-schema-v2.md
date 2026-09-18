# Theme schema v2

Schema v2 ([ADR-0206](../adr/0206-theme-schema-v2-surfaces-motion-and-decoration-themes.md))
extends [schema v1](theme-schema-v1.md) with per-surface materials, per-surface
radii, named motions, an accent mode, and a reference to a separately authored
decoration theme document ([ADR-0207](../adr/0207-decoration-themes-are-their-own-documents.md)).
Every v1 document still loads unchanged: the loader accepts `schemaVersion` 1
or 2, every new field has a default that reproduces v1 exactly, and a v1
document that carries a v2 key is rejected rather than half-read.

## Root object

| Field | Type | Rule |
| --- | --- | --- |
| `schemaVersion` | integer | `1` or `2`; the sections below require `2` |
| every v1 field | as in [schema v1](theme-schema-v1.md) | unchanged |
| `surfaces` | object | Optional; one entry per surface class below |
| `radii` | object | Optional; surface class → integer radius `0`–`32` (default `cornerRadius`) |
| `motion` | object | Optional; motion name → `{duration, easing}` |
| `accent` | object | Optional; `{ "mode": "fixed" \| "wallpaper" }`, default `fixed` |
| `decorationTheme` | string | Optional decoration document id (same grammar as `iconTheme`); empty keeps the inline `decoration` block |

## Surfaces

Surface classes: `panel`, `popup`, `menu`, `containerChrome`, `decoration`,
`desktopIcons`. Unknown names fail the document. Each entry:

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `opacity` | number `0`–`1` | `1.0` | Material opacity. QST raises the published value as far as the text-contrast guardrail needs; the authored value is the design floor |
| `blur` | boolean | `blurEnabled` for `panel`, `popup`, `menu`; `false` otherwise | Ask the compositor to blur what lies behind the surface |
| `tint` | color | none | Vibrancy tint painted over the blurred backdrop before the surface color |
| `border` | number `0`–`1` | `1.0` | Hairline border strength |
| `highlight` | boolean | `false` | One-pixel inner catch light under the top edge |
| `shadow` | number `0`–`2` | `1.0` | Multiplier over the elevation shadow |

`ThemeSpec::surface(name)` returns the authored entry or that default, so no
consumer branches on the schema version. `desktopIcons` with `opacity: 0`
means icon labels sit directly on the wallpaper.

## Motion

Names: `popup`, `menu`, `rollup`, `hover`. Each entry has `duration`
(`0`–`1000` ms, default `motionDuration`) and `easing` (`standard`,
`emphasized`, `decelerate`, `linear`; default `standard`). QST publishes them
under `Tokens.motion` beside the v1 ramp; reduced motion caps every duration
at 80 ms exactly as it caps the ramp.

## Accent

`accent.mode` is `fixed` (the authored `colors.accent`) or `wallpaper`
(reserved for a shell that derives the accent from the wallpaper; until that
consumer exists the authored accent is used and the mode is only carried).

## Decoration theme documents

`data/decorations/<id>.json` authors window and container chrome independently
of colors, so Appearance can pair any color theme with any decoration:

| Field | Type | Rule |
| --- | --- | --- |
| `schemaVersion` | integer | exactly `1` |
| `id`, `name` | string | required; `id` uses the icon-theme grammar |
| `description` | string | optional |
| `buttonPlacement`, `tabDirection`, `buttonStyle`, `hoverGlyphs` | as the v1 `decoration` block | defaults `right`, `left-to-right`, `symbols`, `false` |
| `closeColor`, `minimizeColor`, `maximizeColor`, `restoreColor`, `titleBarColor`, `titleBarInactiveColor` | color | optional; an absent color defers to the color theme's own decoration color |
| `material` | object | `opacity`, `blur`, `tint`, `border`, `highlight`, `shadow` as a surface entry (title bar and shared row) |
| `cornerRadius` | integer `0`–`32` | window frame radius, default `10` |
| `shadow` | object | `extent` `0`–`48` (default `12`), `opacity` `0`–`1` (default `0.30`) |
| `memberHandle` | string | `grip`, `dots`, or `plain` |
| `containerBadge` | string | `pill` or `square` |

A color theme names its companion with `decorationTheme`; the user may
override the pairing for windows and for containers separately from
Appearance (`appearance.windowDecoration`, `appearance.containerDecoration`,
`theme` meaning "follow the color theme"). The shipped documents are `glass`,
`slate`, `paper`, `aurora`, `studio`, and `luna-classic`; the shipped v2 color
themes are Qinda Glass (light and dark), Qinda Slate, Qinda Paper, Qinda
Aurora, and Qinda Studio. Contrast pairs for every built-in theme remain gated
by the [design-token](../architecture/design-tokens.md) test, extended with the
translucent-surface guardrail.
