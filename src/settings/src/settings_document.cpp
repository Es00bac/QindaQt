// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/settings/settings_document.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

#include <utility>

namespace QindaQt::Settings {
namespace {

DocumentLoadResult failure(const QString &message, ValidationResult validation = {})
{
    return {.ok = false, .document = {}, .sourceSchemaVersion = 0,
            .validation = std::move(validation), .error = message};
}

bool validateDocumentHeader(const SettingsDocument &document,
                            const SettingsSchema &schema,
                            QString *error)
{
    if (document.schemaVersion != schema.version()) {
        if (error != nullptr) {
            *error = QStringLiteral("document schemaVersion %1 does not match active version %2")
                         .arg(document.schemaVersion)
                         .arg(schema.version());
        }
        return false;
    }
    // AGENT-CONTRACT: System defaults are versioned inside the schema and
    // session overrides are volatile. Only profile and user documents cross restarts.
    if (!isPersistentLayer(document.layer)) {
        if (error != nullptr) {
            *error = QStringLiteral("layer %1 is not persistable").arg(toString(document.layer));
        }
        return false;
    }
    return true;
}

} // namespace

DocumentLoadResult SettingsDocumentCodec::fromJson(const QByteArray &json,
                                                    const QString &origin,
                                                    const SettingsSchema &schema)
{
    QJsonParseError parseError;
    const auto parsed = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
        return failure(origin + QStringLiteral(": invalid JSON: ") + parseError.errorString());
    }

    const auto root = parsed.object();
    SettingsDocument document;
    document.schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(-1);
    if (!parseSettingLayer(root.value(QStringLiteral("layer")).toString(), &document.layer)) {
        return failure(origin + QStringLiteral(": unknown or missing layer"));
    }
    QString headerError;
    if (!validateDocumentHeader(document, schema, &headerError)) {
        return failure(origin + QStringLiteral(": ") + headerError);
    }
    if (!root.value(QStringLiteral("values")).isObject()) {
        return failure(origin + QStringLiteral(": values must be an object"));
    }

    ValidationResult validation;
    const auto normalized = schema.normalizedLayer(
        root.value(QStringLiteral("values")).toObject().toVariantMap(), &validation);
    if (!normalized.has_value()) {
        return failure(origin + QStringLiteral(": ") + validation.summary(), validation);
    }
    document.values = *normalized;
    return {.ok = true, .document = document, .sourceSchemaVersion = document.schemaVersion,
            .validation = {}, .error = {}};
}

std::optional<QByteArray> SettingsDocumentCodec::toJson(const SettingsDocument &document,
                                                        const SettingsSchema &schema,
                                                        ValidationResult *validation,
                                                        QString *error)
{
    QString headerError;
    if (!validateDocumentHeader(document, schema, &headerError)) {
        if (error != nullptr) {
            *error = headerError;
        }
        return std::nullopt;
    }

    ValidationResult localValidation;
    const auto normalized = schema.normalizedLayer(document.values, &localValidation);
    if (validation != nullptr) {
        *validation = localValidation;
    }
    if (!normalized.has_value()) {
        if (error != nullptr) {
            *error = localValidation.summary();
        }
        return std::nullopt;
    }

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), document.schemaVersion);
    root.insert(QStringLiteral("layer"), toString(document.layer));
    root.insert(QStringLiteral("values"), QJsonObject::fromVariantMap(*normalized));
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

DocumentLoadResult SettingsFileStore::load(const QString &path, const SettingsSchema &schema)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return failure(path + QStringLiteral(": ") + file.errorString());
    }
    return SettingsDocumentCodec::fromJson(file.readAll(), path, schema);
}

bool SettingsFileStore::save(const QString &path,
                             const SettingsDocument &document,
                             const SettingsSchema &schema,
                             ValidationResult *validation,
                             QString *error)
{
    const auto encoded = SettingsDocumentCodec::toJson(document, schema, validation, error);
    if (!encoded.has_value()) {
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = path + QStringLiteral(": ") + file.errorString();
        }
        return false;
    }
    if (file.write(*encoded) != encoded->size()) {
        if (error != nullptr) {
            *error = path + QStringLiteral(": ") + file.errorString();
        }
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error != nullptr) {
            *error = path + QStringLiteral(": ") + file.errorString();
        }
        return false;
    }
    return true;
}

} // namespace QindaQt::Settings
