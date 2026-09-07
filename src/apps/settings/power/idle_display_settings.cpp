// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_power/idle_display_settings.h>

#include <QVariant>

namespace QindaQt::Apps::SettingsPower {

using Session::DesktopControls::IdleDisplayPreferences;
using Session::DesktopControls::Settings1IdlePreferences;

IdleDisplaySettingsModel::IdleDisplaySettingsModel(
    Settings1IdlePreferences &preferences,
    Services::SettingsClient::SettingsClient &client, QObject *parent)
    : QObject(parent), m_preferences(preferences), m_client(client) {
  connect(&m_preferences, &Settings1IdlePreferences::preferencesChanged, this,
          [this](IdleDisplayPreferences) {
            m_busy = false;
            m_errorText.clear();
            publishStatus();
          });
  connect(&m_client, &Services::SettingsClient::SettingsClient::commitFinished,
          this, [this](const Services::SettingsClient::CommitOutcome &outcome) {
            if (!m_busy) return;
            m_busy = false;
            if (outcome.status
                == QindaQt::Services::SettingsProtocol::SettingsWireStatus::Applied) {
              m_errorText.clear();
            } else {
              m_errorText =
                  tr("The display-off preference could not be applied: %1")
                      .arg(outcome.message.isEmpty() ? tr("unknown reason")
                                                     : outcome.message);
            }
            publishStatus();
          });
  connect(&m_client, &Services::SettingsClient::SettingsClient::commitUncertain,
          this, [this](const QString &message) {
            if (!m_busy) return;
            m_busy = false;
            m_errorText =
                tr("The display-off preference may not have been applied: %1")
                    .arg(message.isEmpty() ? tr("unknown reason") : message);
            publishStatus();
          });
  publishStatus();
}

IdleDisplaySettingsModel::~IdleDisplaySettingsModel() = default;

bool IdleDisplaySettingsModel::enabled() const noexcept {
  return m_preferences.currentPreferences().enabled;
}

int IdleDisplaySettingsModel::minutes() const noexcept {
  return m_preferences.currentPreferences().minutes;
}

bool IdleDisplaySettingsModel::busy() const noexcept { return m_busy; }

const QString &IdleDisplaySettingsModel::statusText() const noexcept {
  return m_statusText;
}

const QString &IdleDisplaySettingsModel::errorText() const noexcept {
  return m_errorText;
}

bool IdleDisplaySettingsModel::setEnabled(bool enabled) {
  if (m_busy) return false;
  const auto current = m_preferences.currentPreferences();
  const int minutes =
      current.minutes >= 1 ? current.minutes
                           : IdleDisplayPreferences::defaultTimeoutMinutes();
  return submit(enabled ? minutes : -1);
}

bool IdleDisplaySettingsModel::setMinutes(int minutes) {
  if (m_busy) return false;
  if (minutes < 1 || minutes > IdleDisplayPreferences::maximumTimeoutMinutes()) {
    m_errorText = tr("Choose a timeout between 1 and %1 minutes.")
                      .arg(IdleDisplayPreferences::maximumTimeoutMinutes());
    Q_EMIT changed();
    return false;
  }
  return submit(minutes);
}

bool IdleDisplaySettingsModel::retry() {
  if (m_busy) return false;
  if (m_errorText.isEmpty()) return true;
  m_errorText.clear();
  publishStatus();
  // The last accepted preference is already persisted truth; retry only
  // refreshes the snapshot so a lost owner reconciles the route.
  m_preferences.refresh();
  return true;
}

bool IdleDisplaySettingsModel::submit(qint64 persistedMinutes) {
  QString error;
  if (!m_client.setUserValue(
          QStringLiteral("power.idleDisplayOffMinutes"),
          QVariant::fromValue(persistedMinutes), &error)) {
    m_errorText = error.isEmpty() ? tr("Could not save the display-off preference.")
                                  : error;
    Q_EMIT changed();
    return false;
  }
  m_busy = true;
  m_errorText.clear();
  publishStatus();
  return true;
}

void IdleDisplaySettingsModel::publishStatus() {
  const auto preferences = m_preferences.currentPreferences();
  m_statusText = preferences.enabled
      ? tr("%n minute(s) of inactivity turns the display off.", nullptr,
           preferences.minutes)
      : tr("The display stays on.");
  Q_EMIT changed();
}

} // namespace QindaQt::Apps::SettingsPower
