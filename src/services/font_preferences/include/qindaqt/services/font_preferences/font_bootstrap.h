// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/font_preferences/font_preferences.h"

#include <QFont>

namespace QindaQt::Services::FontPreferences {

// AGENT-CONTRACT: FontRenderingAttributes captures toolkit-level font rendering
// hints and logical DPI derived from validated FontPreferences before application UI launch.
struct FontRenderingAttributes final {
    bool antialiasingEnabled = true;
    QFont::HintingPreference hintingPreference = QFont::PreferDefaultHinting;
    QFont::StyleStrategy styleStrategy = QFont::PreferDefault;
    double logicalDpi = DefaultLogicalDpi;

    [[nodiscard]] bool operator==(const FontRenderingAttributes &other) const noexcept
    {
        return antialiasingEnabled == other.antialiasingEnabled &&
               hintingPreference == other.hintingPreference &&
               styleStrategy == other.styleStrategy &&
               qFuzzyCompare(logicalDpi, other.logicalDpi);
    }
};

// AGENT-NOTE: FontBootstrap is a pure conversion helper to construct QFont
// and rendering properties for first-party QGuiApplication initialization
// prior to QML or scene graph construction.
class FontBootstrap final {
public:
    FontBootstrap() = delete;

    [[nodiscard]] static QFont createApplicationFont(const FontPreferences &prefs);
    [[nodiscard]] static QFont createMonospaceFont(const FontPreferences &prefs);
    [[nodiscard]] static FontRenderingAttributes deriveRenderingAttributes(const FontPreferences &prefs);
    [[nodiscard]] static QFont applyPreferences(QFont baseFont, const FontPreferences &prefs);
};

} // namespace QindaQt::Services::FontPreferences
