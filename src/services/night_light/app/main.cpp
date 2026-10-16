// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/night_light/night_light_schedule_service.h>
#include <qindaqt/services/night_light/night_light_schedule_client.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/display_client/client.h>
#include <qindaqt/services/display_client/qt_display_transport.h>

#include <qindaqt/platform/recovery_backoff/recovery_backoff.h>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QLoggingCategory>

#include <chrono>

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
    // AGENT-NOTE (ADR-0379): at login the bus can be slow to grant the name;
    // retry (1 s doubling to 4 s, five tries, about 11 s inside the unit's 20 s
    // start timeout) and log the real reason each time instead of exiting at
    // once. Name registration is a blocking call, so no retry runs sooner than
    // one second. Settings1 and Display1 are awaited asynchronously by their
    // clients, so their late arrival never fails startup.
    constexpr int MaximumRegistrationAttempts = 5;
    QindaQt::Platform::Recovery::RecoveryBackoff registration(
        {std::chrono::milliseconds{1'000}, std::chrono::milliseconds{4'000}});
    int attempts = 1;
    QObject::connect(&registration, &QindaQt::Platform::Recovery::RecoveryBackoff::retry,
                     &application, [&] {
        const auto retried = service.start(&error);
        ++attempts;
        if (retried == ScheduleServiceStart::Started) {
            qInfo("NightLight registered its session service after %d attempts", attempts);
        } else if (retried == ScheduleServiceStart::NameAlreadyOwned) {
            qInfo("NightLight startup stopped because a sibling owns its session service name");
            application.exit(0);
        } else if (attempts >= MaximumRegistrationAttempts) {
            qCritical("NightLight service startup failed after %d attempts: %s", attempts,
                      qPrintable(error));
            application.exit(1);
        } else {
            qWarning("NightLight service startup retrying: %s", qPrintable(error));
            registration.schedule();
        }
    });
    const auto status = service.start(&error);
    if (status == ScheduleServiceStart::NameAlreadyOwned) {
        qInfo("NightLight startup stopped because a sibling owns its session service name");
        return 0;
    }
    if (status != ScheduleServiceStart::Started) {
        qWarning("NightLight service startup retrying: %s", qPrintable(error));
        registration.schedule();
    }
    QObject::connect(&application, &QCoreApplication::aboutToQuit,
                     &service, &NightLightScheduleService::stop);
    return application.exec();
}
