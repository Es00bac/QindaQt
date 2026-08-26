// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridchromeexposure.h"

namespace QindaQt::Compositor::KWinIntegration {

bool sceneChromeExposed(
    const QString &anchorWindowId,
    const QString &excludedWindowId,
    const QVector<HybridChromeExposureEntry> &stackBottomToTop)
{
    if (anchorWindowId.isEmpty()) {
        return false;
    }
    qsizetype anchorIndex = -1;
    for (qsizetype index = 0; index < stackBottomToTop.size(); ++index) {
        if (stackBottomToTop[index].windowId == anchorWindowId) {
            anchorIndex = index;
            break;
        }
    }
    if (anchorIndex < 0) {
        // AGENT-GUARD: A stale or unpaintable anchor is fail-closed. Allowing
        // a plan-only hit here would recreate click-through during teardown.
        return false;
    }
    for (qsizetype index = anchorIndex + 1;
         index < stackBottomToTop.size(); ++index) {
        const auto &entry = stackBottomToTop[index];
        const bool excluded = !excludedWindowId.isEmpty()
            && entry.windowId == excludedWindowId;
        if (entry.ownsPoint && !excluded) {
            return false;
        }
    }
    return true;
}

} // namespace QindaQt::Compositor::KWinIntegration
