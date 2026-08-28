// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/font_preferences/font_bootstrap.h"

namespace QindaQt::Services::FontPreferences {

namespace {

// AGENT-CONTRACT: Qt's QFont::HintingPreference enum defines PreferNoHinting,
// PreferVerticalHinting (slight), and PreferFullHinting (full), but lacks an
// intermediate medium value. Consequently, FontHinting::Medium and FontHinting::Full
// both map to QFont::PreferFullHinting when adapting to Qt's font rendering pipeline.
QFont::HintingPreference toQtHinting(FontHinting hinting) noexcept
{
    switch (hinting) {
    case FontHinting::None:
        return QFont::PreferNoHinting;
    case FontHinting::Slight:
        return QFont::PreferVerticalHinting;
    case FontHinting::Medium:
        return QFont::PreferFullHinting;
    case FontHinting::Full:
        return QFont::PreferFullHinting;
    }
    return QFont::PreferVerticalHinting;
}

QFont::StyleStrategy toQtStyleStrategy(FontAntialiasing antialiasing, FontSubpixelOrder subpixelOrder) noexcept
{
    Q_UNUSED(subpixelOrder);
    if (antialiasing == FontAntialiasing::None) {
        return QFont::NoAntialias;
    }
    if (antialiasing == FontAntialiasing::Subpixel) {
        return static_cast<QFont::StyleStrategy>(QFont::PreferAntialias | QFont::PreferQuality);
    }
    return QFont::PreferAntialias;
}

} // namespace

QFont FontBootstrap::createApplicationFont(const FontPreferences &prefs)
{
    QFont font(prefs.family());
    font.setPointSizeF(prefs.pointSize());
    font.setHintingPreference(toQtHinting(prefs.hinting()));
    font.setStyleStrategy(toQtStyleStrategy(prefs.antialiasing(), prefs.subpixelOrder()));
    return font;
}

QFont FontBootstrap::createMonospaceFont(const FontPreferences &prefs)
{
    QFont font(prefs.monospaceFamily());
    font.setFixedPitch(true);
    font.setStyleHint(QFont::Monospace);
    font.setPointSizeF(prefs.pointSize());
    font.setHintingPreference(toQtHinting(prefs.hinting()));
    font.setStyleStrategy(toQtStyleStrategy(prefs.antialiasing(), prefs.subpixelOrder()));
    return font;
}

FontRenderingAttributes FontBootstrap::deriveRenderingAttributes(const FontPreferences &prefs)
{
    FontRenderingAttributes attrs;
    attrs.antialiasingEnabled = fontAntialiasingToBool(prefs.antialiasing());
    attrs.hintingPreference = toQtHinting(prefs.hinting());
    attrs.styleStrategy = toQtStyleStrategy(prefs.antialiasing(), prefs.subpixelOrder());
    attrs.logicalDpi = prefs.logicalDpi().value_or(DefaultLogicalDpi);
    return attrs;
}

QFont FontBootstrap::applyPreferences(QFont baseFont, const FontPreferences &prefs)
{
    baseFont.setFamily(prefs.family());
    baseFont.setPointSizeF(prefs.pointSize());
    baseFont.setHintingPreference(toQtHinting(prefs.hinting()));
    baseFont.setStyleStrategy(toQtStyleStrategy(prefs.antialiasing(), prefs.subpixelOrder()));
    return baseFont;
}

} // namespace QindaQt::Services::FontPreferences
