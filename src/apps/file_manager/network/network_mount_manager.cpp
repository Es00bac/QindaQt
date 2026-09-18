// SPDX-License-Identifier: GPL-3.0-or-later
#include "network_mount_manager.h"

#include "../model/state_file.h"
#include "mount_unit.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>

#include <utility>

namespace QindaQt::Apps::FileManager {
namespace {

[[nodiscard]] StateFile unitFile(const QString &directory, const QString &unitName,
                                 const qint64 maximumBytes) {
  return StateFile(directory, unitName.toUtf8(), maximumBytes);
}

} // namespace

NetworkMountManager::NetworkMountManager(QString unitsDirectory,
                                         QString homeDirectory,
                                         SystemdUserUnitsPtr units, QObject *parent)
    : QObject(parent), m_unitsDirectory(QDir::cleanPath(std::move(unitsDirectory))),
      m_homeDirectory(QDir::cleanPath(std::move(homeDirectory))),
      m_units(std::move(units)) {}

QString NetworkMountManager::ownedPrefix() const {
  return MountUnit::escapePath(
             QStringLiteral("%1/%2").arg(
                 m_homeDirectory, QString::fromLatin1(MountUnit::parentDirectoryName))) +
         QStringLiteral("-");
}

QStringList NetworkMountManager::ownedUnitNames() const {
  const QString prefix = ownedPrefix();
  QStringList owned;
  const QFileInfoList entries =
      QDir(m_unitsDirectory)
          .entryInfoList({QStringLiteral("*.mount")}, QDir::Files | QDir::NoSymLinks,
                         QDir::Name);
  for (const QFileInfo &entry : entries) {
    // AGENT-GUARD: the prefix is the whole ownership rule. Without it this
    // would enumerate -- and synchronize() would delete -- units the user
    // wrote themselves.
    if (entry.fileName().startsWith(prefix)) {
      owned.append(entry.fileName());
    }
  }
  return owned;
}

void NetworkMountManager::setLastError(const QString &message) {
  if (m_lastError == message) {
    return;
  }
  m_lastError = message;
  Q_EMIT lastErrorChanged();
}

void NetworkMountManager::clearLastError() { setLastError({}); }

bool NetworkMountManager::writeUnit(const QString &unitName, const QString &contents) {
  const StateFile file = unitFile(m_unitsDirectory, unitName, maximumUnitBytes);
  const StateFile::ReadResult existing = file.read();
  // Idempotent: an unchanged unit is not rewritten, so synchronize() does not
  // churn timestamps or make systemd reload for nothing.
  if (existing.ok() && existing.bytes == contents.toUtf8()) {
    return false;
  }
  const StateFile::WriteResult written = file.write(contents.toUtf8());
  if (!written.ok()) {
    setLastError(QStringLiteral("The mount unit for this location could not be "
                                "written."));
    return false;
  }
  return true;
}

bool NetworkMountManager::removeUnit(const QString &unitName) {
  const StateFile::WriteResult removed =
      unitFile(m_unitsDirectory, unitName, maximumUnitBytes).remove();
  if (!removed.ok()) {
    setLastError(QStringLiteral("An old mount unit could not be removed."));
    return false;
  }
  return true;
}

void NetworkMountManager::synchronize(const QVector<NetworkLocationRecord> &locations) {
  QHash<QString, QString> desired;
  QString firstRefusal;
  for (const NetworkLocationRecord &location : locations) {
    if (!location.mountAtLogin) {
      continue;
    }
    const MountUnitResult unit = MountUnit::build(location, m_homeDirectory);
    if (!unit.ok()) {
      if (firstRefusal.isEmpty()) {
        firstRefusal = unit.message;
      }
      continue;
    }
    desired.insert(unit.unitName, unit.contents);
  }

  bool changed = false;
  const QStringList owned = ownedUnitNames();
  for (const QString &unitName : owned) {
    if (desired.contains(unitName)) {
      continue;
    }
    // Disable before removing: systemd's symlink in default.target.wants
    // outlives the unit file otherwise, and the next daemon-reload warns.
    if (m_units) {
      const QString refused = m_units->disable(unitName);
      if (!refused.isEmpty() && firstRefusal.isEmpty()) {
        firstRefusal = refused;
      }
    }
    changed = removeUnit(unitName) || changed;
  }

  QStringList enable;
  for (auto it = desired.constBegin(); it != desired.constEnd(); ++it) {
    if (writeUnit(it.key(), it.value())) {
      changed = true;
    }
    enable.append(it.key());
  }
  const bool alreadyInStep = !changed && owned == enable;
  if (!alreadyInStep && m_units) {
    const QString reloadRefused = m_units->reload();
    if (!reloadRefused.isEmpty() && firstRefusal.isEmpty()) {
      firstRefusal = reloadRefused;
    }
    enable.sort();
    for (const QString &unitName : std::as_const(enable)) {
      const QString refused = m_units->enable(unitName);
      if (!refused.isEmpty() && firstRefusal.isEmpty()) {
        firstRefusal = refused;
      }
    }
  }
  setLastError(firstRefusal);
  const int mounted = static_cast<int>(desired.size());
  if (mounted != m_mountedLocationCount) {
    m_mountedLocationCount = mounted;
    Q_EMIT mountsChanged();
  }
}

} // namespace QindaQt::Apps::FileManager
