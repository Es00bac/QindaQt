// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QColor>

namespace QindaQt::DesignTokens::Private {

[[nodiscard]] QColor withAlpha(const QColor &color, double alpha);
[[nodiscard]] QColor compositeOver(const QColor &foreground, const QColor &background);
[[nodiscard]] QColor mix(const QColor &from, const QColor &toward, double amount);
[[nodiscard]] QColor contrastForeground(const QColor &background);

} // namespace QindaQt::DesignTokens::Private
