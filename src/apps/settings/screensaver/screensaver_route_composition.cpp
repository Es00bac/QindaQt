// SPDX-License-Identifier: GPL-3.0-or-later
#include "screensaver_route_composition.h"

#include <qindaqt/apps/settings_screen_lock/settings1_screen_lock_settings_model.h>
#include <qindaqt/apps/settings_screensaver/lock_screen_saver_store.h>
#include <qindaqt/apps/settings_screensaver/screensaver_preview.h>
#include <qindaqt/apps/settings_screensaver/screensaver_settings_model.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/lock_preferences/lock_preferences.h>
#include <qindaqt/session/desktop_controls/screensaver_catalog.h>
#include <qindaqt/session/desktop_controls/settings1_screensaver_preferences.h>

#include <QDBusConnection>
#include <QDir>
#include <QStandardPaths>

namespace QindaQt::Apps::SettingsScreensaver {

class ScreensaverRouteComposition::Private final {
public:
    Private()
        : transport(QDBusConnection::sessionBus()),
          client(transport,
                 Session::DesktopControls::Settings1ScreensaverPreferences::scopedKeys()),
          preferences(client, catalog),
          preview(catalog),
          model(preferences, client, catalog, lockScreenSaver, preview),
          lockTransport(QDBusConnection::sessionBus()),
          lockClient(lockTransport, Services::LockPreferences::scopedKeys()),
          lockPreferences(lockClient),
          screenLockSettings(lockClient, lockPreferences)
    {
        QString error;
        if (!client.start(&error)) {
            qWarning("screensaver settings: preference client failed: %s",
                     qUtf8Printable(error));
        }
        QString lockError;
        if (!lockClient.start(&lockError) && QDBusConnection::sessionBus().isConnected()) {
            qWarning("screensaver settings: lock preference client failed: %s",
                     qUtf8Printable(lockError));
        }
    }

    ~Private() { lockClient.stop(); client.stop(); }

    Services::SettingsClient::QtSettingsTransport transport;
    Services::SettingsClient::SettingsClient client;
    // The saver set is discovered from the installed desktop entries, not
    // hard-coded (ADR-0226); declared before the provider and model that
    // borrow it.
    Session::DesktopControls::DesktopEntryScreensaverCatalog catalog;
    Session::DesktopControls::Settings1ScreensaverPreferences preferences;
    // The lock-screen mirror writes only the `[Greeter]` wallpaper keys of
    // the same kscreenlockerrc the walk-away section's own store writes
    // `[Daemon]` in. Preview has no lock-screen process boundary.
    KConfigLockScreenSaverStore lockScreenSaver{
        QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
            .filePath(QStringLiteral("kscreenlockerrc"))};
    ProcessScreensaverPreview preview;
    ScreensaverSettingsModel model;
    Services::SettingsClient::QtSettingsTransport lockTransport;
    Services::SettingsClient::SettingsClient lockClient;
    Services::LockPreferences::PreferencesProvider lockPreferences;
    SettingsScreenLock::Settings1ScreenLockSettingsModel screenLockSettings;
};

ScreensaverRouteComposition::ScreensaverRouteComposition(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

ScreensaverRouteComposition::~ScreensaverRouteComposition() = default;

QObject *ScreensaverRouteComposition::model() const
{
    return &d->model;
}

QObject *ScreensaverRouteComposition::screenLockSettings() const
{
    return &d->screenLockSettings;
}

} // namespace QindaQt::Apps::SettingsScreensaver
