# Finish Customize — visual repair checkpoint

**2026-09-06T10:06:00-06:00**

The first offscreen visual review found structural defects rather than palette
differences. The wide Customize composition allowed the supporting palette and
outline to consume the output preview; each supporting pane is now bounded to
240 logical pixels and the canvas has a 360 logical-pixel minimum. The revised
1080×720 render is stored under the ignored build visual-QA directory.

Appearance now uses a compact selector for the five subpixel-order values so
the font form remains readable at 900×640. Notifications is a QST/Controls
page with no embedded Close action. Its previous standalone QML fixture could
not publish QST, so equivalent DND toggle, conflict/retry, and tab-navigation
checks now run through the token-bound Settings host fixture.
