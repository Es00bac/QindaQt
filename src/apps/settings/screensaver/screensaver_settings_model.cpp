// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_screensaver/screensaver_settings_model.h>

#include <qindaqt/session/desktop_controls/screensaver_catalog.h>

#include <QVariant>

#include <algorithm>

namespace QindaQt::Apps::SettingsScreensaver {

using Session::DesktopControls::ScreensaverCatalogEntry;
using Session::DesktopControls::ScreensaverPreferences;
using Session::DesktopControls::Settings1ScreensaverPreferences;

ScreensaverSettingsModel::ScreensaverSettingsModel(
    Settings1ScreensaverPreferences &preferences,
    Services::SettingsClient::SettingsClient &client,
    const Session::DesktopControls::ScreensaverCatalog &catalog,
    LockScreenSaverStore &lockScreenSaver, ScreensaverPreview &preview,
    QObject *parent)
    : QObject(parent), m_preferences(preferences), m_client(client),
      m_catalog(catalog), m_lockScreenSaver(lockScreenSaver),
      m_preview(preview) {
  connect(&m_preferences,
          &Settings1ScreensaverPreferences::preferencesChanged, this,
          [this](ScreensaverPreferences next) {
            m_busy = false;
            m_errorText.clear();
            // Only a confirmed snapshot reaches the greeter, so a refused or
            // uncertain commit never changes what a locked session shows.
            mirrorToLockScreen(next.saver);
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
  connect(&m_preview, &ScreensaverPreview::finished, this, [this] {
            publishStatus();
          });
  publishStatus();
}

ScreensaverSettingsModel::~ScreensaverSettingsModel() = default;

QVariantList ScreensaverSettingsModel::saverOptions() const {
  QVariantList options;
  options.append(QVariantMap{
      {QStringLiteral("token"), ScreensaverPreferences::noneToken()},
      {QStringLiteral("name"), tr("None")},
      {QStringLiteral("comment"),
       tr("Nothing runs when the session is idle.")},
      {QStringLiteral("iconName"), QStringLiteral("dialog-cancel")},
      {QStringLiteral("showsOnLockScreen"), false},
  });
  options.append(QVariantMap{
      {QStringLiteral("token"), ScreensaverPreferences::blankToken()},
      {QStringLiteral("name"), tr("Blank screen")},
      {QStringLiteral("comment"),
       tr("Nothing runs while the session is unlocked; the lock screen shows "
          "a plain dark screen.")},
      {QStringLiteral("iconName"), QStringLiteral("view-hidden")},
      {QStringLiteral("showsOnLockScreen"), true},
  });
  QList<ScreensaverCatalogEntry> discovered = m_catalog.entries();
  std::sort(discovered.begin(), discovered.end(),
            [](const ScreensaverCatalogEntry &left,
               const ScreensaverCatalogEntry &right) {
              return QString::compare(left.name, right.name,
                                      Qt::CaseInsensitive) < 0;
            });
  for (const ScreensaverCatalogEntry &entry : discovered) {
    options.append(QVariantMap{
        {QStringLiteral("token"), entry.token},
        {QStringLiteral("name"), entry.name},
        {QStringLiteral("comment"), entry.comment},
        {QStringLiteral("iconName"), entry.iconName},
        {QStringLiteral("showsOnLockScreen"), entry.showsOnLockScreen},
    });
  }
  return options;
}

QString ScreensaverSettingsModel::saver() const {
  return m_preferences.currentPreferences().saver;
}

bool ScreensaverSettingsModel::delayEnabled() const {
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

bool ScreensaverSettingsModel::previewAvailable() const {
  return m_preview.kindFor(saver()) != ScreensaverPreview::Kind::Unavailable;
}

bool ScreensaverSettingsModel::previewRunning() const {
  return m_preview.running();
}

const QString &ScreensaverSettingsModel::previewSummary() const noexcept {
  return m_previewSummary;
}

QString ScreensaverSettingsModel::displayName(const QString &saver) const {
  if (saver == ScreensaverPreferences::blankToken()) {
    return tr("Blank screen");
  }
  const std::optional<ScreensaverCatalogEntry> entry = m_catalog.entry(saver);
  if (entry.has_value() && !entry->name.isEmpty()) {
    return entry->name;
  }
  return saver;
}

bool ScreensaverSettingsModel::setSaver(const QString &saver) {
  if (m_busy) return false;
  // Only the built-ins and a discovered saver may be written; anything else
  // is refused here, before any commit, and can never reach persistence.
  if (saver != ScreensaverPreferences::noneToken()
      && saver != ScreensaverPreferences::blankToken()
      && !m_catalog.entry(saver).has_value()) {
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

bool ScreensaverSettingsModel::preview() {
  if (m_busy) return false;
  QString error;
  // The preview always shows persisted truth, which the mirror has already
  // handed to the greeter; a write in flight disables the button instead.
  if (!m_preview.start(saver(), &error)) {
    m_errorText = error.isEmpty() ? tr("The preview could not be started.")
                                  : error;
    Q_EMIT changed();
    return false;
  }
  publishStatus();
  return true;
}

void ScreensaverSettingsModel::refreshSavers() {
  // entries() scans; nothing to do beyond republishing the list.
  Q_EMIT saversChanged();
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

void ScreensaverSettingsModel::mirrorToLockScreen(const QString &saver) {
  // AGENT-GUARD: only a saver the greeter can actually draw may take the lock
  // wallpaper over (ADR-0216). Pointing the greeter at a saver with no QML
  // module would replace the user's own lock wallpaper with a blank ground,
  // so those choices release it instead. The reserved "blank" token is the
  // one deliberate exception: the plugin's painted ground IS the blank screen
  // the user asked for (ADR-0226).
  QString mirrored = ScreensaverPreferences::noneToken();
  if (saver == ScreensaverPreferences::blankToken()) {
    mirrored = ScreensaverPreferences::blankToken();
  } else {
    const std::optional<ScreensaverCatalogEntry> entry = m_catalog.entry(saver);
    if (entry.has_value() && entry->showsOnLockScreen) {
      mirrored = saver;
    }
  }
  if (m_lockScreenSaver.currentSaver() == mirrored) return;
  QString error;
  if (!m_lockScreenSaver.save(mirrored, &error)) {
    m_errorText = tr("The screensaver was saved, but the lock screen could not "
                     "be told about it: %1")
                      .arg(error.isEmpty() ? tr("unknown reason") : error);
  }
}

void ScreensaverSettingsModel::publishStatus() {
  const auto preferences = m_preferences.currentPreferences();
  if (preferences.saver == ScreensaverPreferences::blankToken()) {
    m_statusText = tr("Nothing runs while the session is unlocked. The lock "
                      "screen shows a plain dark screen.");
  } else if (!preferences.enabled()) {
    m_statusText = tr("No screensaver starts when the session is idle.");
  } else {
    const std::optional<ScreensaverCatalogEntry> entry =
        m_catalog.entry(preferences.saver);
    if (entry.has_value() && entry->showsOnLockScreen) {
      m_statusText = tr("%1 starts after %n minute(s) of inactivity, and keeps "
                        "showing while the screen is locked.",
                        nullptr, preferences.minutes)
                         .arg(displayName(preferences.saver));
    } else {
      // Honest about the split: this saver has no scene the locker can draw,
      // so a locked screen keeps whatever wallpaper it already had.
      m_statusText = tr("%1 starts after %n minute(s) of inactivity. The lock "
                        "screen keeps its own wallpaper.",
                        nullptr, preferences.minutes)
                         .arg(displayName(preferences.saver));
    }
  }
  publishPreviewSummary();
  Q_EMIT changed();
}

void ScreensaverSettingsModel::publishPreviewSummary() {
  switch (m_preview.kindFor(saver())) {
  case ScreensaverPreview::Kind::TestingGreeter:
    if (saver() == ScreensaverPreferences::blankToken()) {
      m_previewSummary =
          tr("Opens the lock screen in its testing mode, showing the plain "
             "dark screen a locked session would show. The session is never "
             "locked.");
    } else {
      m_previewSummary =
          tr("Opens the lock screen in its testing mode, drawing %1 the way a "
             "locked session would. The session is never locked.")
              .arg(displayName(saver()));
    }
    break;
  case ScreensaverPreview::Kind::SaverProgram:
    m_previewSummary =
        tr("Runs %1 itself, the way it appears while the session is unlocked "
           "but idle. The greeter cannot draw this one, so a locked screen "
           "keeps its own wallpaper. Any input dismisses the preview.")
            .arg(displayName(saver()));
    break;
  case ScreensaverPreview::Kind::Unavailable:
    m_previewSummary = QString();
    break;
  }
}

} // namespace QindaQt::Apps::SettingsScreensaver
