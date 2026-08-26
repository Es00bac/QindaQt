// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/hybrid_input/interactioncontroller.h"

#include <QLineF>

#include <cmath>
#include <utility>

namespace QindaQt::HybridInput {

namespace {

constexpr qreal DefaultDragThreshold = 8.0;
constexpr qreal DefaultKeyboardStep = 10.0;

bool hasContradictoryResizeEdges(Qt::Edges edges)
{
    return (edges.testFlag(Qt::LeftEdge) && edges.testFlag(Qt::RightEdge))
        || (edges.testFlag(Qt::TopEdge) && edges.testFlag(Qt::BottomEdge));
}

} // namespace

HitTarget::HitTarget(HitKind targetKind,
                     QString targetContainerId,
                     QString targetMemberId,
                     QString targetDividerId,
                     Qt::Edges targetEdges,
                     QString targetPageId)
    : kind(targetKind)
    , containerId(std::move(targetContainerId))
    , memberId(std::move(targetMemberId))
    , dividerId(std::move(targetDividerId))
    , edges(targetEdges)
    , pageId(std::move(targetPageId))
{
}

bool HitTarget::isValid() const
{
    switch (kind) {
    case HitKind::OuterTitle:
        return !containerId.isEmpty();
    case HitKind::MemberTitle:
    case HitKind::Tab:
        return !memberId.isEmpty();
    case HitKind::Divider:
        return !containerId.isEmpty() && !dividerId.isEmpty();
    case HitKind::OuterResize:
        return !containerId.isEmpty() && edges != Qt::Edges{}
            && !hasContradictoryResizeEdges(edges);
    case HitKind::None:
        return false;
    }
    return false;
}

bool DockTarget::isValid() const
{
    return zone != DockZone::None && (!containerId.isEmpty() || !memberId.isEmpty());
}

InteractionController::InteractionController(const InteractionTargetResolver &resolver,
                                             InteractionBindings bindings)
    : m_resolver(resolver)
    , m_bindings(std::move(bindings))
{
    if (!std::isfinite(m_bindings.dragThreshold) || m_bindings.dragThreshold < 0.0) {
        m_bindings.dragThreshold = DefaultDragThreshold;
    }
    if (!std::isfinite(m_bindings.keyboardStep) || m_bindings.keyboardStep <= 0.0) {
        m_bindings.keyboardStep = DefaultKeyboardStep;
    }
}

InteractionDecision InteractionController::pointerPress(const PointerEvent &event)
{
    if (m_state != State::Idle || event.changedButton != m_bindings.pointerButton
        || !pointerBindingMatches(event)) {
        return {};
    }

    const auto source = m_resolver.hitTest(event.position);
    const auto kind = kindForHit(source.kind);
    if (!source.isValid() || kind == InteractionKind::None) {
        return {};
    }

    m_state = State::PointerPending;
    m_kind = kind;
    m_source = source;
    m_pressPosition = event.position;
    m_lastPosition = event.position;
    m_displacement = {};
    return {.consumed = true, .intents = {}};
}

InteractionDecision InteractionController::pointerMove(const PointerEvent &event)
{
    if (m_state != State::PointerPending && m_state != State::PointerActive) {
        return {};
    }

    InteractionDecision decision{.consumed = true, .intents = {}};
    const auto distance = QLineF(m_pressPosition, event.position).length();
    m_lastPosition = event.position;
    m_displacement = event.position - m_pressPosition;
    if (m_state == State::PointerPending && distance < m_bindings.dragThreshold) {
        return decision;
    }

    if (m_state == State::PointerPending) {
        m_state = State::PointerActive;
        decision.intents.append(intent(IntentPhase::Begin, event.position));
    }

    auto update = intent(IntentPhase::Update, event.position);
    if (m_kind == InteractionKind::MemberDock) {
        const auto target = m_resolver.pointerDockTarget(m_source, event.position);
        if (target != m_previewTarget) {
            m_previewTarget = target;
            update.target = target;
            decision.intents.append(update);
        }
    } else {
        decision.intents.append(update);
    }
    return decision;
}

InteractionDecision InteractionController::pointerRelease(const PointerEvent &event)
{
    if ((m_state != State::PointerPending && m_state != State::PointerActive)
        || event.changedButton != m_bindings.pointerButton) {
        return {};
    }

    InteractionDecision decision{.consumed = true, .intents = {}};
    m_lastPosition = event.position;
    m_displacement = event.position - m_pressPosition;
    if (m_state == State::PointerActive) {
        auto commit = intent(IntentPhase::Commit, event.position);
        if (m_kind == InteractionKind::MemberDock) {
            commit.target = m_resolver.pointerDockTarget(m_source, event.position);
            // AGENT-GUARD: A missing target commits a detach gesture only for
            // an existing group member. Independent windows cannot be detached.
            if (!commit.target.isValid() && m_source.containerId.isEmpty()) {
                commit.phase = IntentPhase::Cancel;
            }
        }
        decision.intents.append(commit);
    } else {
        decision.intents.append(intent(IntentPhase::Cancel, event.position));
    }
    reset();
    return decision;
}

InteractionDecision InteractionController::keyEvent(const KeyEvent &event)
{
    if (!event.pressed || m_state == State::Idle) {
        return {};
    }

    if (event.key == Qt::Key_Escape) {
        InteractionDecision decision{.consumed = true, .intents = {}};
        decision.intents.append(intent(IntentPhase::Cancel, m_lastPosition));
        reset();
        return decision;
    }

    if (m_state != State::KeyboardActive) {
        return {};
    }

    if (event.key == Qt::Key_Return || event.key == Qt::Key_Enter) {
        InteractionDecision decision{.consumed = true, .intents = {}};
        auto commit = intent(IntentPhase::Commit);
        commit.target = m_previewTarget;
        if (m_kind == InteractionKind::MemberDock
            && !commit.target.isValid() && !m_keyboardDetachSelected) {
            commit.phase = IntentPhase::Cancel;
        }
        decision.intents.append(commit);
        reset();
        return decision;
    }

    if (m_kind == InteractionKind::MemberDock) {
        if (event.key == Qt::Key_D) {
            InteractionDecision decision{.consumed = true, .intents = {}};
            // AGENT-GUARD: An invalid target means detach at the Hybrid runtime
            // boundary. Only a currently grouped source may intentionally
            // select it; otherwise Enter must still resolve to Cancel.
            if (!m_source.containerId.isEmpty()) {
                m_previewTarget = {};
                m_keyboardDetachSelected = true;
                decision.intents.append(intent(IntentPhase::Update));
            }
            return decision;
        }

        const auto zone = zoneForKey(event.key);
        if (zone == DockZone::None) {
            return {};
        }
        m_keyboardDetachSelected = false;
        m_previewTarget = m_resolver.keyboardDockTarget(m_source, zone);
        auto update = intent(IntentPhase::Update);
        update.target = m_previewTarget;
        return {.consumed = true, .intents = {update}};
    }

    if (!displacementKeyApplies(event.key)) {
        return {};
    }
    m_displacement += displacementForKey(event.key);
    auto update = intent(IntentPhase::Update);
    return {.consumed = true, .intents = {update}};
}

InteractionDecision InteractionController::beginKeyboardDock(const HitTarget &source)
{
    const auto sourceKind = kindForHit(source.kind);
    return sourceKind == InteractionKind::MemberDock
        ? beginKeyboardInteraction(source, InteractionKind::MemberDock)
        : InteractionDecision{};
}

InteractionDecision InteractionController::beginKeyboardMove(const HitTarget &source)
{
    return source.kind == HitKind::OuterTitle
        ? beginKeyboardInteraction(source, InteractionKind::ContainerMove)
        : InteractionDecision{};
}

InteractionDecision InteractionController::beginKeyboardDividerResize(
    const HitTarget &source)
{
    return source.kind == HitKind::Divider
        ? beginKeyboardInteraction(source, InteractionKind::DividerResize)
        : InteractionDecision{};
}

InteractionDecision InteractionController::beginKeyboardContainerResize(
    const HitTarget &source)
{
    return source.kind == HitKind::OuterResize
        ? beginKeyboardInteraction(source, InteractionKind::ContainerResize)
        : InteractionDecision{};
}

InteractionDecision InteractionController::cancel()
{
    if (m_state == State::Idle) {
        return {};
    }
    InteractionDecision decision{.consumed = true, .intents = {}};
    decision.intents.append(intent(IntentPhase::Cancel, m_lastPosition));
    reset();
    return decision;
}

bool InteractionController::active() const
{
    return m_state != State::Idle;
}

InteractionKind InteractionController::interactionKind() const
{
    return m_kind;
}

InteractionKind InteractionController::kindForHit(HitKind kind) const
{
    switch (kind) {
    case HitKind::MemberTitle:
    case HitKind::Tab:
        return InteractionKind::MemberDock;
    case HitKind::OuterTitle:
        return InteractionKind::ContainerMove;
    case HitKind::Divider:
        return InteractionKind::DividerResize;
    case HitKind::OuterResize:
        return InteractionKind::ContainerResize;
    case HitKind::None:
        return InteractionKind::None;
    }
    return InteractionKind::None;
}

InteractionDecision InteractionController::beginKeyboardInteraction(
    const HitTarget &source, InteractionKind expectedKind)
{
    if (m_state != State::Idle || !source.isValid()
        || kindForHit(source.kind) != expectedKind) {
        return {};
    }
    m_state = State::KeyboardActive;
    m_kind = expectedKind;
    m_source = source;
    m_displacement = {};
    m_keyboardDetachSelected = false;
    return {.consumed = true, .intents = {intent(IntentPhase::Begin)}};
}

InteractionIntent InteractionController::intent(IntentPhase phase, const QPointF &position) const
{
    return {.kind = m_kind,
            .phase = phase,
            .source = m_source,
            .target = m_previewTarget,
            .position = position,
            // AGENT-CONTRACT: HybridContainerPlacement and divider geometry
            // apply each intent to a fixed interaction baseline. Delta is
            // therefore total logical displacement, never an event increment.
            .delta = m_displacement};
}

bool InteractionController::pointerBindingMatches(const PointerEvent &event) const
{
    // Exact matching avoids hijacking accessibility or user-defined chords
    // that merely contain the QindaQt gesture modifiers.
    return event.modifiers == m_bindings.pointerModifiers
        && event.buttons.testFlag(m_bindings.pointerButton);
}

DockZone InteractionController::zoneForKey(Qt::Key key) const
{
    switch (key) {
    case Qt::Key_Left:
        return DockZone::Left;
    case Qt::Key_Right:
        return DockZone::Right;
    case Qt::Key_Up:
        return DockZone::Top;
    case Qt::Key_Down:
        return DockZone::Bottom;
    case Qt::Key_T:
        return DockZone::Tab;
    default:
        return DockZone::None;
    }
}

QPointF InteractionController::displacementForKey(Qt::Key key) const
{
    switch (key) {
    case Qt::Key_Left:
        return {-m_bindings.keyboardStep, 0.0};
    case Qt::Key_Right:
        return {m_bindings.keyboardStep, 0.0};
    case Qt::Key_Up:
        return {0.0, -m_bindings.keyboardStep};
    case Qt::Key_Down:
        return {0.0, m_bindings.keyboardStep};
    default:
        return {};
    }
}

bool InteractionController::displacementKeyApplies(Qt::Key key) const
{
    const bool horizontal = key == Qt::Key_Left || key == Qt::Key_Right;
    const bool vertical = key == Qt::Key_Up || key == Qt::Key_Down;
    if (!horizontal && !vertical) {
        return false;
    }
    if (m_kind != InteractionKind::ContainerResize) {
        return true;
    }

    const bool hasHorizontalEdge = m_source.edges.testFlag(Qt::LeftEdge)
        || m_source.edges.testFlag(Qt::RightEdge);
    const bool hasVerticalEdge = m_source.edges.testFlag(Qt::TopEdge)
        || m_source.edges.testFlag(Qt::BottomEdge);
    return (horizontal && hasHorizontalEdge) || (vertical && hasVerticalEdge);
}

void InteractionController::reset()
{
    m_state = State::Idle;
    m_kind = InteractionKind::None;
    m_source = {};
    m_previewTarget = {};
    m_pressPosition = {};
    m_lastPosition = {};
    m_displacement = {};
    m_keyboardDetachSelected = false;
}

} // namespace QindaQt::HybridInput
