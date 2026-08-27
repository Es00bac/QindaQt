// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/settings/settings_schema.h"

#include <QByteArray>
#include <QString>
#include <QVariantMap>

namespace QindaQt::Settings {

struct SettingsDocument final {
    int schemaVersion = QINDAQT_SETTINGS_SCHEMA_VERSION;
    SettingLayer layer = SettingLayer::UserOverrides;
    QVariantMap values;
};

struct DocumentLoadResult final {
    bool ok = false;
    SettingsDocument document;
    // Original on-disk/input version. This stays 1 when compatibility loading
    // returns a validated v2 migration candidate, so the service can decide
    // whether it must durably replace a user document after winning its name.
    int sourceSchemaVersion = 0;
    ValidationResult validation;
    QString error;
};

class SettingsDocumentCodec final {
public:
    [[nodiscard]] static DocumentLoadResult fromJson(const QByteArray &json,
                                                     const QString &origin,
                                                     const SettingsSchema &schema);
    [[nodiscard]] static std::optional<QByteArray> toJson(const SettingsDocument &document,
                                                         const SettingsSchema &schema,
                                                         ValidationResult *validation = nullptr,
                                                         QString *error = nullptr);
};

class SettingsFileStore final {
public:
    [[nodiscard]] static DocumentLoadResult load(const QString &path,
                                                 const SettingsSchema &schema);
    // AGENT-CONTRACT: save validates before opening the destination and commits
    // through QSaveFile. Callers own parent-directory creation and permissions.
    [[nodiscard]] static bool save(const QString &path,
                                   const SettingsDocument &document,
                                   const SettingsSchema &schema,
                                   ValidationResult *validation = nullptr,
                                   QString *error = nullptr);
};

} // namespace QindaQt::Settings
