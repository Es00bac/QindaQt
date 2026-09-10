// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QColor>
#include <array>

namespace QindaQt::Apps::Terminal {
// Pure, thread-neutral terminal protocol adaptation. Colors are opaque values;
// callers retain ownership. No theme selection or profile persistence occurs.
// AGENT-NOTE: terminalContrastRatio is the WCAG relative-luminance ratio, a
// local pure function behaviorally identical to the retired
// DesignTokenDeriver::contrastRatio; ADR-0116 removed the DesignTokens link.
[[nodiscard]] double terminalContrastRatio(const QColor &first,
                                           const QColor &second);
[[nodiscard]] QColor terminalReadableColor(QColor color, const QColor &background,
                                           double minimumContrast);
[[nodiscard]] std::array<QColor, 16> terminalAnsiPalette(
    const QColor &background, bool highContrast);
} // namespace QindaQt::Apps::Terminal
