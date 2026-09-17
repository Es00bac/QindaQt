# ADR-0206: a theme authors surfaces, radii and motion, not only colors

- **Status:** Accepted
- **Date:** 2026-09-17
- **Owners:** Platform (themes, design tokens, appearance)
- **Supersedes:** None (extends [ADR-0013](0013-own-qst1-semantic-tokens.md) and the schema v1 loader contract)
- **Superseded by:** None

## Context

Schema v1 themes are a palette plus one corner radius, one motion duration
and one `blurEnabled` flag. Every surface in the desktop paints the same
opaque colors; the panel's translucency is a constant in QML, the
decoration painter's radius and shadow are compile-time numbers, and QST
publishes one elevation ramp for everything. A theme could not say "frosted
panels and title bars, sharp corners on containers, slow menus". The catalog
had six themes with one visual personality in six palettes.

Adding materials means text ends up on surfaces whose backdrop is unknown:
whatever window or wallpaper lies behind a translucent panel. The WCAG
contrast gate that protects every v1 pair (ADR-0013) knows nothing about
that.

## Decision

### Schema v2 is additive and defaulted

A theme document may carry `schemaVersion: 2` and, only then, `surfaces`,
`radii`, `motion`, `accent.mode` and `decorationTheme`; see
[Theme schema v2](../reference/theme-schema-v2.md). Every new field has a
default that reproduces v1 exactly, and `ThemeSpec::surface(name)`,
`surfaceRadius(name)` and `motion(name)` return those defaults for v1
documents, so no consumer branches on the version. A v1 document carrying a
v2 key is rejected, not half-read: the loader's strictness is the reason
themes stay trustworthy.

### QST publishes materials and per-surface motion

QST revision 1 gains an additive `material` group (`panel`, `popup`,
`menu`, `containerChrome`, `decoration`, `desktopIcons`, each with
`opacity`, `blur`, `tint`, `border`, `highlight`, `radius`, `shadow`) and
per-surface entries under `motion` (`popup`, `menu`, `rollup`, `hover`, each
`{duration, easing}`). No revision-1 key changes meaning; the installed
consumer contract checks the exact key sets. Consumers paint the published
values as given: the deriver has already applied the accessibility inputs.

### The contrast guardrail owns opacity

A translucent surface composites over an unknown backdrop, so the deriver
publishes not the authored opacity but the authored opacity raised, in
0.02 steps, until `fg.default` and `fg.muted` keep 4.5:1 against the
surface composited over black and over white. An opaque surface is
untouched; the catalog test proves every shipped surface passes at its
published opacity. Reduce transparency and high contrast flatten every
surface to opaque, unblurred and untinted, exactly a v1 theme.

### Translucency and motion are the accessibility switches

The Appearance route offers "Translucency" and "Motion" beside the themes
that use them. They are the existing `accessibility.reducedTransparency`
and `accessibility.reducedMotion` keys carried in the route's scoped draft,
never a second pair of appearance keys, so the preview derives its tokens
from the same values the Accessibility route persists.

### The session seeds the effects

`sessiondefaults.cpp` seeds `[Plugins] blurEnabled=true`,
`contrastEnabled=true` and a moderate `[Effect-blur]` strength, only when
the keys are absent: a user who switched blur off keeps that choice, and a
theme that authors no translucency costs nothing.

## Consequences

- Six new schema v2 themes ship (Qinda Glass light and dark, Slate, Paper,
  Aurora, Studio) beside the six v1 themes; every v1 document loads
  byte-identically and the built-in contrast gate covers all twelve.
- Container chrome, window title bars and the previews paint the materials
  through the shared painters (ADR-0207); panel and popup QML consume
  `Tokens.material` as those files are next touched, which is why the
  tokens carry the values rather than the painters reaching into themes.
- `accent.mode: wallpaper` is carried, validated and unused until a shell
  consumer derives an accent from the wallpaper.
- The QST revision stays 1: a consumer built against the previous key set
  still finds every role it knew.

## Verification

- `qindaqt.theme-formats`: v2 loads, v1 defaults, v1 rejects v2 keys, every
  built-in round-trips its own document.
- `qindaqt.design-tokens-derivation`: opaque v1 materials, v2 materials and
  motion, the guardrail raising a 5 % panel to the first passing opacity,
  reduce transparency and high contrast flattening.
- `qindaqt.design-tokens-built-in-contrast`: every shipped surface keeps
  both text roles at 4.5:1 over black and white at its published opacity.
- `session.sessiondefaults`: effect seeds and an explicit `blurEnabled=false`
  preserved.
