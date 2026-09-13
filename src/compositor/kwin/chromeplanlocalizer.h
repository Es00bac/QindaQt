// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_chrome/chromelayoutengine.h"

#include <QPointF>

namespace QindaQt::Compositor::KWinIntegration {

// Converts the global logical plan used by input routing into the image-local
// coordinate space used by KWin's scene item. Every painted rectangle must be
// translated; omitted geometry remains clickable but is clipped from paint.
[[nodiscard]] HybridChrome::ChromeRenderPlan localizeChromeRenderPlan(
    HybridChrome::ChromeRenderPlan plan, const QPointF &origin);

} // namespace QindaQt::Compositor::KWinIntegration
