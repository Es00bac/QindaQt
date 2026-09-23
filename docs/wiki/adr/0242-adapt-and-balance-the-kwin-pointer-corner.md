# ADR-0242: Adapt and balance KWin's pointer-corner callback

- **Status:** Accepted
- **Date:** 2026-09-22
- **Owners:** Compositor integration
- **Corrects:** [ADR-0232](0232-the-gather-overview-replaces-the-upper-left-corner.md)

## Context

Releasing KWin's overview from the top edge did not make the QindaQt corner
respond. KWin 6.6.6 `Edge::handleByCallback()` invokes reserved callbacks as
`bool(ElectricBorder)`. QindaQt had reserved the pure
`PointerCornerGesture::cornerTriggered()` method directly; that method takes
no argument, so Qt's reflective invocation failed without a trigger signal.

The same KWin implementation increments its edge reservation count on every
`reserve()` call, even when the callback map already holds the same object.
QindaQt re-armed on startup and output changes without first unreserving, but
its destructor unreserved only once. The remaining count could leave the
edge active after the plugin unloaded.

## Decision

The KWin-only `KWinPointerCornerReserver` is now a QObject adapter with a
`bool(ElectricBorder)` slot. It accepts only `ElectricTopLeft` and forwards to
the pure gesture's zero-argument slot. The pure gesture stays independent of
KWin types and continues to emit `("top-left", "overview")` to the shell.

Before every re-arm, the gesture releases its previous reservation. It then
reserves once on KWin's current edge object. Destruction releases the final
reservation. A focused adapter test invokes the slot with the same
`Q_ARG(ElectricBorder, ...)` form KWin uses; the pure test checks balanced
reserve/unreserve calls across repeated re-arms.

## Consequences

The repaired callback and balance take effect when KWin next loads the
updated plugin. The user's valid KWin edge assignments remain untouched; the
separate malformed `BorderActivate` migration remains ADR-0240.
