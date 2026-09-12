# ADR-0099: Shade a whole container by hiding member content, not shrinking it

- Status: Accepted
- Date: 2026-09-07

## Context

The product requires rolling a whole window container up to a compact,
still-visible, still-movable title strip ("shade"), distinct from the existing
whole-container minimize/iconify (`HybridChrome::WindowAction::Minimize`,
[Hybrid constraints](../architecture/hybrid-constraints.md)), which fully hides
every member and removes the group from the screen down to one dock entry.

An earlier version of this decision reflowed the container's real committed
layout down to the shared row's height and accepted the constraint solver's
existing overflow reporting for members that could not fit. Root review
rejected that design: `ConstraintSolver::solve()` never fails on a small
frame, but it also never hides content that cannot fit — it reports overflow
and still places every member's tile at whatever size its recursive minimums
require, which is larger than the collapsed frame. That leaves live
Terminal/Editor content, decoration, and input reachable outside the compact
strip, which is not a roll-up; it is a resize with a hidden failure mode. The
review also required that no live app be shrunk merely to *pretend* to be
hidden, and asked for a mechanism that genuinely hides content while keeping
the shared-chrome anchor visible, plus nested/runtime evidence rather than
frame-height unit tests alone.

Shared chrome is one KWin scene `ImageItem` parented to the topmost
active-page member's `WindowItem` ([ADR-0005](0005-scene-resident-hybrid-chrome.md)).
Reading KWin 6.6.5 upstream directly settled the two open questions this
correction needed:

- `KWin::Window::isHidden()` genuinely removes a window from pointer-input
  targeting: `InputRedirection::findToplevel()` (`src/input.cpp`) skips any
  window whose `isMinimized()`, `isHidden()`, or `isHiddenByShowDesktop()` is
  true. This is not a paint-only flag; setting it is sufic to stop input
  reaching a member's real client surface.
- `WindowItem::updateVisibility()` (`src/scene/windowitem.cpp`) computes the
  item's own visibility from those same flags, but consults per-reason
  reference counts (`m_forceVisibleByHiddenCount`, and the sibling counts for
  minimize/desktop/activity) before deciding, and `WindowItem::refVisible()`/
  `unrefVisible(WindowItem::PAINT_DISABLED_BY_HIDDEN)` are the exact public,
  KWin-exported API that increments/decrements that count. This is the same
  mechanism KWin's own minimize and "genie"/"magic lamp" effects use to keep
  painting a window's item during its close/minimize animation despite the
  window already being logically hidden or minimized — it exists precisely to
  decouple an item's own paintability from its window's hidden/minimized
  state. `WindowItem::windowContainer()` (an `Item` holding exactly the
  member's `SurfaceItem`/`DecorationItem` children) and `WindowItem::shadowItem()`
  are separate child/sibling items whose own `Item::isVisible()` is never
  touched by `updateVisibility()`, so they can be hidden independently of the
  parent `WindowItem`'s forced visibility.

## Decision

Shade never reflows, resizes, or moves any real member window. Instead:

1. **Placement layer** (`HybridContainerPlacementController`, unchanged from
   the reflow approach only in spirit): `shade()`/`unshade()` track an
   independent "strip frame" (`m_shadeStripFrames`, keyed by container id) at
   the container's current position and width with height fixed to the
   shared chrome row (`ChromeMetrics::titleBarHeight + 2*outerBorder`, plus
   one logical pixel so `ChromeLayoutEngine::build()`'s nonempty-content-rect
   check keeps passing). The real committed layout (`m_layout`) is read once,
   at `shade()` time, purely to seed the strip's initial position/width, and
   is otherwise left completely untouched: no `reflow()`/`m_reflow` call
   happens while shaded. Only `unshade()` performs one real reflow, to the
   original (pre-shade) size at the strip's *current* position — which may
   have moved under drag, so relocating the strip while shaded genuinely
   relocates where the container reappears. Outer resize is rejected while
   shaded (there is no content to resize into); outer move is redirected to a
   dedicated `handleShadedMove()` path that mutates only the tracked strip
   frame, never `m_reflow`.
2. **Chrome plan** (`HybridChromePlanBuilder::build()`): a new
   `options.shaded`/`options.shadedOuterFrame` branch, checked before the
   normal per-member solution logic, builds the render plan directly from the
   strip frame with empty `tabs`/`members`/`dividers` — the whole visible and
   hit-testable rectangle really is just the row, independent of the frozen
   real layout.
3. **Member visibility** (`HybridShadeController` + `HybridShadeMemberPlatform`,
   `src/compositor/kwin/hybridshadecontroller.{h,cpp}`, `kwinhybridshade.cpp`):
   every member is `Window::setHidden(true)` (removing paint and pointer
   input). The container's current chrome anchor (`KWinHybridGroupStacking::
   anchorMemberId()`, its topmost live member) additionally gets
   `WindowItem::refVisible(PAINT_DISABLED_BY_HIDDEN)` plus explicit
   `windowContainer()->setVisible(false)` and `shadowItem()->setVisible(false)`,
   so its outer `WindowItem` (and the chrome `ImageItem` parented to it, per
   ADR-0005) stays paintable while its own content, decoration, and shadow do
   not. This is pure orchestration behind an injectable platform interface,
   the same pattern `HybridMemberPolicy`/`HybridMemberPolicyPlatform` already
   uses, so the "which member gets which treatment, and is restore exact and
   idempotent" logic is fake-platform testable
   (`tests/compositor/tst_hybridshadecontroller.cpp`) independent of a live
   KWin session.
4. `KWinHybridGroupStacking::chromeExposedAt()`'s anchor-paintability guard
   (`paintableInCurrentContext`) is rewritten to query the anchor's
   `WindowItem::isVisible()` directly instead of duplicating the
   `isHidden()`/`isMinimized()` heuristic, because that heuristic would
   otherwise treat a shaded, force-visible anchor as unpaintable and make the
   shaded strip unclickable. `Item::isVisible()` is a strict superset of the
   replaced heuristic for the non-shaded case (it is exactly what
   `computeVisibility()` computes), so ordinary exposure is unaffected.
5. Shading and minimizing/iconifying remain independent, orthogonal states: a
   shaded container is never added to the minimized-containers set and never
   reports `minimized` in task-list facts (member hiding uses
   `Window::setHidden`, not `Window::setMinimized`, which is also what avoids
   re-triggering the existing native-minimize-to-whole-container-minimize
   interception in `KWinTaskIdentityManager`).
6. Shutdown and any container teardown (ungroup, detach-to-singleton,
   forget/close) restore member visibility before the normal independent
   `WindowRestoreState` reapplication runs (`KWinHybridSession::
   forgetShadedContainer()`/`restoreShadeForShutdown()`), so a container that
   disappears while shaded can never leave a real window permanently hidden.

`ContainerControl::ToggleShade` (title-bar control) and
`GroupContextMenuCommandKind::ToggleShadeGroup` (group-menu entry) are
unchanged from the original decision: both dispatch through
`KWinHybridSession::shadeContainer()`/`unshadeContainer()`, which now
orchestrate the member-visibility step in addition to the placement step.

## Consequences

No live application is ever resized to simulate being hidden; member windows
keep their exact frame throughout a shade/unshade cycle. Content, decoration,
and shadow are genuinely absent from the screen for the whole shaded
duration, and pointer input cannot reach any member — both are real KWin
scene/input properties, not chrome-side conventions, verified against
upstream KWin's own `findToplevel()`/`WindowItem` behavior. The dependency
on `WindowItem::refVisible()`/`windowContainer()`/`shadowItem()` is a
narrowly-scoped use of already-exported, already-precedented KWin plugin ABI
(the same API KWin's own effects use for the analogous "keep painting despite
hidden" need), confined to `kwinhybridshade.cpp`.

Fake-platform tests cover the shade/unshade orchestration (anchor vs.
non-anchor treatment, idempotency, partial-failure rollback, and
teardown-safe restore) and the placement layer's strip-tracking and
frozen-real-layout behavior. Neither proves the real KWin visual/input
result; that requires a nested live-Wayland workflow exercising an actual
shaded container (see the testing harness) — coordinated with the team so it
does not collide with a concurrent nested test slot.

## Follow-up (2026-09-12): no occlusion ghost and no activation reveal

Private nested captures of the KWin 6.6.6 scene disproved two assumptions of
the mechanism above. The window inventory reported every member hidden, yet:

1. **Occlusion ghost.** Rolling up a group whose active member draws
   client-side decorations left that member's last frame on screen, plus
   anything later painted over it, such as the closed group menu.
   `WorkspaceScene::preparePaintSimpleScreen()` adds the opaque region of every
   child item of a window whose `opacity()` is exactly 1.0 without consulting
   item visibility, and `collectDamage()` culls damage beneath that region. The
   force-visible anchor kept occluding with its hidden client surface, so the
   desktop beneath it was never repainted. Clients that declare an opaque
   surface region are affected: client-side-decorated toolkits, Electron,
   Firefox, and borderless clients; the active member is the anchor because it
   is topmost.
2. **Activation reveal.** `Workspace::activateWindow()` calls
   `Window::setHidden(false)` itself. An admitted xdg-activation request (and
   equally X11 activation, Alt-Tab, an unminimize, or a reflow) unhid a shaded
   member. The anchor then became an input-eligible, focused window whose
   content stayed invisible, an invisible input blocker, and the group could no
   longer be unrolled.

The decision stands: shade hides member content and never shrinks members.
Items 3 and 6 gain these obligations:

- Every member, not only the anchor, has its `windowContainer()` and
  `shadowItem()` hidden explicitly while shaded, so no reveal can repaint
  content. Transient windows of members are hidden and restored with their
  owner, including transients mapped while the group is shaded.
- The anchor's `Window::opacity()` is held at 0.999 while shaded, so the scene
  never treats it as an occluder. The original value is restored on unroll and
  re-lowered if a rule or client raises it while shaded; the shared chrome it
  carries renders unchanged.
- The KWin adapter records exactly what it changed for each window (forced
  `WindowItem`, hidden items, opacity, transients) and reports every
  `hiddenChanged` reveal to `HybridShadeController::reassertMember()`. The
  controller re-applies the original treatment inside KWin's own activation,
  and keyboard focus is then handed to the next shown window. An unroll
  releases this enforcement first, because its reflow legitimately activates a
  member; a rejected unroll re-hides whatever that reflow revealed.
- An anchor that closes while shaded is replaced by a surviving member before
  the lifecycle resynchronizes chrome. If no survivor can anchor, the group is
  unrolled rather than leaving hidden members with no strip. A compositor scene
  restart re-applies the treatment to the recreated `WindowItem`s.
- An explicit unroll hands keyboard focus back to the member that held it when
  the group rolled up.

Consequences: an admitted activation of a rolled-up member no longer shows,
focuses, or unrolls it; the dock and task-list Activate path remains the
explicit unroll-and-activate route. The correction depends on two further
KWin 6.6 internals, the opacity-gated occlusion pass and activation's
`setHidden(false)`, and stays confined to `kwinhybridshade.cpp`. Blur is not
addressed: `BlurEffect::drawWindow()` blurs behind any painted window that has
a blur region, so an OpenGL session whose anchor requests blur may still show a
blurred rectangle. The QPainter nested harness cannot render blur, so that case
is unverified. Evidence lives in the `compositor.hybrid-shade-controller`
fake-platform tests and the `compositor.shade-visibility.*` nested rows, which
judge captured private-compositor pixels, real client presses, and focus; see
the [testing harness](../development/testing-harness.md).

## Revisit when

A later milestone wants the shaded strip itself to show live thumbnail
content (which would need a different, deliberately-paintable member
surface), or wants shade to survive a compositor scene restart identically to
normal chrome (`aboutToToggleCompositing`/`aboutToDestroy` handling has not
been extended for the shaded-member-visibility state in this change; the
2026-09-12 follow-up re-applies member visibility after `compositingToggled`,
but no nested restart-while-shaded evidence exists yet). Also revisit if a
future KWin version changes `WindowItem`'s force-visible ref-counting
contract, gates scene occlusion on item visibility instead of window opacity,
or stops unhiding windows on activation, since this decision depends on those
exact semantics.
