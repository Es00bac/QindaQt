// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/settings_client/do_not_disturb_controller.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <cstdio>

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
    parser.process(application);
    const QString page = parser.value(pageOption);
    if (page != QLatin1String("notifications")) {
        std::fprintf(stderr, "qindaqt-settings: unknown page: %s\n", qPrintable(page));
        return 2;
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

    QQmlApplicationEngine engine;
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
