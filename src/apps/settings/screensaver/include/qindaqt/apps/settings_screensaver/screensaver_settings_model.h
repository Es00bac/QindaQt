// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/apps/settings_screensaver/lock_screen_saver_store.h>
#include <qindaqt/apps/settings_screensaver/screensaver_preview.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/session/desktop_controls/screensaver_preferences.h>
#include <qindaqt/session/desktop_controls/settings1_screensaver_preferences.h>

#include <QObject>
#include <QString>
#include <QVariantList>

namespace QindaQt::Session::DesktopControls {
class ScreensaverCatalog;
}

namespace QindaQt::Apps::SettingsScreensaver {

// Screen saver route model (ADR-0226). Truth comes from the purpose-scoped
// Settings1 pair `power.screensaver` / `power.screensaverMinutes` through the
// shared provider; writes go through the same purpose-scoped client, so an
// uncertain commit never fabricates success and the next confirmed snapshot
// reconciles the route.
//
// The saver list is discovered from the installed desktop entries through the
// shared catalog, never hard-coded, so a newly packaged saver appears without
// a code change; "none" and "blank" are the two built-in choices. Confirmed
// truth is also mirrored into the screen locker's greeter through the
// injected LockScreenSaverStore, so a locked session keeps showing the saver
// the user chose (ADR-0216) -- including "blank", which is the plugin's plain
// dark ground. That mirror is the only thing this model writes outside
// Settings1, and it never touches the automatic-lock or display-off
// preferences, which keep their own stores.
class ScreensaverSettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList saverOptions READ saverOptions NOTIFY saversChanged)
  Q_PROPERTY(QString saver READ saver NOTIFY changed)
  Q_PROPERTY(bool delayEnabled READ delayEnabled NOTIFY changed)
  Q_PROPERTY(int minutes READ minutes NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
  Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
  Q_PROPERTY(bool previewAvailable READ previewAvailable NOTIFY changed)
  Q_PROPERTY(bool previewRunning READ previewRunning NOTIFY changed)
  Q_PROPERTY(QString previewSummary READ previewSummary NOTIFY changed)

public:
  ScreensaverSettingsModel(
      Session::DesktopControls::Settings1ScreensaverPreferences &preferences,
      Services::SettingsClient::SettingsClient &client,
      const Session::DesktopControls::ScreensaverCatalog &catalog,
      LockScreenSaverStore &lockScreenSaver, ScreensaverPreview &preview,
      QObject *parent = nullptr);
  ~ScreensaverSettingsModel() override;

  ScreensaverSettingsModel(const ScreensaverSettingsModel &) = delete;
  ScreensaverSettingsModel &operator=(const ScreensaverSettingsModel &) = delete;

  // One row per choice: the two built-ins, then every discovered saver. Each
  // row carries token, name, comment, iconName, and showsOnLockScreen.
  [[nodiscard]] QVariantList saverOptions() const;
  [[nodiscard]] QString saver() const;
  // Whether the "start after" delay row is live: a real program is chosen,
  // so not for "none" or "blank".
  [[nodiscard]] bool delayEnabled() const;
  [[nodiscard]] int minutes() const;
  [[nodiscard]] bool busy() const noexcept;
  [[nodiscard]] const QString &statusText() const noexcept;
  [[nodiscard]] const QString &errorText() const noexcept;
  [[nodiscard]] bool previewAvailable() const;
  [[nodiscard]] bool previewRunning() const;
  [[nodiscard]] const QString &previewSummary() const noexcept;

  Q_INVOKABLE bool setSaver(const QString &saver);
  Q_INVOKABLE bool setMinutes(int minutes);
  Q_INVOKABLE bool retry();
  Q_INVOKABLE bool preview();
  // Re-scans the catalog (a saver package (un)installed while the page is
  // open) and re-publishes the option list; persisted truth is untouched.
  Q_INVOKABLE void refreshSavers();

Q_SIGNALS:
  void changed();
  void saversChanged();

private:
  void publishStatus();
  void publishPreviewSummary();
  // Mirrors confirmed truth into the greeter. A failure here is reported as
  // its own error: the preference itself is persisted either way.
  void mirrorToLockScreen(const QString &saver);
  [[nodiscard]] bool submit(const QString &key, const QVariant &value);
  [[nodiscard]] QString displayName(const QString &saver) const;

  Session::DesktopControls::Settings1ScreensaverPreferences &m_preferences;
  Services::SettingsClient::SettingsClient &m_client;
  const Session::DesktopControls::ScreensaverCatalog &m_catalog;
  LockScreenSaverStore &m_lockScreenSaver;
  ScreensaverPreview &m_preview;
  QString m_statusText;
  QString m_errorText;
  QString m_previewSummary;
  bool m_busy = false;
};

} // namespace QindaQt::Apps::SettingsScreensaver
