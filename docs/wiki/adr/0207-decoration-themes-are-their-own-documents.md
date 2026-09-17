# ADR-0207: a decoration theme is its own document

- **Status:** Accepted
- **Date:** 2026-09-17
- **Owners:** Platform (decoration painter, compositor chrome, appearance)
- **Supersedes:** None (extends [ADR-0129](0129-configure-window-and-container-chrome.md) and [ADR-0127](0127-preview-window-chrome-and-toolkit-through-one-painter.md))
- **Superseded by:** None

## Context

The window decoration and the container chrome were styled by the color
theme's inline `decoration` block: button style, placement, tab direction,
hover glyphs and the optional worn Luna colors. Radius, shadow and the title
material were constants in the painter. A user could pick a color theme and
refine the arrangement (ADR-0129), but could not say "Qinda Paper colors
with the Glass chrome", and a theme author could not ship a chrome
personality without also shipping a palette.

## Decision

### Decoration documents live in `data/decorations/*.json`

A decoration document (schema 1) authors button placement, style and hover
glyphs, optional title-bar and button colors, a title `material`
(`opacity`, `blur`, `tint`, `border`, `highlight`, `shadow`), the corner
radius, the shadow extent and opacity, the member handle style and the
container badge style; see [Theme schema v2](../reference/theme-schema-v2.md).
Colors are optional: a document that authors none is neutral and serves
every color theme. The loader is as strict as the theme loader and a
malformed document fails the whole catalog closed.

### A color theme names its pairing; the user may override it

A v2 theme's `decorationTheme` names the document it was designed with.
Settings1 carries two purpose-scoped keys, `appearance.windowDecoration`
and `appearance.containerDecoration`, whose value is `theme` or a document
id. Resolution is the same everywhere: the theme's own v2 decoration
surface, then the selected document (the user's choice when installed, else
the theme's pairing, else none), then the user's arrangement preferences
(ADR-0129) on top. A v1 theme with no document paints the shipped chrome
byte for byte.

### The chrome map carries the material

`DecorationChrome` gains `titleOpacity`, `titleBlur`, `titleHighlight`,
`titleTint`, `cornerRadius`, `shadowExtent`, `shadowOpacity` and
`handleStyle`; `ChromeStyle` gains a `ChromeMaterial`. Both round-trip
through their variant maps, omitting defaults, so the compositor publishes
one map to the decoration plugin and the Appearance previews paint from the
same shape. The plugin sets its KDecoration border radius, shadow and blur
region from that map; the container renderer scales its surface alpha,
paints the tint and the catch light, and scales the identity frame's
strength.

### The compositor reads documents through a third scoped client

The compositor loads the installed documents once and subscribes to the two
pairing keys through their own Settings1 client, beside the theme client and
the arrangement client, so an older resident service that does not know the
keys costs only the pairing, never the theme or the arrangement.

## Consequences

- Six documents ship: Glass, Slate, Paper, Aurora, Studio and Luna Classic.
  Luna Classic authors the worn Luna colors itself, so the classic bar is
  available over any palette; a material document over a Luna theme owns
  the title surface instead of blending under the worn bar.
- The Appearance route's Windows destination gains a window and a container
  decoration chooser whose cards are painted by the real renderers with the
  draft theme, arrangement and wallpaper, and the theme grid's cards are
  rendered thumbnails of each theme with its pairing.
- The badge pill geometry stays with the shaded badge painter; a square
  badge style flattens the shaded frame and is carried for the badge
  painter to adopt.
- Two settings keys join the appearance schema, so the resident Settings1
  service must run the new schema before the Appearance route can commit.

## Verification

- `qindaqt.decoration-theme-formats`: every shipped document loads, six
  distinct personalities, optional colors, invalid values rejected, the
  catalog finds by id, every theme pairing exists.
- `qindaqt.decoration-documents`: v1 themes resolve the shipped chrome with
  no material keys, pairing and override precedence, material round trip
  and tolerant decode, container material carriage, arrangement preferences
  winning over a document, Luna Classic over Slate and Glass over Bliss.
- `hybrid-chrome.renderer`: default material paints the shipped pixels;
  opacity, highlight and border strength repaint the surface.
- `qindaqt.appearance-values`, `qindaqt.appearance-page`: the pairing keys
  decode strictly and the choosers drive the draft.
