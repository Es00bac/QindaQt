// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_display/display_night_light_route.h>

#include "qindaqt/compositor_names/compositor_names.h"

#include <qindaqt/apps/settings_display/display_night_light_model.h>
#include <qindaqt/services/night_light/night_light_schedule_client.h>
#include <qindaqt/services/night_light/night_light_settings_importer.h>
#include <qindaqt/services/night_light/night_light_state_port.h>
#include <qindaqt/services/night_light/night_light_config_port.h>
#include <qindaqt/services/night_light/night_light_values.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include <QtCore/QLoggingCategory>
#include <QtCore/QStandardPaths>
#include <QtDBus/QDBusConnection>

namespace QindaQt::Apps::SettingsDisplay {

namespace {

Q_LOGGING_CATEGORY(lcDisplayNightLightRoute,
                   "qindaqt.settings.display.nightlight", QtInfoMsg)

QString configFilePath(const QString &fileName)
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QLatin1Char('/') + fileName;
}

QStringList displayNightLightKeys()
{
    return {
        QStringLiteral("display.nightLight.active"),
        QStringLiteral("display.nightLight.mode"),
        QStringLiteral("display.nightLight.dayTemperatureKelvin"),
        QStringLiteral("display.nightLight.nightTemperatureKelvin"),
        QStringLiteral("display.nightLight.scheduleSource"),
        QStringLiteral("display.nightLight.automaticLocation"),
        QStringLiteral("display.nightLight.latitudeDegrees"),
        QStringLiteral("display.nightLight.longitudeDegrees"),
        QStringLiteral("display.nightLight.sunriseStart"),
        QStringLiteral("display.nightLight.sunsetStart"),
        QStringLiteral("display.nightLight.transitionSeconds"),
        QStringLiteral("display.nightLight.disabledOutputs"),
        QStringLiteral("display.nightLight.legacyImported")};
}

} // namespace

class DisplayNightLightRoute::Private {
public:
    Private()
        : bus(QDBusConnection::sessionBus()),
          settingsTransport(bus),
          settingsClient(settingsTransport, displayNightLightKeys()),
          legacyReader(
              configFilePath(QString(QindaQt::CompositorNames::configFile)),
              configFilePath(QStringLiteral("knighttimerc")), bus),
          statePort(bus),
          scheduleClient(bus),
          importer(settingsClient, legacyReader),
          model(settingsClient, statePort, scheduleClient, importer)
    {
        // AGENT-GUARD: Busless previews run with fatal Qt warnings. Clients
        // stay stopped there, leaving each public model boundary unavailable.
        if (!bus.isConnected()) {
            qCInfo(lcDisplayNightLightRoute,
                   "no session bus; night light renders unavailable truth");
            return;
        }
        QString error;
        if (!settingsClient.start(&error)) {
            qCWarning(lcDisplayNightLightRoute,
                      "Settings1 client did not start: %s", qPrintable(error));
        }
        statePort.start();
        scheduleClient.start();
        importer.start();
    }

    ~Private()
    {
        scheduleClient.stop();
        statePort.stop();
        settingsClient.stop();
    }

    QDBusConnection bus;
    QindaQt::Services::SettingsClient::QtSettingsTransport settingsTransport;
    QindaQt::Services::SettingsClient::SettingsClient settingsClient;
    QindaQt::Services::NightLight::QtConfigNightLightPort legacyReader;
    QindaQt::Services::NightLight::QtNightLightStatePort statePort;
    QindaQt::Services::NightLight::QtNightLightScheduleClient scheduleClient;
    QindaQt::Services::NightLight::NightLightSettingsImporter importer;
    DisplayNightLightModel model;
};

DisplayNightLightRoute::DisplayNightLightRoute(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>())
{
}

DisplayNightLightRoute::~DisplayNightLightRoute() = default;

QObject *DisplayNightLightRoute::model() const
{
    return &d->model;
}

} // namespace QindaQt::Apps::SettingsDisplay
