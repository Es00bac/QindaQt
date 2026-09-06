// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_route_composition.h"

#include <qindaqt/apps/settings_power/power_settings_model.h>
#include <qindaqt/apps/settings_power/screen_lock_settings.h>
#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/services/power_client/qt_power_transport.h>
#include <qindaqt/services/session_actions/session_actions_client.h>

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtDBus/QDBusConnection>

namespace QindaQt::Apps::SettingsPower {

class PowerRouteComposition::Private final {
public:
  Private()
      : transport(QDBusConnection::sessionBus()), client(&transport),
        sessionActions(QDBusConnection::sessionBus(),
                       QDBusConnection::systemBus()),
        screenLockStore(std::make_unique<IniScreenLockPreferencesStore>(
            QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
                .filePath(QStringLiteral("kscreenlockerrc")))),
        screenLockConfigure(),
        screenLockSettings(std::move(screenLockStore), screenLockConfigure),
        model(client, &sessionActions) {
    client.start();
    sessionActions.start();
  }

  ~Private() {
    sessionActions.stop();
    client.stop();
  }

  Power::QtPowerTransport transport;
  Power::PowerClient client;
  Services::SessionActions::SessionActionsClient sessionActions;
  std::unique_ptr<IniScreenLockPreferencesStore> screenLockStore;
  QtScreenLockConfigureClient screenLockConfigure;
  ScreenLockSettingsModel screenLockSettings;
  PowerSettingsModel model;
};

PowerRouteComposition::PowerRouteComposition(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>()) {}

PowerRouteComposition::~PowerRouteComposition() = default;

QObject *PowerRouteComposition::model() const { return &d->model; }

QObject *PowerRouteComposition::screenLockSettings() const {
  return &d->screenLockSettings;
}

} // namespace QindaQt::Apps::SettingsPower
