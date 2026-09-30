// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/night_light/night_light_schedule_service.h>
#include <qindaqt/services/night_light/night_light_schedule_client.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/display_client/client.h>
#include <qindaqt/services/display_client/qt_display_transport.h>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QLoggingCategory>

using namespace QindaQt::Services::NightLight;
using namespace QindaQt::Services::SettingsClient;

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("qindaqt-night-light-service"));
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.connect(QString{}, QStringLiteral("/org/freedesktop/DBus/Local"),
                     QStringLiteral("org.freedesktop.DBus.Local"),
                     QStringLiteral("Disconnected"), &application, SLOT(quit()))) {
        qCritical("NightLight could not bind constructing session-bus lifetime");
        return 1;
    }
    QtSettingsTransport transport(bus);
    SettingsClient settings(transport, nightLightSettingKeys());
    QString error;
    if (!settings.start(&error)) {
        qCritical("NightLight could not start Settings1 client: %s", qPrintable(error));
        return 1;
    }
    QindaQt::DisplayClient::QtDisplayTransport displayTransport(bus);
    QindaQt::DisplayClient::Client displayClient(&displayTransport);
    displayClient.start();
    GeoClueLocationProvider location(QDBusConnection::systemBus());
    NightLightScheduleService service(bus, settings, displayClient, location);
    const auto status = service.start(&error);
    if (status == ScheduleServiceStart::NameAlreadyOwned) {
        qInfo("NightLight startup stopped because a sibling owns its session service name");
        return 0;
    }
    if (status != ScheduleServiceStart::Started) {
        qCritical("NightLight service startup failed: %s", qPrintable(error));
        return 1;
    }
    QObject::connect(&application, &QCoreApplication::aboutToQuit,
                     &service, &NightLightScheduleService::stop);
    return application.exec();
}
