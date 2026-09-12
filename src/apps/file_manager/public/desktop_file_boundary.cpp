// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop_file_boundary.h"

#include "../model/local_directory_lister.h"
#include "../mutation/local_mutation_backend.h"

#include <QDir>
#include <QStandardPaths>

namespace QindaQt::Apps::FileManager::Desktop {

ListingResult FileBoundary::listLocalFolder(const QString &absolutePath) {
  return LocalDirectoryLister().list(absolutePath);
}

LaunchResult FileBoundary::launchLocalFile(const QString &absolutePath) {
  return DesktopFileLauncher().launch(absolutePath);
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
