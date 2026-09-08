// SPDX-License-Identifier: LGPL-3.0-or-later
#include "native_palette.h"
#include <qindaqt/design_tokens/design_tokens.h>
#include <qindaqt/design_tokens/token_deriver.h>

namespace QindaQt::QtTheme {
std::optional<NativeAppearance> nativeAppearance(
    const Themes::ThemeSpec &theme, DesignTokens::AccessibilityInputs inputs)
{
    inputs.highContrast = inputs.highContrast || theme.variant == QStringLiteral("high-contrast");
    const auto derived = DesignTokens::DesignTokenDeriver::derive(theme, inputs);
    if (!derived.ok()) return std::nullopt;
    const auto &tokens = *derived.tokens;
    NativeAppearance result;
    auto &palette = result.palette;
    palette.setColor(QPalette::Window, tokens.background().base);
    palette.setColor(QPalette::WindowText, tokens.foreground().defaultColor);
    palette.setColor(QPalette::Base, tokens.background().raised);
    palette.setColor(QPalette::AlternateBase, tokens.background().highest);
    palette.setColor(QPalette::Text, tokens.foreground().defaultColor);
    palette.setColor(QPalette::Button, tokens.background().raised);
    palette.setColor(QPalette::ButtonText, tokens.foreground().defaultColor);
    palette.setColor(QPalette::BrightText, tokens.foreground().defaultColor);
    palette.setColor(QPalette::Light, tokens.background().highest);
    palette.setColor(QPalette::Midlight, tokens.background().raised);
    palette.setColor(QPalette::Mid, tokens.divider());
    palette.setColor(QPalette::Dark, tokens.strongOutline());
    palette.setColor(QPalette::Shadow, tokens.strongOutline());
    palette.setColor(QPalette::Link, tokens.accent().defaultColor);
    palette.setColor(QPalette::LinkVisited, tokens.accent().defaultColor);
    palette.setColor(QPalette::Highlight, tokens.accent().defaultColor);
    palette.setColor(QPalette::Accent, tokens.accent().defaultColor);
    palette.setColor(QPalette::HighlightedText, tokens.accent().foreground);
    palette.setColor(QPalette::ToolTipBase, tokens.background().highest);
    palette.setColor(QPalette::ToolTipText, tokens.foreground().defaultColor);
    palette.setColor(QPalette::PlaceholderText, tokens.foreground().muted);
    for (auto role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, tokens.foreground().disabled);
    result.font = QFont(tokens.typeScale().fontFamily);
    result.font.setPointSizeF(tokens.typeScale().body);
    result.fixedFont = QFont(tokens.typeScale().monoFontFamily);
    result.fixedFont.setPointSizeF(tokens.typeScale().body);
    result.fixedFont.setStyleHint(QFont::Monospace);
    result.fixedFont.setFixedPitch(true);
    result.iconTheme = theme.iconTheme;
    result.scheme = palette.color(QPalette::Window).lightnessF() < 0.5
        ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light;
    result.highContrast = inputs.highContrast;
    return result;
}
}
