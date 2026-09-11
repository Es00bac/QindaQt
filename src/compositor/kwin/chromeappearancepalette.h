// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_chrome/chrometypes.h"
#include <QPalette>
#include <QVariantMap>

namespace QindaQt::Themes {
class ThemeSpec;
}

namespace QindaQt::Compositor::KWinIntegration {

[[nodiscard]] HybridChrome::ChromePalette
chromePaletteForTheme(const Themes::ThemeSpec &theme);

[[nodiscard]] QPalette nativePaletteForTheme(const Themes::ThemeSpec &theme);

[[nodiscard]] QVariantMap
decorationPaletteProperties(const HybridChrome::ChromePalette &palette,
                            const Themes::ThemeSpec &theme);

} // namespace QindaQt::Compositor::KWinIntegration
