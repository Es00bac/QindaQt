# ADR-0287: Pass tablet proximity through window decorations

- **Status:** Accepted
- **Date:** 2026-09-28
- **Owners:** Compositor
- **Supersedes:** None
- **Superseded by:** None

## Context

In KWin 6.6.6 the decoration input filter consumes a tablet tool's proximity
events whenever the pen is over a window decoration. The forward filter is the
only code that sends tablet-v2 `proximity_in` to a client, and
`TabletToolV2Interface::setCurrentSurface()` re-sends proximity to a new surface
only after a first one was sent. A pen that entered range over a title bar
therefore never announced itself to any client: moving it into the window sent
motion without proximity, which Qt clients drop ("Can't send tablet event with
no proximity surface"; 195 times in one owner session). Drawing did not work
until the pen left range and came back directly over the application.

## Decision

A second pinned KWin patch,
`compositor/patches/0002-tablet-proximity-over-decoration.patch`, keeps the
decoration filter's hover handling for proximity but no longer consumes the
event, so the forward filter sends `proximity_in`/`proximity_out` to the
decorated window's surface as it does everywhere else. Axis, tip and button
events over the decoration are unchanged. The patch is listed with its SHA-256
in `compositor/patches/series.json` after the ADR-0277 patch and is applied by
the overlay's `kde-plasma/kwin` package; `gui-wm/qindaqt-desktop` depends on the
revision that carries it.

## Consequences

A client can receive `proximity_in` while the pen is over its title bar, at a
surface-local position outside its surface; clients already tolerate that
(pointer-style enter). The patch belongs to QindaQt's KWin build and must be
re-applied or dropped at each KWin upgrade (see the KWin upgrade procedure).

## Revisit when

Upstream KWin forwards tablet proximity past decorations itself, or QindaQt's
own KWin build replaces the decoration input filter.
