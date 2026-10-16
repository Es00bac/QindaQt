// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_chrome/chrometypes.h"

#include <QImage>
#include <QSizeF>

namespace QindaQt::Compositor::KWinIntegration {

// Rasterizes one container's shared chrome as two images so that the common
// changes stay small (ADR-0383):
//
//   band  the top rows through the shared title row plus a small spill
//         margin; every title, tab, button, control and hover change lands
//         here;
//   body  the whole frame with the band rows left transparent.
//
// A plan that differs only in title text, tab geometry or hover state
// repaints and hands out only the band; an identical plan repaints nothing;
// anything else repaints both. Stacked at the same origin, band over body,
// the result is identical whichever repaint produced it, and matches one full
// ChromeRenderer::paint() to within one antialiasing step on wide strokes.
//
// Pure value object: no KWin, no timers. Images are replaced, never painted
// in place, so a consumer holding the previous image (a KWin ImageItem) keeps
// a stable copy and sees a new QImage::cacheKey() only for what changed.
class ChromeOverlayRaster final
{
public:
    enum class Repaint : quint8 {
        None,
        Band,
        Full,
    };

    struct Counters final
    {
        quint64 fullRenders = 0;
        quint64 bandRenders = 0;
        quint64 unchangedUpdates = 0;
        // Bytes of every image handed out for upload: body plus band for a
        // full repaint, band alone for a band repaint.
        qint64 uploadedBytes = 0;
    };

    // Logical rows painted below the title row in the band, so antialiasing
    // that spills past the row cannot be left behind in the body.
    static constexpr qreal BandSpillMargin = 4.0;

    // `plan` must already be localized (outer frame at the origin).
    Repaint setPlan(HybridChrome::ChromeRenderPlan plan);
    Repaint setPaintState(const HybridChrome::ChromePaintState &state);
    // The next setPlan() or setPaintState() repaints both images even when
    // nothing compares different (for example, after a button-painter swap).
    void invalidate() noexcept;
    void clear() noexcept;

    [[nodiscard]] const HybridChrome::ChromeRenderPlan &plan() const noexcept { return m_plan; }
    [[nodiscard]] const HybridChrome::ChromePaintState &paintState() const noexcept
    {
        return m_state;
    }
    [[nodiscard]] const QImage &body() const noexcept { return m_body; }
    [[nodiscard]] const QImage &band() const noexcept { return m_band; }
    // Logical item sizes. The band's height is proportional to the body's
    // physical-to-logical ratio so both textures share one texel grid.
    [[nodiscard]] QSizeF bodyLogicalSize() const noexcept;
    [[nodiscard]] QSizeF bandLogicalSize() const noexcept;
    [[nodiscard]] const Counters &counters() const noexcept { return m_counters; }

private:
    [[nodiscard]] Repaint repaintFor(bool bandEligible);
    void renderFull();
    void renderBand();
    [[nodiscard]] QImage paintBand() const;
    [[nodiscard]] int bandPixelHeight() const;
    [[nodiscard]] bool rowGeometryInsideBand(const HybridChrome::ChromeRenderPlan &plan) const;

    HybridChrome::ChromeRenderPlan m_plan;
    HybridChrome::ChromePaintState m_state;
    QImage m_body;
    QImage m_band;
    QSize m_physicalSize;
    Counters m_counters;
    bool m_hasPlan = false;
    bool m_invalidated = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
