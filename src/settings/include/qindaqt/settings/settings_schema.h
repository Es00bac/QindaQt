// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/settings/settings_types.h"

#include <QByteArray>
#include <QMap>
#include <QStringList>
#include <QVariantMap>

#include <optional>

namespace QindaQt::Settings {

struct SettingDefinition final {
    QString key;
    SettingDomain domain = SettingDomain::Appearance;
    SettingValueType type = SettingValueType::String;
    QVariant defaultValue;
    std::optional<double> minimum;
    std::optional<double> maximum;
    QStringList allowedValues;
    bool nonEmpty = false;
};

class SettingsSchema final {
public:
    SettingsSchema(const SettingsSchema &) = default;
    SettingsSchema(SettingsSchema &&) noexcept = default;
    SettingsSchema &operator=(const SettingsSchema &) = default;
    SettingsSchema &operator=(SettingsSchema &&) noexcept = default;

    [[nodiscard]] static std::optional<SettingsSchema> fromFile(
        const QString &path,
        ValidationResult *validation = nullptr,
        QString *error = nullptr,
        int expectedVersion = QINDAQT_SETTINGS_SCHEMA_VERSION);
    [[nodiscard]] static std::optional<SettingsSchema> fromJson(
        const QByteArray &json,
        const QString &origin,
        ValidationResult *validation = nullptr,
        QString *error = nullptr,
        int expectedVersion = QINDAQT_SETTINGS_SCHEMA_VERSION);

    [[nodiscard]] int version() const noexcept { return m_version; }
    [[nodiscard]] bool contains(const QString &key) const noexcept;
    [[nodiscard]] const SettingDefinition *definition(const QString &key) const noexcept;
    [[nodiscard]] const QMap<QString, SettingDefinition> &definitions() const noexcept
    {
        return m_definitions;
    }

    // AGENT-CONTRACT: Every schema key has one normalized system default. The
    // layered model relies on that total map to make effective reads non-null.
    [[nodiscard]] const QVariantMap &systemDefaults() const noexcept { return m_systemDefaults; }

    [[nodiscard]] ValidationResult validateValue(const QString &key, const QVariant &value) const;
    [[nodiscard]] ValidationResult validateLayer(const QVariantMap &values) const;
    [[nodiscard]] std::optional<QVariant> normalizedValue(
        const QString &key,
        const QVariant &value,
        ValidationResult *validation = nullptr) const;
    [[nodiscard]] std::optional<QVariantMap> normalizedLayer(
        const QVariantMap &values,
        ValidationResult *validation = nullptr) const;

private:
    SettingsSchema() = default;

    int m_version = QINDAQT_SETTINGS_SCHEMA_VERSION;
    QMap<QString, SettingDefinition> m_definitions;
    QVariantMap m_systemDefaults;
};

} // namespace QindaQt::Settings
