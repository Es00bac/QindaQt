// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/themes/theme_spec.h"

namespace QindaQt::Themes {

QVariantMap DecorationSpec::toVariantMap() const
{
    return {{QStringLiteral("buttonPlacement"), buttonPlacement},
            {QStringLiteral("tabDirection"), tabDirection},
            {QStringLiteral("buttonStyle"), buttonStyle},
            {QStringLiteral("hoverGlyphs"), hoverGlyphs},
            {QStringLiteral("closeColor"), closeColor},
            {QStringLiteral("minimizeColor"), minimizeColor},
            {QStringLiteral("maximizeColor"), maximizeColor}};
}

QVariantMap ThemeSpec::toVariantMap() const
{
    QVariantMap colorValues;
    for (auto iterator = colors.cbegin(); iterator != colors.cend(); ++iterator) {
        colorValues.insert(iterator.key(), iterator.value());
    }
    return {{QStringLiteral("schemaVersion"), schemaVersion},
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), name},
            {QStringLiteral("variant"), variant},
            {QStringLiteral("fontFamily"), fontFamily},
            {QStringLiteral("monoFontFamily"), monoFontFamily},
            {QStringLiteral("colors"), colorValues},
            {QStringLiteral("cornerRadius"), cornerRadius},
            {QStringLiteral("motionDuration"), motionDuration},
            {QStringLiteral("blurEnabled"), blurEnabled},
            {QStringLiteral("decoration"), decoration.toVariantMap()}};
}

} // namespace QindaQt::Themes
