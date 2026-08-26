// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QVector>

namespace QindaQt::Compositor::KWinIntegration {

// Value-only projection of KWin's bottom-to-top input stack at one point.
// Entries that do not own the point remain present because the chrome anchor
// is a paint/stack identity even where its native frame does not extend.
struct HybridChromeExposureEntry final
{
    QString windowId;
    bool ownsPoint = false;
};

// Scene chrome is painted with anchorWindowId. It is input-addressable only
// when no eligible native input owner above that anchor covers the point.
// excludedWindowId supports a dragged source: the source may float above the
// destination while the user intentionally targets chrome underneath it.
[[nodiscard]] bool sceneChromeExposed(
    const QString &anchorWindowId,
    const QString &excludedWindowId,
    const QVector<HybridChromeExposureEntry> &stackBottomToTop);

} // namespace QindaQt::Compositor::KWinIntegration
