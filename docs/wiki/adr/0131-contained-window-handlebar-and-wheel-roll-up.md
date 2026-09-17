# ADR-0131: Contained windows keep a handlebar; the wheel rolls chrome up

- **Status:** Accepted
- **Date:** 2026-09-11
- **Owners:** Decorations, Compositor chrome
- **Supersedes:** None
- **Superseded by:** [ADR-0191](0191-an-ordinary-window-rolls-up-to-its-icon.md) for decision 3's wheel over an ordinary window's title bar only; the handlebar and the container wheel roll-up stand

## Context

A window inside a container kept its full 24 px native title bar: caption,
buttons, and all, directly under the container's own bar, which already names
every page in its tabs. The stacked title bars read as duplicated chrome and
cost vertical space in every split. The product owner asked for contained
windows to look like a docked dialog, with a handlebar at the top big enough
to grab and miniature stoplight controls, and for the mouse wheel to control
roll-up.

## Decision

1. **Handlebar for container members.** While the compositor marks a window
   `qindaqtContainerMember`, the KDecoration plugin draws a 14 px handlebar
   instead of the title bar. The bar uses the title color (Luna themes keep
   their sheen), a centered grip, and no caption. Miniature stoplights sit in
   12 px hit cells on the effective button side, with the user's visible
   button set applied (ADR-0129). A "more" control at the opposite end opens
   the QindaQt window menu. Glyph-style themes keep classic stoplight colors
   on the handlebar. The shared painter owns the metrics, layout, and
   painting (`DecorationMemberHandleHeight`, `layoutMemberHandle`,
   `paintMemberHandle`), so the Settings container preview draws the same
   bar. Its value layout carries every control target, the centered grip, and
   the button-free native drag region. At the 108-logical-pixel minimum
   supported width it shrinks the grip to 8 px; roomier bars retain the 36 px
   preferred grip. The live decoration consumes the same layout instead of
   independently positioning its targets.
2. **Member title regions follow the bar.** The compositor's
   `chromeMetrics()` sets `memberTitleHeight` to the handlebar height, so
   modified-pointer targets, hit tests, and the preview's layout request
   describe exactly what the decoration draws. The container's own title
   row keeps its size.
3. **Wheel roll-up.** A modifier-free vertical wheel over a container's
   title row, its tabs and controls, or a member handlebar rolls the whole
   container up (wheel turned away from the user) or down (toward the user).
   `KWinInteractionFilter` routes the axis event to
   `HybridChromePointerRouter::pointerWheel`, which emits a
   `ChromeShadeRequest`, and the session applies it through the existing
   shade path. Natural scrolling is respected through KWin's `inverted`
   flag. Over an ordinary window's title bar, the decoration plugin shades
   and unshades that window the same way. A held chrome grab, modifiers, and
   client content never trigger roll-up.

## Consequences

- Contained windows gain ten pixels of content height each, and the
  container reads as one surface with docked panes.
- Every per-window action stays reachable: stoplights for close, minimize,
  and maximize, and the "more" menu for roll-up, keep above or below, and
  workspaces. The existing title-toggle control and `Meta+Shift+C` still hide
  member bars entirely.
- Widths below 108 logical pixels are not a complete handlebar presentation:
  the layout reports them unsupported and omits the grip rather than painting
  it through a control. Container constraint handling remains responsible for
  avoiding such undersized member tiles.
- The decoration and compositor changes load in the compositor process and
  take effect at the next login after installation.
- `qindaqt.member-handle-layout` pins the minimum and roomy handlebar layouts
  for classic and glyph chrome, maximized and restored frames, and proves at
  pixel level that grip ink never enters a control. The existing decoration
  painter/plugin rows preserve the complete renderer and live factory; the
  chrome pointer router suite pins wheel routing over the title row, tabs, and
  handlebars, and the pass-through cases.
