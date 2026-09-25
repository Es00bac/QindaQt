// SPDX-License-Identifier: GPL-3.0-or-later
#include "proton_removal.h"

#include "fs_ops.h"
#include "ge_proton_releases.h"
#include "job_log.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

namespace QindaQt::QindaLutris {

namespace {

const QString kTrashName = QStringLiteral(".qindalutris-trash");

QString canonicalOrClean(const QString &path) {
  const QString canonical = QFileInfo(path).canonicalFilePath();
  return canonical.isEmpty() ? QDir::cleanPath(path) : canonical;
}

bool isWithin(const QString &path, const QString &root) {
  if (root == QLatin1String("/")) {
    return true;
  }
  return path == root || path.startsWith(root + QLatin1Char('/'));
}

void note(JobLog *log, const QString &text) {
  if (log != nullptr) {
    log->append(text);
  }
}

} // namespace

QStringList ProtonRemovalRequest::defaultSystemRoots() {
  return {QStringLiteral("/usr"),       QStringLiteral("/opt"),      QStringLiteral("/etc"),
          QStringLiteral("/lib"),       QStringLiteral("/lib64"),    QStringLiteral("/bin"),
          QStringLiteral("/sbin"),      QStringLiteral("/boot"),     QStringLiteral("/nix/store"),
          QStringLiteral("/gnu/store"), QStringLiteral("/snap")};
}

std::optional<QString> checkProtonRemoval(const ProtonRemovalRequest &request) {
  const QString name = request.buildName;
  if (!isSafeToolName(name)) {
    return QStringLiteral("That Proton build name is not valid, so nothing was removed.");
  }
  if (request.userRoot.isEmpty() || QDir::isRelativePath(request.userRoot)) {
    return QStringLiteral("QindaLutris does not know where that Proton build is, so "
                          "nothing was removed.");
  }
  const QString build = QDir::cleanPath(request.userRoot) + QLatin1Char('/') + name;
  const QStringList buildPaths{build, canonicalOrClean(build),
                               canonicalOrClean(request.userRoot) + QLatin1Char('/') + name};
  if (QDir::cleanPath(request.userRoot) == QLatin1String("/") ||
      canonicalOrClean(request.userRoot) == QLatin1String("/")) {
    return QStringLiteral("%1 belongs to the system, so QindaLutris cannot remove it.").arg(name);
  }
  for (const QString &systemRoot : request.systemRoots) {
    if (systemRoot.isEmpty() || QDir::isRelativePath(systemRoot)) {
      continue;
    }
    const QStringList systemPaths{QDir::cleanPath(systemRoot), canonicalOrClean(systemRoot)};
    for (const QString &candidate : buildPaths) {
      for (const QString &systemPath : systemPaths) {
        if (isWithin(candidate, systemPath)) {
          return QStringLiteral("%1 was installed by the system's package manager, so "
                                "QindaLutris cannot remove it.")
              .arg(name);
        }
      }
    }
  }
  if (request.pinnedBuildNames.contains(name)) {
    return QStringLiteral("%1 is still used by at least one of your games, so it was "
                          "not removed. Move those games to another Proton build "
                          "first.")
        .arg(name);
  }
  const QString path = request.userRoot + QLatin1Char('/') + name;
  const QFileInfo info(path);
  if (info.isSymLink()) {
    return QStringLiteral("%1 is not a normal Proton folder, so QindaLutris left it "
                          "alone.")
        .arg(name);
  }
  if (!info.exists() || !info.isDir()) {
    return QStringLiteral("%1 is not installed.").arg(name);
  }
  return std::nullopt;
}

ProtonRemovalResult removeProtonBuild(const ProtonRemovalRequest &request, JobLog *log) {
  ProtonRemovalResult result;
  if (const auto refused = checkProtonRemoval(request)) {
    note(log, QStringLiteral("Removal refused: %1").arg(*refused));
    result.message = *refused;
    return result;
  }
  const QString root = QDir::cleanPath(request.userRoot);
  const QString trash = root + QLatin1Char('/') + kTrashName;
  if (!QDir().mkpath(trash) || isSymlink(trash)) {
    note(log, QStringLiteral("Cannot prepare %1").arg(trash));
    result.message = QStringLiteral("%1 could not be removed.").arg(request.buildName);
    return result;
  }
  const QString source = root + QLatin1Char('/') + request.buildName;
  const QString parked = trash + QLatin1Char('/') + request.buildName + QLatin1Char('-') +
                         QUuid::createUuid().toString(QUuid::Id128);
  QString error;
  if (!renameNoReplace(source, parked, &error)) {
    note(log, QStringLiteral("Rename %1 -> %2 failed: %3").arg(source, parked, error));
    result.message = QStringLiteral("%1 could not be removed.").arg(request.buildName);
    return result;
  }
  note(log, QStringLiteral("Moved %1 to trash").arg(source));
  result.ok = true; // gone from the root: no catalog lists it any more
  QString leftover;
  result.complete = removeTreeForcibly(parked, &leftover);
  if (!result.complete) {
    note(log, QStringLiteral("Trash not fully deleted: %1").arg(leftover));
    result.message = QStringLiteral("%1 was removed, but some of its files could not be "
                                    "deleted and still use disk space.")
                         .arg(request.buildName);
    return result;
  }
  result.message = QStringLiteral("%1 was removed.").arg(request.buildName);
  return result;
}

bool sweepProtonTrash(const QString &userRoot) {
  if (userRoot.isEmpty() || QDir::isRelativePath(userRoot)) {
    return false;
  }
  const QString trash = QDir::cleanPath(userRoot) + QLatin1Char('/') + kTrashName;
  if (isSymlink(trash)) {
    return QFile::remove(trash); // never followed
  }
  return removeTreeForcibly(trash);
}

} // namespace QindaQt::QindaLutris
