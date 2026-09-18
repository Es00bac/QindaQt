// SPDX-License-Identifier: GPL-3.0-or-later
#include "streaming_route_composition.h"

#include <qindaqt/apps/settings_streaming/settings1_streaming_preferences.h>
#include <qindaqt/apps/settings_streaming/streaming_settings_model.h>

#include <qindaqt/services/obs_client/obs_provisioning.h>
#include <qindaqt/services/obs_client/qt_obs_transport.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>

#include <QtDBus/QDBusConnection>

namespace QindaQt::Apps::SettingsStreaming {

class StreamingRouteComposition::Private final {
public:
    Private()
        : bus(QDBusConnection::sessionBus()), secrets(bus),
          settingsTransport(bus),
          settingsClient(settingsTransport,
                         Settings1StreamingPreferences::scopedKeys()),
          preferences(settingsClient),
          model(client, secrets, preferences, Obs::defaultObsConfigRoot()) {
        QString error;
        // A Settings1 owner that is not up yet leaves the preferences at
        // their documented defaults rather than blocking the route.
        if (!settingsClient.start(&error)) {
            settingsStartError = error;
        }
    }

    QDBusConnection bus;
    Obs::QtObsTransport transport;
    Obs::ObsClient client{transport};
    Obs::SecretServiceObsStore secrets;
    Services::SettingsClient::QtSettingsTransport settingsTransport;
    Services::SettingsClient::SettingsClient settingsClient;
    Settings1StreamingPreferences preferences;
    StreamingSettingsModel model;
    QString settingsStartError;
};

StreamingRouteComposition::StreamingRouteComposition(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>()) {}

StreamingRouteComposition::~StreamingRouteComposition() = default;

QObject *StreamingRouteComposition::streaming() const { return &d->model; }

} // namespace QindaQt::Apps::SettingsStreaming
