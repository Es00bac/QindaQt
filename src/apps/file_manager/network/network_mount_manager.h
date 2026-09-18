// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "network_locations_store.h"
#include "systemd_user_units.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: keeps the systemd user `.mount` units under the injected
// units directory in step with the saved locations that asked to be mounted
// at login (ADR-0199). It writes unit files and asks the user's own service
// manager to notice them. It never mounts, unmounts, or starts anything
// itself, never runs a filesystem helper, and never asks for a credential.
//
// AGENT-GUARD: it only ever touches unit files whose name is the systemd path
// escaping of something under `<home>/Network/`. A unit the user wrote by
// hand, or any other unit in the same directory, is never read, rewritten, or
// removed -- the prefix check is what makes that true, so do not relax it.
//
// AGENT-GUARD: turning the knob on is what makes `net-fs/sshfs` a runtime
// dependency. This class does not install it and cannot: a missing sshfs
// shows up as a failed mount in the user's journal, which is why the
// Preferences copy says so plainly.
class NetworkMountManager final : public QObject {
  Q_OBJECT

  // The last problem writing or registering a unit, empty when all is well.
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)
  // How many locations this manager currently has a unit for. A property, not
  // an invokable, so the Preferences copy re-evaluates when it changes.
  Q_PROPERTY(int mountedLocationCount READ mountedLocationCount NOTIFY mountsChanged FINAL)

public:
  static constexpr qint64 maximumUnitBytes = 16 * 1024;

  // unitsDirectory is normally $XDG_CONFIG_HOME/systemd/user; homeDirectory is
  // the parent of the Network folder. Both are injected so no test can reach
  // the developer's own configuration. A null `units` is supported and means
  // "write the files but ask nothing of systemd", which is exactly what a
  // focused row wants.
  NetworkMountManager(QString unitsDirectory, QString homeDirectory,
                      SystemdUserUnitsPtr units, QObject *parent = nullptr);

  // Makes the on-disk units match `locations`. Idempotent: calling it twice
  // with the same input writes nothing the second time.
  void synchronize(const QVector<NetworkLocationRecord> &locations);

  [[nodiscard]] QString lastError() const { return m_lastError; }
  [[nodiscard]] int mountedLocationCount() const { return m_mountedLocationCount; }
  Q_INVOKABLE void clearLastError();

  // The units this manager owns in its directory right now, sorted. Exposed
  // for the focused rows and for the Preferences window's "what is mounted"
  // list.
  [[nodiscard]] QStringList ownedUnitNames() const;
  // The unit-name prefix every owned unit starts with.
  [[nodiscard]] QString ownedPrefix() const;

signals:
  void lastErrorChanged();
  void mountsChanged();

private:
  void setLastError(const QString &message);
  [[nodiscard]] bool writeUnit(const QString &unitName, const QString &contents);
  [[nodiscard]] bool removeUnit(const QString &unitName);

  QString m_unitsDirectory;
  QString m_homeDirectory;
  SystemdUserUnitsPtr m_units;
  QString m_lastError;
  int m_mountedLocationCount = 0;
};

} // namespace QindaQt::Apps::FileManager
