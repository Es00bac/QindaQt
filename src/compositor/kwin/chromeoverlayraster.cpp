// SPDX-License-Identifier: GPL-3.0-or-later
#include "chromeoverlayraster.h"

#include "chromeplancomparison.h"

#include "qindaqt/hybrid_chrome/chromerenderer.h"

#include <QPainter>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

using HybridChrome::ChromeRenderPlan;

bool samePaintState(const HybridChrome::ChromePaintState &a,
                    const HybridChrome::ChromePaintState &b)
{
    return a.controlsHovered == b.controlsHovered && a.hoveredTarget == b.hoveredTarget
        && a.pressedTarget == b.pressedTarget;
}

QSize physicalSizeFor(const ChromeRenderPlan &plan)
{
    const qreal ratio = plan.devicePixelRatio;
    if (!plan.outerFrame.isValid() || !std::isfinite(ratio) || ratio <= 0.0) {
        return {};
    }
    return {qCeil(plan.outerFrame.width() * ratio), qCeil(plan.outerFrame.height() * ratio)};
}

} // namespace

ChromeOverlayRaster::Repaint ChromeOverlayRaster::setPlan(ChromeRenderPlan plan)
{
    if (!m_hasPlan || m_invalidated) {
        m_plan = std::move(plan);
        m_hasPlan = true;
        return repaintFor(false);
    }
    const auto differences = compareChromePlans(m_plan, plan);
    // Member titles live in each member's own decoration; carrying the new
    // text keeps the published model current without touching pixels.
    const bool painted = differences.testFlag(ChromePlanDifference::TitleText)
        || differences.testFlag(ChromePlanDifference::TabGeometry)
        || differences.testFlag(ChromePlanDifference::Structure);
    if (!painted) {
        m_plan = std::move(plan);
        ++m_counters.unchangedUpdates;
        return Repaint::None;
    }
    const bool bandEligible = !differences.testFlag(ChromePlanDifference::Structure)
        && rowGeometryInsideBand(m_plan) && rowGeometryInsideBand(plan);
    m_plan = std::move(plan);
    return repaintFor(bandEligible);
}

ChromeOverlayRaster::Repaint ChromeOverlayRaster::setPaintState(
    const HybridChrome::ChromePaintState &state)
{
    if (samePaintState(m_state, state) && !m_invalidated) {
        return Repaint::None;
    }
    m_state = state;
    if (!m_hasPlan) {
        return Repaint::None;
    }
    // Hover and press only restyle buttons, container controls and deck
    // cards, all of which live in the shared title row.
    return repaintFor(rowGeometryInsideBand(m_plan));
}

void ChromeOverlayRaster::invalidate() noexcept
{
    m_invalidated = true;
}

void ChromeOverlayRaster::clear() noexcept
{
    m_plan = {};
    m_state = {};
    m_body = {};
    m_band = {};
    m_physicalSize = {};
    m_hasPlan = false;
    m_invalidated = false;
}

ChromeOverlayRaster::Repaint ChromeOverlayRaster::repaintFor(bool bandEligible)
{
    if (bandEligible && !m_invalidated && !m_body.isNull()
        && physicalSizeFor(m_plan) == m_physicalSize && bandPixelHeight() > 0) {
        renderBand();
        return Repaint::Band;
    }
    m_invalidated = false;
    renderFull();
    return Repaint::Full;
}

int ChromeOverlayRaster::bandPixelHeight() const
{
    if (m_physicalSize.isEmpty()) {
        return 0;
    }
    const qreal top = m_plan.outerFrame.top();
    const qreal bottom = m_plan.shaded
        ? m_plan.outerFrame.bottom()
        : m_plan.outerTitleBar.bottom() + BandSpillMargin;
    const int rows = qCeil(std::max<qreal>(0.0, bottom - top) * m_plan.devicePixelRatio);
    return std::clamp(rows, 0, m_physicalSize.height());
}

bool ChromeOverlayRaster::rowGeometryInsideBand(const ChromeRenderPlan &plan) const
{
    if (plan.shaded) {
        // The rolled-up strip is the band in its entirety.
        return true;
    }
    const qreal rowBottom = plan.outerTitleBar.bottom();
    const auto inside = [rowBottom](const QRectF &rect) {
        return rect.isNull() || rect.bottom() <= rowBottom + 0.5;
    };
    for (const auto &tab : plan.tabs) {
        if (!inside(tab.rect)) {
            return false;
        }
    }
    for (const auto &button : plan.buttons) {
        if (!inside(button.rect)) {
            return false;
        }
    }
    for (const auto &control : plan.controls) {
        if (!inside(control.rect)) {
            return false;
        }
    }
    return inside(plan.tabStrip) && inside(plan.outerTitleDragRect)
        && inside(plan.titleTab) && inside(plan.deckPiece) && inside(plan.titleLabelRect)
        && inside(plan.indexBadgeRect) && inside(plan.badgeLabelRect);
}

void ChromeOverlayRaster::renderFull()
{
    m_body = {};
    m_band = {};
    m_physicalSize = physicalSizeFor(m_plan);
    if (m_physicalSize.isEmpty()) {
        return;
    }
    QImage full(m_physicalSize, QImage::Format_ARGB32_Premultiplied);
    full.setDevicePixelRatio(m_plan.devicePixelRatio);
    full.fill(Qt::transparent);
    {
        QPainter painter(&full);
        HybridChrome::ChromeRenderer::paint(painter, m_plan, m_state);
    }
    const int rows = bandPixelHeight();
    // AGENT-GUARD: the body's band rows must be fully transparent, not merely
    // overdrawn by the band: translucent chrome would otherwise show stale
    // title text through the band after the next band-only repaint.
    // Premultiplied transparent is all-zero bytes.
    for (int row = 0; row < rows; ++row) {
        std::memset(full.scanLine(row), 0, static_cast<size_t>(full.bytesPerLine()));
    }
    m_body = std::move(full);
    // AGENT-NOTE: the band is always its own band-sized render, never a crop
    // of the full image. QPainter strokes wide antialiased paths slightly
    // differently on differently sized devices (the focus glow's rounded
    // corners differ by one alpha step), so a cropped band followed by a
    // band-only repaint could flicker. Rendering it one way keeps every
    // repaint path pixel-identical to every other.
    m_band = paintBand();
    ++m_counters.fullRenders;
    m_counters.uploadedBytes += m_body.sizeInBytes() + m_band.sizeInBytes();
}

void ChromeOverlayRaster::renderBand()
{
    m_band = paintBand();
    ++m_counters.bandRenders;
    m_counters.uploadedBytes += m_band.sizeInBytes();
}

QImage ChromeOverlayRaster::paintBand() const
{
    QImage band(QSize(m_physicalSize.width(), bandPixelHeight()),
                QImage::Format_ARGB32_Premultiplied);
    band.setDevicePixelRatio(m_plan.devicePixelRatio);
    band.fill(Qt::transparent);
    // The same paint call as a full render into a shorter device; rows below
    // the band are discarded by the device bounds.
    QPainter painter(&band);
    HybridChrome::ChromeRenderer::paint(painter, m_plan, m_state);
    painter.end();
    return band;
}

QSizeF ChromeOverlayRaster::bodyLogicalSize() const noexcept
{
    return m_body.isNull() ? QSizeF{} : m_plan.outerFrame.size();
}

QSizeF ChromeOverlayRaster::bandLogicalSize() const noexcept
{
    if (m_band.isNull() || m_physicalSize.height() <= 0) {
        return {};
    }
    const QSizeF logical = m_plan.outerFrame.size();
    return {logical.width(),
            logical.height() * qreal(m_band.height()) / qreal(m_physicalSize.height())};
}

} // namespace QindaQt::Compositor::KWinIntegration
