// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellpreferencevalues.h"

#include <QFileInfo>
#include <QMetaType>

namespace QindaQt::Shell {
namespace {

constexpr auto LayoutProfileKey = "panels.layoutProfile";
constexpr auto ThemeKey = "appearance.theme";
constexpr auto FontFamilyKey = "fonts.family";
constexpr auto WallpaperKey = "appearance.wallpaper";
constexpr auto WallpaperModeKey = "appearance.wallpaperMode";
constexpr auto FontPointSizeKey = "fonts.pointSize";
constexpr auto HighContrastKey = "accessibility.highContrast";
constexpr auto ReducedMotionKey = "accessibility.reducedMotion";
constexpr auto ReducedTransparencyKey = "accessibility.reducedTransparency";
constexpr auto TextScaleKey = "accessibility.textScale";

bool exactString(const QVariantMap &values, const char *key, QString *result)
{
    const QVariant value = values.value(QLatin1StringView(key));
    if (value.metaType().id() != QMetaType::QString
        || value.toString().trimmed().isEmpty()) {
        return false;
    }
    *result = value.toString();
    return true;
}

bool exactBool(const QVariantMap &values, const char *key, bool *result)
{
    const QVariant value = values.value(QLatin1StringView(key));
    if (value.metaType().id() != QMetaType::Bool) {
        return false;
    }
    *result = value.toBool();
    return true;
}

bool exactNumber(const QVariantMap &values, const char *key, double *result)
{
    // AGENT-GUARD: Settings1 normalizes schema "number" values to Double but
    // integral user input can arrive as LongLong. Rejecting either silently
    // drops confirmed accessibility preferences (review finding P1-1 pattern).
    const QVariant value = values.value(QLatin1StringView(key));
    const int type = value.metaType().id();
    if (type != QMetaType::Double && type != QMetaType::LongLong) {
        return false;
    }
    bool valid = false;
    const double number = value.toDouble(&valid);
    if (!valid) {
        return false;
    }
    *result = number;
    return true;
}

} // namespace

QStringList ShellPreferenceValues::scopedKeys()
{
    return {QStringLiteral("panels.layoutProfile"),
            QStringLiteral("appearance.theme"),
            QStringLiteral("fonts.family"),
            QStringLiteral("appearance.wallpaper"),
            QStringLiteral("appearance.wallpaperMode"),
            QStringLiteral("fonts.pointSize"),
            QStringLiteral("accessibility.highContrast"),
            QStringLiteral("accessibility.reducedMotion"),
            QStringLiteral("accessibility.reducedTransparency"),
            QStringLiteral("accessibility.textScale")};
}

std::optional<ShellPreferenceValues>
ShellPreferenceValues::fromVariantMap(const QVariantMap &values, QString *error)
{
    ShellPreferenceValues result;
    bool reducedMotion = false;
    bool reducedTransparency = false;
    bool highContrast = false;
    double basePointSize = DesignTokens::AccessibilityInputs::defaultBasePointSize;
    double textScale = DesignTokens::AccessibilityInputs::defaultTextScale;
    const bool ok = exactString(values, LayoutProfileKey, &result.layoutProfileId)
        && exactString(values, ThemeKey, &result.themeId)
        && exactString(values, FontFamilyKey, &result.fontFamily)
        && values.value(QLatin1StringView(WallpaperKey)).metaType().id() == QMetaType::QString
        && exactString(values, WallpaperModeKey, &result.wallpaperMode)
        && exactNumber(values, FontPointSizeKey, &basePointSize)
        && exactBool(values, HighContrastKey, &highContrast)
        && exactBool(values, ReducedMotionKey, &reducedMotion)
        && exactBool(values, ReducedTransparencyKey, &reducedTransparency)
        && exactNumber(values, TextScaleKey, &textScale);
    result.wallpaper = values.value(QLatin1StringView(WallpaperKey)).toString();
    if (!ok) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "Settings1 returned an incomplete or mistyped shell preference snapshot");
        }
        return std::nullopt;
    }
    result.accessibility.basePointSize = basePointSize;
    result.accessibility.textScale = textScale;
    result.accessibility.reducedMotion = reducedMotion;
    result.accessibility.reducedTransparency = reducedTransparency;
    result.accessibility.highContrast = highContrast;
    result.accessibility = result.accessibility.normalized();
    return result;
}

QString resolveWallpaperSource(const QString &preference,
                               const QStringList &dataRoots)
{
    if (preference.startsWith(QStringLiteral("qindaqt:"))) {
        const QString name = preference.sliced(8);
        if (name.isEmpty() || name.contains(QLatin1Char('/'))) {
            return {};
        }
        for (const QString &root : dataRoots) {
            const QFileInfo file(root + QStringLiteral("/qindaqt/wallpapers/")
                                 + name + QStringLiteral(".png"));
            if (file.isFile() && file.isReadable()) {
                return file.absoluteFilePath();
            }
        }
        return {};
    }
    const QFileInfo file(preference);
    return file.isAbsolute() && file.isFile() && file.isReadable()
        ? file.absoluteFilePath() : QString{};
}

QString resolveStartupProfileId(
    const QString &explicitProfileId,
    const std::optional<ShellPreferenceValues> &preferences)
{
    if (!explicitProfileId.isEmpty()) {
        return explicitProfileId;
    }
    if (preferences.has_value()) {
        return preferences->layoutProfileId;
    }
    return QStringLiteral("qindaqt");
}

QString resolveStartupThemeId(
    const QString &explicitThemeId,
    const std::optional<ShellPreferenceValues> &preferences,
    const QString &profileDefaultThemeId)
{
    if (!explicitThemeId.isEmpty()) {
        return explicitThemeId;
    }
    if (preferences.has_value()) {
        return preferences->themeId;
    }
    return profileDefaultThemeId;
}

} // namespace QindaQt::Shell
