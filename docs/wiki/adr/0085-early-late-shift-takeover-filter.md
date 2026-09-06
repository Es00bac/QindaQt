# ADR-0085: Pre-empt KWin's native custom-tile from an early, narrow input filter

- **Status:** Accepted
- **Date:** 2026-09-06
- **Owners:** Hybrid input and gesture ownership
- **Supersedes:** None
- **Superseded by:** None

## Context

A plain (unmodified) drag on a grouped member's preserved title falls through
to KWin's own native interactive move, and member policy detaches that member
to independent for the remainder of the same move
([Hybrid topology coordination](../architecture/hybrid-topology.md)). If the
user then adds Shift mid-drag - a real, reachable sequence, not a synthetic
one - KWin applies its own built-in custom-tile ("thirds") placement on
release, because `Window::finishInteractiveMoveResize` checks `wasMove &&
(modifiers & Qt::ShiftModifier)` unconditionally, keyed on whatever modifiers
its own `MoveResizeFilter` last observed. This happens regardless of whether
Shift was held from the very start of the press (already excluded, since the
existing exact-modifier filter claims a press with the full chord before any
native move can begin) or added afterward.

Cancelling the native move on its own does not avoid this: both the public
`cancelInteractiveMoveResize()` and a synthesized Escape route through the
same `finishInteractiveMoveResize`, whose second `if`-block (the custom-tile
check) is not gated on the `cancel` flag at all - it is a separate,
unconditional branch. The only way to make cancellation safe is to ensure the
tracked modifiers it reads have not yet been updated to include Shift, which
requires acting before KWin's own filter ever sees the event that adds it.

Upstream KWin 6.6.5 (`src/window.cpp`, `src/input.cpp`) confirms:
`Workspace::moveResizeWindow()` and `Window::isInteractiveMove()`/
`cancelInteractiveMoveResize()` are public; `KWin::InputFilterOrder` places
`GlobalShortcut` strictly before `InteractiveMoveResize`; and no filter
between them swallows a bare Shift key (`ScreenEdgeInputFilter` has no
`keyboardKey` at all, `TabBoxInputFilter`/effects filters only intercept
during their own grab, and `GlobalShortcutFilter` only consumes a key bound to
an existing global shortcut).

## Decision

A second `KWin::InputEventFilter`, owned alongside the existing
Decoration-order filter inside `KWinInteractionFilter`, installs at
`GlobalShortcut` order. It observes every keyboard/pointer event through a
toolkit-neutral `HybridInput::LateShiftTakeoverDetector`, which fires exactly
once per native move the instant the exact docking chord's modifiers become
newly fully held (having a real `KWin::Window*` currently mid a *native move*,
never a resize, is the only precondition; the detector itself takes an opaque
identity and carries no KWin dependency). On that one event, the filter
consumes it (so `MoveResizeFilter` never updates the window's tracked
modifiers past this point), calls `cancelInteractiveMoveResize()` (now safe,
since the frozen modifiers cannot satisfy the custom-tile check), and adopts
the current pointer position into the same `InteractionController` the
Decoration-order filter already drives, via a new `adoptDrag()` entry point
that begins `PointerActive` directly - skipping the drag-threshold state a
fresh press would need, since the native move it is replacing already proved
the drag is real.

A plain drag that never has Shift added, and a fresh press that already holds
the full chord (claimed before any native move begins), are both unaffected;
this filter only ever acts on the one transition event of the late-add case.

## Consequences

The combine chord now owns the gesture regardless of press-order timing, with
no competing KWin custom-tile placement, at the cost of one visible frame
where the adopted window settles back to its pre-drag position before the
dock preview resumes from the current cursor - a deterministic UX tradeoff
against the alternative (a native "thirds" placement no chord asked for). The
transition-detection logic (`LateShiftTakeoverDetector`) is pure and
unit-tested directly; the KWin glue that supplies it a real window identity
and calls `cancelInteractiveMoveResize()`/`adoptDrag()` is, like the rest of
this module's KWin-facing code, not unit-testable without a live
`KWin::Window` and is covered by the private gesture QA hook instead.

## Revisit when

KWin changes `finishInteractiveMoveResize` to gate its custom-tile check on
the `cancel` flag, or exposes a supported way to reset tracked interactive-move
modifiers directly, removing the need to pre-empt the transition at the input
level at all.
