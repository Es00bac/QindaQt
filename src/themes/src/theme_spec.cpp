// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/themes/theme_spec.h"

namespace QindaQt::Themes {

QVariantMap DecorationSpec::toVariantMap() const
{
    QVariantMap values = {{QStringLiteral("buttonPlacement"), buttonPlacement},
            {QStringLiteral("tabDirection"), tabDirection},
            {QStringLiteral("buttonStyle"), buttonStyle},
            {QStringLiteral("hoverGlyphs"), hoverGlyphs},
            {QStringLiteral("closeColor"), closeColor},
            {QStringLiteral("minimizeColor"), minimizeColor},
            {QStringLiteral("maximizeColor"), maximizeColor}};
    // Optional authored Luna fields round-trip only when present so the
    // strict JSON round-trip proof never grows keys the source file omits.
    if (titleBarColor.isValid()) {
        values.insert(QStringLiteral("titleBarColor"), titleBarColor);
    }
    if (titleBarInactiveColor.isValid()) {
        values.insert(QStringLiteral("titleBarInactiveColor"), titleBarInactiveColor);
    }
    if (restoreColor.isValid()) {
        values.insert(QStringLiteral("restoreColor"), restoreColor);
    }
    return values;
}

QVariantMap ThemeSpec::toVariantMap() const
{
    QVariantMap colorValues;
    for (auto iterator = colors.cbegin(); iterator != colors.cend(); ++iterator) {
        colorValues.insert(iterator.key(), iterator.value());
    }
    QVariantMap values = {{QStringLiteral("schemaVersion"), schemaVersion},
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
    if (!iconTheme.isEmpty()) {
        values.insert(QStringLiteral("iconTheme"), iconTheme);
    }
    return values;
}

} // namespace QindaQt::Themes
