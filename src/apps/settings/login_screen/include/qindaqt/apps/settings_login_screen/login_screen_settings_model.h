// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/apps/settings_login_screen/sddm_config_store.h>
#include <qindaqt/apps/settings_login_screen/sddm_discovery.h>
#include <qindaqt/apps/settings_login_screen/sddm_owned_config.h>

#include <QtCore/QObject>
#include <QtCore/QVariantList>

#include <memory>

namespace QindaQt::Apps::SettingsLoginScreen {

// Every filesystem location the route reads, in one struct so the
// production composition and the tests each inject their own. The model
// never resolves a path itself.
struct LoginScreenPaths final {
  QString themesDirectory;
  QStringList waylandSessionDirectories;
  QStringList xSessionDirectories;
  QString passwdFile;
  // Lowest to highest precedence (production: /usr/lib/sddm/sddm.conf.d,
  // /etc/sddm.conf.d); SDDM reads the legacy main file last.
  QStringList sddmConfigScanDirectories;
  QString sddmLegacyMainFile;
  // The drop-in this route owns; used only to explain where its writes
  // land and to detect a later file overriding them.
  QString ownedConfigFile;
};

// Client-side seam for the privileged write. One write is outstanding at a
// time; the model serializes intents through it. Implementations emit
// writeFinished exactly once per write() call.
class SddmConfigWriteClient : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  ~SddmConfigWriteClient() override = default;

  virtual void write(const SddmOwnedChangeSet &changes) = 0;
  [[nodiscard]] virtual bool writeInFlight() const = 0;

Q_SIGNALS:
  void writeFinished(bool ok, const QString &errorText);
};

// Asynchronous answer to "may this user configure the login screen at
// all?" The route renders read-only with the reason until the probe says
// otherwise -- a page that lets you edit and fails at save time is the
// failure mode this seam exists to prevent.
class LoginScreenAuthorityProbe : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  ~LoginScreenAuthorityProbe() override = default;

  virtual void probe() = 0;

Q_SIGNALS:
  void probeFinished(bool writable, const QString &readOnlyReason);
};

// AGENT-CONTRACT: QML-facing projection for the Login screen route. The
// published values are always what the merged SDDM configuration last said,
// never what the user just asked for -- a control that moved optimistically
// would claim a polkit-gated write succeeded when it had not. Every intent
// is validated before elevation, serialized (one outstanding write, FIFO
// for the rest), and re-read from disk after completion.
class LoginScreenSettingsModel final : public QObject {
  Q_OBJECT

  Q_PROPERTY(QVariantList themes READ themes NOTIFY viewChanged FINAL)
  Q_PROPERTY(QString currentTheme READ currentTheme NOTIFY viewChanged FINAL)
  Q_PROPERTY(QVariantList sessions READ sessions NOTIFY viewChanged FINAL)
  Q_PROPERTY(QString defaultSession READ defaultSession NOTIFY viewChanged FINAL)
  Q_PROPERTY(bool autologinEnabled READ autologinEnabled NOTIFY viewChanged FINAL)
  Q_PROPERTY(QString autologinUser READ autologinUser NOTIFY viewChanged FINAL)
  Q_PROPERTY(QVariantList users READ users NOTIFY viewChanged FINAL)
  Q_PROPERTY(QString numlockMode READ numlockMode NOTIFY viewChanged FINAL)
  Q_PROPERTY(QString cursorTheme READ cursorTheme NOTIFY viewChanged FINAL)
  Q_PROPERTY(bool writable READ writable NOTIFY viewChanged FINAL)
  Q_PROPERTY(QString readOnlyReason READ readOnlyReason NOTIFY viewChanged FINAL)
  Q_PROPERTY(bool busy READ busy NOTIFY viewChanged FINAL)
  Q_PROPERTY(QString statusText READ statusText NOTIFY viewChanged FINAL)
  Q_PROPERTY(QString errorText READ errorText NOTIFY viewChanged FINAL)
  Q_PROPERTY(QString overrideText READ overrideText NOTIFY viewChanged FINAL)

public:
  LoginScreenSettingsModel(LoginScreenPaths paths,
                           std::unique_ptr<SddmConfigWriteClient> writer,
                           std::unique_ptr<LoginScreenAuthorityProbe> probe,
                           QString currentUserName, QObject *parent = nullptr);
  ~LoginScreenSettingsModel() override;

  [[nodiscard]] QVariantList themes() const;
  [[nodiscard]] QString currentTheme() const { return m_values.theme; }
  [[nodiscard]] QVariantList sessions() const;
  [[nodiscard]] QString defaultSession() const {
    return m_values.autologinSession;
  }
  [[nodiscard]] bool autologinEnabled() const {
    return !m_values.autologinUser.isEmpty();
  }
  [[nodiscard]] QString autologinUser() const;
  [[nodiscard]] QVariantList users() const;
  [[nodiscard]] QString numlockMode() const;
  [[nodiscard]] QString cursorTheme() const { return m_values.cursorTheme; }
  [[nodiscard]] bool writable() const { return m_writable; }
  [[nodiscard]] QString readOnlyReason() const { return m_readOnlyReason; }
  [[nodiscard]] bool busy() const;
  [[nodiscard]] QString statusText() const;
  [[nodiscard]] QString errorText() const { return m_errorText; }
  [[nodiscard]] QString overrideText() const { return m_overrideText; }

  Q_INVOKABLE void refresh();
  Q_INVOKABLE bool selectTheme(const QString &themeId);
  Q_INVOKABLE bool selectDefaultSession(const QString &sessionId);
  Q_INVOKABLE bool setNumlockMode(const QString &mode);
  Q_INVOKABLE bool applyCursorTheme(const QString &theme);
  Q_INVOKABLE bool setAutologinEnabled(bool enabled);
  Q_INVOKABLE bool selectAutologinUser(const QString &userName);
  Q_INVOKABLE void clearError();

  // Test seams independent of QML marshalling.
  [[nodiscard]] QList<SddmThemeEntry> themeEntries() const { return m_themes; }
  [[nodiscard]] QList<SddmSessionEntry> sessionEntries() const {
    return m_sessions;
  }
  [[nodiscard]] QStringList loginUsers() const { return m_users; }
  [[nodiscard]] SddmEffectiveConfig effectiveValues() const { return m_values; }

Q_SIGNALS:
  void viewChanged();

private:
  [[nodiscard]] bool guardWritable();
  void enqueue(const SddmOwnedChangeSet &changes);
  void flushQueue();
  void onWriteFinished(bool ok, const QString &errorText);
  void onProbeFinished(bool writable, const QString &readOnlyReason);
  [[nodiscard]] QStringList themeIds() const;
  [[nodiscard]] QStringList sessionIds() const;

  LoginScreenPaths m_paths;
  std::unique_ptr<SddmConfigWriteClient> m_writer;
  std::unique_ptr<LoginScreenAuthorityProbe> m_probe;
  QString m_currentUserName;

  QList<SddmThemeEntry> m_themes;
  QList<SddmSessionEntry> m_sessions;
  QStringList m_users;
  SddmEffectiveConfig m_values;
  QHash<QString, QString> m_winnerFileByKey;
  QStringList m_unreadableFiles;
  QString m_overrideText;
  QString m_errorText;
  bool m_writable = false;
  QString m_readOnlyReason;
  QList<SddmOwnedChangeSet> m_writeQueue;
};

} // namespace QindaQt::Apps::SettingsLoginScreen
