// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/portal/appearance_policy.h"

#include "qindaqt/design_tokens/accessibility_inputs.h"
#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/themes/theme_spec.h"

#include <QColor>
#include <QMetaType>
#include <QSet>

#include <cmath>
#include <utility>

namespace QindaQt::Services::Portal {
namespace {

AppearanceProjectionResult failure(AppearanceProjectionError error,
                                   QString diagnostic)
{
    return {.policy = std::nullopt,
            .error = error,
            .diagnostic = std::move(diagnostic)};
}

std::optional<PortalColorScheme> decodeScheme(const QVariant &value)
{
    if (value.metaType().id() != QMetaType::QString) {
        return std::nullopt;
    }
    const QString token = value.toString();
    if (token == QStringLiteral("system")) {
        return PortalColorScheme::NoPreference;
    }
    if (token == QStringLiteral("dark")) {
        return PortalColorScheme::PreferDark;
    }
    if (token == QStringLiteral("light")) {
        return PortalColorScheme::PreferLight;
    }
    return std::nullopt;
}

std::optional<bool> exactBoolean(const QVariant &value)
{
    if (value.metaType().id() != QMetaType::Bool) {
        return std::nullopt;
    }
    return value.toBool();
}

} // namespace

AppearancePolicyProjector::AppearancePolicyProjector(
    QVector<Themes::ThemeSpec> themes)
    : m_themes(std::move(themes))
{
    if (m_themes.isEmpty()) {
        m_catalogError = QStringLiteral("appearance theme catalog is empty");
        return;
    }
    QSet<QString> ids;
    for (const auto &theme : m_themes) {
        if (theme.id.isEmpty() || ids.contains(theme.id)) {
            m_catalogError = QStringLiteral(
                "appearance theme catalog has an empty or duplicate id");
            return;
        }
        ids.insert(theme.id);
        const auto derived = DesignTokens::DesignTokenDeriver::derive(theme);
        if (!derived.ok()) {
            m_catalogError = QStringLiteral("theme '%1' is not QST-1 compatible: %2")
                                 .arg(theme.id, derived.diagnostic);
            return;
        }
    }
}

bool AppearancePolicyProjector::isValid() const noexcept
{
    return m_catalogError.isEmpty();
}

const QString &AppearancePolicyProjector::catalogError() const noexcept
{
    return m_catalogError;
}

AppearanceProjectionResult AppearancePolicyProjector::project(
    const QVariantMap &settingsValues) const
{
    if (!isValid()) {
        return failure(AppearanceProjectionError::InvalidCatalog, m_catalogError);
    }
    const QStringList keys = scopedSettingsKeys();
    if (settingsValues.size() != keys.size()) {
        return failure(AppearanceProjectionError::MalformedSnapshot,
                       QStringLiteral("appearance Settings1 snapshot has the wrong field set"));
    }
    for (const QString &key : keys) {
        if (!settingsValues.contains(key)) {
            return failure(AppearanceProjectionError::MalformedSnapshot,
                           QStringLiteral("appearance Settings1 snapshot is missing '%1'")
                               .arg(key));
        }
    }

    const QVariant themeValue =
        settingsValues.value(QString::fromLatin1(kThemeSetting));
    const auto scheme = decodeScheme(
        settingsValues.value(QString::fromLatin1(kColorSchemeSetting)));
    const auto highContrast = exactBoolean(
        settingsValues.value(QString::fromLatin1(kHighContrastSetting)));
    const auto reducedTransparency = exactBoolean(
        settingsValues.value(QString::fromLatin1(kReducedTransparencySetting)));
    if (themeValue.metaType().id() != QMetaType::QString
        || themeValue.toString().isEmpty() || !scheme.has_value()
        || !highContrast.has_value() || !reducedTransparency.has_value()) {
        return failure(AppearanceProjectionError::MalformedSnapshot,
                       QStringLiteral("appearance Settings1 snapshot has a wrong type or token"));
    }

    const Themes::ThemeSpec *selected = nullptr;
    for (const auto &theme : m_themes) {
        if (theme.id == themeValue.toString()) {
            selected = &theme;
            break;
        }
    }
    if (selected == nullptr) {
        return failure(AppearanceProjectionError::UnknownTheme,
                       QStringLiteral("selected appearance theme is not installed"));
    }

    DesignTokens::AccessibilityInputs inputs;
    inputs.highContrast = *highContrast
        || selected->variant == QStringLiteral("high-contrast");
    inputs.reducedTransparency = *reducedTransparency;
    const auto derived = DesignTokens::DesignTokenDeriver::derive(*selected, inputs);
    if (!derived.ok()) {
        return failure(AppearanceProjectionError::QstDerivationFailed,
                       derived.diagnostic);
    }
    const QColor accent = derived.tokens->accent().defaultColor;
    if (!accent.isValid() || accent.alpha() != 255) {
        return failure(AppearanceProjectionError::NonOpaqueAccent,
                       QStringLiteral("QST accent cannot be represented as portal RGB"));
    }

    const AppearancePolicy policy{
        .colorScheme = *scheme,
        .accentColor = {.red = static_cast<double>(accent.redF()),
                        .green = static_cast<double>(accent.greenF()),
                        .blue = static_cast<double>(accent.blueF())},
        .contrast = inputs.highContrast ? PortalContrast::PreferHigh
                                        : PortalContrast::NoPreference};
    const auto finiteUnit = [](double component) {
        return std::isfinite(component) && component >= 0.0 && component <= 1.0;
    };
    if (!finiteUnit(policy.accentColor.red)
        || !finiteUnit(policy.accentColor.green)
        || !finiteUnit(policy.accentColor.blue)) {
        return failure(AppearanceProjectionError::QstDerivationFailed,
                       QStringLiteral("QST accent is outside the portal sRGB domain"));
    }
    return {.policy = policy,
            .error = AppearanceProjectionError::None,
            .diagnostic = {}};
}

QStringList AppearancePolicyProjector::scopedSettingsKeys()
{
    return {QString::fromLatin1(kThemeSetting),
            QString::fromLatin1(kColorSchemeSetting),
            QString::fromLatin1(kHighContrastSetting),
            QString::fromLatin1(kReducedTransparencySetting)};
}

} // namespace QindaQt::Services::Portal
