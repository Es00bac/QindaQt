# ADR-0131: Contained windows keep a handlebar; the wheel rolls chrome up

- **Status:** Accepted
- **Date:** 2026-09-11
- **Owners:** Decorations, Compositor chrome
- **Supersedes:** None
- **Superseded by:** None

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
   painting (`DecorationMemberHandleHeight`, `layoutMemberHandleButtons`,
   `paintMemberHandle`), so the Settings container preview draws the same
   bar.
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
- The decoration and compositor changes load in the compositor process and
  take effect at the next login after installation.
- `qindaqt.decoration-painter` pins the handlebar layout for classic and
  glyph chrome and its painted pixels; the chrome pointer router suite pins
  wheel routing over the title row, tabs, and handlebars, and the
  pass-through cases.
