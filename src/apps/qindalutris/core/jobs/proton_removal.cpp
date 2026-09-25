// SPDX-License-Identifier: GPL-3.0-or-later
#include "proton_removal.h"

#include "fs_ops.h"
#include "ge_proton_releases.h"
#include "job_log.h"

#include <QDir>
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
  return {QStringLiteral("/usr"), QStringLiteral("/opt"), QStringLiteral("/etc"),
          QStringLiteral("/var"), QStringLiteral("/lib"), QStringLiteral("/lib64"),
          QStringLiteral("/bin"), QStringLiteral("/sbin"), QStringLiteral("/boot"),
          QStringLiteral("/nix"), QStringLiteral("/gnu")};
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
  const QString root = canonicalOrClean(request.userRoot);
  if (root == QLatin1String("/")) {
    return QStringLiteral("%1 belongs to the system, so QindaLutris cannot remove it.").arg(name);
  }
  for (const QString &systemRoot : request.systemRoots) {
    if (systemRoot.isEmpty()) {
      continue;
    }
    if (isWithin(root, canonicalOrClean(systemRoot)) ||
        isWithin(QDir::cleanPath(request.userRoot), QDir::cleanPath(systemRoot))) {
      return QStringLiteral("%1 was installed by the system's package manager, so "
                            "QindaLutris cannot remove it.")
          .arg(name);
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
  if (!QDir(parked).removeRecursively()) {
    // The build is already gone from the root; the leftover is swept later.
    note(log, QStringLiteral("Some trashed files remain in %1").arg(parked));
  }
  result.ok = true;
  result.message = QStringLiteral("%1 was removed.").arg(request.buildName);
  return result;
}

void sweepProtonTrash(const QString &userRoot) {
  const QString trash = QDir::cleanPath(userRoot) + QLatin1Char('/') + kTrashName;
  if (userRoot.isEmpty() || isSymlink(trash) || !QFileInfo(trash).isDir()) {
    return;
  }
  QDir(trash).removeRecursively();
}

} // namespace QindaQt::QindaLutris
