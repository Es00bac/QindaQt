// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/apps/settings_appearance/appearance_settings_model.h"
#include "qindaqt/apps/settings_appearance/appearance_values.h"
#include "qindaqt/services/settings_client/do_not_disturb_controller.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/themes/theme_catalog.h"

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QStyleHints>

#include <cstdio>

namespace {

// Same installed-theme discovery contract as the text editor: standard data
// locations first, then the layout beside the installed executable.
[[nodiscard]] QStringList themeSearchDirectories()
{
    QStringList directories;
    directories.append(QStandardPaths::locateAll(
        QStandardPaths::GenericDataLocation, QStringLiteral("qindaqt/themes"),
        QStandardPaths::LocateDirectory));
    directories.append(
        QDir(QCoreApplication::applicationDirPath())
            .absoluteFilePath(QStringLiteral("../share/qindaqt/themes")));
    directories.removeDuplicates();
    return directories;
}

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("qindaqt-settings"));
    application.setOrganizationName(QStringLiteral("QindaQt"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("QindaQt Settings"));
    parser.addHelpOption();
    const QCommandLineOption pageOption(QStringLiteral("page"),
                                        QStringLiteral("Open a settings page"),
                                        QStringLiteral("route"),
                                        QStringLiteral("notifications"));
    parser.addOption(pageOption);
    const QCommandLineOption themeDirectoryOption(
        QStringLiteral("theme-directory"),
        QStringLiteral("Additional local theme directory"), QStringLiteral("path"));
    parser.addOption(themeDirectoryOption);
    parser.process(application);
    const QString page = parser.value(pageOption);
    const bool appearanceRoute = page == QLatin1String("appearance");
    if (!appearanceRoute && page != QLatin1String("notifications")) {
        std::fprintf(stderr, "qindaqt-settings: unknown page: %s\n", qPrintable(page));
        return 2;
    }

    QQmlApplicationEngine engine;
    // AGENT-NOTE: Build-tree runs resolve sibling QML modules (Tokens,
    // Controls, SettingsApp.Appearance) from the generated qml directory;
    // installed prefixes use Qt's default import path instead. Adding a
    // nonexistent path is harmless, so one unconditional seam covers both.
    engine.addImportPath(QDir(QCoreApplication::applicationDirPath())
                             .absoluteFilePath(QStringLiteral("../qml")));

    if (appearanceRoute) {
        // AGENT-GUARD: The route stack must outlive application.exec(); every
        // collaborator below is deliberately a local of this branch. QST-1
        // requires one complete token publication before page QML loads, so
        // the facade is bound and handed to the model before loadFromModule.
        QindaQt::Services::SettingsClient::QtSettingsTransport transport(
            QDBusConnection::sessionBus());
        QindaQt::Services::SettingsClient::SettingsClient client(
            transport,
            QindaQt::Apps::SettingsAppearance::AppearanceKeys::scopedKeys());

        QindaQt::Themes::ThemeCatalog catalog;
        QStringList directories = themeSearchDirectories();
        const QString explicitThemeDirectory =
            parser.value(themeDirectoryOption);
        if (!explicitThemeDirectory.isEmpty()) {
            directories.prepend(QFileInfo(explicitThemeDirectory).absoluteFilePath());
        }
        QString catalogError;
        bool catalogLoaded = false;
        for (const QString &directory : directories) {
            if (catalog.loadDirectory(directory, &catalogError)) {
                catalogLoaded = true;
                break;
            }
        }
        if (!catalogLoaded) {
            std::fprintf(stderr, "qindaqt-settings: %s\n", qPrintable(catalogError));
            return 3;
        }

        QString facadeError;
        auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
            engine, &facadeError);
        if (facade == nullptr) {
            std::fprintf(stderr, "qindaqt-settings: %s\n", qPrintable(facadeError));
            return 3;
        }

        QindaQt::Apps::SettingsAppearance::AppearanceSettingsModel
            appearanceSettings(client, catalog.themes(),
                               application.styleHints()->colorScheme(), facade);
        QString clientError;
        if (!client.start(&clientError)) {
            qWarning("qindaqt-settings: Settings1 client unavailable: %s",
                     qPrintable(clientError));
        }

        engine.setInitialProperties(
            {{QStringLiteral("appearanceSettings"),
              QVariant::fromValue(static_cast<QObject *>(&appearanceSettings))},
             {QStringLiteral("route"), page}});
        engine.loadFromModule(QStringLiteral("QindaQt.SettingsApp"),
                              QStringLiteral("Main"));
        if (engine.rootObjects().isEmpty()) {
            return 3;
        }
        return application.exec();
    }

    QindaQt::Services::SettingsClient::QtSettingsTransport transport(
        QDBusConnection::sessionBus());
    QindaQt::Services::SettingsClient::SettingsClient client(
        transport, {QStringLiteral("services.doNotDisturb")});
    QindaQt::Services::SettingsClient::DoNotDisturbController quieting(client);
    QString error;
    if (!client.start(&error)) {
        qWarning("qindaqt-settings: Settings1 client unavailable: %s", qPrintable(error));
    }

    engine.setInitialProperties({
        {QStringLiteral("quietingSettings"),
         QVariant::fromValue(static_cast<QObject *>(&quieting))},
        {QStringLiteral("route"), page},
    });
    engine.loadFromModule(QStringLiteral("QindaQt.SettingsApp"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) {
        return 3;
    }
    return application.exec();
}
