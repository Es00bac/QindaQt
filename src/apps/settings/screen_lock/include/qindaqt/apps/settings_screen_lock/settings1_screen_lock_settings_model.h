#pragma once
#include <QObject>
#include <QVariant>
#include <QString>
#include <optional>
#include <qindaqt/services/lock_preferences/lock_preferences.h>
namespace QindaQt::Services::SettingsClient { class SettingsClient; }
namespace QindaQt::Apps::SettingsScreenLock {
class Settings1ScreenLockSettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool automaticLock READ automaticLock NOTIFY changed)
  Q_PROPERTY(int timeoutMinutes READ timeoutMinutes NOTIFY changed)
  Q_PROPERTY(bool lockOnResume READ lockOnResume NOTIFY changed)
  Q_PROPERTY(int lockGraceSeconds READ lockGraceSeconds NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
  Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
public:
  static constexpr int MinimumTimeoutMinutes = 1;
  static constexpr int MaximumTimeoutMinutes = 240;
  explicit Settings1ScreenLockSettingsModel(
      Services::SettingsClient::SettingsClient &client,
      Services::LockPreferences::PreferencesProvider &provider,
      QObject *parent = nullptr);
  bool automaticLock() const noexcept;
  int timeoutMinutes() const noexcept;
  bool lockOnResume() const noexcept;
  int lockGraceSeconds() const noexcept;
  bool busy() const noexcept;
  QString statusText() const;
  QString errorText() const;
  Q_INVOKABLE bool setAutomaticLock(bool enabled);
  Q_INVOKABLE bool setTimeoutMinutes(int minutes);
  Q_INVOKABLE bool setLockOnResume(bool enabled);
  Q_INVOKABLE bool setLockGraceSeconds(int seconds);
  Q_INVOKABLE bool retryLiveApply();
Q_SIGNALS:
  void changed();
private:
  void refresh();
  bool write(const QString &key, const QVariant &value);
  Services::SettingsClient::SettingsClient &m_client;
  Services::LockPreferences::PreferencesProvider &m_provider;
  std::optional<Services::LockPreferences::Preferences> m_values;
  QString m_statusText;
  QString m_errorText;
  bool m_writePending = false;
  bool m_readbackPending = false;
  QString m_pendingKey;
  QVariant m_pendingValue;
};
}
