// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_route_composition.h"

#include <qindaqt/apps/settings_power/power_settings_model.h>
#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/services/power_client/qt_power_transport.h>

#include <QtDBus/QDBusConnection>

namespace QindaQt::Apps::SettingsPower {

class PowerRouteComposition::Private final {
public:
  Private()
      : transport(QDBusConnection::sessionBus()), client(&transport),
        model(client) {
    client.start();
  }

  ~Private() { client.stop(); }

  Power::QtPowerTransport transport;
  Power::PowerClient client;
  PowerSettingsModel model;
};

PowerRouteComposition::PowerRouteComposition(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>()) {}

PowerRouteComposition::~PowerRouteComposition() = default;

QObject *PowerRouteComposition::model() const { return &d->model; }

} // namespace QindaQt::Apps::SettingsPower
