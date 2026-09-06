// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_chrome/chrometypes.h"
#include <QVariantMap>

namespace QindaQt::Themes {
class ThemeSpec;
}

namespace QindaQt::Compositor::KWinIntegration {

[[nodiscard]] HybridChrome::ChromePalette
chromePaletteForTheme(const Themes::ThemeSpec &theme);

[[nodiscard]] QVariantMap
decorationPaletteProperties(const HybridChrome::ChromePalette &palette);

} // namespace QindaQt::Compositor::KWinIntegration
