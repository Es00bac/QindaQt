# ADR-0139: Identity borders, focus emphasis, and the rolled-up badge

- Status: Accepted
- Date: 2026-09-11

## Context

Containers gain a stable identity color in this wave (the
`HybridContainerAppearanceStore`); the color reaches the chrome through
wire W1 as a plan input, not through any new coupling. Today the shared chrome
shows focus with a theme-accent line along
the top of the focused container, a 2 px accent underline on the active tab,
and a 2 px accent ring around the focused member
([hybrid chrome](../architecture/hybrid-chrome.md)). None of these carry the
container's own color, and a rolled-up container shrinks to a full-width
empty title strip that says nothing: no name, no tabs, no per-tab unroll.

The user outcome for this lane: borders carry identity, the active window
inside a container is unmistakable, and a rolled-up container becomes a small
badge that still names its foremost page and unrolls to the exact tab the user
picks.

## Decision

### One identity color, derived shades

Every visible identity shade derives from exactly one `identityColor` plan
input (CONTRACTS §2.4) plus the resolved theme palette, in
`QindaQt::HybridChrome::resolveIdentityShades()`. An invalid `identityColor`
falls back to `ChromePalette::accent`, which is exactly today's cue source, so
an uncolored container keeps today's reading. The identity lane never picks
render shades; the chrome lane never picks container colors.

All contrast is measured with the WCAG 2.x relative-luminance definition:

- **Borders against the surface: at least 3.0:1.**
- **Text and glyphs on identity fills: at least 4.5:1.**

`resolveIdentityShades()` enforces both thresholds by construction, adjusting
the identity color minimally (alpha raise, then lightness toward the nearer of
black/white) until the target ratio holds, so no theme or user color can
produce an unreadable combination. The derivation is pure and tested per
theme, including a negative control that an unset color yields accent-derived
shades.

### The visual spec (numbers)

| Element | Value |
| --- | --- |
| Inactive container border | 2.0 px, identity border color at reduced strength (55% mix toward the surface, then contrast-raised to 3.0:1) |
| Focused container border | 3.0 px, full-strength identity border color |
| Focus glow | one additional 6.0 px stroke outside the frame, identity at 28% alpha, radius `cornerRadius + 2` |
| Title-row stripe | 3.0 px line along the top edge of the shared title row, full-strength identity (replaces the old focused-only accent line; present whether focused or not) |
| Active tab fill | identity tint: identity at 16% alpha composited over `surfaceRaised` |
| Active tab underline | 3.0 px full-strength identity, same 4 px inset rule as today's 2 px line |
| Inactive tab text | `textMuted`, faded toward the surface but never below 4.5:1 against its fill |
| Container title text | `textOnFill` (identity-derived, 4.5:1 against the title-row fill) |
| Index badge (W2) | 18 x 18 logical px rounded square (radius 9) at the leading edge of the title row; filled with the identity border color; digits in the 4.5:1 identity ink; hidden when `indexBadge` is 0 |
| Focused member ring | 3.0 px (was 2.0), identity, clipped to the paintable chrome outside the native member frame exactly as today |
| Focused member handlebar | filled with the identity color; stoplight glyphs and the grip painted in the identity ink (4.5:1); neutral members keep today's title-bar fill |

Glyph/text ink flips between light and dark automatically: the ink is
whichever of the theme text color, white, and black has the highest contrast
against the fill, preferring the theme text color when it already passes.

### The rolled-up badge

Shading keeps its existing placement contract (ADR-0099's frozen committed
layout; unrolling restores the exact previous size at the strip's current
position). What changes is the strip's shape and paint:

- The strip frame is a small badge anchored at the container's left edge:
  same top-left position as today, height
  `ceil(titleBarHeight + 2 * outerBorder + 1)` (unchanged), width computed by
  `HybridShadeStripGeometry` from the metrics and the tab count, never wider
  than the old full-width strip and never wider than the work area.
- The badge holds, leading to trailing: the three container controls (16 px
  cells, 6 px spacing, unchanged metrics), the label, then one pill per tab,
  then the overflow pill. All inside a 2 px full-strength identity border with
  the standard corner radius.
- The label is the foremost (active) tab's title; when the container is
  renamed it reads `<container name> · <foremost tab title>`, elided.
- Pills are 10 px tall rounded caps in varied tints of the identity color
  (tint alpha cycles 12% / 18% / 24% / 30% by pill index, composited over the
  badge surface), at most 8 pills, then a text pill `+N` counting the rest.
  The active page's pill gets the strongest tint of its step.
- Interactions: clicking a pill activates that page and unrolls to it;
  turning the wheel toward the user over any badge region unrolls (the
  existing wheel contract, unchanged); double-click on the badge unrolls;
  dragging the badge (label or border region, not a control or pill) moves it,
  and the container controls keep working. Unrolling restores the exact
  previous geometry (existing placement contract).

### Hit testing and accessibility

Badge pills are `ChromeTabSpec` tabs in the shaded plan (the shaded branch no
longer passes an empty tab list), laid out by `ChromeShadedBadge` into
`TabGeometry` rects. Consequently the existing tab hit kind, tab activation
path, and the chrome accessibility adapter's named, invokable tab nodes apply
to pills without any accessibility-model change; pill nodes expose the tab
title as their accessible name and unroll-to-that-tab as their default
action. The `+N` overflow pill is a paint-only counter, not a control; the
full tab list returns on unroll. This lane requests no accessibility model
change through the Program Manager.

A `shadedBadge` flag on `ChromeHitTarget::Tab` targets lets the pointer router
emit the matching unroll request together with the pill activation, and its
double-click detector (constructor-injected interval) turns a second click on
a shaded badge into an unroll request. Both arrive as the existing
`ChromeShadeRequest` and activation decisions; no new decision kind exists.

### Themes

All six bundled themes (qinda-light, qinda-dark, qinda-high-contrast,
qinda-macos, qinda-dusk, qinda-bliss) stay readable through the derivation,
not through per-theme exceptions: pixel tests render each theme and assert the
3.0:1 and 4.5:1 thresholds on the drawn result. The Settings Appearance
container preview feeds the same renderer and painter (ADR-0127), so it cannot
drift from live drawing; it gains an identity color input and a rolled-up
badge state behind a keyboard-reachable toggle.

## Consequences

- `HybridChromePlanOptions` and `ChromeLayoutRequest`/`ChromeRenderPlan` gain
  `identityColor`, `tabTitleOverrides`, and `indexBadge` with
  reproduce-today defaults (invalid color = accent, no overrides, badge
  hidden). The Program Manager fills them from the identity store (W1) and
  the selection hint (W2).
- New modules: `chromeidentity` (pure shade derivation),
  `chromeshadedbadge` (badge layout + paint), and
  `HybridShadeStripGeometry` (compositor strip width/anchoring).
- `qindaqt.decoration-painter` baseline pins change deliberately; before/after
  images are kept with the lane build output and listed in the handoff.
- The old focused-only top accent line is replaced by the always-on identity
  stripe; focus emphasis moves entirely to the 3 px border + glow.
