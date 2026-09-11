// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/decoration_painter/decoration_painter.h"

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
// The same map with the user's window arrangement applied (ADR-0129).
[[nodiscard]] QVariantMap
decorationPaletteProperties(const HybridChrome::ChromePalette &palette,
                            const Themes::ThemeSpec &theme,
                            const Decoration::ChromePreferences &preferences);

} // namespace QindaQt::Compositor::KWinIntegration
