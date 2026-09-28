// SPDX-License-Identifier: GPL-3.0-or-later
//
// Whole-container maximize for HybridContainerPlacementController: maximize,
// restore, following the maximize area, and leaving maximize implicitly by
// moving, resizing, or dragging a rolled-up strip (ADR-0282).
//
// AGENT-NOTE: a further translation unit of the same class, split out of
// hybridcontainerplacement.cpp (at its shape limit) the same way
// hybridcontainershade.cpp and hybridcontainerrescue.cpp are. The state it
// owns is m_maximizeRestoreFrames; the drag bookkeeping it touches
// (m_moveDrags, FrameDrag::resumeMaximizeRestore) is shared with the move and
// resize state machines in hybridcontainerplacement.cpp.

#include "hybridcontainerplacement.h"

#include <QtMath>

#include <algorithm>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

// Where a maximized container lands when a title drag pulls it out of
// maximize: its restore size, keeping the press point at the same fraction
// of the title width and the title row at the same height, the way every
// other desktop "drag to unmaximize" works. A press that is not on the
// maximized frame at all (no pointer position, e.g. an injected intent) centers
// the restored frame horizontally instead of flinging it to the press point.
[[nodiscard]] QRect restoredFrameUnderPress(const QRect &maximized,
                                            const QSize &restoreSize,
                                            const QPointF &press)
{
    if (!QRectF(maximized).contains(press)) {
        return QRect(QPoint(maximized.x() + (maximized.width() - restoreSize.width()) / 2,
                            maximized.y()),
                     restoreSize);
    }
    const qreal fraction = maximized.width() > 0
        ? std::clamp((press.x() - maximized.x()) / qreal(maximized.width()), 0.0, 1.0)
        : 0.5;
    return QRect(QPoint(qRound(press.x() - fraction * restoreSize.width()), maximized.y()),
                 restoreSize);
}

} // namespace

bool HybridContainerPlacementController::maximize(
    const QString &containerId, QString *error)
{
    if (isMaximized(containerId)) {
        return true;
    }
    if (isShaded(containerId)) {
        assignError(error, QStringLiteral("unroll a shaded container before maximizing it"));
        return false;
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
    // AGENT-GUARD: a rolled-up container's members are never reflowed
    // (ADR-0099). Restoring while rolled up only re-targets the unroll: the
    // strip moves to the restore position and unshade() brings back the
    // restore size there.
    if (const auto strip = m_shadeStripFrames.find(containerId);
        strip != m_shadeStripFrames.end()) {
        m_maximizeRestoreFrames.remove(containerId);
        strip->moveTopLeft(frame.topLeft());
        m_shadeRestoreSizes.insert(containerId, frame.size());
        if (m_changed) {
            m_changed();
        }
        return true;
    }
    m_maximizeRestoreFrames.erase(found);
    if (!reflow(containerId, frame, error)) {
        m_maximizeRestoreFrames.insert(containerId, frame);
        return false;
    }
    return true;
}

QStringList HybridContainerPlacementController::refreshMaximizedAreas()
{
    QStringList failures;
    auto containerIds = m_maximizeRestoreFrames.keys();
    containerIds.sort();
    for (const auto &containerId : std::as_const(containerIds)) {
        // A rolled-up maximized container is never reflowed while rolled up;
        // unshade() re-resolves the maximize area when it unrolls.
        if (isShaded(containerId)) {
            continue;
        }
        const auto current = m_layout ? m_layout(containerId) : std::nullopt;
        const auto workArea = m_workArea ? m_workArea(containerId) : QRect{};
        if (!current || !workArea.isValid()) {
            failures.append(QStringLiteral("container '%1' has no valid maximize area")
                                .arg(containerId));
            continue;
        }
        if (current->outerFrame == workArea) {
            continue;
        }
        QString error;
        if (!reflow(containerId, workArea, &error)) {
            failures.append(QStringLiteral("container '%1': %2")
                                .arg(containerId, error));
        }
    }
    return failures;
}

DirectInteractionResult HybridContainerPlacementController::beginMaximizedMove(
    const QString &containerId, const HybridInput::InteractionIntent &intent)
{
    const auto current = m_layout ? m_layout(containerId) : std::nullopt;
    const auto restoreFrame = m_maximizeRestoreFrames.value(containerId);
    if (!container(containerId) || !current || !current->outerFrame.isValid()
        || !restoreFrame.isValid()) {
        m_refusedGestures.insert(containerId);
        return DirectInteractionResult::rejected(
            QStringLiteral("maximized container has no placement to restore"));
    }
    // AGENT-GUARD: this used to refuse the whole gesture ("restore a
    // maximized container before moving it"), silently. A maximized group
    // then could not be moved -- nor rolled up -- by any pointer gesture,
    // which users read as a container that had stopped working (ADR-0282).
    const QRect maximized = current->outerFrame;
    const QRect baseline = restoredFrameUnderPress(
        maximized, restoreFrame.size(), intent.position - intent.delta);
    const QRect applied = baseline.translated(qRound(intent.delta.x()),
                                              qRound(intent.delta.y()));
    m_moveDrags.remove(containerId);
    m_maximizeRestoreFrames.remove(containerId);
    QString error;
    if (!reflow(containerId, applied, &error)) {
        m_maximizeRestoreFrames.insert(containerId, restoreFrame);
        m_refusedGestures.insert(containerId);
        return DirectInteractionResult::rejected(std::move(error));
    }
    m_moveDrags.insert(containerId, FrameDrag{.baseline = baseline,
                                              .applied = applied,
                                              .edges = {},
                                              .resumeMaximizeRestore = restoreFrame,
                                              .cancelFrame = maximized});
    return DirectInteractionResult::handled();
}

DirectInteractionResult HybridContainerPlacementController::cancelMaximizedDrag(
    const QString &containerId, const FrameDrag &drag)
{
    // The caller already erased the drag. Reinstate maximize first so a
    // failed reflow still leaves the container reporting the state its frame
    // is closest to, and a later work-area refresh can retry the fit.
    m_maximizeRestoreFrames.insert(containerId, *drag.resumeMaximizeRestore);
    const auto workArea = m_workArea ? m_workArea(containerId) : QRect{};
    const QRect target = workArea.isValid() ? workArea : drag.cancelFrame;
    QString error;
    if (drag.applied != target && !reflow(containerId, target, &error)) {
        return DirectInteractionResult::rejected(std::move(error));
    }
    return DirectInteractionResult::handled();
}

} // namespace QindaQt::Compositor::KWinIntegration
