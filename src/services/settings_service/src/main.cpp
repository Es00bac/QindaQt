// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/settings_service/resident_settings_service.h"
#include "qindaqt/settings/settings_schema.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <cstdio>

using namespace QindaQt::Services::SettingsService;

namespace {
QString schemaDirectory()
{
    const QString overridePath = qEnvironmentVariable("QINDAQT_SETTINGS_SCHEMA_DIR");
    if (!overridePath.isEmpty()) {
        return overridePath;
    }
    const QString installed = QDir(QCoreApplication::applicationDirPath())
                                  .absoluteFilePath(QStringLiteral("../share/qindaqt/settings"));
    if (QFileInfo::exists(installed + QStringLiteral("/schema-v2.json"))) {
        return installed;
    }
    return QStringLiteral(QINDAQT_SOURCE_SETTINGS_DIR);
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("qindaqt-settings-service"));
    const QString schemas = schemaDirectory();
    QString error;
    auto active = QindaQt::Settings::SettingsSchema::fromFile(
        schemas + QStringLiteral("/schema-v2.json"), nullptr, &error);
    auto legacy = QindaQt::Settings::SettingsSchema::fromFile(
        schemas + QStringLiteral("/schema-v1.json"), nullptr, &error, 1);
    if (!active || !legacy) {
        std::fprintf(stderr, "qindaqt-settings-service: %s\n", qPrintable(error));
        return 2;
    }
    const QString storage = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
                                .filePath(QStringLiteral("qindaqt/settings-v2.json"));
    const QString profileDefaults = schemas
                                    + QStringLiteral("/profile-defaults/qindaqt.json");
    ResidentSettingsService service(QDBusConnection::sessionBus(), std::move(*active),
                                    std::move(*legacy), profileDefaults, storage);
    const auto started = service.start();
    if (!started.ok()) {
        std::fprintf(stderr, "qindaqt-settings-service: %s: %s\n",
                     qPrintable(settingsServiceStartStatusName(started.status)),
                     qPrintable(started.message));
        return 3;
    }
    return application.exec();
}
