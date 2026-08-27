// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/settings/settings_schema.h"

#include "settings_value_normalizer_p.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

namespace QindaQt::Settings {
namespace {

bool readStringArray(const QJsonValue &value, QStringList *strings)
{
    if (!value.isArray()) {
        return false;
    }
    for (const auto &item : value.toArray()) {
        if (!item.isString() || strings->contains(item.toString())) {
            return false;
        }
        strings->append(item.toString());
    }
    return true;
}

void setOutputValidation(const ValidationResult &result, ValidationResult *output)
{
    if (output != nullptr) {
        *output = result;
    }
}

} // namespace

std::optional<SettingsSchema> SettingsSchema::fromFile(const QString &path,
                                                        ValidationResult *validation,
                                                        QString *error,
                                                        int expectedVersion)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = path + QStringLiteral(": ") + file.errorString();
        }
        return std::nullopt;
    }
    return fromJson(file.readAll(), path, validation, error, expectedVersion);
}

std::optional<SettingsSchema> SettingsSchema::fromJson(const QByteArray &json,
                                                        const QString &origin,
                                                        ValidationResult *validation,
                                                        QString *error,
                                                        int expectedVersion)
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error != nullptr) {
            *error = origin + QStringLiteral(": invalid JSON: ") + parseError.errorString();
        }
        return std::nullopt;
    }

    SettingsSchema schema;
    ValidationResult result;
    const auto root = document.object();
    schema.m_version = root.value(QStringLiteral("schemaVersion")).toInt(-1);
    if (schema.m_version != expectedVersion) {
        result.add({},
                   QStringLiteral("unsupported-schema-version"),
                   QStringLiteral("unsupported schemaVersion %1, expected %2")
                       .arg(schema.m_version)
                       .arg(expectedVersion));
    }

    const auto settingsValue = root.value(QStringLiteral("settings"));
    if (!settingsValue.isArray() || settingsValue.toArray().isEmpty()) {
        result.add({},
                   QStringLiteral("missing-definitions"),
                   QStringLiteral("settings must be a non-empty array"));
    }

    static const QRegularExpression keyPattern(
        QStringLiteral("^[a-z][A-Za-z0-9]*(?:\\.[a-z][A-Za-z0-9]*)+$"));
    for (const auto &entryValue : settingsValue.toArray()) {
        if (!entryValue.isObject()) {
            result.add({}, QStringLiteral("invalid-definition"), QStringLiteral("setting must be an object"));
            continue;
        }

        const auto entry = entryValue.toObject();
        SettingDefinition definition;
        definition.key = entry.value(QStringLiteral("key")).toString();
        if (!keyPattern.match(definition.key).hasMatch()) {
            result.add(definition.key,
                       QStringLiteral("invalid-key"),
                       QStringLiteral("key must be a dotted stable identifier"));
            continue;
        }
        if (schema.m_definitions.contains(definition.key)) {
            result.add(definition.key,
                       QStringLiteral("duplicate-key"),
                       QStringLiteral("setting key is repeated"));
            continue;
        }
        if (!parseSettingDomain(entry.value(QStringLiteral("domain")).toString(), &definition.domain)
            || !parseSettingValueType(entry.value(QStringLiteral("type")).toString(), &definition.type)) {
            result.add(definition.key,
                       QStringLiteral("invalid-definition"),
                       QStringLiteral("domain or type is unknown"));
            continue;
        }
        if (!definition.key.startsWith(domainKeyPrefix(definition.domain) + QLatin1Char('.'))) {
            result.add(definition.key,
                       QStringLiteral("domain-mismatch"),
                       QStringLiteral("key prefix does not match its domain"));
            continue;
        }
        if (!entry.contains(QStringLiteral("default"))) {
            result.add(definition.key,
                       QStringLiteral("missing-default"),
                       QStringLiteral("every setting requires a system default"));
            continue;
        }

        if (entry.contains(QStringLiteral("constraints"))
            && !entry.value(QStringLiteral("constraints")).isObject()) {
            result.add(definition.key,
                       QStringLiteral("invalid-constraints"),
                       QStringLiteral("constraints must be an object"));
            continue;
        }
        const auto constraints = entry.value(QStringLiteral("constraints")).toObject();
        if (constraints.contains(QStringLiteral("minimum"))) {
            if (!constraints.value(QStringLiteral("minimum")).isDouble()) {
                result.add(definition.key,
                           QStringLiteral("invalid-constraints"),
                           QStringLiteral("minimum must be numeric"));
                continue;
            }
            definition.minimum = constraints.value(QStringLiteral("minimum")).toDouble();
        }
        if (constraints.contains(QStringLiteral("maximum"))) {
            if (!constraints.value(QStringLiteral("maximum")).isDouble()) {
                result.add(definition.key,
                           QStringLiteral("invalid-constraints"),
                           QStringLiteral("maximum must be numeric"));
                continue;
            }
            definition.maximum = constraints.value(QStringLiteral("maximum")).toDouble();
        }
        if (constraints.contains(QStringLiteral("nonEmpty"))
            && !constraints.value(QStringLiteral("nonEmpty")).isBool()) {
            result.add(definition.key,
                       QStringLiteral("invalid-constraints"),
                       QStringLiteral("nonEmpty must be boolean"));
            continue;
        }
        definition.nonEmpty = constraints.value(QStringLiteral("nonEmpty")).toBool(false);
        if (constraints.contains(QStringLiteral("allowedValues"))
            && !readStringArray(constraints.value(QStringLiteral("allowedValues")),
                                &definition.allowedValues)) {
            result.add(definition.key,
                       QStringLiteral("invalid-constraints"),
                       QStringLiteral("allowedValues must contain unique strings"));
            continue;
        }

        const bool numeric = definition.type == SettingValueType::Integer
            || definition.type == SettingValueType::Number;
        if ((!numeric && (definition.minimum.has_value() || definition.maximum.has_value()))
            || (definition.minimum.has_value() && definition.maximum.has_value()
                && *definition.minimum > *definition.maximum)
            || (!definition.allowedValues.isEmpty() && definition.type != SettingValueType::String)) {
            result.add(definition.key,
                       QStringLiteral("invalid-constraints"),
                       QStringLiteral("constraints are incompatible with the setting type"));
            continue;
        }

        QVariant normalizedDefault;
        QString defaultError;
        if (!Internal::normalizeSettingValue(definition,
                                             entry.value(QStringLiteral("default")).toVariant(),
                                             &normalizedDefault,
                                             &defaultError)) {
            result.add(definition.key, QStringLiteral("invalid-default"), defaultError);
            continue;
        }

        definition.defaultValue = normalizedDefault;
        schema.m_definitions.insert(definition.key, definition);
        schema.m_systemDefaults.insert(definition.key, normalizedDefault);
    }

    setOutputValidation(result, validation);
    if (!result.isValid()) {
        if (error != nullptr) {
            *error = origin + QStringLiteral(": ") + result.summary();
        }
        return std::nullopt;
    }
    return schema;
}

bool SettingsSchema::contains(const QString &key) const noexcept
{
    return m_definitions.contains(key);
}

const SettingDefinition *SettingsSchema::definition(const QString &key) const noexcept
{
    const auto iterator = m_definitions.constFind(key);
    return iterator == m_definitions.cend() ? nullptr : &iterator.value();
}

ValidationResult SettingsSchema::validateValue(const QString &key, const QVariant &value) const
{
    ValidationResult result;
    static_cast<void>(normalizedValue(key, value, &result));
    return result;
}

ValidationResult SettingsSchema::validateLayer(const QVariantMap &values) const
{
    ValidationResult result;
    static_cast<void>(normalizedLayer(values, &result));
    return result;
}

std::optional<QVariant> SettingsSchema::normalizedValue(const QString &key,
                                                        const QVariant &value,
                                                        ValidationResult *validation) const
{
    const auto *setting = definition(key);
    if (setting == nullptr) {
        if (validation != nullptr) {
            validation->add(key,
                            QStringLiteral("unknown-key"),
                            QStringLiteral("setting is not defined by schema version %1").arg(m_version));
        }
        return std::nullopt;
    }

    QVariant normalized;
    QString message;
    if (!Internal::normalizeSettingValue(*setting, value, &normalized, &message)) {
        if (validation != nullptr) {
            validation->add(key, QStringLiteral("invalid-value"), message);
        }
        return std::nullopt;
    }
    return normalized;
}

std::optional<QVariantMap> SettingsSchema::normalizedLayer(const QVariantMap &values,
                                                           ValidationResult *validation) const
{
    ValidationResult local;
    QVariantMap normalized;
    for (auto iterator = values.cbegin(); iterator != values.cend(); ++iterator) {
        const auto value = normalizedValue(iterator.key(), iterator.value(), &local);
        if (value.has_value()) {
            normalized.insert(iterator.key(), *value);
        }
    }
    if (validation != nullptr) {
        *validation = local;
    }
    if (!local.isValid()) {
        return std::nullopt;
    }
    return normalized;
}

} // namespace QindaQt::Settings
