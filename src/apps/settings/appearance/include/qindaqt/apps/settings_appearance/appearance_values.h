// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QLatin1String>
#include <QMetaType>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>

namespace QindaQt::Apps::SettingsAppearance {

// AGENT-CONTRACT: These key constants and their schema definitions
// (data/settings/schema-v2.json) must stay in sync. The Settings1 repository
// rejects unknown keys, so a key renamed here without the schema (or vice
// versa) makes every appearance commit fail as UnknownKey.
namespace AppearanceKeys {
inline constexpr QLatin1String Theme{"appearance.theme"};
inline constexpr QLatin1String ColorScheme{"appearance.colorScheme"};
inline constexpr QLatin1String Wallpaper{"appearance.wallpaper"};
inline constexpr QLatin1String WallpaperMode{"appearance.wallpaperMode"};
inline constexpr QLatin1String UiScale{"appearance.uiScale"};
inline constexpr QLatin1String FontFamily{"fonts.family"};
inline constexpr QLatin1String FontPointSize{"fonts.pointSize"};
inline constexpr QLatin1String FontAntialiasing{"fonts.antialiasing"};
inline constexpr QLatin1String FontHinting{"fonts.hinting"};
inline constexpr QLatin1String FontSubpixelOrder{"fonts.subpixelOrder"};

// Deterministic commit order; also the SettingsClient scope for the route.
[[nodiscard]] QStringList scopedKeys();
} // namespace AppearanceKeys

enum class ColorSchemePreference { System, Light, Dark };
enum class WallpaperMode { Scaled, Centered, Tiled };
enum class FontHinting { None, Slight, Medium, Full };
enum class SubpixelOrder { None, Rgb, Bgr, Vrgb, Vbgr };

[[nodiscard]] QString colorSchemeToken(ColorSchemePreference scheme);
[[nodiscard]] std::optional<ColorSchemePreference>
colorSchemeFromToken(const QString &token);
[[nodiscard]] QString wallpaperModeToken(WallpaperMode mode);
[[nodiscard]] std::optional<WallpaperMode>
wallpaperModeFromToken(const QString &token);
[[nodiscard]] QString fontHintingToken(FontHinting hinting);
[[nodiscard]] std::optional<FontHinting>
fontHintingFromToken(const QString &token);
[[nodiscard]] QString subpixelOrderToken(SubpixelOrder order);
[[nodiscard]] std::optional<SubpixelOrder>
subpixelOrderFromToken(const QString &token);

// One validated appearance preference set. Field defaults equal the shipped
// schema-v2 system defaults so a Loading model can preview deterministically
// before the first authoritative baseline arrives.
struct AppearanceValues final {
    QString themeId{QStringLiteral("qinda-dark")};
    ColorSchemePreference colorScheme{ColorSchemePreference::System};
    QString wallpaper;
    WallpaperMode wallpaperMode{WallpaperMode::Scaled};
    double uiScale = 1.0;
    QString fontFamily{QStringLiteral("Noto Sans")};
    double fontPointSize = 10.0;
    bool fontAntialiasing = true;
    FontHinting fontHinting{FontHinting::Slight};
    SubpixelOrder fontSubpixelOrder{SubpixelOrder::Rgb};

    [[nodiscard]] bool operator==(const AppearanceValues &) const = default;

    // Total decode of one scoped Settings1 snapshot. Schema system defaults
    // guarantee every scoped key exists, so a missing or wrong-typed value
    // fails the complete decode instead of partially trusting authority.
    [[nodiscard]] static std::optional<AppearanceValues>
    fromVariantMap(const QVariantMap &values, QString *error = nullptr);

    // Canonical wire shape used for draft/confirmed diffs and commit values.
    [[nodiscard]] QVariantMap toVariantMap() const;
};

struct AppearanceValidation final {
    bool valid = true;
    // Draft field key -> human-readable problem; empty when valid.
    QVariantMap fieldErrors;
};

// Draft-level validation that complements the service schema: it adds the
// installed-theme requirement and rejects embedded NUL before an optimistic
// commit can fail as ValidationFailed. Pure and catalog-driven.
[[nodiscard]] AppearanceValidation
validateAppearanceDraft(const AppearanceValues &values,
                        const QSet<QString> &installedThemeIds);

} // namespace QindaQt::Apps::SettingsAppearance
