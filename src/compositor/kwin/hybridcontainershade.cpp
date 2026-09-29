// SPDX-License-Identifier: GPL-3.0-or-later
//
// Whole-container roll-up for HybridContainerPlacementController: shade,
// unshade, and the strip frame that stands in for the container while it is
// rolled up (ADR-0099, ADR-0139, ADR-0189).
//
// AGENT-NOTE: split out of hybridcontainerplacement.cpp, which was over its
// 600-line shape limit. This is a cohesive slice — nothing here changes the
// drag, resize, maximize or aspect-pin state machines (it only reads whether
// the container is maximized) — so the split is by behaviour rather than by
// line count. The members it uses
// (m_shadeStripFrames, m_shadeRestoreSizes, m_layout, m_workArea, m_changed,
// and isMaximized() for ADR-0282's maximized roll-up) stay private
// to the controller; this is the same class, a second translation unit.

#include "hybridcontainerplacement.h"

#include "hybridshadestripgeometry.h"

#include "qindaqt/hybrid_chrome/chromeshadedbadge.h"
#include "qindaqt/hybrid_chrome/chrometypes.h"

#include <QtMath>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

// AGENT-GUARD: kept in sync with ChromeMetrics defaults and with the copy in
// hybridcontainerplacement.cpp's own anonymous namespace. The chrome layout
// engine rejects a request whose inner height is not strictly greater than
// metrics.titleBarHeight, so a strip exactly titleBarHeight + 2*outerBorder
// tall would make every subsequent chrome-plan rebuild fail.
int shadedOuterHeight()
{
    const HybridChrome::ChromeMetrics metrics;
    return qMax(1, qCeil(metrics.titleBarHeight + 2.0 * metrics.outerBorder + 1.0));
}

} // namespace

bool HybridContainerPlacementController::shade(
    const QString &containerId, QString *error)
{
    if (isShaded(containerId)) {
        return true;
    }
    // ADR-0282: a maximized container rolls up too and stays maximized; its
    // strip starts at the maximized frame's top-left and unshade() returns it
    // to the current maximize area. Refusing here left a maximized group
    // impossible to roll up from its title, wheel, menu, or double-click.
    const auto current = m_layout ? m_layout(containerId) : std::nullopt;
    if (!current || !current->outerFrame.isValid()) {
        assignError(error, QStringLiteral("container has no valid frame to shade"));
        return false;
    }
    // AGENT-GUARD: The real committed layout is never reflowed for shade
    // (see ADR-0099's follow-up correction): member windows keep their exact
    // frame so no live app is resized to fake being hidden. The strip frame
    // is purely this controller's own bookkeeping; the KWin adapter is
    // responsible for actually hiding member content/input. ADR-0139: the
    // strip is a content-sized badge anchored at the frame's left edge.
    const auto *snapshot = container(containerId);
    const auto tabCount = snapshot ? snapshot->pages().size() : qsizetype{1};
    // ADR-0189: this controller has no page titles and no font, so it reserves
    // the label minimum. The session resizes the strip to the measured label
    // immediately afterwards through resizeShadeStrip(), and again whenever a
    // title changes while rolled up.
    m_shadeStripFrames.insert(
        containerId,
        QRect(current->outerFrame.topLeft(),
              QSize(HybridShadeStripGeometry::stripWidth(
                        tabCount, current->outerFrame,
                        HybridChrome::ChromeShadedBadge::LabelMinimumWidth),
                    shadedOuterHeight())));
    m_shadeRestoreSizes.insert(containerId, current->outerFrame.size());
    if (m_changed) {
        m_changed();
    }
    return true;
}

bool HybridContainerPlacementController::resizeShadeStrip(const QString &containerId,
                                                          const qreal labelWidth)
{
    const auto existing = m_shadeStripFrames.constFind(containerId);
    if (existing == m_shadeStripFrames.cend()) {
        return false;
    }
    const auto *snapshot = container(containerId);
    const auto tabCount = snapshot ? snapshot->pages().size() : qsizetype{1};
    // The strip may never outgrow the frame it was shaded from, which is the
    // restore size rather than the strip's own current frame.
    const auto restore = m_shadeRestoreSizes.constFind(containerId);
    const QRect bound = restore != m_shadeRestoreSizes.cend()
        ? QRect(existing->topLeft(), *restore)
        : *existing;
    const int width = HybridShadeStripGeometry::stripWidth(tabCount, bound, labelWidth);
    if (width == existing->width()) {
        return false;
    }
    m_shadeStripFrames.insert(containerId,
                              QRect(existing->topLeft(),
                                    QSize(width, existing->height())));
    if (m_changed) {
        m_changed();
    }
    return true;
}

bool HybridContainerPlacementController::unshade(
    const QString &containerId, QString *error)
{
    const auto stripFound = m_shadeStripFrames.constFind(containerId);
    const auto sizeFound = m_shadeRestoreSizes.constFind(containerId);
    if (stripFound == m_shadeStripFrames.cend()
        || sizeFound == m_shadeRestoreSizes.cend()) {
        assignError(error, QStringLiteral("container is not shaded"));
        return false;
    }
    // AGENT-CONTRACT: the strip's current position may have moved under drag
    // since shade() was called; unrolling restores the original size at that
    // (possibly moved) position, so "moving the rolled strip" genuinely
    // relocates where the container reappears. This is the one legitimate
    // reflow in the shade lifecycle: a real, intentional full restore.
    //
    // ADR-0282: a container still maximized unrolls into the maximize area as
    // it is now (the panels may have changed while it was rolled up). Moving
    // the strip leaves maximize, so a maximized strip has not moved.
    const QRect workArea = isMaximized(containerId) && m_workArea
        ? m_workArea(containerId) : QRect{};
    const QPoint restorePosition = m_gatherRestorePositions.value(
        containerId, stripFound->topLeft());
    const QRect restoreFrame = workArea.isValid()
        ? workArea : QRect(restorePosition, *sizeFound);
    if (!reflow(containerId, restoreFrame, error)) {
        return false;
    }
    m_shadeStripFrames.erase(stripFound);
    m_shadeRestoreSizes.remove(containerId);
    m_gatherRestorePositions.remove(containerId);
    m_moveDrags.remove(containerId);
    return true;
}

bool HybridContainerPlacementController::placeShadeStripForGather(
    const QString &containerId, const QPoint &topLeft)
{
    auto found = m_shadeStripFrames.find(containerId);
    if (found == m_shadeStripFrames.end()) {
        return false;
    }
    if (found->topLeft() == topLeft) {
        return true;
    }
    if (!m_gatherRestorePositions.contains(containerId)) {
        m_gatherRestorePositions.insert(containerId, found->topLeft());
    }
    found->moveTopLeft(topLeft);
    return true;
}

std::optional<QRect> HybridContainerPlacementController::shadedFrame(
    const QString &containerId) const
{
    const auto found = m_shadeStripFrames.constFind(containerId);
    return found == m_shadeStripFrames.cend() ? std::nullopt
                                              : std::optional<QRect>(*found);
}

QStringList HybridContainerPlacementController::shadedContainerIds() const
{
    return m_shadeStripFrames.keys();
}

} // namespace QindaQt::Compositor::KWinIntegration
