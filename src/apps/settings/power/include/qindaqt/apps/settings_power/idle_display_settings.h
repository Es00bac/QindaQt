// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/session/desktop_controls/idle_display_preferences.h>
#include <qindaqt/session/desktop_controls/settings1_idle_preferences.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include <QObject>
#include <QString>

namespace QindaQt::Apps::SettingsPower {

// Settings Power route model for the configurable idle display-off policy.
// Truth comes from the purpose-scoped Settings1 key
// `power.idleDisplayOffMinutes` through the shared provider; writes go
// through the same purpose-scoped client, so an uncertain commit never
// fabricates success and the next confirmed snapshot reconciles the route.
class IdleDisplaySettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
  Q_PROPERTY(int minutes READ minutes NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
  Q_PROPERTY(QString errorText READ errorText NOTIFY changed)

public:
  explicit IdleDisplaySettingsModel(
      Session::DesktopControls::Settings1IdlePreferences &preferences,
      Services::SettingsClient::SettingsClient &client, QObject *parent = nullptr);
  ~IdleDisplaySettingsModel() override;

  IdleDisplaySettingsModel(const IdleDisplaySettingsModel &) = delete;
  IdleDisplaySettingsModel &operator=(const IdleDisplaySettingsModel &) = delete;

  [[nodiscard]] bool enabled() const noexcept;
  [[nodiscard]] int minutes() const noexcept;
  [[nodiscard]] bool busy() const noexcept;
  [[nodiscard]] const QString &statusText() const noexcept;
  [[nodiscard]] const QString &errorText() const noexcept;

  Q_INVOKABLE bool setEnabled(bool enabled);
  Q_INVOKABLE bool setMinutes(int minutes);
  Q_INVOKABLE bool retry();

Q_SIGNALS:
  void changed();

private:
  void publishStatus();
  [[nodiscard]] bool submit(qint64 persistedMinutes);

  Session::DesktopControls::Settings1IdlePreferences &m_preferences;
  Services::SettingsClient::SettingsClient &m_client;
  QString m_statusText;
  QString m_errorText;
  bool m_busy = false;
};

} // namespace QindaQt::Apps::SettingsPower
