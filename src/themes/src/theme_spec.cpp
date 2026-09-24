// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/themes/theme_spec.h"

namespace QindaQt::Themes {

QStringList SurfaceNames::all()
{
    return {QString(Panel), QString(Popup), QString(Menu), QString(ContainerChrome),
            QString(Decoration), QString(DesktopIcons)};
}

QStringList MotionNames::all()
{
    return {QString(Popup), QString(Menu), QString(Rollup), QString(Hover)};
}

QStringList MotionNames::easings()
{
    return {QStringLiteral("standard"), QStringLiteral("emphasized"),
            QStringLiteral("decelerate"), QStringLiteral("linear")};
}

QVariantMap SurfaceMaterialSpec::toVariantMap() const
{
    QVariantMap values = {{QStringLiteral("opacity"), opacity},
                          {QStringLiteral("blur"), blur},
                          {QStringLiteral("border"), border},
                          {QStringLiteral("highlight"), highlight},
                          {QStringLiteral("shadow"), shadow}};
    if (tint.isValid()) {
        values.insert(QStringLiteral("tint"), tint);
    }
    return values;
}

QVariantMap MotionSpec::toVariantMap() const
{
    return {{QStringLiteral("duration"), duration}, {QStringLiteral("easing"), easing}};
}

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
    // ADR-0268 behaviour keys, likewise only when the document authors them.
    if (!titleDoubleClick.isEmpty()) {
        values.insert(QStringLiteral("titleDoubleClick"), titleDoubleClick);
    }
    if (minimizeAction != QLatin1String("minimize")) {
        values.insert(QStringLiteral("minimizeAction"), minimizeAction);
    }
    if (!titleWear) {
        values.insert(QStringLiteral("titleWear"), false);
    }
    return values;
}

SurfaceMaterialSpec ThemeSpec::surface(const QString &surfaceName) const
{
    const auto authored = surfaces.constFind(surfaceName);
    if (authored != surfaces.cend()) {
        return *authored;
    }
    SurfaceMaterialSpec fallback;
    fallback.blur = blurEnabled
        && (surfaceName == SurfaceNames::Panel || surfaceName == SurfaceNames::Popup
            || surfaceName == SurfaceNames::Menu);
    return fallback;
}

int ThemeSpec::surfaceRadius(const QString &surfaceName) const
{
    return radii.value(surfaceName, cornerRadius);
}

MotionSpec ThemeSpec::motion(const QString &motionName) const
{
    const auto authored = motions.constFind(motionName);
    if (authored != motions.cend()) {
        return *authored;
    }
    MotionSpec fallback;
    fallback.duration = motionDuration;
    return fallback;
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
    // AGENT-GUARD: schema-v1 maps must not grow keys (the strict round trip
    // compares them against the source file); v2 keys appear only for v2
    // documents and only for what they authored.
    if (schemaVersion >= 2) {
        QVariantMap surfaceValues;
        for (auto it = surfaces.cbegin(); it != surfaces.cend(); ++it) {
            surfaceValues.insert(it.key(), it.value().toVariantMap());
        }
        if (!surfaceValues.isEmpty()) {
            values.insert(QStringLiteral("surfaces"), surfaceValues);
        }
        QVariantMap radiusValues;
        for (auto it = radii.cbegin(); it != radii.cend(); ++it) {
            radiusValues.insert(it.key(), it.value());
        }
        if (!radiusValues.isEmpty()) {
            values.insert(QStringLiteral("radii"), radiusValues);
        }
        QVariantMap motionValues;
        for (auto it = motions.cbegin(); it != motions.cend(); ++it) {
            motionValues.insert(it.key(), it.value().toVariantMap());
        }
        if (!motionValues.isEmpty()) {
            values.insert(QStringLiteral("motion"), motionValues);
        }
        values.insert(QStringLiteral("accent"),
                      QVariantMap{{QStringLiteral("mode"), accentMode}});
        if (!decorationTheme.isEmpty()) {
            values.insert(QStringLiteral("decorationTheme"), decorationTheme);
        }
    }
    return values;
}

} // namespace QindaQt::Themes
