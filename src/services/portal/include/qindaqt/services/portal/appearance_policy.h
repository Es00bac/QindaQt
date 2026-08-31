// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/themes/theme_spec.h"

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include <optional>

namespace QindaQt::Services::Portal {

inline constexpr auto kAppearanceNamespace = "org.freedesktop.appearance";
inline constexpr auto kColorSchemeKey = "color-scheme";
inline constexpr auto kAccentColorKey = "accent-color";
inline constexpr auto kContrastKey = "contrast";

inline constexpr auto kThemeSetting = "appearance.theme";
inline constexpr auto kColorSchemeSetting = "appearance.colorScheme";
inline constexpr auto kHighContrastSetting = "accessibility.highContrast";
inline constexpr auto kReducedTransparencySetting =
    "accessibility.reducedTransparency";

enum class PortalColorScheme : quint32 {
    NoPreference = 0,
    PreferDark = 1,
    PreferLight = 2,
};

enum class PortalContrast : quint32 {
    NoPreference = 0,
    PreferHigh = 1,
};

// The installed backend Settings contract serializes this value as `(ddd)`.
// Keeping the transport-neutral RGB value here lets projection remain pure;
// QtDBus operators are private to the service adapter.
struct PortalAccentColor final {
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;

    [[nodiscard]] bool operator==(const PortalAccentColor &) const = default;
};

struct AppearancePolicy final {
    PortalColorScheme colorScheme = PortalColorScheme::NoPreference;
    PortalAccentColor accentColor;
    PortalContrast contrast = PortalContrast::NoPreference;

    [[nodiscard]] bool operator==(const AppearancePolicy &) const = default;
};

enum class AppearanceProjectionError {
    None,
    InvalidCatalog,
    MalformedSnapshot,
    UnknownTheme,
    QstDerivationFailed,
    NonOpaqueAccent,
};

struct AppearanceProjectionResult final {
    std::optional<AppearancePolicy> policy;
    AppearanceProjectionError error = AppearanceProjectionError::None;
    QString diagnostic;

    [[nodiscard]] bool ok() const noexcept { return policy.has_value(); }
};

// AGENT-CONTRACT: This is the sole Settings1/QST-to-portal policy projection.
// It owns no transport, persistence, theme discovery, or D-Bus state. The
// catalog is copied at construction and must contain unique, loader-valid,
// QST-derivable themes. A malformed authoritative snapshot yields no partial
// policy, so callers can withdraw stale portal truth atomically.
class AppearancePolicyProjector final {
public:
    explicit AppearancePolicyProjector(QVector<Themes::ThemeSpec> themes);

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] const QString &catalogError() const noexcept;
    [[nodiscard]] AppearanceProjectionResult
    project(const QVariantMap &settingsValues) const;

    [[nodiscard]] static QStringList scopedSettingsKeys();

private:
    QVector<Themes::ThemeSpec> m_themes;
    QString m_catalogError;
};

} // namespace QindaQt::Services::Portal

Q_DECLARE_METATYPE(QindaQt::Services::Portal::PortalAccentColor)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::AppearancePolicy)
