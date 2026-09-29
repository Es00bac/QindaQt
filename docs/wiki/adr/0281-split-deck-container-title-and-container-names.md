# ADR-0281: Split-deck container title row, and a container is titled by its own name

- **Status:** Accepted
- **Date:** 2026-09-28
- **Owners:** Hybrid chrome, Compositor chrome, Themes, Settings Appearance
- **Supersedes:** [ADR-0168](0168-a-generated-name-never-displaces-a-real-title.md)
  (the rolled-up badge label rule) and the generated `Container N` numbering of
  [ADR-0163](0163-generated-container-names-for-the-rolled-up-badge.md)
- **Superseded by:** None

## Context

The Corner Bar experience (ADR-0268, ADR-0277) gives ordinary windows a
BeOS-like title tab, but containers kept the full-width shared row every theme
uses. The product owner asked for BeOS-like container chrome: the container's
name with its close, maximize/restore and minimize buttons in a tab on the
left; the page tabs and the other buttons on the right; the two growing toward
each other with a gap between; the name at most about 20% of the width and the
tabs at most 75%; overflowing tabs scrolled with the wheel, shown as a card
deck with the active tab in front and its neighbours smaller behind it.

The owner also asked that a container's title be only "Container" or whatever
the user names it, never an application's name. Before this change the
unrolled row painted no container title unless the user renamed it, the
rolled-up badge showed the active page title (ADR-0168), and a never-renamed
container carried a generated `Container N` (ADR-0163).

Container chrome is a paint-only scene item (ADR-0005). The compositor routes
pointer input by hit testing the published plan, so a region the hit tester
does not claim reaches whatever KWin window lies underneath. Unlike ordinary
Corner Bar windows (ADR-0277), no KWin input-shape change is needed for a gap.

## Decision

1. **The row arrangement is data.** `HybridChrome::ContainerTitleLayout` is
   `Classic` (the shipped row) or `SplitDeck`. A theme's `decoration` block may
   author `containerTitleLayout` (`classic` or `split-deck`; absent means
   classic), a decoration document may author the same key, and the
   `appearance.containerTitleLayout` preference (`theme`, `classic`,
   `split-deck`) wins over both. Resolution follows ADR-0129/0207/0264.
2. **Split-deck geometry** (`ChromeSplitDeck`):
   - the title tab starts at the left edge and holds the window buttons in the
     theme's placement and order plus the name; it is as wide as its content
     but never wider than 20% of the row, except that the buttons and a 24 px
     drag surface are always kept (the name elides first);
   - the deck is anchored to the right edge and holds the group controls at
     the far right with the page tabs growing leftward from them; it is never
     wider than 75% of the row, and it shrinks further so at least 5% of the
     row between the two pieces stays unpainted;
   - tabs that fit sit side by side in logical order (`tabDirection` still
     decides which end tab 0 is on). Tabs that do not fit become a card
     carousel: the active tab is a full-size front card in the middle of the
     deck; each card behind it is scaled by 0.84 per step and overlapped by
     the nearer card, receding toward both ends; three cards are painted per
     side and deeper cards sit exactly behind the third.
   The plan keeps every tab in logical order with a `deckDepth`; the nearest
   card owns a point for both hit testing and paint order.
3. **The gap is not chrome.** The renderer fills, clips and strokes only the
   silhouette of the body plus the two pieces, and the hit tester returns
   nothing in the gap and no top resize edge above it. A press there reaches
   the window underneath.
4. **Input.** The whole title tab moves the container, its buttons act, and a
   double-click there runs the container's title double-click. A plain
   vertical wheel over the deck (cards or deck surface) steps the active page
   one tab in logical order, stopping at either end, through the same
   activation path as a click; over the title tab the wheel still rolls the
   container up (ADR-0131). A wheel with any modifier, Meta in particular, is
   never a deck step. Tab drag, close, context menu and accessibility work
   unchanged because every tab is still a plan tab.
5. **Motion.** A change of active card glides over the theme's
   `motionDuration`; `accessibility.reducedMotion` makes it instant. The glide
   is paint-only: the published plan, and so hit testing, is always final.
6. **A container is titled by its own name.** Its title is the user's rename,
   else the translatable default "Container". It is never a member's
   application or window title. This applies to every layout: the split-deck
   title tab, the classic row's title (in muted ink while it is the default),
   and the rolled-up badge label, which now shows the name alone. The
   generated `Container N` numbering is retired.
7. **Corner Bar treatments.** Qinda Marigold, Qinda Sea Glass and Qinda Lilac
   and their new dark twins (`qinda-marigold-dark`, `qinda-corner-teal-dark`,
   `qinda-corner-violet-dark`, variant `dark`) author `split-deck`. The title
   tab wears the theme's `titleBarColor` (focused) and `titleBarInactiveColor`.

8. **The name tab rolls up, like the window tab.** A theme may author
   `decoration.containerTitleDoubleClick` (`maximize`, `roll-up`,
   `minimize`); the six Corner Bar treatments author `roll-up`.
   `appearance.containerTitleDoubleClick` gains the token `theme`, now its
   default, which follows that authored value and otherwise stays inert;
   `none` and the action tokens are explicit user choices that win. This
   supersedes ADR-0264's container default of `none` for theme-following
   users; themes that author nothing behave exactly as before.

## Consequences

- Every theme that does not author the key keeps its classic row, and a
  classic plan is laid out exactly as before; the only classic difference is
  the muted default name in the row's leftover drag region.
- A rolled-up container that was never renamed reads "Container"; its page
  identity is carried by the pills and tabs, not the label. Two unnamed
  rolled-up containers are told apart by color and position, or by renaming.
- Live container overrides remain process-local. An explicitly saved
  workspace persists the displayed name and color with its layout and reapplies
  them to the newly adopted container after restart; unsaved live topology is
  not automatically restored (see
  [Saved workspaces](../architecture/workspaces.md)).
- `appearance.containerTitleLayout` joins the arrangement scope, so an older
  resident Settings1 without it rejects the arrangement snapshot until the
  service restarts with the new schema, as ADR-0129 and ADR-0264 recorded.
- The compositor's appearance client now also reads
  `accessibility.reducedMotion`.
- The split-deck row ignores translucent materials: it is painted opaque.
- A user whose Settings1 store already holds the old default `none` (for
  example after an earlier theme-card Apply wrote every default) keeps an
  inert container bar until they pick **Default** or apply a theme card
  again; `none` is indistinguishable from an explicit choice.
- The router gains a tab-step decision; `KWinChromeManager::dispatchTabStep`
  revalidates it against the published plan like a click.

## Verification

`hybrid-chrome.splitdeck` (caps, gap, button placement on either side,
logical order in both tab directions, carousel geometry, accessibility
completeness, narrow and too-narrow rows, nearest-card hit testing, the gap's
missing hits and resize edge, the keyboard chip, wheel stepping, rendering of
the gap, tab colors and front card, the glide), `qindaqt.hybrid-chrome-shaded-badge`
(the name-only label), `compositor.hybrid-chrome-pointer-router` (deck wheel
steps; Meta and other modifiers never step), `compositor.kwin-chrome-manager`
(tab step activation, ends, quarantine, gap resolves to no chrome),
`compositor.hybrid-chrome-plan-builder` (measured name), 
`compositor.hybrid-container-appearance` (the default name),
`qindaqt.decoration-title-options` (theme, document and preference
resolution and the preview map), `qindaqt.theme-formats` (the six Corner Bar
treatments, dark pairing and tab ink, strict tokens), and the built-in theme
contrast rows.

## Revisit when

Container names gain persistence (workspaces), a theme wants a translucent
split-deck row, or the owner wants the badge to show page titles again.
