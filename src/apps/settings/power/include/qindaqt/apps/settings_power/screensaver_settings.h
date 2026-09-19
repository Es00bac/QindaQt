// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/apps/settings_power/lock_screen_saver_store.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/session/desktop_controls/screensaver_preferences.h>
#include <qindaqt/session/desktop_controls/settings1_screensaver_preferences.h>

#include <QObject>
#include <QString>

namespace QindaQt::Apps::SettingsPower {

// Settings Power route model for the idle screensaver. Truth comes from the
// purpose-scoped Settings1 pair `power.screensaver` /
// `power.screensaverMinutes` through the shared provider; writes go through
// the same purpose-scoped client, so an uncertain commit never fabricates
// success and the next confirmed snapshot reconciles the route.
//
// Confirmed truth is also mirrored into the screen locker's greeter through
// the injected LockScreenSaverStore, so a locked session keeps showing the
// saver the user chose (ADR-0216). That mirror is the only thing this route
// writes outside Settings1, and it never touches the automatic-lock or
// display-off preferences, which keep their own sections.
class ScreensaverSettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString saver READ saver NOTIFY changed)
  Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
  Q_PROPERTY(int minutes READ minutes NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
  Q_PROPERTY(QString errorText READ errorText NOTIFY changed)

public:
  ScreensaverSettingsModel(
      Session::DesktopControls::Settings1ScreensaverPreferences &preferences,
      Services::SettingsClient::SettingsClient &client,
      LockScreenSaverStore &lockScreenSaver, QObject *parent = nullptr);
  ~ScreensaverSettingsModel() override;

  ScreensaverSettingsModel(const ScreensaverSettingsModel &) = delete;
  ScreensaverSettingsModel &operator=(const ScreensaverSettingsModel &) = delete;

  [[nodiscard]] QString saver() const;
  [[nodiscard]] bool enabled() const;
  [[nodiscard]] int minutes() const;
  [[nodiscard]] bool busy() const noexcept;
  [[nodiscard]] const QString &statusText() const noexcept;
  [[nodiscard]] const QString &errorText() const noexcept;

  Q_INVOKABLE bool setSaver(const QString &saver);
  Q_INVOKABLE bool setMinutes(int minutes);
  Q_INVOKABLE bool retry();

Q_SIGNALS:
  void changed();

private:
  void publishStatus();
  // Mirrors confirmed truth into the greeter. A failure here is reported as
  // its own error: the preference itself is persisted either way.
  void mirrorToLockScreen(const QString &saver);
  [[nodiscard]] bool submit(const QString &key, const QVariant &value);

  Session::DesktopControls::Settings1ScreensaverPreferences &m_preferences;
  Services::SettingsClient::SettingsClient &m_client;
  LockScreenSaverStore &m_lockScreenSaver;
  QString m_statusText;
  QString m_errorText;
  bool m_busy = false;
};

} // namespace QindaQt::Apps::SettingsPower
