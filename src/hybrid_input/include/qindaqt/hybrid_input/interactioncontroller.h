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
