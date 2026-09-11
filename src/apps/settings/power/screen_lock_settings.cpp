// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_power/screen_lock_settings.h>

#include <QtCore/QSettings>
#include <QtCore/QVariant>

#include <cmath>
#include <utility>

#include <algorithm>

namespace QindaQt::Apps::SettingsPower {
namespace {
constexpr auto DaemonGroup = "Daemon";
constexpr auto AutolockKey = "Autolock";
constexpr auto TimeoutKey = "Timeout";
constexpr auto LockOnResumeKey = "LockOnResume";
constexpr auto LockGraceKey = "LockGrace";

// Defaults mirror upstream kscreenlocker v6.6.6
// settings/kscreenlockersettings.kcfg so an absent key is treated as the
// locker's own default instead of an invented value.
constexpr bool DefaultAutolock = true;
constexpr double DefaultTimeoutMinutes = 5.0;
constexpr bool DefaultLockOnResume = true;
constexpr int DefaultLockGraceSeconds = 5;

int boundedTimeout(const double minutes) {
  if (!std::isfinite(minutes)) return ScreenLockSettingsModel::MinimumTimeoutMinutes;
  // Clamp in double before qRound: a huge finite INI value would otherwise
  // overflow the int conversion.
  const double clamped = qBound(
      static_cast<double>(ScreenLockSettingsModel::MinimumTimeoutMinutes), minutes,
      static_cast<double>(ScreenLockSettingsModel::MaximumTimeoutMinutes));
  return qRound(clamped);
}

QString storageError(const QSettings &settings) {
  return QStringLiteral("could not save screen-lock settings in %1")
      .arg(settings.fileName());
}
} // namespace

bool ScreenLockSettingsModel::isValidGraceSeconds(const int seconds) {
  return std::find(GraceChoices.begin(), GraceChoices.end(), seconds) !=
         GraceChoices.end();
}

IniScreenLockPreferencesStore::IniScreenLockPreferencesStore(QString filePath)
    : m_filePath(std::move(filePath)) {}

bool IniScreenLockPreferencesStore::load(ScreenLockPreferences *preferences,
                                         QString *error) {
  if (preferences == nullptr || m_filePath.trimmed().isEmpty()) {
    if (error) *error = QStringLiteral("screen-lock settings location is unavailable");
    return false;
  }
  QSettings settings(m_filePath, QSettings::IniFormat);
  settings.beginGroup(QString::fromLatin1(DaemonGroup));
  const bool automatic =
      settings.value(QString::fromLatin1(AutolockKey), DefaultAutolock).toBool();
  bool timeoutOk = false;
  const double timeout = settings.value(QString::fromLatin1(TimeoutKey), DefaultTimeoutMinutes)
                             .toDouble(&timeoutOk);
  const bool lockOnResume =
      settings.value(QString::fromLatin1(LockOnResumeKey), DefaultLockOnResume).toBool();
  bool graceOk = false;
  const int lockGrace = settings.value(QString::fromLatin1(LockGraceKey), DefaultLockGraceSeconds)
                            .toInt(&graceOk);
  settings.endGroup();
  if (settings.status() != QSettings::NoError) {
    if (error) *error = storageError(settings);
    return false;
  }
  preferences->automaticLock = automatic;
  preferences->timeoutMinutes = boundedTimeout(timeoutOk ? timeout : DefaultTimeoutMinutes);
  preferences->lockOnResume = lockOnResume;
  // An out-of-set stored grace (an upstream KCM custom value) is kept as-is so
  // an unrelated-key save never invents a value the locker did not have; only
  // the model setters validate against the offered choice set.
  preferences->lockGraceSeconds = graceOk ? lockGrace : DefaultLockGraceSeconds;
  return true;
}

bool IniScreenLockPreferencesStore::save(const ScreenLockPreferences &preferences,
                                         QString *error) {
  QSettings settings(m_filePath, QSettings::IniFormat);
  settings.beginGroup(QString::fromLatin1(DaemonGroup));
  // AGENT-GUARD: write only keys whose requested value differs from the file,
  // comparing with the same defaults load() uses. Unconditional writes would
  // clobber a concurrent external edit to a key this save did not intend to
  // change, and would rewrite - and so normalize - a pristine locker config.
  const bool storedAutolock =
      settings.value(QString::fromLatin1(AutolockKey), DefaultAutolock).toBool();
  if (storedAutolock != preferences.automaticLock) {
    settings.setValue(QString::fromLatin1(AutolockKey), preferences.automaticLock);
  }
  const double storedTimeout =
      settings.value(QString::fromLatin1(TimeoutKey), DefaultTimeoutMinutes).toDouble();
  const double requestedTimeout = boundedTimeout(preferences.timeoutMinutes);
  if (storedTimeout != requestedTimeout) {
    settings.setValue(QString::fromLatin1(TimeoutKey), requestedTimeout);
  }
  const bool storedLockOnResume =
      settings.value(QString::fromLatin1(LockOnResumeKey), DefaultLockOnResume).toBool();
  if (storedLockOnResume != preferences.lockOnResume) {
    settings.setValue(QString::fromLatin1(LockOnResumeKey), preferences.lockOnResume);
  }
  const int storedLockGrace =
      settings.value(QString::fromLatin1(LockGraceKey), DefaultLockGraceSeconds).toInt();
  if (storedLockGrace != preferences.lockGraceSeconds) {
    settings.setValue(QString::fromLatin1(LockGraceKey), preferences.lockGraceSeconds);
  }
  settings.endGroup();
  settings.sync();
  if (settings.status() != QSettings::NoError) {
    if (error) *error = storageError(settings);
    return false;
  }
  return true;
}

ScreenLockSettingsModel::ScreenLockSettingsModel(
    std::unique_ptr<ScreenLockPreferencesStore> store,
    ScreenLockConfigureClient &configureClient, QObject *parent)
    : QObject(parent), m_store(std::move(store)), m_configureClient(configureClient) {
  QString error;
  if (!m_store || !m_store->load(&m_preferences, &error)) {
    m_loadFailed = true;
    m_errorText = error.isEmpty() ? tr("Screen-lock settings are unavailable.") : error;
  } else {
    m_statusText = m_preferences.automaticLock
        ? tr("The screen locks after %1 minutes of inactivity.")
              .arg(timeoutMinutes())
        : tr("Automatic screen locking is off.");
  }
  connect(&m_configureClient, &ScreenLockConfigureClient::configured, this,
          [this](const bool accepted, const QString &errorText) {
    if (!m_busy) return;
    m_busy = false;
    if (accepted) {
      m_errorText.clear();
      m_statusText = m_preferences.automaticLock
          ? tr("Screen-lock settings applied.")
          : tr("Automatic screen locking is off.");
    } else {
      m_statusText = tr("Screen-lock settings were saved.");
      m_errorText = tr("The preference was saved, but the running screen locker could not refresh: %1")
                        .arg(errorText.isEmpty() ? tr("unknown error") : errorText);
    }
    Q_EMIT changed();
  });
}

bool ScreenLockSettingsModel::automaticLock() const noexcept { return m_preferences.automaticLock; }
int ScreenLockSettingsModel::timeoutMinutes() const noexcept { return boundedTimeout(m_preferences.timeoutMinutes); }
bool ScreenLockSettingsModel::lockOnResume() const noexcept { return m_preferences.lockOnResume; }
int ScreenLockSettingsModel::lockGraceSeconds() const noexcept { return m_preferences.lockGraceSeconds; }
bool ScreenLockSettingsModel::busy() const noexcept { return m_busy; }
const QString &ScreenLockSettingsModel::statusText() const noexcept { return m_statusText; }
const QString &ScreenLockSettingsModel::errorText() const noexcept { return m_errorText; }

bool ScreenLockSettingsModel::reloadLatest(ScreenLockPreferences *preferences,
                                           const bool marksLoadFailure) {
  QString error;
  if (!m_store || !m_store->load(preferences, &error)) {
    if (marksLoadFailure) m_loadFailed = true;
    m_errorText = error.isEmpty() ? tr("Screen-lock settings are unavailable.") : error;
    Q_EMIT changed();
    return false;
  }
  if (marksLoadFailure) m_loadFailed = false;
  return true;
}

bool ScreenLockSettingsModel::persist(const ScreenLockPreferences &preferences,
                                      const PendingSaveField pendingField) {
  ScreenLockPreferences bounded = preferences;
  bounded.timeoutMinutes = boundedTimeout(preferences.timeoutMinutes);
  QString error;
  if (!m_store || !m_store->save(bounded, &error)) {
    m_savePending = true;
    m_pendingSaveField = pendingField;
    m_pendingPreferences = bounded;
    m_errorText = error.isEmpty() ? tr("Could not save screen-lock settings.") : error;
    Q_EMIT changed();
    return false;
  }
  m_savePending = false;
  m_pendingSaveField = PendingSaveField::None;
  m_preferences = bounded;
  m_errorText.clear();
  return true;
}

void ScreenLockSettingsModel::beginLiveConfigure(const QString &pendingText) {
  m_busy = true;
  m_statusText = pendingText;
  Q_EMIT changed();
  m_configureClient.requestConfigure();
}

bool ScreenLockSettingsModel::setAutomaticLock(const bool enabled) {
  if (m_busy) return false;
  // Merge onto the latest stored pair so an external edit to the untouched
  // key survives this save; a failed reload rejects the mutation instead of
  // blindly overwriting a config we cannot read.
  ScreenLockPreferences next;
  if (!reloadLatest(&next)) return false;
  next.automaticLock = enabled;
  if (!persist(next, PendingSaveField::AutomaticLock)) return false;
  beginLiveConfigure(tr("Updating automatic screen locking…"));
  return true;
}

bool ScreenLockSettingsModel::setTimeoutMinutes(const int minutes) {
  if (m_busy) return false;
  if (minutes < MinimumTimeoutMinutes || minutes > MaximumTimeoutMinutes) {
    m_errorText = tr("Choose a timeout between %1 and %2 minutes.")
                      .arg(MinimumTimeoutMinutes).arg(MaximumTimeoutMinutes);
    Q_EMIT changed();
    return false;
  }
  ScreenLockPreferences next;
  if (!reloadLatest(&next)) return false;
  next.timeoutMinutes = minutes;
  if (!persist(next, PendingSaveField::Timeout)) return false;
  beginLiveConfigure(tr("Updating screen-lock timeout…"));
  return true;
}

bool ScreenLockSettingsModel::setLockOnResume(const bool enabled) {
  if (m_busy) return false;
  ScreenLockPreferences next;
  if (!reloadLatest(&next)) return false;
  next.lockOnResume = enabled;
  if (!persist(next, PendingSaveField::LockOnResume)) return false;
  beginLiveConfigure(tr("Updating resume locking…"));
  return true;
}

bool ScreenLockSettingsModel::setLockGraceSeconds(const int seconds) {
  if (m_busy) return false;
  if (!isValidGraceSeconds(seconds)) {
    m_errorText = tr("Choose one of the offered unlock delays.");
    Q_EMIT changed();
    return false;
  }
  ScreenLockPreferences next;
  if (!reloadLatest(&next)) return false;
  next.lockGraceSeconds = seconds;
  if (!persist(next, PendingSaveField::LockGrace)) return false;
  beginLiveConfigure(tr("Updating the unlock delay…"));
  return true;
}

bool ScreenLockSettingsModel::retryLiveApply() {
  if (m_busy) return false;
  // Retry targets the step that actually failed. A live configure request is
  // only meaningful for a persisted pair, so storage failures re-run storage
  // and keep their error visible until it succeeds.
  if (m_loadFailed) {
    ScreenLockPreferences loaded;
    if (!reloadLatest(&loaded)) return false;
    m_preferences = loaded;
    m_errorText.clear();
    m_statusText = m_preferences.automaticLock
        ? tr("The screen locks after %1 minutes of inactivity.")
              .arg(timeoutMinutes())
        : tr("Automatic screen locking is off.");
    Q_EMIT changed();
    return true;
  }
  if (m_savePending) {
    // A save can fail long enough for another writer to update the other
    // locker key. Merge again before retrying so the retry has the same
    // external-edit guarantee as the original user mutation.
    ScreenLockPreferences latest;
    if (!reloadLatest(&latest, false)) return false;
    switch (m_pendingSaveField) {
    case PendingSaveField::AutomaticLock:
      latest.automaticLock = m_pendingPreferences.automaticLock;
      break;
    case PendingSaveField::Timeout:
      latest.timeoutMinutes = m_pendingPreferences.timeoutMinutes;
      break;
    case PendingSaveField::LockOnResume:
      latest.lockOnResume = m_pendingPreferences.lockOnResume;
      break;
    case PendingSaveField::LockGrace:
      latest.lockGraceSeconds = m_pendingPreferences.lockGraceSeconds;
      break;
    case PendingSaveField::None:
      m_errorText = tr("Could not determine which screen-lock setting to save.");
      Q_EMIT changed();
      return false;
    }
    if (!persist(latest, m_pendingSaveField)) return false;
    beginLiveConfigure(tr("Updating the running screen locker…"));
    return true;
  }
  m_errorText.clear();
  beginLiveConfigure(tr("Updating the running screen locker…"));
  return true;
}

} // namespace QindaQt::Apps::SettingsPower
