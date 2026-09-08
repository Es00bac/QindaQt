// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QColor>
#include <array>

namespace QindaQt::Apps::Terminal {
// Pure, thread-neutral terminal protocol adaptation. Colors are opaque values;
// callers retain ownership. No theme selection or profile persistence occurs.
[[nodiscard]] QColor terminalReadableColor(QColor color, const QColor &background,
                                           double minimumContrast);
[[nodiscard]] std::array<QColor, 16> terminalAnsiPalette(
    const QColor &background, bool highContrast);
} // namespace QindaQt::Apps::Terminal
