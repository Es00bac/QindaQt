// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridshadestripgeometry.h"

#include "qindaqt/hybrid_chrome/chromeshadedbadge.h"
#include "qindaqt/hybrid_chrome/chrometypes.h"

#include <QtMath>

#include <algorithm>

namespace QindaQt::Compositor::KWinIntegration {

int HybridShadeStripGeometry::stripWidth(qsizetype tabCount,
                                         const QRect &containerFrame)
{
    const HybridChrome::ChromeMetrics metrics;
    const qreal width = HybridChrome::ChromeShadedBadge::badgeWidth(metrics, tabCount)
        + 2.0 * metrics.outerBorder;
    return qBound(1, qCeil(width), qMax(1, containerFrame.width()));
}

} // namespace QindaQt::Compositor::KWinIntegration
