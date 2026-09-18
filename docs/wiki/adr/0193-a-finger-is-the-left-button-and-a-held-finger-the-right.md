# ADR-0193: a finger is the left button, and a held finger the right

- **Status:** Accepted
- **Date:** 2026-09-17
- **Owners:** Platform (compositor input, controls)
- **Supersedes:** None (extends the chrome pointer routing of [ADR-0131](0131-contained-window-handlebar-and-wheel-roll-up.md) and the controls contract of [ADR-0021](0021-isolate-controls-visual-rows.md))
- **Superseded by:** None

## Context

Touch reached only the development input spy. `KWinInteractionFilter`
implemented pointer and wheel, so a finger on compositor-painted container
chrome fell through to the member window beneath it; the shell, the desktop
surface and the file manager had no long-press handler, so touch had no
right-click anywhere; and no control knew whether the last input was a
finger, so every hit target stayed pointer-sized on a touch panel.

## Decision

### The chrome router speaks pointer; the finger is translated to it

`KWinInteractionFilter` gains `touchDown/touchMotion/touchUp/touchCancel`.
The first finger that lands on a chrome target the router owns becomes the
router's left button: press, moves, release, through the existing
`HybridChromePointerRouter`, so tab activation, title drag, divider and
outer resize keep one implementation. The hit test is the router's own with
a touch radius: exact position first and, only when that hits nothing at
all, a ring of probes out to 40 px, so a finger beside a control still finds
it (hit test only; nothing is painted larger). The ring applies only where
nothing is underneath the finger: the chrome resolver cannot tell empty
desktop from a window it does not manage, so the filter asks KWin which
window is under the point and keeps the exact test alone over any window
other than the desktop, and the probes never leave the finger's own output.
A finger that lands on a member title bar, on client content, on an
un-contained window or on a panel is not consumed and is never pulled onto
nearby chrome: decorations and clients receive it exactly as before.

### Holding still is the right click; two fingers are the wheel

A pure policy (`HybridChromeTouchPolicy`) decides what a sequence means:
held still for 500 ms within 8 px it is a long press, on which the router's
press is abandoned (the lift never activates) and the container menu opens
where the finger rests, for targets a right click would open it on; a
second finger on a roll target makes a two-finger gesture, and 40 px of
vertical travel rolls the container up (swipe up) or down (swipe down),
exactly the wheel's shade request. A second finger joins the gesture only
when it lands on the same container's chrome; a finger anywhere else while a
gesture runs belongs to KWin and is not consumed. After a long press, a
swipe, or any two-finger sequence -- whichever finger lifts first -- the rest
of that sequence is consumed silently, so a swipe never becomes a drag and a
menu never gets a stray click. The policy is tested with plain calls; the
libobs-style headless row for the filter is the nested harness, whose
development input device now injects `touch-down/motion/up` contacts and
reports `isTouch`, so a private session drives real fingers.

### One long-press convention in the controls

`QindaQt.Controls.TouchContextArea` is the only long-press handler: a
`TapHandler` for touchscreens and styli that takes no grab until the hold
completes and emits `contextRequested(position)`. It is placed over the
same area as the existing right-click handler and calls exactly the same
menu: the desktop surface and its icon tiles, the file manager grid, list
and entries, and the task list entry's applet-local handler. A mouse never
triggers it; mice have a right button.

### Touch mode is a token

`Tokens.touch` publishes `available` (a touchscreen is among the seat's
devices), `active` (the last input was a finger, observed from the
application's own events), and the sizes controls adopt while active:
`minimumTarget` 44, `rowHeight` 48, `gap` 8, all zero otherwise so a
control's pointer size wins in `Math.max`. Button, CheckBox, Switch,
ComboBox, TabButton and TextField use it; compositions and tests may set the
state directly.

## Consequences

- Container chrome is usable by finger: tap a tab, hold for the menu, drag
  the title row, resize on a divider, swipe with two fingers to roll up.
- Window decorations, the on-screen keyboard, edge gestures and the Touch
  settings destination are the O14 remainder; long-press on decorations
  stays with KWin's own decoration touch handling for now.
- `PanelContent.qml` and `PanelAppletRow.qml` are untouched (O9 owns them);
  applets adopt `TouchContextArea` locally.
- The 40 px touch radius only widens chrome into empty space: a finger that
  lands on a member's title bar, on client content, on an un-contained
  window or on a panel stays KWin's even when a tab is 12 px away, so native
  touch on decorations and applications is never stolen.
- A gesture that receives nothing for 5 s (the grab went elsewhere and no up
  or cancel followed) is abandoned by the next finger, and a touch cancel is
  passed on to the rest of KWin's filter chain rather than swallowed.
- The development seat claims touch only for rows that set
  `QINDAQT_DEVELOPMENT_INPUT_TOUCH=1`, so pointer-mode nested rows keep
  pointer sizing.

## Verification

- `compositor.hybrid-chrome-touch-policy`: tap, long press, slop, drag,
  two-finger swipes, non-roll targets, cancel.
- `compositor.hybrid-chrome-pointer-router`: the touch hit test reaches
  nearby owned targets only.
- `compositor.development-input-protocol`, `compositor.kwin-development-input-injector`:
  touch contacts parse, are bounded, and are framed.
- `qindaqt.controls-touch`: a held finger requests the menu once, a tap and
  a mouse do not, touch mode grows a Button to 44 and back.
- Nested `compositor.touch-chrome.*`: tap on a tab activates it, a held
  finger on the title row opens the group menu, a title drag moves the
  container, two-finger swipes roll it up and down, with captures.
