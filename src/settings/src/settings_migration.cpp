// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/settings/settings_migration.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace QindaQt::Settings {
namespace {

DocumentLoadResult failure(QString message, ValidationResult validation = {})
{
    return {.ok = false, .document = {}, .validation = std::move(validation), .error = std::move(message)};
}

} // namespace

DocumentLoadResult SettingsMigration::migrateV1ToV2(const QByteArray &v1Json,
                                                     const QString &origin,
                                                     const SettingsSchema &v1Schema,
                                                     const SettingsSchema &v2Schema)
{
    if (v1Schema.version() != 1 || v2Schema.version() != 2) {
        return failure(
            QStringLiteral("migrateV1ToV2 requires schema versions 1 and 2, got %1 and %2")
                .arg(v1Schema.version())
                .arg(v2Schema.version()));
    }

    const auto loaded = SettingsDocumentCodec::fromJson(v1Json, origin, v1Schema);
    if (!loaded.ok) {
        // Corrupt/invalid v1 input fails exactly as an ordinary v1 load
        // would; nothing about migration changes that outcome.
        return loaded;
    }

    SettingsDocument migrated;
    migrated.schemaVersion = v2Schema.version();
    migrated.layer = loaded.document.layer;
    // AGENT-NOTE: every v1 key is also a v2 key (v2 only adds
    // "services.doNotDisturb"), so every normalized v1 value carries forward
    // unchanged. The new key is deliberately left absent here so it resolves
    // through ordinary layered-resolution default backfill (false) instead
    // of the migrator inventing a value the user never chose.
    migrated.values = loaded.document.values;

    ValidationResult validation;
    const auto normalized = v2Schema.normalizedLayer(migrated.values, &validation);
    if (!normalized.has_value()) {
        return failure(origin
                           + QStringLiteral(": migrated document failed v2 validation: ")
                           + validation.summary(),
                       validation);
    }
    migrated.values = *normalized;
    return {.ok = true, .document = migrated, .validation = {}, .error = {}};
}

DocumentLoadResult SettingsCompatibilityLoader::load(const QString &path,
                                                      const SettingsSchema &activeSchema,
                                                      const SettingsSchema &legacySchema)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return failure(path + QStringLiteral(": ") + file.errorString());
    }
    const QByteArray bytes = file.readAll();
    file.close();

    QJsonParseError parseError;
    const auto parsed = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
        return failure(path + QStringLiteral(": invalid JSON: ") + parseError.errorString());
    }

    const int sniffedVersion = parsed.object().value(QStringLiteral("schemaVersion")).toInt(-1);
    if (sniffedVersion == activeSchema.version()) {
        return SettingsDocumentCodec::fromJson(bytes, path, activeSchema);
    }
    if (sniffedVersion == legacySchema.version()) {
        return SettingsMigration::migrateV1ToV2(bytes, path, legacySchema, activeSchema);
    }
    return failure(path + QStringLiteral(": unsupported schemaVersion %1").arg(sniffedVersion));
}

} // namespace QindaQt::Settings
