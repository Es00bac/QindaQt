// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridcontainerplacement.h"

#include "hybridshadestripgeometry.h"

#include "qindaqt/hybrid_chrome/chromeshadedbadge.h"
#include "qindaqt/hybrid_chrome/chrometypes.h"

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

// AGENT-NOTE: whole-container roll-up (shade/unshade/resizeShadeStrip and the
// strip frame) lives in hybridcontainershade.cpp and whole-container maximize
// (maximize/restore/area refresh and leaving maximize by moving or resizing,
// ADR-0282) in hybridcontainermaximize.cpp: further translation units of this
// same class, because this file is at its shape limit.
QString interactionContainerId(const HybridInput::InteractionIntent &intent)
{
    return intent.source.containerId;
}

// AGENT-NOTE: The aspect lock pins the content-area ratio, so its math needs
// the same chrome offsets sceneMetrics() bakes into contentInsets from the
// default ChromeMetrics (outerBorder 1, titleBarHeight 28). Keep these three
// helpers in sync with that construction; see ADR-0162.
qreal aspectChromeHorizontal()
{
    const HybridChrome::ChromeMetrics metrics;
    return 2.0 * metrics.outerBorder;
}

qreal aspectChromeVertical()
{
    const HybridChrome::ChromeMetrics metrics;
    return 2.0 * metrics.outerBorder + metrics.titleBarHeight;
}

// AGENT-CONTRACT: A pinned resize always keeps the content-area ratio exact
// (within integer rounding): the drag's dominant axis leads, the follower
// extent is derived from the ratio, and the follower edge stays anchored at
// the baseline edge the drag does not move. Minimum frame sizes win over the
// lock: the clamped extent becomes the leader and the follower is re-derived
// from it, so the clamp point still satisfies the ratio. Violating this by
// applying raw drag deltas would silently distort every grouped game window.
void applyAspectRatioLock(const QRect &baseline, Qt::Edges edges, double ratio,
                          int &x, int &y, int &width, int &height)
{
    const qreal chromeHorizontal = aspectChromeHorizontal();
    const qreal chromeVertical = aspectChromeVertical();
    const bool horizontalDrag = edges.testFlag(Qt::LeftEdge)
        || edges.testFlag(Qt::RightEdge);
    const bool verticalDrag = edges.testFlag(Qt::TopEdge)
        || edges.testFlag(Qt::BottomEdge);
    // Width leads on horizontal-edge and corner drags; height leads only for
    // a pure vertical-edge drag, so a corner drag stays predictable.
    const bool widthLeads = horizontalDrag || !verticalDrag;
    const auto outerWidthFromContentHeight = [&](qreal contentHeight) {
        return qRound(contentHeight * ratio + chromeHorizontal);
    };
    const auto outerHeightFromContentWidth = [&](qreal contentWidth) {
        return qRound(contentWidth / ratio + chromeVertical);
    };

    int newWidth = width;
    int newHeight = height;
    if (widthLeads) {
        newHeight = outerHeightFromContentWidth(width - chromeHorizontal);
        if (newHeight < MinimumOuterHeight) {
            newHeight = MinimumOuterHeight;
            newWidth = outerWidthFromContentHeight(newHeight - chromeVertical);
        }
    } else {
        newWidth = outerWidthFromContentHeight(height - chromeVertical);
        if (newWidth < MinimumOuterWidth) {
            newWidth = MinimumOuterWidth;
            newHeight = outerHeightFromContentWidth(newWidth - chromeHorizontal);
        }
    }

    // Keep the follower's baseline edge fixed. When width leads, the vertical
    // position follows the lock: dragging the top edge grows/shrinks downward
    // from the baseline bottom; otherwise the top stays put. Symmetrically
    // for height leading with the left/right edges.
    if (newHeight != height && edges.testFlag(Qt::TopEdge)) {
        y = baseline.y() + baseline.height() - newHeight;
    }
    if (newWidth != width && edges.testFlag(Qt::LeftEdge)) {
        x = baseline.x() + baseline.width() - newWidth;
    }
    width = newWidth;
    height = newHeight;
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
    // AGENT-GUARD: A Begin always starts a new gesture (ADR-0282). The input
    // layer holds at most one grab per container, so an entry that is still
    // here belongs to a gesture whose Commit/Cancel never arrived (a commit
    // refused upstream, a revoked chrome publication, a lost release).
    // Refusing the new Begin made that container permanently immovable until
    // all of its windows were dragged out and it dissolved. The abandoned
    // gesture keeps the frame it last applied: that is what the user sees.
    drags.remove(containerId);
    const auto current = m_layout ? m_layout(containerId) : std::nullopt;
    if (!container(containerId) || !current || !current->outerFrame.isValid()) {
        assignError(error, QStringLiteral("container has no committed placement"));
        return false;
    }
    drags.insert(containerId, FrameDrag{.baseline = current->outerFrame,
                                        .applied = current->outerFrame,
                                        .edges = edges,
                                        .resumeMaximizeRestore = std::nullopt,
                                        .cancelFrame = current->outerFrame});
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
    if (isShaded(containerId)) {
        return handleShadedMove(containerId, intent);
    }
    QString error;
    if (intent.phase == HybridInput::IntentPhase::Begin) {
        m_refusedGestures.remove(containerId);
        if (isMaximized(containerId)) {
            return beginMaximizedMove(containerId, intent);
        }
        if (beginDrag(m_moveDrags, containerId, {}, &error)) {
            return DirectInteractionResult::handled();
        }
        m_refusedGestures.insert(containerId);
        return DirectInteractionResult::rejected(std::move(error));
    }

    auto found = m_moveDrags.find(containerId);
    if (found == m_moveDrags.end()) {
        // This gesture's Begin was already refused and said why. The
        // interaction controller keeps sending phases regardless, so absorb
        // them instead of repeating the refusal once per motion event.
        if (m_refusedGestures.contains(containerId)) {
            if (intent.phase != HybridInput::IntentPhase::Update) {
                m_refusedGestures.remove(containerId);
            }
            return DirectInteractionResult::handled();
        }
        return DirectInteractionResult::rejected(
            QStringLiteral("container move has no active baseline"));
    }
    if (intent.phase == HybridInput::IntentPhase::Cancel) {
        const FrameDrag drag = *found;
        m_moveDrags.erase(found);
        if (drag.resumeMaximizeRestore) {
            return cancelMaximizedDrag(containerId, drag);
        }
        if (drag.applied != drag.baseline && !reflow(containerId, drag.baseline, &error)) {
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

// AGENT-CONTRACT: A shaded container's real committed layout is frozen (no
// member is ever reflowed while shaded), so dragging the strip must never
// call reflow()/m_reflow: that would move real KWin member windows that are
// deliberately untouched. Instead this updates only m_shadeStripFrames,
// which the session reads to rebuild the shaded chrome plan. Reuses
// m_moveDrags for baseline/applied bookkeeping; a container cannot be both a
// normal drag and a shaded drag at once, so the shared key space is safe.
DirectInteractionResult HybridContainerPlacementController::handleShadedMove(
    const QString &containerId, const HybridInput::InteractionIntent &intent)
{
    if (intent.phase == HybridInput::IntentPhase::Begin) {
        // Supersedes an abandoned gesture exactly like beginDrag().
        m_moveDrags.remove(containerId);
        const auto baseline = m_shadeStripFrames.value(containerId);
        if (!baseline.isValid()) {
            return DirectInteractionResult::rejected(
                QStringLiteral("shaded container has no strip frame"));
        }
        m_moveDrags.insert(containerId, FrameDrag{.baseline = baseline,
                                                  .applied = baseline,
                                                  .edges = {},
                                                  .resumeMaximizeRestore = std::nullopt,
                                                  .cancelFrame = baseline});
        return DirectInteractionResult::handled();
    }

    auto found = m_moveDrags.find(containerId);
    if (found == m_moveDrags.end()) {
        return DirectInteractionResult::rejected(
            QStringLiteral("container move has no active baseline"));
    }
    if (intent.phase == HybridInput::IntentPhase::Cancel) {
        m_shadeStripFrames[containerId] = found->baseline;
        m_moveDrags.erase(found);
        if (m_changed) {
            m_changed();
        }
        return DirectInteractionResult::handled();
    }

    const QRect requested = found->baseline.translated(
        qRound(intent.delta.x()), qRound(intent.delta.y()));
    if (requested != found->applied) {
        m_shadeStripFrames[containerId] = requested;
        found->applied = requested;
        if (m_changed) {
            m_changed();
        }
    }
    if (intent.phase == HybridInput::IntentPhase::Commit) {
        const bool moved = found->applied != found->baseline;
        m_moveDrags.erase(found);
        // ADR-0282: a rolled-up maximized container that was actually moved
        // leaves maximize, so unrolling restores its restore size where the
        // strip was put rather than snapping back to the maximize area.
        if (moved) {
            m_maximizeFractions.remove(containerId);
            if (const auto restoreFrame = m_maximizeRestoreFrames.take(containerId);
                restoreFrame.isValid()) {
                m_shadeRestoreSizes.insert(containerId, restoreFrame.size());
            }
        }
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
        m_refusedGestures.remove(containerId);
        // AGENT-GUARD: A shaded outer frame has no content to expose; letting
        // an outer-edge drag resize it would silently unshade or produce a
        // frame the compositor never solved a real layout for. Move remains
        // allowed while shaded (that is the point of a movable strip).
        if (isShaded(containerId)) {
            m_refusedGestures.insert(containerId);
            return DirectInteractionResult::rejected(
                QStringLiteral("unroll a shaded container before resizing it"));
        }
        // ADR-0282: resizing leaves maximize in place (chrome hides the resize
        // border while maximized, so this is the keyboard resize): the
        // gesture starts from the maximized frame, and Cancel re-maximizes.
        const auto restoreFrame = m_maximizeRestoreFrames.take(containerId);
        const auto fraction=m_maximizeFractions.take(containerId);
        if (beginDrag(m_resizeDrags, containerId, intent.source.edges, &error)) {
            if (restoreFrame.isValid()) {
                auto &drag = m_resizeDrags[containerId];
                drag.resumeMaximizeRestore = restoreFrame;
                drag.resumeMaximizeFraction = fraction;
            }
            return DirectInteractionResult::handled();
        }
        if (restoreFrame.isValid()) {
            m_maximizeRestoreFrames.insert(containerId, restoreFrame);
            m_maximizeFractions.insert(containerId,fraction);
        }
        m_refusedGestures.insert(containerId);
        return DirectInteractionResult::rejected(std::move(error));
    }

    auto found = m_resizeDrags.find(containerId);
    if (found == m_resizeDrags.end()) {
        if (m_refusedGestures.contains(containerId)) {
            if (intent.phase != HybridInput::IntentPhase::Update) {
                m_refusedGestures.remove(containerId);
            }
            return DirectInteractionResult::handled();
        }
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
        const FrameDrag drag = *found;
        m_resizeDrags.erase(found);
        if (drag.resumeMaximizeRestore) {
            return cancelMaximizedDrag(containerId, drag);
        }
        if (drag.applied != drag.baseline && !reflow(containerId, drag.baseline, &error)) {
            return DirectInteractionResult::rejected(std::move(error));
        }
        return DirectInteractionResult::handled();
    }

    const auto requested = resizedFrame(*found, intent.delta,
                                        m_aspectPins.value(containerId));
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
    const FrameDrag &drag, const QPointF &delta,
    const std::optional<double> &pinnedContentRatio)
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
    if (pinnedContentRatio && *pinnedContentRatio > 0.0
        && std::isfinite(*pinnedContentRatio)) {
        applyAspectRatioLock(drag.baseline, drag.edges, *pinnedContentRatio,
                             x, y, width, height);
    }
    return {x, y, width, height};
}

bool HybridContainerPlacementController::setAspectRatioPin(
    const QString &containerId, std::optional<double> contentRatio, QString *error)
{
    if (!container(containerId)) {
        assignError(error, QStringLiteral("container is unknown"));
        return false;
    }
    if (!contentRatio) {
        m_aspectPins.remove(containerId);
        return true;
    }
    if (!std::isfinite(*contentRatio) || *contentRatio <= 0.0) {
        assignError(error, QStringLiteral("aspect ratio must be finite and positive"));
        return false;
    }
    m_aspectPins.insert(containerId, *contentRatio);
    return true;
}

std::optional<double> HybridContainerPlacementController::aspectRatioPin(
    const QString &containerId) const noexcept
{
    const auto found = m_aspectPins.constFind(containerId);
    return found == m_aspectPins.cend() ? std::nullopt
                                        : std::optional<double>(*found);
}

double HybridContainerPlacementController::contentAspectRatioForOuterFrame(
    const QRect &outerFrame) noexcept
{
    const qreal contentWidth = qreal(outerFrame.width()) - aspectChromeHorizontal();
    const qreal contentHeight = qreal(outerFrame.height()) - aspectChromeVertical();
    if (contentWidth <= 0.0 || contentHeight <= 0.0) {
        return 0.0;
    }
    return double(contentWidth / contentHeight);
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

void HybridContainerPlacementController::forgetContainer(
    const QString &containerId) noexcept
{
    m_moveDrags.remove(containerId);
    m_resizeDrags.remove(containerId);
    m_refusedGestures.remove(containerId);
    m_maximizeRestoreFrames.remove(containerId);
    m_maximizeFractions.remove(containerId);
    m_aspectPins.remove(containerId);
    m_shadeStripFrames.remove(containerId);
    m_shadeRestoreSizes.remove(containerId);
}

bool HybridContainerPlacementController::gestureRefused(
    const QString &containerId) const noexcept
{
    return m_refusedGestures.contains(containerId);
}

void HybridContainerPlacementController::cancelAll() noexcept
{
    m_refusedGestures.clear();
    m_moveDrags.clear();
    m_resizeDrags.clear();
}

} // namespace QindaQt::Compositor::KWinIntegration
