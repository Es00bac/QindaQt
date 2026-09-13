// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop_file_boundary.h"

#include "../model/local_directory_lister.h"
#include "../mutation/local_mutation_backend.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace QindaQt::Apps::FileManager::Desktop {

ListingResult FileBoundary::listLocalFolder(const QString &absolutePath) {
  return LocalDirectoryLister().list(absolutePath);
}

LaunchResult FileBoundary::launchLocalFile(const QString &absolutePath) {
  return DesktopFileLauncher().launch(absolutePath);
}

QStringList FileBoundary::fileManagerProgramCandidates() {
  // AGENT-NOTE: Same order as DesktopControls::FileManagerFolderOpener, the
  // accepted shell precedent for starting File Manager on one folder.
  const QString sibling =
      QCoreApplication::applicationDirPath() + QStringLiteral("/qindaqt-file-manager");
  QStringList candidates{sibling};
  const QString onPath =
      QStandardPaths::findExecutable(QStringLiteral("qindaqt-file-manager"));
  if (!onPath.isEmpty() && onPath != sibling) {
    candidates.append(onPath);
  }
  return candidates;
}

FolderOpenResult FileBoundary::openLocalFolder(const QString &absolutePath,
                                               const ListedIdentity listed,
                                               const QStringList &programCandidates,
                                               const ProcessStarter &start) {
  const QFileInfo requested(absolutePath);
  if (absolutePath.isEmpty() || !requested.isAbsolute()) {
    return {FolderOpenError::NotFound,
            QStringLiteral("%1 is not an absolute local folder").arg(absolutePath), {}};
  }
  const auto current = LocalMutationBackend::identityForPath(absolutePath);
  if (!current) {
    return {FolderOpenError::NotFound,
            QStringLiteral("%1 does not exist").arg(absolutePath), {}};
  }
  // AGENT-GUARD: Compare the listed object before resolving anything. Opening
  // whatever now occupies the path would launch a folder the user never saw.
  if (current->device != listed.device || current->inode != listed.inode) {
    return {FolderOpenError::Replaced,
            QStringLiteral("%1 changed since the Desktop listed it").arg(absolutePath), {}};
  }
  const QString canonical = requested.canonicalFilePath();
  if (canonical.isEmpty()) {
    return {FolderOpenError::NotFound,
            QStringLiteral("%1 is a broken link").arg(absolutePath), {}};
  }
  const QFileInfo target(canonical);
  if (!target.isDir()) {
    return {FolderOpenError::NotDirectory,
            QStringLiteral("%1 is not a folder").arg(absolutePath), {}};
  }
  if (!target.isReadable() || !target.isExecutable()) {
    return {FolderOpenError::Unreadable,
            QStringLiteral("%1 cannot be opened").arg(absolutePath), {}};
  }
  QString program;
  for (const QString &candidate : programCandidates) {
    const QFileInfo info(candidate);
    if (info.isAbsolute() && info.isFile() && info.isExecutable()) {
      program = info.absoluteFilePath();
      break;
    }
  }
  if (program.isEmpty()) {
    return {FolderOpenError::NotInstalled,
            QStringLiteral("QindaQt File Manager is not installed"), {}};
  }
  // AGENT-GUARD: The canonical directory is one literal argv element; never
  // join it into a command line or hand it to a shell.
  const QStringList arguments{canonical};
  const bool started =
      start ? start(program, arguments) : QProcess::startDetached(program, arguments);
  if (!started) {
    return {FolderOpenError::LaunchRefused,
            QStringLiteral("QindaQt File Manager could not open %1").arg(absolutePath), {}};
  }
  return {FolderOpenError::None, {}, canonical};
}

std::unique_ptr<MutationController>
FileBoundary::createLocalMutationController(QObject *parent) {
  // AGENT-NOTE: Mirrors main.cpp's exact home-Trash root composition so a
  // Desktop-initiated Trash/restore lands in the same freedesktop.org
  // $XDG_DATA_HOME/Trash File Manager itself uses; see ADR-0064.
  const QString trashRoot =
      QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
          .filePath(QStringLiteral("Trash"));
  return std::make_unique<MutationController>(
      std::make_unique<LocalMutationBackend>(trashRoot), parent);
}

} // namespace QindaQt::Apps::FileManager::Desktop
