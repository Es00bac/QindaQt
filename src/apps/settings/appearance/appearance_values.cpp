// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_values.h"

#include <QtGlobal>

#include <cmath>

namespace QindaQt::Apps::SettingsAppearance {
namespace {

// AGENT-CONTRACT: Draft bounds mirror data/settings/schema-v2.json constraints
// for these keys. The service re-validates optimistically, but local feedback
// must fail for the same inputs or the page would offer values it cannot save.
constexpr double MinimumFontPointSize = 6.0;
constexpr double MaximumFontPointSize = 36.0;
constexpr double MinimumUiScale = 0.5;
constexpr double MaximumUiScale = 3.0;

[[nodiscard]] bool isUsableString(const QString &value)
{
    return !value.contains(QLatin1Char('\0'));
}

[[nodiscard]] bool isBoundedNumber(const QVariant &value, double minimum,
                                   double maximum, double *decoded)
{
    switch (value.metaType().id()) {
    case QMetaType::Double:
    case QMetaType::Float:
    case QMetaType::LongLong:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::ULongLong: {
        const double decodedValue = value.toDouble();
        if (!qIsFinite(decodedValue) || decodedValue < minimum
            || decodedValue > maximum) {
            return false;
        }
        *decoded = decodedValue;
        return true;
    }
    default:
        return false;
    }
}

} // namespace

QStringList AppearanceKeys::scopedKeys()
{
    return {QLatin1String(Theme), QLatin1String(ColorScheme),
            QLatin1String(FontFamily), QLatin1String(FontPointSize),
            QLatin1String(FontAntialiasing), QLatin1String(FontHinting),
            QLatin1String(FontSubpixelOrder), QLatin1String(Wallpaper),
            QLatin1String(WallpaperMode), QLatin1String(UiScale)};
}

QString colorSchemeToken(ColorSchemePreference scheme)
{
    switch (scheme) {
    case ColorSchemePreference::System: return QStringLiteral("system");
    case ColorSchemePreference::Light: return QStringLiteral("light");
    case ColorSchemePreference::Dark: return QStringLiteral("dark");
    }
    return QStringLiteral("system");
}

std::optional<ColorSchemePreference>
colorSchemeFromToken(const QString &token)
{
    if (token == QLatin1String("system")) return ColorSchemePreference::System;
    if (token == QLatin1String("light")) return ColorSchemePreference::Light;
    if (token == QLatin1String("dark")) return ColorSchemePreference::Dark;
    return std::nullopt;
}

QString wallpaperModeToken(WallpaperMode mode)
{
    switch (mode) {
    case WallpaperMode::Scaled: return QStringLiteral("scaled");
    case WallpaperMode::Centered: return QStringLiteral("centered");
    case WallpaperMode::Tiled: return QStringLiteral("tiled");
    }
    return QStringLiteral("scaled");
}

std::optional<WallpaperMode> wallpaperModeFromToken(const QString &token)
{
    if (token == QLatin1String("scaled")) return WallpaperMode::Scaled;
    if (token == QLatin1String("centered")) return WallpaperMode::Centered;
    if (token == QLatin1String("tiled")) return WallpaperMode::Tiled;
    return std::nullopt;
}

QString fontHintingToken(FontHinting hinting)
{
    switch (hinting) {
    case FontHinting::None: return QStringLiteral("none");
    case FontHinting::Slight: return QStringLiteral("slight");
    case FontHinting::Medium: return QStringLiteral("medium");
    case FontHinting::Full: return QStringLiteral("full");
    }
    return QStringLiteral("slight");
}

std::optional<FontHinting> fontHintingFromToken(const QString &token)
{
    if (token == QLatin1String("none")) return FontHinting::None;
    if (token == QLatin1String("slight")) return FontHinting::Slight;
    if (token == QLatin1String("medium")) return FontHinting::Medium;
    if (token == QLatin1String("full")) return FontHinting::Full;
    return std::nullopt;
}

QString subpixelOrderToken(SubpixelOrder order)
{
    switch (order) {
    case SubpixelOrder::None: return QStringLiteral("none");
    case SubpixelOrder::Rgb: return QStringLiteral("rgb");
    case SubpixelOrder::Bgr: return QStringLiteral("bgr");
    case SubpixelOrder::Vrgb: return QStringLiteral("vrgb");
    case SubpixelOrder::Vbgr: return QStringLiteral("vbgr");
    }
    return QStringLiteral("rgb");
}

std::optional<SubpixelOrder> subpixelOrderFromToken(const QString &token)
{
    if (token == QLatin1String("none")) return SubpixelOrder::None;
    if (token == QLatin1String("rgb")) return SubpixelOrder::Rgb;
    if (token == QLatin1String("bgr")) return SubpixelOrder::Bgr;
    if (token == QLatin1String("vrgb")) return SubpixelOrder::Vrgb;
    if (token == QLatin1String("vbgr")) return SubpixelOrder::Vbgr;
    return std::nullopt;
}

std::optional<AppearanceValues>
AppearanceValues::fromVariantMap(const QVariantMap &values, QString *error)
{
    const auto fail = [error](const QString &key, const QString &reason) {
        if (error != nullptr) {
            *error = QStringLiteral("%1: %2").arg(key, reason);
        }
        return std::nullopt;
    };

    AppearanceValues decoded;
    const auto requireString = [&](QLatin1String key,
                                   QString *target) -> bool {
        const QVariant value = values.value(QLatin1String(key));
        if (value.metaType().id() != QMetaType::QString || !isUsableString(value.toString())) {
            fail(QLatin1String(key),
                 QStringLiteral("expected a usable string value"));
            return false;
        }
        *target = value.toString();
        return true;
    };

    if (!requireString(AppearanceKeys::Theme, &decoded.themeId)) {
        return std::nullopt;
    }
    if (decoded.themeId.isEmpty()) {
        return fail(QLatin1String(AppearanceKeys::Theme),
                    QStringLiteral("expected a non-empty string value"));
    }
    if (!requireString(AppearanceKeys::FontFamily, &decoded.fontFamily)) {
        return std::nullopt;
    }
    if (decoded.fontFamily.isEmpty()) {
        return fail(QLatin1String(AppearanceKeys::FontFamily),
                    QStringLiteral("expected a non-empty string value"));
    }
    if (!requireString(AppearanceKeys::Wallpaper, &decoded.wallpaper)) {
        return std::nullopt;
    }

    const auto requireToken = [&](QLatin1String key, QString *target) {
        const QVariant value = values.value(QLatin1String(key));
        if (value.metaType().id() != QMetaType::QString) {
            fail(QLatin1String(key), QStringLiteral("expected a string token"));
            return false;
        }
        *target = value.toString();
        return true;
    };
    QString schemeToken;
    if (!requireToken(AppearanceKeys::ColorScheme, &schemeToken)) {
        return std::nullopt;
    }
    const auto scheme = colorSchemeFromToken(schemeToken);
    if (!scheme.has_value()) {
        return fail(QLatin1String(AppearanceKeys::ColorScheme),
                    QStringLiteral("unknown scheme token '%1'").arg(schemeToken));
    }
    decoded.colorScheme = *scheme;

    QString modeToken;
    if (!requireToken(AppearanceKeys::WallpaperMode, &modeToken)) {
        return std::nullopt;
    }
    const auto mode = wallpaperModeFromToken(modeToken);
    if (!mode.has_value()) {
        return fail(QLatin1String(AppearanceKeys::WallpaperMode),
                    QStringLiteral("unknown mode token '%1'").arg(modeToken));
    }
    decoded.wallpaperMode = *mode;

    QString hintingToken;
    if (!requireToken(AppearanceKeys::FontHinting, &hintingToken)) {
        return std::nullopt;
    }
    const auto hinting = fontHintingFromToken(hintingToken);
    if (!hinting.has_value()) {
        return fail(QLatin1String(AppearanceKeys::FontHinting),
                    QStringLiteral("unknown hinting token '%1'").arg(hintingToken));
    }
    decoded.fontHinting = *hinting;

    QString subpixelToken;
    if (!requireToken(AppearanceKeys::FontSubpixelOrder, &subpixelToken)) {
        return std::nullopt;
    }
    const auto subpixel = subpixelOrderFromToken(subpixelToken);
    if (!subpixel.has_value()) {
        return fail(QLatin1String(AppearanceKeys::FontSubpixelOrder),
                    QStringLiteral("unknown subpixel token '%1'").arg(subpixelToken));
    }
    decoded.fontSubpixelOrder = *subpixel;

    const QVariant antialiasing =
        values.value(QLatin1String(AppearanceKeys::FontAntialiasing));
    if (antialiasing.metaType().id() != QMetaType::Bool) {
        return fail(QLatin1String(AppearanceKeys::FontAntialiasing),
                    QStringLiteral("expected a Boolean"));
    }
    decoded.fontAntialiasing = antialiasing.toBool();

    const QVariant pointSize =
        values.value(QLatin1String(AppearanceKeys::FontPointSize));
    if (!isBoundedNumber(pointSize, MinimumFontPointSize, MaximumFontPointSize,
                         &decoded.fontPointSize)) {
        return fail(QLatin1String(AppearanceKeys::FontPointSize),
                    QStringLiteral("expected a finite number in [%1, %2]")
                        .arg(MinimumFontPointSize)
                        .arg(MaximumFontPointSize));
    }

    const QVariant uiScale = values.value(QLatin1String(AppearanceKeys::UiScale));
    if (!isBoundedNumber(uiScale, MinimumUiScale, MaximumUiScale,
                         &decoded.uiScale)) {
        return fail(QLatin1String(AppearanceKeys::UiScale),
                    QStringLiteral("expected a finite number in [%1, %2]")
                        .arg(MinimumUiScale)
                        .arg(MaximumUiScale));
    }

    return decoded;
}

QVariantMap AppearanceValues::toVariantMap() const
{
    return {{QLatin1String(AppearanceKeys::Theme), themeId},
            {QLatin1String(AppearanceKeys::ColorScheme),
             colorSchemeToken(colorScheme)},
            {QLatin1String(AppearanceKeys::FontFamily), fontFamily},
            {QLatin1String(AppearanceKeys::FontPointSize), fontPointSize},
            {QLatin1String(AppearanceKeys::FontAntialiasing), fontAntialiasing},
            {QLatin1String(AppearanceKeys::FontHinting),
             fontHintingToken(fontHinting)},
            {QLatin1String(AppearanceKeys::FontSubpixelOrder),
             subpixelOrderToken(fontSubpixelOrder)},
            {QLatin1String(AppearanceKeys::Wallpaper), wallpaper},
            {QLatin1String(AppearanceKeys::WallpaperMode),
             wallpaperModeToken(wallpaperMode)},
            {QLatin1String(AppearanceKeys::UiScale), uiScale}};
}

AppearanceValidation
validateAppearanceDraft(const AppearanceValues &values,
                        const QSet<QString> &installedThemeIds)
{
    AppearanceValidation result;
    const auto reject = [&result](QLatin1String key, const QString &message) {
        result.valid = false;
        result.fieldErrors.insert(QLatin1String(key), message);
    };

    if (values.themeId.isEmpty()) {
        reject(AppearanceKeys::Theme, QStringLiteral("Choose a theme"));
    } else if (!isUsableString(values.themeId)) {
        reject(AppearanceKeys::Theme,
               QStringLiteral("Theme identifiers must not contain embedded NUL"));
    } else if (!installedThemeIds.contains(values.themeId)) {
        reject(AppearanceKeys::Theme,
               QStringLiteral("Theme '%1' is not installed")
                   .arg(values.themeId));
    }
    if (values.fontFamily.isEmpty()) {
        reject(AppearanceKeys::FontFamily,
               QStringLiteral("Enter a font family name"));
    } else if (!isUsableString(values.fontFamily)) {
        reject(AppearanceKeys::FontFamily,
               QStringLiteral("Font family must not contain embedded NUL"));
    }
    if (!isUsableString(values.wallpaper)) {
        reject(AppearanceKeys::Wallpaper,
               QStringLiteral("Wallpaper path must not contain embedded NUL"));
    }
    if (!qIsFinite(values.fontPointSize) || values.fontPointSize < MinimumFontPointSize
        || values.fontPointSize > MaximumFontPointSize) {
        reject(AppearanceKeys::FontPointSize,
               QStringLiteral("Font size must be between %1 and %2 points")
                   .arg(MinimumFontPointSize)
                   .arg(MaximumFontPointSize));
    }
    if (!qIsFinite(values.uiScale) || values.uiScale < MinimumUiScale
        || values.uiScale > MaximumUiScale) {
        reject(AppearanceKeys::UiScale,
               QStringLiteral("UI scale must be between %1 and %2")
                   .arg(MinimumUiScale)
                   .arg(MaximumUiScale));
    }
    return result;
}

} // namespace QindaQt::Apps::SettingsAppearance
