// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "interactiontargetresolver.h"

namespace QindaQt::HybridInput {

class InteractionController final
{
public:
    // AGENT-CONTRACT: The resolver is borrowed, must outlive this controller,
    // and is called synchronously on the controller's owning input thread.
    explicit InteractionController(
        const InteractionTargetResolver &resolver,
        InteractionBindings bindings = {});

    [[nodiscard]] InteractionDecision pointerPress(const PointerEvent &event);
    [[nodiscard]] InteractionDecision pointerMove(const PointerEvent &event);
    [[nodiscard]] InteractionDecision pointerRelease(const PointerEvent &event);
    [[nodiscard]] InteractionDecision keyEvent(const KeyEvent &event);

    // Adopts a drag already in progress at `position` - entering
    // PointerActive directly rather than PointerPending, since a caller only
    // ever calls this once a pointer button is already held down elsewhere
    // (a competing native move it just cancelled). Idle only; a caller must
    // never invoke this while the controller already owns an interaction.
    // Emits Begin immediately (and, for MemberDock, an immediate preview
    // Update) since there is no earlier press position to wait out a drag
    // threshold against.
    [[nodiscard]] InteractionDecision adoptDrag(const QPointF &position);

    // Same adoption with an explicitly identified source. AGENT-CONTRACT: a
    // caller that knows the dragged window's identity (the ADR-0085 late-Shift
    // takeover knows KWin's interactive-move owner) must pass it here rather
    // than let the position overload hit-test: cancelling the native move
    // snaps the window back to its pre-drag frame, so the pointer usually
    // sits over a different window - often the intended drop target's - and a
    // hit-test would silently adopt (and later move or detach) that stranger.
    // An invalid source adopts the swallowed no-target grab exactly like an
    // unresolved hit-test.
    [[nodiscard]] InteractionDecision adoptDrag(const HitTarget &source,
                                                const QPointF &position);

    // The exact modifiers a pointer press/adopted drag must match. Exposed
    // so a caller watching for a competing native gesture to intercept knows
    // which chord to watch for without duplicating the bindings.
    [[nodiscard]] Qt::KeyboardModifiers pointerModifiers() const;

    // A KGlobalAccel QAction calls this with the currently focused member. The
    // controller remains toolkit-neutral and never discovers focus itself.
    // Arrows choose edges, T chooses tabs/reordering, and D selects detach for
    // a grouped member. All keyboard modes use Enter/Return to commit and
    // Escape to cancel; geometry modes accumulate arrows by keyboardStep.
    [[nodiscard]] InteractionDecision beginKeyboardDock(const HitTarget &source);
    [[nodiscard]] InteractionDecision beginKeyboardMove(const HitTarget &source);
    [[nodiscard]] InteractionDecision beginKeyboardDividerResize(
        const HitTarget &source);
    [[nodiscard]] InteractionDecision beginKeyboardContainerResize(
        const HitTarget &source);
    [[nodiscard]] InteractionDecision cancel();

    [[nodiscard]] bool active() const;
    [[nodiscard]] InteractionKind interactionKind() const;

private:
    enum class State {
        Idle,
        PointerPending,
        PointerActive,
        KeyboardActive,
    };

    [[nodiscard]] InteractionKind kindForHit(HitKind kind) const;
    [[nodiscard]] InteractionDecision beginKeyboardInteraction(
        const HitTarget &source, InteractionKind expectedKind);
    [[nodiscard]] InteractionIntent intent(IntentPhase phase,
                                           const QPointF &position = {}) const;
    // Cancels whatever the controller currently owns. Emits a Cancel intent
    // only when a real target kind was ever resolved; a swallowed no-target
    // grab (see pointerPress) resets silently instead of reaching the runtime
    // with InteractionKind::None.
    [[nodiscard]] InteractionDecision cancelActive(const QPointF &position);
    [[nodiscard]] bool pointerBindingMatches(const PointerEvent &event) const;
    [[nodiscard]] DockZone zoneForKey(Qt::Key key) const;
    [[nodiscard]] QPointF displacementForKey(Qt::Key key) const;
    [[nodiscard]] bool displacementKeyApplies(Qt::Key key) const;
    void reset();

    const InteractionTargetResolver &m_resolver;
    InteractionBindings m_bindings;
    State m_state = State::Idle;
    InteractionKind m_kind = InteractionKind::None;
    HitTarget m_source;
    DockTarget m_previewTarget;
    QPointF m_pressPosition;
    QPointF m_lastPosition;
    QPointF m_displacement;
    bool m_keyboardDetachSelected = false;
};

} // namespace QindaQt::HybridInput
