// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_route_composition.h"

#include <qindaqt/apps/settings_power/external_display_brightness_model.h>
#include <qindaqt/apps/settings_power/source_idle_display_settings.h>
#include <qindaqt/apps/settings_power/power_settings_model.h>
#include <qindaqt/apps/settings_screen_lock/settings1_screen_lock_settings_model.h>
#include <qindaqt/services/display_client/client.h>
#include <qindaqt/services/display_client/qt_display_transport.h>
#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/services/power_client/qt_power_transport.h>
#include <qindaqt/services/session_actions/session_actions_client.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/lock_preferences/lock_preferences.h>
#include <qindaqt/session/idle_policy/source_preferences.h>
#include <qindaqt/session/powerdevil_lid/powerdevil_lid_adapter.h>
#include <qindaqt/session/powerdevil_profile/powerdevil_profile_adapter.h>

#include "qt_powerdevil_lid_port.h"
#include "qt_powerdevil_profile_port.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtDBus/QDBusConnection>

namespace QindaQt::Apps::SettingsPower {

class PowerRouteComposition::Private final {
public:
  Private()
      : transport(QDBusConnection::sessionBus()), client(&transport),
        displayTransport(QDBusConnection::sessionBus()),
        displayClient(&displayTransport), externalBrightness(displayClient),
        sessionActions(QDBusConnection::sessionBus(),
                       QDBusConnection::systemBus()),
        screenLockTransport(QDBusConnection::sessionBus()),
        screenLockClient(screenLockTransport,
                         Services::LockPreferences::scopedKeys()),
        screenLockPreferences(screenLockClient),
        screenLockSettings(screenLockClient, screenLockPreferences),
        idleTransport(QDBusConnection::sessionBus()),
        idleClient(idleTransport, Session::IdlePolicy::perSourceDisplayOffSettingsKeys()),
        idleDisplaySettings(client, idleClient),
        lidPowerButton(QDBusConnection::sessionBus()),
        lidPowerButtonPort(lidPowerButton),
        profilePowerAdapter(QDBusConnection::sessionBus()),
        profilePowerPort(profilePowerAdapter),
        model(client, &sessionActions, &lidPowerButtonPort, &externalBrightness,
              &profilePowerPort) {
    client.start();
    if (QDBusConnection::sessionBus().isConnected()) {
      QString lockError;
      if (!screenLockClient.start(&lockError))
        qWarning("power settings: lock preference client failed: %s",
                 qUtf8Printable(lockError));
    }
    displayClient.start();
    sessionActions.start();
    lidPowerButton.start();
    profilePowerAdapter.start();
    // AGENT-GUARD: offscreen harnesses run with QT_FATAL_WARNINGS and no
    // session bus; a missing bus is the expected degraded route, not a
    // warning-worthy failure. Only a connected bus with a failed start logs.
    if (QDBusConnection::sessionBus().isConnected()) {
      QString idleError;
      if (!idleClient.start(&idleError)) {
        qWarning("power settings: idle display-off preference client failed: %s",
                 qUtf8Printable(idleError));
      }
    }
  }

  ~Private() {
    sessionActions.stop();
    client.stop();
    screenLockClient.stop();
    displayClient.stop();
    idleClient.stop();
    lidPowerButton.stop();
    profilePowerAdapter.stop();
  }

  Power::QtPowerTransport transport;
  Power::PowerClient client;
  // ADR-0150: external-display brightness uses its own public Display client;
  // member order destroys the model and client before their transport.
  DisplayClient::QtDisplayTransport displayTransport;
  DisplayClient::Client displayClient;
  ExternalDisplayBrightnessModel externalBrightness;
  Services::SessionActions::SessionActionsClient sessionActions;
  Services::SettingsClient::QtSettingsTransport screenLockTransport;
  Services::SettingsClient::SettingsClient screenLockClient;
  Services::LockPreferences::PreferencesProvider screenLockPreferences;
  SettingsScreenLock::Settings1ScreenLockSettingsModel screenLockSettings;
  Services::SettingsClient::QtSettingsTransport idleTransport;
  Services::SettingsClient::SettingsClient idleClient;
  SourceIdleDisplaySettingsModel idleDisplaySettings;
  Session::PowerDevilLid::PowerDevilLidAdapter lidPowerButton;
  QtPowerDevilLidPolicyPort lidPowerButtonPort;
  Session::PowerDevilProfile::PowerDevilProfileAdapter profilePowerAdapter;
  QtPowerDevilProfilePolicyPort profilePowerPort;
  PowerSettingsModel model;
};

PowerRouteComposition::PowerRouteComposition(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>()) {}

PowerRouteComposition::~PowerRouteComposition() = default;

QObject *PowerRouteComposition::model() const { return &d->model; }

QObject *PowerRouteComposition::screenLockSettings() const {
  return &d->screenLockSettings;
}

QObject *PowerRouteComposition::idleDisplaySettings() const {
  return &d->idleDisplaySettings;
}

} // namespace QindaQt::Apps::SettingsPower
