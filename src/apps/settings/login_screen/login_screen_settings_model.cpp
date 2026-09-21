// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/apps/settings_login_screen/login_screen_settings_model.h>

#include <QtCore/QVariantMap>

namespace QindaQt::Apps::SettingsLoginScreen {

LoginScreenSettingsModel::LoginScreenSettingsModel(
    LoginScreenPaths paths, std::unique_ptr<SddmConfigWriteClient> writer,
    std::unique_ptr<LoginScreenAuthorityProbe> probe, QString currentUserName,
    QObject *parent)
    : QObject(parent),
      m_paths(std::move(paths)),
      m_writer(std::move(writer)),
      m_probe(std::move(probe)),
      m_currentUserName(std::move(currentUserName)) {
  connect(m_writer.get(), &SddmConfigWriteClient::writeFinished, this,
          &LoginScreenSettingsModel::onWriteFinished);
  connect(m_probe.get(), &LoginScreenAuthorityProbe::probeFinished, this,
          &LoginScreenSettingsModel::onProbeFinished);
  // The probe starts pessimistic (read-only) and only opens the controls
  // when the authority answers; a page that assumed writable would let a
  // user edit values it then cannot save.
  m_readOnlyReason = QStringLiteral("Checking whether you may configure the "
                                    "login screen…");
  refresh();
  m_probe->probe();
}

LoginScreenSettingsModel::~LoginScreenSettingsModel() = default;

QStringList LoginScreenSettingsModel::themeIds() const {
  QStringList ids;
  ids.reserve(m_themes.size());
  for (const SddmThemeEntry &entry : m_themes) {
    ids.append(entry.id);
  }
  return ids;
}

QStringList LoginScreenSettingsModel::sessionIds() const {
  QStringList ids;
  ids.reserve(m_sessions.size());
  for (const SddmSessionEntry &entry : m_sessions) {
    ids.append(entry.id);
  }
  return ids;
}

QVariantList LoginScreenSettingsModel::themes() const {
  QVariantList rows;
  rows.reserve(m_themes.size());
  for (const SddmThemeEntry &entry : m_themes) {
    rows.append(QVariantMap{
        {QStringLiteral("id"), entry.id},
        {QStringLiteral("name"), entry.name},
        {QStringLiteral("previewPath"), entry.previewPath},
        {QStringLiteral("qindaqt"), entry.qindaqt},
        {QStringLiteral("current"), entry.id == m_values.theme},
    });
  }
  return rows;
}

QVariantList LoginScreenSettingsModel::sessions() const {
  QVariantList rows;
  rows.reserve(m_sessions.size());
  for (const SddmSessionEntry &entry : m_sessions) {
    rows.append(QVariantMap{
        {QStringLiteral("id"), entry.id},
        {QStringLiteral("name"), entry.name},
        {QStringLiteral("comment"), entry.comment},
        {QStringLiteral("wayland"), entry.wayland},
        {QStringLiteral("current"), entry.id == m_values.autologinSession},
    });
  }
  return rows;
}

QString LoginScreenSettingsModel::autologinUser() const {
  // With no stored user the picker still needs a truthful default: the
  // person sitting at the machine, who is who autologin would name in
  // practice.
  return m_values.autologinUser.isEmpty() ? m_currentUserName
                                          : m_values.autologinUser;
}

QVariantList LoginScreenSettingsModel::users() const {
  QVariantList rows;
  rows.reserve(m_users.size() + 1);
  for (const QString &user : m_users) {
    rows.append(user);
  }
  // A stored user the passwd scan no longer offers (removed account, uid
  // below the listing floor) is still shown -- hiding it would lie about
  // what the configuration says.
  if (!m_values.autologinUser.isEmpty() &&
      !m_users.contains(m_values.autologinUser)) {
    rows.append(m_values.autologinUser);
  }
  return rows;
}

QString LoginScreenSettingsModel::numlockMode() const {
  // SDDM's documented default is "none" (leave the lock as the firmware
  // set it); an unset key and an explicit "none" are the same truth.
  return m_values.numlock.isEmpty() ? QStringLiteral("none")
                                    : m_values.numlock;
}

bool LoginScreenSettingsModel::busy() const {
  return m_writer->writeInFlight() || !m_writeQueue.isEmpty();
}

QString LoginScreenSettingsModel::statusText() const {
  if (busy()) {
    return QStringLiteral("Saving the login screen configuration — "
                          "authenticate if the system asks.");
  }
  if (!m_unreadableFiles.isEmpty()) {
    return QStringLiteral("Some configuration files could not be read: %1")
        .arg(m_unreadableFiles.join(QStringLiteral(", ")));
  }
  return QString();
}

void LoginScreenSettingsModel::refresh() {
  m_themes = listSddmThemes(m_paths.themesDirectory);
  m_sessions = listSddmSessions(m_paths.waylandSessionDirectories,
                                m_paths.xSessionDirectories);
  m_users = listSddmLoginUsers(m_paths.passwdFile);
  const SddmConfigReadResult read =
      readSddmConfig(m_paths.sddmConfigScanDirectories,
                     m_paths.sddmLegacyMainFile, m_paths.ownedConfigFile);
  m_values = read.values;
  m_winnerFileByKey = read.winnerFileByKey;
  m_unreadableFiles = read.unreadableFiles;
  if (!read.shadowedKeys.isEmpty()) {
    m_overrideText =
        QStringLiteral("A later configuration file (%1) overrides %2 — "
                       "choices made here will not take effect until that "
                       "file is changed.")
            .arg(read.shadowingFiles.join(QStringLiteral(", ")),
                 read.shadowedKeys.join(QStringLiteral(", ")));
  } else {
    m_overrideText.clear();
  }
  Q_EMIT viewChanged();
}

bool LoginScreenSettingsModel::guardWritable() {
  if (m_writable) {
    return true;
  }
  m_errorText = m_readOnlyReason.isEmpty()
      ? QStringLiteral("You may view these settings but not change them.")
      : m_readOnlyReason;
  Q_EMIT viewChanged();
  return false;
}

void LoginScreenSettingsModel::enqueue(const SddmOwnedChangeSet &changes) {
  m_writeQueue.append(changes);
  flushQueue();
  Q_EMIT viewChanged();
}

void LoginScreenSettingsModel::flushQueue() {
  if (m_writer->writeInFlight() || m_writeQueue.isEmpty()) {
    return;
  }
  m_writer->write(m_writeQueue.takeFirst());
}

void LoginScreenSettingsModel::onWriteFinished(const bool ok,
                                               const QString &errorText) {
  if (!ok) {
    // A refused or failed write abandons everything queued behind it: the
    // user must see the refusal, not a cascade of stale intents.
    m_writeQueue.clear();
    m_errorText = errorText.isEmpty()
        ? QStringLiteral("The change was not saved.")
        : errorText;
    // Truth is re-read after a failure too: a control must not stay moved
    // for a write that never happened.
    refresh();
    return;
  }
  if (!m_writeQueue.isEmpty()) {
    flushQueue();
    Q_EMIT viewChanged();
    return;
  }
  // A successful write is only real once the file says it.
  refresh();
}

void LoginScreenSettingsModel::onProbeFinished(const bool writable,
                                               const QString &readOnlyReason) {
  m_writable = writable;
  m_readOnlyReason = readOnlyReason;
  Q_EMIT viewChanged();
}

bool LoginScreenSettingsModel::selectTheme(const QString &themeId) {
  if (!guardWritable()) {
    return false;
  }
  SddmOwnedChangeSet changes;
  changes.theme = themeId;
  const QString error =
      validateOwnedChangeSet(changes, themeIds(), sessionIds(), m_users);
  if (!error.isEmpty()) {
    m_errorText = error;
    Q_EMIT viewChanged();
    return false;
  }
  enqueue(changes);
  return true;
}

bool LoginScreenSettingsModel::selectDefaultSession(const QString &sessionId) {
  if (!guardWritable()) {
    return false;
  }
  SddmOwnedChangeSet changes;
  changes.autologinSession = sessionId;
  const QString error =
      validateOwnedChangeSet(changes, themeIds(), sessionIds(), m_users);
  if (!error.isEmpty()) {
    m_errorText = error;
    Q_EMIT viewChanged();
    return false;
  }
  enqueue(changes);
  return true;
}

bool LoginScreenSettingsModel::setNumlockMode(const QString &mode) {
  if (!guardWritable()) {
    return false;
  }
  SddmOwnedChangeSet changes;
  changes.numlock = mode;
  const QString error =
      validateOwnedChangeSet(changes, themeIds(), sessionIds(), m_users);
  if (!error.isEmpty()) {
    m_errorText = error;
    Q_EMIT viewChanged();
    return false;
  }
  enqueue(changes);
  return true;
}

bool LoginScreenSettingsModel::applyCursorTheme(const QString &theme) {
  if (!guardWritable()) {
    return false;
  }
  SddmOwnedChangeSet changes;
  changes.cursorTheme = theme;
  const QString error =
      validateOwnedChangeSet(changes, themeIds(), sessionIds(), m_users);
  if (!error.isEmpty()) {
    m_errorText = error;
    Q_EMIT viewChanged();
    return false;
  }
  enqueue(changes);
  return true;
}

bool LoginScreenSettingsModel::setAutologinEnabled(const bool enabled) {
  if (!guardWritable()) {
    return false;
  }
  SddmOwnedChangeSet changes;
  if (!enabled) {
    // Autologin off is User= empty. The pinned session is left in place;
    // it is inert without a user and remembered for next time.
    changes.autologinUser = QString();
  } else {
    const QString session = m_values.autologinSession;
    if (session.isEmpty()) {
      m_errorText = QStringLiteral(
          "Choose a default session before turning on automatic login.");
      Q_EMIT viewChanged();
      return false;
    }
    changes.autologinUser = autologinUser();
    changes.autologinSession = session;
  }
  const QString error =
      validateOwnedChangeSet(changes, themeIds(), sessionIds(), m_users);
  if (!error.isEmpty()) {
    m_errorText = error;
    Q_EMIT viewChanged();
    return false;
  }
  enqueue(changes);
  return true;
}

bool LoginScreenSettingsModel::selectAutologinUser(const QString &userName) {
  if (!guardWritable()) {
    return false;
  }
  if (!autologinEnabled()) {
    return false;
  }
  SddmOwnedChangeSet changes;
  changes.autologinUser = userName;
  const QString error =
      validateOwnedChangeSet(changes, themeIds(), sessionIds(), m_users);
  if (!error.isEmpty()) {
    m_errorText = error;
    Q_EMIT viewChanged();
    return false;
  }
  enqueue(changes);
  return true;
}

void LoginScreenSettingsModel::clearError() {
  m_errorText.clear();
  Q_EMIT viewChanged();
}

} // namespace QindaQt::Apps::SettingsLoginScreen
