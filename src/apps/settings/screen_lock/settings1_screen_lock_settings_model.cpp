#include <qindaqt/apps/settings_screen_lock/settings1_screen_lock_settings_model.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>
#include <algorithm>
namespace QindaQt::Apps::SettingsScreenLock {
using Services::SettingsProtocol::SettingsWireStatus;
Settings1ScreenLockSettingsModel::Settings1ScreenLockSettingsModel(
    Services::SettingsClient::SettingsClient &client,
    Services::LockPreferences::PreferencesProvider &provider, QObject *parent)
    : QObject(parent), m_client(client), m_provider(provider) {
  connect(&m_provider, &Services::LockPreferences::PreferencesProvider::changed,
          this, &Settings1ScreenLockSettingsModel::refresh);
  connect(&m_client, &Services::SettingsClient::SettingsClient::commitFinished,
          this, [this](const Services::SettingsClient::CommitOutcome &outcome) {
    if (!m_writePending) return;
    m_writePending = false;
    if (outcome.status == SettingsWireStatus::Applied) {
      m_errorText.clear();
      m_readbackPending = true;
      m_statusText = tr("Confirming saved screen-lock preferences.");
    } else {
      m_readbackPending = false;
      m_errorText = outcome.message.isEmpty()
          ? tr("Settings1 rejected the screen-lock preference.")
          : outcome.message;
      m_statusText = tr("Screen-lock preferences were not saved.");
    }
    refresh();
  });
  connect(&m_client, &Services::SettingsClient::SettingsClient::commitUncertain,
          this, [this](const QString &error) {
    if (!m_writePending) return;
    m_writePending = false;
    m_readbackPending = false;
    m_errorText = error;
    m_statusText = tr("The screen-lock preference save has an uncertain result.");
    refresh();
  });
  refresh();
}
void Settings1ScreenLockSettingsModel::refresh() {
  m_values = m_provider.preferences();
  if (m_values && m_readbackPending) {
    const auto &snapshot = m_client.snapshot();
    if (snapshot && snapshot->values.value(m_pendingKey) == m_pendingValue) {
      m_readbackPending = false;
      m_statusText = tr("Screen-lock preferences saved.");
    }
  } else if (!m_values && !m_writePending && !m_readbackPending) {
    m_statusText = tr("Waiting for confirmed Settings1 screen-lock preferences.");
  } else if (m_values && m_statusText.isEmpty()) {
    m_statusText = tr("Screen-lock preferences confirmed in Settings1.");
  }
  Q_EMIT changed();
}
bool Settings1ScreenLockSettingsModel::automaticLock() const noexcept {
  return m_values ? m_values->automaticLock : true;
}
int Settings1ScreenLockSettingsModel::timeoutMinutes() const noexcept {
  if (!m_values) return 5;
  return std::clamp(static_cast<int>((m_values->idleTimeoutSeconds + 30) / 60),
                    MinimumTimeoutMinutes, MaximumTimeoutMinutes);
}
bool Settings1ScreenLockSettingsModel::lockOnResume() const noexcept {
  return m_values ? m_values->lockOnResume : true;
}
int Settings1ScreenLockSettingsModel::lockGraceSeconds() const noexcept {
  return m_values ? static_cast<int>(m_values->graceSeconds) : 5;
}
bool Settings1ScreenLockSettingsModel::busy() const noexcept {
  return m_writePending || !m_values;
}
QString Settings1ScreenLockSettingsModel::statusText() const { return m_statusText; }
QString Settings1ScreenLockSettingsModel::errorText() const { return m_errorText; }
bool Settings1ScreenLockSettingsModel::write(const QString &key, const QVariant &value) {
  if (busy()) return false;
  QString error;
  m_writePending = true;
  m_pendingKey = key;
  m_pendingValue = value;
  m_statusText = tr("Saving screen-lock preferences…");
  m_errorText.clear();
  if (!m_client.setUserValue(key, value, &error)) {
    m_writePending = false;
    m_pendingKey.clear();
    m_pendingValue.clear();
    m_statusText = tr("Screen-lock preferences were not saved.");
    m_errorText = error.isEmpty()
        ? tr("Settings1 is not ready to save this preference.") : error;
    Q_EMIT changed();
    return false;
  }
  Q_EMIT changed();
  return true;
}
bool Settings1ScreenLockSettingsModel::setAutomaticLock(bool enabled) {
  return write(QStringLiteral("lock.automaticEnabled"), enabled);
}
bool Settings1ScreenLockSettingsModel::setTimeoutMinutes(int minutes) {
  if (minutes < MinimumTimeoutMinutes || minutes > MaximumTimeoutMinutes) {
    m_errorText = tr("Choose a timeout between %1 and %2 minutes.")
                      .arg(MinimumTimeoutMinutes).arg(MaximumTimeoutMinutes);
    Q_EMIT changed();
    return false;
  }
  return write(QStringLiteral("lock.idleTimeoutSeconds"), minutes * 60);
}
bool Settings1ScreenLockSettingsModel::setLockOnResume(bool enabled) {
  return write(QStringLiteral("lock.onResume"), enabled);
}
bool Settings1ScreenLockSettingsModel::setLockGraceSeconds(int seconds) {
  if (seconds != 0 && seconds != 5 && seconds != 30 && seconds != 60 &&
      seconds != 300) {
    m_errorText = tr("Choose a supported screen-lock grace period.");
    Q_EMIT changed();
    return false;
  }
  return write(QStringLiteral("lock.graceSeconds"), seconds);
}
bool Settings1ScreenLockSettingsModel::retryLiveApply() {
  m_errorText.clear();
  m_statusText = tr("Refreshing confirmed Settings1 screen-lock preferences.");
  m_client.refresh();
  Q_EMIT changed();
  return true;
}
}
