# ADR-0234: Qt and the compositor measure output scale with different rulers

- **Status:** Accepted
- **Date:** 2026-09-21
- **Owners:** shell_orchestration, shell_surface
- **Supersedes:** None
- **Superseded by:** None

## Context

Before publishing panel surfaces the shell proves that Qt and the compositor
describe the same output generation. `OutputInventoryMatcher::match()` compared
identity, logical geometry, and scale, and required all three to be exactly
equal. See [Panel visibility policy](../shell/panel-visibility.md).

Both inventories reach the matcher as `ShellLayout::LogicalOutput`, whose
`scale` member was filled from two different quantities:

- the compositor publishes the **wl_output logical scale**, which may be
  fractional (1.25, 1.5, 1.75); and
- `QtOutputInventory::read()` stores `QScreen::devicePixelRatio()`, which the Qt
  Wayland platform reports as the **integer buffer scale** for the output — the
  compositor scale rounded up. Fractional scaling is applied per surface through
  `wp_fractional_scale_v1`, not at `QScreen` level.

The two are never equal on a fractionally scaled output. A 1.25-scaled 1536x864
panel therefore produced `output 'eDP-1' scale differs` on every reconcile. The
match failed, `reconcileSurfaces()` fell back to
`PanelRuntimePlanAssembler::safeVisible()`, and **every panel was pinned
visible: automatic hiding could not work at all on such a display.** The
fallback also re-ran at reconcile rate, logging the same warning roughly twice a
second.

The same conflation had a second edge. `LogicalOutput::scale` becomes
`PanelSurfaceConfiguration::outputScale`, which `LayerShellSurfaceBackend`
compares against `QScreen::devicePixelRatio()` before it will prepare or keep a
surface. Adopting the compositor's fractional scale into the layout — the
obvious "fix" for the matcher alone — would have made every surface fail
preparation instead.

## Decision

`ShellLayout::LogicalOutput::scale` carries **Qt's render scale** (the device
pixel ratio) everywhere it flows into layout solving and surface configuration.
The compositor's wl_output logical scale is a separate quantity used only to
prove generation agreement.

1. `OutputInventoryMatcher::match()` takes `(compositorInventory, qtInventory)`
   in that order; the arguments are not interchangeable. Identity, count, and
   logical geometry must still agree exactly. Scale agrees when the two values
   are equal **or** when the Qt value is the compositor value rounded up to the
   next integer. No other value is accepted, so a genuinely crossed output
   generation still fails closed.
2. When the shell adopts an accepted compositor generation it takes the
   compositor's **logical geometry** — the rectangle the visibility policy
   intersects — and keeps **Qt's render scale**, so `outputScale` stays the
   quantity `LayerShellSurfaceBackend` validates against.

## Consequences

- Automatic panel hiding works on fractionally scaled outputs. This is the
  qualification gap the panel-visibility page previously disclaimed.
- The matcher is deliberately asymmetric. Its header and implementation both say
  so; tightening the scale relation back to equality reintroduces the defect.
- Tests that asserted a 1.5-vs-2.0 pair was drift encoded the defect and now
  assert the opposite. Drift coverage uses values that satisfy neither relation.
- A persistent safe-visible cause is now reported once per distinct cause rather
  than at reconcile rate, and recovery is logged.
- Qt's rounding relation is a platform behaviour, not a protocol guarantee. It is
  isolated in one predicate so a future Qt that reports fractional device pixel
  ratios keeps working through the equality branch.

## Revisit when

Qt reports fractional `QScreen::devicePixelRatio()` on Wayland, or a compositor
publishes a scale whose Qt buffer scale is not its ceiling. Either makes the
integer-envelope branch dead or wrong, and the predicate should be rewritten
against whatever relation then holds.
