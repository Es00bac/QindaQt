# ADR-0236: Fade the scene root, because Wayland has no window opacity

- **Status:** Accepted
- **Date:** 2026-09-22
- **Owners:** shell runtime (panel visibility producers)
- **Supersedes:** None
- **Superseded by:** None

## Context

`QtPanelVisibilityAnimation::animate()` ran a `QPropertyAnimation` over the
`opacity` property of the panel's `QWindow`, and
`PanelVisibilityAnimationProducer` reset the same property to `1.0` whenever it
abandoned a transition.

Qt's Wayland platform implements no window opacity. `QWaylandWindow` has no
`setOpacity()` override, so `QWindow::setOpacity()` reaches
`QPlatformWindow::setOpacity()`, whose entire body is a warning:

```
This plugin does not support setting window opacity
```

The live session logged 662 of these. Panels therefore *popped* in and out
instead of fading, and the configured motion duration had no visual effect at
all — a setting that claimed to do something it could not.

Measured directly against the session compositor on Qt 6.11.1: a `QQuickWindow`
created on the `wayland` platform warns on `setOpacity()` and its rendered
pixels do not change, while setting `opacity` on `QQuickWindow::contentItem()`
changes them as expected (white over black at 0.25 grabs as `#404040`).

The transition itself was never broken. Mapping is what hides a panel, and the
`QPropertyAnimation` still ran and still emitted `finished`, so the visibility
hold was released correctly. Only the visual fade and the log noise were wrong.

Alternatives considered and rejected:

- *Slide the layer-surface margin instead of fading.* A different motion
  language from the one the rest of the shell uses, and it fights the layout
  solver for the surface's geometry.
- *Drop the fade and the duration setting.* Honest, but it removes working
  motion on X11 and gives up a transition the scene graph can render.

## Decision

The visibility fade animates the **panel's scene root**
(`QQuickWindow::contentItem()`), not the panel `QWindow`.
`PanelVisibilityAnimationPort` owns that choice: callers name a window and a
0..1 progression and never touch opacity themselves.

The port gains `restore(QWindow &)` — stop any running fade and return the
surface to fully opaque — and it is the only supported way to abandon a
transition. `cancel()` alone leaves the surface at whatever partial opacity it
reached. Every producer path that previously wrote `QWindow::setOpacity(1.0)`
(lost compositor authority, a settled or unmapped commit, a panel leaving the
admitted set) now calls `restore()`.

The `QWindow` fallback remains for a window that is not a `QQuickWindow`: an
X11 session and the non-Quick test doubles both support window opacity.

## Consequences

- Panels and the dock fade, and the configured motion duration means what it
  says. The 662-per-session warning stream is gone.
- Producers may no longer reset panel opacity themselves. This is the
  obligation the ADR exists to record: a producer that writes
  `QWindow::setOpacity()` directly is now writing to the wrong object and will
  strand a panel part-transparent, because the fade lives on a different one.
- The fade composites the whole panel subtree once per frame rather than
  asking the compositor to blend a surface. On a panel-sized scene this is not
  measurable, but it is real work where the window path was free.
- Covered by `qindaqt.shell-visibility-panel-fade`, which asserts the scene
  root moves, the window opacity does not, and `restore()` returns the root to
  opaque; and by `qindaqt.shell-visibility-producer-animation`, which asserts
  the producer abandons a transition through `restore()`.

## Revisit when

Qt's Wayland platform implements window opacity (it would need a compositor
protocol for it; none is in wl_compositor today), or the shell adopts a
different motion language for panel visibility.
