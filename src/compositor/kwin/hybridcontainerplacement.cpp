// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridcontainerplacement.h"

#include <QtMath>

#include <algorithm>
#include <cmath>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

constexpr int MinimumOuterWidth = 240;
constexpr int MinimumOuterHeight = 160;
constexpr double MinimumSplitRatio = 0.01;
constexpr double MaximumSplitRatio = 0.99;

QString interactionContainerId(const HybridInput::InteractionIntent &intent)
{
    return intent.source.containerId;
}

HybridInput::IntentPhase intentPhase(HybridChrome::DragPhase phase)
{
    switch (phase) {
    case HybridChrome::DragPhase::Begin:
        return HybridInput::IntentPhase::Begin;
    case HybridChrome::DragPhase::Update:
        return HybridInput::IntentPhase::Update;
    case HybridChrome::DragPhase::Commit:
        return HybridInput::IntentPhase::Commit;
    case HybridChrome::DragPhase::Cancel:
        return HybridInput::IntentPhase::Cancel;
    }
    return HybridInput::IntentPhase::Cancel;
}

} // namespace

HybridContainerPlacementController::HybridContainerPlacementController(
    HybridTopologyLookup topology,
    HybridCommittedLayoutLookup layout,
    HybridContainerReflow reflowHandler,
    HybridWorkAreaLookup workArea,
    HybridPlacementChangedSink changed)
    : m_topology(std::move(topology))
    , m_layout(std::move(layout))
    , m_reflow(std::move(reflowHandler))
    , m_workArea(std::move(workArea))
    , m_changed(std::move(changed))
{
}

void HybridContainerPlacementController::assignError(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
}

const Core::WindowContainer *HybridContainerPlacementController::container(
    const QString &containerId) const
{
    return m_topology ? m_topology().container(containerId) : nullptr;
}

bool HybridContainerPlacementController::reflow(const QString &containerId,
                                                const QRect &frame,
                                                QString *error)
{
    const auto *snapshot = container(containerId);
    if (!snapshot || !m_reflow) {
        assignError(error, QStringLiteral("container placement dependencies are unavailable"));
        return false;
    }
    const auto result = m_reflow(*snapshot, frame);
    if (!result.succeeded) {
        assignError(error, result.message);
        return false;
    }
    if (m_changed) {
        m_changed();
    }
    return true;
}

bool HybridContainerPlacementController::beginDrag(
    QHash<QString, FrameDrag> &drags,
    const QString &containerId,
    Qt::Edges edges,
    QString *error)
{
    if (drags.contains(containerId)) {
        assignError(error, QStringLiteral("container already has an active placement drag"));
        return false;
    }
    const auto current = m_layout ? m_layout(containerId) : std::nullopt;
    if (!container(containerId) || !current || !current->outerFrame.isValid()) {
        assignError(error, QStringLiteral("container has no committed placement"));
        return false;
    }
    drags.insert(containerId, FrameDrag{current->outerFrame, current->outerFrame, edges});
    return true;
}

DirectInteractionResult HybridContainerPlacementController::handleMove(
    const HybridInput::InteractionIntent &intent)
{
    if (intent.kind != HybridInput::InteractionKind::ContainerMove
        || intent.source.kind != HybridInput::HitKind::OuterTitle
        || interactionContainerId(intent).isEmpty()) {
        return DirectInteractionResult::rejected(
            QStringLiteral("container move intent is invalid"));
    }
    const auto &containerId = interactionContainerId(intent);
    QString error;
    if (intent.phase == HybridInput::IntentPhase::Begin) {
        if (isMaximized(containerId)) {
            return DirectInteractionResult::rejected(
                QStringLiteral("restore a maximized container before moving it"));
        }
        return beginDrag(m_moveDrags, containerId, {}, &error)
            ? DirectInteractionResult::handled()
            : DirectInteractionResult::rejected(std::move(error));
    }

    auto found = m_moveDrags.find(containerId);
    if (found == m_moveDrags.end()) {
        return DirectInteractionResult::rejected(
            QStringLiteral("container move has no active baseline"));
    }
    if (intent.phase == HybridInput::IntentPhase::Cancel) {
        const auto baseline = found->baseline;
        const bool changed = found->applied != baseline;
        m_moveDrags.erase(found);
        if (changed && !reflow(containerId, baseline, &error)) {
            return DirectInteractionResult::rejected(std::move(error));
        }
        return DirectInteractionResult::handled();
    }

    const QRect requested = found->baseline.translated(
        qRound(intent.delta.x()), qRound(intent.delta.y()));
    if (requested != found->applied) {
        if (!reflow(containerId, requested, &error)) {
            // AGENT-GUARD: InteractionController ends the grab after every
            // Commit, including a rejected one. Retaining this baseline would
            // make the next legitimate Begin look like a duplicate drag.
            if (intent.phase == HybridInput::IntentPhase::Commit) {
                m_moveDrags.erase(found);
            }
            return DirectInteractionResult::rejected(std::move(error));
        }
        found->applied = requested;
    }
    if (intent.phase == HybridInput::IntentPhase::Commit) {
        m_moveDrags.erase(found);
    }
    return DirectInteractionResult::handled();
}

DirectInteractionResult HybridContainerPlacementController::handleResize(
    const HybridInput::InteractionIntent &intent)
{
    if (intent.kind != HybridInput::InteractionKind::ContainerResize
        || intent.source.kind != HybridInput::HitKind::OuterResize
        || !intent.source.isValid()) {
        return DirectInteractionResult::rejected(
            QStringLiteral("container resize intent is invalid"));
    }

    const auto &containerId = interactionContainerId(intent);
    QString error;
    if (intent.phase == HybridInput::IntentPhase::Begin) {
        if (isMaximized(containerId)) {
            return DirectInteractionResult::rejected(
                QStringLiteral("restore a maximized container before resizing it"));
        }
        return beginDrag(m_resizeDrags, containerId, intent.source.edges, &error)
            ? DirectInteractionResult::handled()
            : DirectInteractionResult::rejected(std::move(error));
    }

    auto found = m_resizeDrags.find(containerId);
    if (found == m_resizeDrags.end()) {
        return DirectInteractionResult::rejected(
            QStringLiteral("container resize has no active baseline"));
    }
    // AGENT-GUARD: The selected edge/corner is part of the Begin baseline.
    // Allowing it to change mid-grab would reinterpret cumulative deltas.
    if (found->edges != intent.source.edges) {
        return DirectInteractionResult::rejected(
            QStringLiteral("container resize edges changed during the interaction"));
    }
    if (intent.phase == HybridInput::IntentPhase::Cancel) {
        const auto baseline = found->baseline;
        const bool changed = found->applied != baseline;
        m_resizeDrags.erase(found);
        if (changed && !reflow(containerId, baseline, &error)) {
            return DirectInteractionResult::rejected(std::move(error));
        }
        return DirectInteractionResult::handled();
    }

    const auto requested = resizedFrame(*found, intent.delta);
    if (requested != found->applied) {
        if (!reflow(containerId, requested, &error)) {
            // See the matching move path: a failed terminal phase must not
            // poison the controller after the input layer releases its grab.
            if (intent.phase == HybridInput::IntentPhase::Commit) {
                m_resizeDrags.erase(found);
            }
            return DirectInteractionResult::rejected(std::move(error));
        }
        found->applied = requested;
    }
    if (intent.phase == HybridInput::IntentPhase::Commit) {
        m_resizeDrags.erase(found);
    }
    return DirectInteractionResult::handled();
}

DividerGeometryResult HybridContainerPlacementController::dividerRatio(
    const HybridInput::InteractionIntent &intent) const
{
    if (intent.kind != HybridInput::InteractionKind::DividerResize
        || intent.source.containerId.isEmpty() || intent.source.dividerId.isEmpty()) {
        return DividerGeometryResult::unavailable(
            QStringLiteral("divider resize intent is invalid"));
    }
    const auto current = m_layout ? m_layout(intent.source.containerId) : std::nullopt;
    const auto *snapshot = container(intent.source.containerId);
    const auto *node = snapshot ? snapshot->findNode(intent.source.dividerId) : nullptr;
    if (!current || !node || !node->isSplit()) {
        return DividerGeometryResult::unavailable(
            QStringLiteral("divider has no committed placement"));
    }
    const auto placement = current->activePage.splits.constFind(intent.source.dividerId);
    if (placement == current->activePage.splits.cend()) {
        return DividerGeometryResult::unavailable(
            QStringLiteral("divider is not on the active page"));
    }

    const bool horizontal = *node->orientation() == Core::SplitOrientation::Horizontal;
    const int primaryExtent = horizontal ? placement->frame.width()
                                         : placement->frame.height();
    const int dividerExtent = horizontal ? placement->dividerFrame.width()
                                         : placement->dividerFrame.height();
    const int distributable = primaryExtent - dividerExtent;
    if (distributable <= 0) {
        return DividerGeometryResult::unavailable(
            QStringLiteral("divider frame has no distributable extent"));
    }
    const qreal delta = horizontal ? intent.delta.x() : intent.delta.y();
    const int currentFirst = horizontal ? placement->firstTileFrame.width()
                                        : placement->firstTileFrame.height();
    const double requested = (double(currentFirst) + double(delta))
        / double(distributable);
    return DividerGeometryResult::resolved(
        std::clamp(requested, MinimumSplitRatio, MaximumSplitRatio));
}

QRect HybridContainerPlacementController::resizedFrame(
    const FrameDrag &drag, const QPointF &delta)
{
    int x = drag.baseline.x();
    int y = drag.baseline.y();
    int width = drag.baseline.width();
    int height = drag.baseline.height();
    const int dx = qRound(delta.x());
    const int dy = qRound(delta.y());

    if (drag.edges.testFlag(Qt::LeftEdge)) {
        x += dx;
        width -= dx;
    } else if (drag.edges.testFlag(Qt::RightEdge)) {
        width += dx;
    }
    if (drag.edges.testFlag(Qt::TopEdge)) {
        y += dy;
        height -= dy;
    } else if (drag.edges.testFlag(Qt::BottomEdge)) {
        height += dy;
    }
    if (width < MinimumOuterWidth) {
        if (drag.edges.testFlag(Qt::LeftEdge)) {
            x -= MinimumOuterWidth - width;
        }
        width = MinimumOuterWidth;
    }
    if (height < MinimumOuterHeight) {
        if (drag.edges.testFlag(Qt::TopEdge)) {
            y -= MinimumOuterHeight - height;
        }
        height = MinimumOuterHeight;
    }
    return {x, y, width, height};
}

bool HybridContainerPlacementController::handleOuterResize(
    const QString &containerId,
    const HybridChrome::ChromeDragEvent &event,
    QString *error)
{
    if (event.target.kind != HybridChrome::HitKind::OuterResize
        || containerId.isEmpty()) {
        assignError(error, QStringLiteral("outer resize event is invalid"));
        return false;
    }
    const HybridInput::InteractionIntent intent{
        .kind = HybridInput::InteractionKind::ContainerResize,
        .phase = intentPhase(event.phase),
        .source = {HybridInput::HitKind::OuterResize,
                   containerId, {}, {}, event.target.resizeEdges},
        .target = {},
        .position = event.globalPosition,
        .delta = event.delta,
    };
    const auto result = handleResize(intent);
    if (!result.accepted) {
        assignError(error, result.message);
    }
    return result.accepted;
}

bool HybridContainerPlacementController::maximize(
    const QString &containerId, QString *error)
{
    if (isMaximized(containerId)) {
        return true;
    }
    const auto current = m_layout ? m_layout(containerId) : std::nullopt;
    const auto workArea = m_workArea ? m_workArea(containerId) : QRect{};
    if (!current || !workArea.isValid()) {
        assignError(error, QStringLiteral("container has no valid maximize area"));
        return false;
    }
    m_maximizeRestoreFrames.insert(containerId, current->outerFrame);
    if (!reflow(containerId, workArea, error)) {
        m_maximizeRestoreFrames.remove(containerId);
        return false;
    }
    return true;
}

bool HybridContainerPlacementController::restore(
    const QString &containerId, QString *error)
{
    const auto found = m_maximizeRestoreFrames.constFind(containerId);
    if (found == m_maximizeRestoreFrames.cend()) {
        assignError(error, QStringLiteral("container has no maximize restore frame"));
        return false;
    }
    const auto frame = *found;
    m_maximizeRestoreFrames.erase(found);
    if (!reflow(containerId, frame, error)) {
        m_maximizeRestoreFrames.insert(containerId, frame);
        return false;
    }
    return true;
}

void HybridContainerPlacementController::forgetContainer(
    const QString &containerId) noexcept
{
    m_moveDrags.remove(containerId);
    m_resizeDrags.remove(containerId);
    m_maximizeRestoreFrames.remove(containerId);
}

void HybridContainerPlacementController::cancelAll() noexcept
{
    m_moveDrags.clear();
    m_resizeDrags.clear();
}

} // namespace QindaQt::Compositor::KWinIntegration
