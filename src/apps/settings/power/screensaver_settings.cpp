// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_power/screensaver_settings.h>

#include <QVariant>

namespace QindaQt::Apps::SettingsPower {

using Session::DesktopControls::ScreensaverPreferences;
using Session::DesktopControls::Settings1ScreensaverPreferences;

namespace {

// Presentation names for the saver tokens Settings offers.
QString displayName(const QString &saver) {
  if (saver == QLatin1String("qinda-patrol"))
    return ScreensaverSettingsModel::tr("Qinda Patrol");
  if (saver == QLatin1String("circuit-reef"))
    return ScreensaverSettingsModel::tr("Circuit Reef");
  return ScreensaverSettingsModel::tr("None");
}

} // namespace

ScreensaverSettingsModel::ScreensaverSettingsModel(
    Settings1ScreensaverPreferences &preferences,
    Services::SettingsClient::SettingsClient &client, QObject *parent)
    : QObject(parent), m_preferences(preferences), m_client(client) {
  connect(&m_preferences,
          &Settings1ScreensaverPreferences::preferencesChanged, this,
          [this](ScreensaverPreferences) {
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
                  tr("The screensaver preference could not be applied: %1")
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
                tr("The screensaver preference may not have been applied: %1")
                    .arg(message.isEmpty() ? tr("unknown reason") : message);
            publishStatus();
          });
  publishStatus();
}

ScreensaverSettingsModel::~ScreensaverSettingsModel() = default;

QString ScreensaverSettingsModel::saver() const {
  return m_preferences.currentPreferences().saver;
}

bool ScreensaverSettingsModel::enabled() const {
  return m_preferences.currentPreferences().enabled();
}

int ScreensaverSettingsModel::minutes() const {
  return m_preferences.currentPreferences().minutes;
}

bool ScreensaverSettingsModel::busy() const noexcept { return m_busy; }

const QString &ScreensaverSettingsModel::statusText() const noexcept {
  return m_statusText;
}

const QString &ScreensaverSettingsModel::errorText() const noexcept {
  return m_errorText;
}

bool ScreensaverSettingsModel::setSaver(const QString &saver) {
  if (m_busy) return false;
  if (!ScreensaverPreferences::knownSavers().contains(saver)) {
    m_errorText = tr("That screensaver is not available.");
    Q_EMIT changed();
    return false;
  }
  return submit(QStringLiteral("power.screensaver"), saver);
}

bool ScreensaverSettingsModel::setMinutes(int minutes) {
  if (m_busy) return false;
  if (minutes < 1 || minutes > ScreensaverPreferences::maximumTimeoutMinutes()) {
    m_errorText = tr("Choose a delay between 1 and %1 minutes.")
                      .arg(ScreensaverPreferences::maximumTimeoutMinutes());
    Q_EMIT changed();
    return false;
  }
  return submit(QStringLiteral("power.screensaverMinutes"),
                QVariant::fromValue(static_cast<qint64>(minutes)));
}

bool ScreensaverSettingsModel::retry() {
  if (m_busy) return false;
  if (m_errorText.isEmpty()) return true;
  m_errorText.clear();
  publishStatus();
  // The last accepted preference is already persisted truth; retry only
  // refreshes the snapshot so a lost owner reconciles the route.
  m_preferences.refresh();
  return true;
}

bool ScreensaverSettingsModel::submit(const QString &key, const QVariant &value) {
  QString error;
  if (!m_client.setUserValue(key, value, &error)) {
    m_errorText = error.isEmpty() ? tr("Could not save the screensaver preference.")
                                  : error;
    Q_EMIT changed();
    return false;
  }
  m_busy = true;
  m_errorText.clear();
  publishStatus();
  return true;
}

void ScreensaverSettingsModel::publishStatus() {
  const auto preferences = m_preferences.currentPreferences();
  m_statusText = preferences.enabled()
      ? tr("%1 starts after %n minute(s) of inactivity.", nullptr,
           preferences.minutes)
            .arg(displayName(preferences.saver))
      : tr("No screensaver starts when the session is idle.");
  Q_EMIT changed();
}

} // namespace QindaQt::Apps::SettingsPower
