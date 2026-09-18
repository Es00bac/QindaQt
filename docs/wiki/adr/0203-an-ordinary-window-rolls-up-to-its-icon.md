# ADR-0203: an ordinary window rolls up to its icon

- **Status:** Accepted
- **Date:** 2026-09-17
- **Owners:** Compositor chrome, Decorations, Shell task list
- **Supersedes:** [ADR-0131](0131-contained-window-handlebar-and-wheel-roll-up.md)
  decision 3 for the wheel over an ordinary window's title bar only; the
  container handlebar and the container wheel roll-up stand
- **Superseded by:** None

## Context

ADR-0131 made the QindaQt decoration shade an ordinary window when the wheel
turns away from the user over its title bar. KWin's native shade leaves a
full-width empty title strip on screen, cannot be moved without unshading,
carries no identity beyond its caption, and is not a source for the exact
`Meta+Shift+Left` docking gesture. Containers already roll up into a compact,
movable identity badge ([ADR-0099](0099-shade-whole-containers-by-hiding-member-content.md),
[ADR-0139](0139-identity-borders-focus-and-rolled-up-badge.md)), so an
ordinary window rolling into a strip while a container rolls into a badge read
as two different products.

The requested outcome: a modifier-free wheel away from the user over an
ordinary window's title bar rolls the window up into a floating application
icon; the wheel toward the user, a double-click, or activation unrolls it in
place; the icon can be dragged, and `Meta+Shift+Left`-dragged into a container
exactly like a title, where the window is restored and docked in the position
it takes in the container; the task list shows a rolled hint.

## Decision

1. **Iconified is a compositor-owned, session-local state of an independent
   window.** Container members are never iconified (their handlebar wheel
   rolls the whole container, ADR-0131); ungrouping does not iconify; nothing
   persists, and a compositor unload restores every window at its recorded
   frame. `HybridIconifyController` is pure orchestration: it records the
   restore frame, the chip frame, and the focus held at roll-up, and knows
   exactly which platform treatment to undo. `KWinIconifyPlatform` applies the
   ADR-0099 technique per window: `Window::setHidden` removes the window from
   pointer targeting and the focus chain, `WindowItem::refVisible` keeps its
   scene item paintable so the chip parented to it renders, the window's own
   container and shadow items are hidden explicitly, its opacity is held at
   0.999 against the occlusion ghost, and its transients hide and restore with
   it.
2. **The chip is shared chrome, not a window.** `ChromeIconChip` in
   `src/hybrid_chrome` lays out and paints a 48 px identity pill: the
   application icon KWin resolves for the window (its desktop entry, so
   ADR-0169's identity for opaque classes), a hover label carrying the caption,
   a close glyph while hovered, and the identity glow while hovered. It is
   anchored at the title bar's leading edge, clamped into the output, and
   painted as a paint-only KWin scene `ImageItem` parented to the window's own
   `WindowItem` (ADR-0005): no `QWindow`, no input surface, no managed client.
3. **Input.** `KWinInteractionFilter` consumes a modifier-free wheel away
   from the user over an independent, server-decorated window's decoration
   title bar before native KDecoration sees it and rolls the window up; the
   decoration itself no longer shades on wheel (KWin's shade stays reachable
   from the window menu). Over a chip, `HybridIconChipRouter`, a sibling of
   the chrome pointer router with the same modifier semantics, owns the
   ordinary sequence: a body press raises the window and its chip, a drag past
   the threshold moves the chip and the restore frame together so the window
   reappears under the chip, a double-click unrolls, a wheel toward the user
   unrolls (a wheel away is swallowed), the close glyph closes, and a right
   click opens a small **Unroll**/**Close** menu. Any held modifier passes the
   event through, so the exact `Meta+Shift+Left` chord reaches the
   `InteractionController` untouched.
4. **Activation is an unroll, never a reassert.** KWin's `activateWindow()`
   clears `isHidden()`; the platform reports that reveal, the controller
   forgets the window and undoes the content treatment, and the window
   reappears at its restore frame as a real, focusable client. Dock and
   task-list **Activate**, a client's own activation token, and an unminimize
   therefore all unroll. This is the deliberate opposite of shade's re-hide
   (ADR-0099 follow-up): a rolled-up group must not be revealed by a stray
   activation, but an iconified window is one window the user asked for.
5. **A chip is a dock source.** `HybridInput::HitKind` gains `IconChip`:
   `KWinInteractionTargetResolver` reports an exposed chip as that source for
   its hidden window, the `InteractionController` docks it exactly like a
   member title, and, unlike an independent title, a release with no target
   still commits so the session can act on it. On a valid drop the session
   un-iconifies first (restore frame, no activation) and then hands the window
   to the ordinary docking runtime, so it joins the container as a tab or an
   edge split at its restore size; a drop with no target moves the chip to
   the drop point; Escape leaves the chip where it was.
6. **Task facts publish `iconified`.** The schema-1 task-facts window object
   gains the boolean (exact keys, both codecs). The task list keeps the entry,
   because unlike a minimized window the window is still on screen as its
   chip, and renders it as a rolled hint: a dimmed row and the accessible
   name suffix ", rolled up".
7. **Development input gains `pointer-axis`** (vertical or horizontal, a
   bounded non-zero logical delta, negative away from the user, one 15-unit
   notch per 120-unit v120 step) so nested rows drive real wheel notches
   through KWin's ordinary axis pipeline.

## Consequences

- An ordinary window rolls up to something recognizable and movable, and can
  be docked from that state, with the same modifier grammar as container
  chrome. Nothing about container shade, the badge, or the group menu changes.
- The mechanism depends on the same KWin 6.6 internals as ADR-0099
  (`WindowItem::refVisible`, the opacity-gated occlusion pass, activation's
  `setHidden(false)`), confined to `kwinhybridiconify.cpp`. The adapter
  deliberately duplicates the shade adapter's mechanics instead of sharing
  them, because the shade adapter is owned by another lane in this wave;
  unifying the two is a later, dedicated change.
- Only a server-decorated window can be rolled up by wheel: a client-side
  decorated window has no title bar the compositor knows. A hidden window is
  absent from KWin's own switcher; the dock, the task list, and client
  activation are the unroll routes. A dialog mapped while its owner is
  iconified stays hidden until the owner unrolls. Blur behind a chip's anchor
  is unverified, as for shade.
- Proof: `qindaqt.hybrid-chrome-icon-chip` (layout, clamping, scaling,
  hit-testing, icon ink, hover glyphs), `compositor.hybrid-iconify-controller`
  (invariants: platform rollback, restore-once, chip relocation with bounds,
  reveal-as-unroll, close, shutdown restore), `compositor.hybrid-icon-chip-router`
  (raise, double-click, close, thresholded drag, context menu, cancel, wheel,
  modifier pass-through), the development-input protocol and injector rows for
  `pointer-axis`, the task-facts and task-list rows for `iconified`, and the
  nested `compositor.iconify-visibility.*` rows, which judge captured pixels,
  real presses, the window inventory and the hybrid diagnostics for the wheel
  roll-up, chip drag, unroll under the chip, activation unroll, double-click,
  scene restart, close while iconified, and the chip as an edge-split, tab,
  no-target and Escape-cancelled dock source. See the
  [testing harness](../development/testing-harness.md).
- Documentation: [Hybrid container chrome](../architecture/hybrid-chrome.md)
  ("Iconified windows"), [Window containers](../architecture/window-containers.md)
  (chip as a drag source), [Task list](../shell/task-list.md), and the
  [Compositor1 reference](../reference/compositor-control-v1.md).

## Revisit when

A keyboard or menu route is wanted for rolling up client-side decorated
windows; iconified state should survive a login (which would need the
workspace persistence owner, like container names); or a KWin version changes
the force-visible ref-counting, the opacity-gated occlusion pass, or
activation's `setHidden(false)`, since this decision depends on those exact
semantics.
