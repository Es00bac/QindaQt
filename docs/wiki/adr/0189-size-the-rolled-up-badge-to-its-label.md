# ADR-0189: Paint the rolled-up badge's label, and size the badge to it

- **Status:** Accepted
- **Date:** 2026-09-17
- **Owners:** Hybrid chrome and the KWin container session
- **Amends:** [ADR-0139](0139-identity-borders-focus-and-rolled-up-badge.md)'s badge
  geometry. [ADR-0163](0163-generated-container-names-for-the-rolled-up-badge.md)'s
  and [ADR-0168](0168-a-generated-name-never-displaces-a-real-title.md)'s label
  rules stay in force and become observable for the first time
- **Superseded by:** None

## Context

A rolled-up container is a badge: controls, a label, and one pill per page
([ADR-0139](0139-identity-borders-focus-and-rolled-up-badge.md)). The label is the
only thing on it that says which page this is.

Its width was a constant. `ChromeShadedBadge` reserved a label rect of
`[48, 140]` logical pixels, and `badgeWidth()` — the function
`HybridShadeStripGeometry::stripWidth()` uses to size the strip frame — always
added the 140 px maximum whatever the title said. So the strip could not grow
with a title, and `paint()` then elided the title into whatever the rect
happened to be. An ordinary page title such as "Quarterly revenue model,
revision 12" was cut to a few words the moment the container rolled up.

[ADR-0168](0168-a-generated-name-never-displaces-a-real-title.md) had already fixed
the worst case of this — a generated "Container 7 · " prefix eating the whole
rect — and its reasoning names the 48–140 px rect as the constraint. The rect
itself was the remaining defect.

There was a second, structural half. `paint()` derived the label text from the
plan while painting, and nothing else could see it. The strip geometry runs
long before any painter exists, so the two could not agree even in principle:
one sized for a constant, the other drew a string.

And underneath both of those, **the label was never painted at all in the real
compositor.** `localizeChromeRenderPlan()` converts a plan from global logical
coordinates into the scene item's image-local space, and its own header says
"every painted rectangle must be translated; omitted geometry remains clickable
but is clipped from paint". It translated the frame, the title bar, the drag
rect, the tab strip, the content rect, every button, control, tab, member and
divider — and neither `badgeLabelRect` nor `indexBadgeRect`. So a rolled-up
container drew its controls and pills at frame-local coordinates and its label
at screen coordinates, outside a 277×31 image, clipped away entirely.

That is the defect the user actually reported. A nested capture shows controls
and one pill on a wide empty strip; the compositor's own published plan carries
the correct label text and a correctly measured rect, so every plan-level check
passes while nothing is on screen. [ADR-0163](0163-generated-container-names-for-the-rolled-up-badge.md)
and [ADR-0168](0168-a-generated-name-never-displaces-a-real-title.md) both
rewrote the label *text* rule in response to "the name disappears when rolled
up", and neither could have changed what the user saw.

## Decision

`localizeChromeRenderPlan()` translates `badgeLabelRect` and `indexBadgeRect`
with everything else. This is the fix that makes a rolled-up container show a
title at all; the sizing below is what makes the title worth reading.

The label is resolved once, measured once, and both numbers travel with the
request.

- `ChromeShadedBadge::resolveLabel(containerTitle, generated, foremostTitle)`
  is a pure function carrying ADR-0168's rule. It is the single source of
  truth for the text.
- `ChromeShadedBadge::labelWidthFor(label, metrics)` measures that text and
  clamps it to `[LabelMinimumWidth, LabelMaximumWidth]` = `[48, 320]`. The
  maximum exists so a pathological title cannot make a strip as wide as the
  screen; it is not a target. `ChromeShadedBadge::labelFont()` names the font
  used, because `paint()` draws with the painter's font and the compositor
  paints chrome into a `QImage` carrying the application default font — a
  renderer font change must change both or the strip is sized for one font and
  painted in another.
- `ChromeLayoutRequest` gains `badgeLabelText` and `badgeLabelWidth`, and
  `ChromeRenderPlan` echoes `badgeLabelText`. `layout()` reserves the measured
  width; `paint()` draws the carried text. A request that carries no width
  falls back to the *minimum*, never to a fixed guess.
- `badgeWidth()` and `stripWidth()` both take the label width. It is a required
  parameter: a default would silently reintroduce the constant.
- `HybridChromePlanBuilder::shadedLabel()` does the resolving and measuring for
  both callers, so the width the strip reserves and the text the badge paints
  come from the same call.
- Pills yield before the label. When a strip cannot hold both, pills drop into
  "+N" first and the label keeps its measured width down to the minimum.

`shade()` cannot do the sizing itself: `HybridContainerPlacementController` has
no page titles and no font, by design. It reserves the label minimum, and
`KWinHybridSession::synchronizeChrome()` — which already has the container
name, the generated-name decision and the live page titles — calls
`resizeShadeStrip()` with the measured width. That runs on every chrome
synchronization, so a strip also grows and shrinks as its foremost page title
changes while the container stays rolled up, which the old code could not do at
all. The strip keeps its top-left and never outgrows the frame it was shaded
from.

## Consequences

- A rolled-up container shows its page title at all, and shows up to 320 px of
  it rather than a fixed 140 px window onto it. The strip widens and narrows as
  the title changes.
- Plan-level assertions are not sufficient evidence for anything the badge
  paints. Every check short of pixels passed throughout the period the label
  was invisible, including a compositor diagnostic reporting the label text and
  its rect. The nested shade rows now count ink inside the strip, and the
  chrome-manager suite paints a localized shaded plan and counts ink in the
  label rect.
- Any future painted rectangle added to `ChromeRenderPlan` must be added to the
  localizer. Omitting one makes it invisible rather than misplaced, which is
  why this went unnoticed for so long.
- Two callers must now supply a label width. Both are in this change; a third
  would be a compile error rather than a silent constant, which is the point.
- The badge's label text is computed outside the painter. A fixture that builds
  a request by hand and sets no label still paints the derived label, so older
  tests keep their meaning, but they reserve only the minimum.
- **Container names still do not survive a compositor restart, and this change
  does not fix that.** `HybridContainerAppearanceStore` is documented
  process-local and non-persisted; there is no automatic save or restore of
  live container topology at compositor start; and the explicit saved-workspace
  path rebuilds the layout with a fresh container ID on adoption, so a
  per-container name has nothing stable to attach to. Making names durable
  needs a persistence owner for container identity, which is a separate
  decision and not a parallel file next to this one.
- The 320 px maximum is a judgement, not a measurement. A user with a very
  wide panel and long titles may want more.

## Revisit when

- Container identity gains a persistence owner, at which point the name half of
  the original request becomes implementable.
- The renderer stops painting chrome with the application default font.
- A theme wants the badge label to use a different font from the title bar.
