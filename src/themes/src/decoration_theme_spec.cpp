// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/themes/decoration_theme_spec.h"

namespace QindaQt::Themes {

QStringList DecorationThemeTokens::buttonStyles()
{
    // The painter table order: the traffic-light fallback first, then the
    // other shipped styles, then the W19 styles (ADR-0264).
    return {QStringLiteral("traffic-lights"), QStringLiteral("symbols"),
            QStringLiteral("glyph"), QStringLiteral("flat"),
            QStringLiteral("gel"), QStringLiteral("bevel"),
            QStringLiteral("blue-tiles"), QStringLiteral("wide"),
            QStringLiteral("tab"), QStringLiteral("bold"),
            QStringLiteral("minimal"), QStringLiteral("pills"),
            QStringLiteral("dots"), QStringLiteral("outline"),
            QStringLiteral("chunky")};
}

QStringList DecorationThemeTokens::memberHandleStyles()
{
    return {QStringLiteral("grip"), QStringLiteral("dots"), QStringLiteral("plain")};
}

QStringList DecorationThemeTokens::containerBadgeStyles()
{
    return {QStringLiteral("pill"), QStringLiteral("square")};
}

bool DecorationThemeSpec::hasAuthoredColors() const
{
    return decoration.closeColor.isValid() && decoration.minimizeColor.isValid()
        && decoration.maximizeColor.isValid();
}

QVariantMap DecorationThemeSpec::toVariantMap() const
{
    QVariantMap values = {{QStringLiteral("schemaVersion"), schemaVersion},
                          {QStringLiteral("id"), id},
                          {QStringLiteral("name"), name},
                          {QStringLiteral("buttonPlacement"), decoration.buttonPlacement},
                          {QStringLiteral("tabDirection"), decoration.tabDirection},
                          {QStringLiteral("buttonStyle"), decoration.buttonStyle},
                          {QStringLiteral("hoverGlyphs"), decoration.hoverGlyphs},
                          {QStringLiteral("material"), titleMaterial.toVariantMap()},
                          {QStringLiteral("cornerRadius"), cornerRadius},
                          {QStringLiteral("shadow"),
                           QVariantMap{{QStringLiteral("extent"), shadowExtent},
                                       {QStringLiteral("opacity"), shadowOpacity}}},
                          {QStringLiteral("memberHandle"), memberHandleStyle},
                          {QStringLiteral("containerBadge"), containerBadgeStyle}};
    if (!description.isEmpty()) {
        values.insert(QStringLiteral("description"), description);
    }
    // Optional colors round-trip only when authored, like the theme block.
    const auto optionalColor = [&values](const char *key, const QColor &color) {
        if (color.isValid()) {
            values.insert(QString::fromLatin1(key), color);
        }
    };
    optionalColor("closeColor", decoration.closeColor);
    optionalColor("minimizeColor", decoration.minimizeColor);
    optionalColor("maximizeColor", decoration.maximizeColor);
    optionalColor("restoreColor", decoration.restoreColor);
    optionalColor("titleBarColor", decoration.titleBarColor);
    optionalColor("titleBarInactiveColor", decoration.titleBarInactiveColor);
    return values;
}

} // namespace QindaQt::Themes
