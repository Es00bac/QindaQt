// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/session_autostart/autostart_catalog.h>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariantList>

#include <memory>

namespace QindaQt::Apps::SettingsStartup {

// One XDG autostart entry (freedesktop.org Desktop Entry Specification,
// "Autostart" appendix), merged across the system directories
// (/etc/xdg/autostart, /usr/share/autostart) and the user directory
// ($XDG_CONFIG_HOME/autostart, usually ~/.config/autostart) by basename: a
// user-directory file with the same basename as a system one shadows it
// entirely, which is how "disable a system-provided autostart entry"
// works without ever writing to a package-owned file.
struct AutostartEntry final {
  QString id;      // basename without ".desktop"; the store's stable key
  QString name;
  QString comment;
  QString iconName;
  QString exec;
  bool enabled = true;
  bool eligible = false;
  QString ineligibilityReason;
  // True only for an entry this route itself created (X-QindaQt-Custom=true
  // in the file); only a custom entry can be removed outright rather than
  // just disabled, because removing a real installed application's
  // autostart file would not be undone by reinstalling it.
  bool custom = false;

  friend bool operator==(const AutostartEntry &,
                         const AutostartEntry &) = default;
};

// Local-file boundary for XDG autostart entries. Never touches a system
// (package-owned) autostart file directly: disabling one writes a user
// override that shadows it; every other mutation stays within the user
// directory this store owns outright.
class AutostartStore {
public:
  virtual ~AutostartStore() = default;
  [[nodiscard]] virtual QList<AutostartEntry> list(QString *error) = 0;
  [[nodiscard]] virtual bool setEnabled(const QString &id, bool enabled,
                                        QString *error) = 0;
  // `command` is stored verbatim as Exec=; the caller is responsible for
  // shell-safety expectations the same way a user's own shortcut/launcher
  // text is (AGENT-GUARD: this route never interprets or shell-expands it).
  // Returns the new entry's id on success, empty on failure.
  [[nodiscard]] virtual QString addCommand(const QString &name,
                                           const QString &command,
                                           QString *error) = 0;
  [[nodiscard]] virtual bool removeCustom(const QString &id,
                                          QString *error) = 0;
};

// XDG_CONFIG_HOME/autostart, plus every XDG_CONFIG_DIRS/autostart directory
// (typically /etc/xdg/autostart) as system-provided read-only sources.
class XdgAutostartStore final : public AutostartStore {
public:
  XdgAutostartStore();
  // Test seam: explicit directories instead of the real XDG search path.
  // `systemDirectories` are read-only sources in priority order (first
  // found wins for the "is this system-provided" question); `userDirectory`
  // is the one directory ever written to.
  XdgAutostartStore(QString userDirectory, QStringList systemDirectories);
  explicit XdgAutostartStore(SessionAutostart::ScanOptions options);

  [[nodiscard]] QList<AutostartEntry> list(QString *error) override;
  [[nodiscard]] bool setEnabled(const QString &id, bool enabled,
                                QString *error) override;
  [[nodiscard]] QString addCommand(const QString &name, const QString &command,
                                   QString *error) override;
  [[nodiscard]] bool removeCustom(const QString &id, QString *error) override;

private:
  SessionAutostart::ScanOptions m_options;
};

// QML-facing projection: one flat list of AutostartEntry rows plus intents.
// No authority/service state machine here -- the store is a plain local
// filesystem boundary, so a mutation either succeeds and refreshes the list,
// or fails and leaves the previous list showing with an error.
class StartupSettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList entries READ entries NOTIFY viewChanged)
  Q_PROPERTY(QString errorText READ errorText NOTIFY viewChanged)

public:
  explicit StartupSettingsModel(std::unique_ptr<AutostartStore> store,
                                QObject *parent = nullptr);

  [[nodiscard]] QVariantList entries() const;
  [[nodiscard]] const QString &errorText() const noexcept { return m_errorText; }

  Q_INVOKABLE void refresh();
  Q_INVOKABLE bool setEnabled(const QString &id, bool enabled);
  Q_INVOKABLE bool addCommand(const QString &name, const QString &command);
  Q_INVOKABLE bool removeCustom(const QString &id);

Q_SIGNALS:
  void viewChanged();

private:
  [[nodiscard]] QVariantMap projectEntry(const AutostartEntry &entry) const;

  std::unique_ptr<AutostartStore> m_store;
  QList<AutostartEntry> m_entries;
  QString m_errorText;
};

} // namespace QindaQt::Apps::SettingsStartup
