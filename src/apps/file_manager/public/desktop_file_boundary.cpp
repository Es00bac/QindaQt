// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop_file_boundary.h"

#include "../model/local_directory_lister.h"
#include "../mutation/local_mutation_backend.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include <optional>

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

namespace {

// The refusal for a path that is no longer the object the Desktop listed, if
// any. AGENT-GUARD: compare the listed object before resolving anything;
// acting on whatever now occupies the path would show something the user
// never saw.
[[nodiscard]] std::optional<FolderOpenResult> refuseUnlisted(const QString &absolutePath,
                                                              const ListedIdentity listed) {
  if (absolutePath.isEmpty() || !QFileInfo(absolutePath).isAbsolute()) {
    return FolderOpenResult{FolderOpenError::NotFound,
                            QStringLiteral("%1 is not an absolute local folder").arg(absolutePath),
                            {}};
  }
  const auto current = LocalMutationBackend::identityForPath(absolutePath);
  if (!current) {
    return FolderOpenResult{FolderOpenError::NotFound,
                            QStringLiteral("%1 does not exist").arg(absolutePath), {}};
  }
  if (current->device != listed.device || current->inode != listed.inode) {
    return FolderOpenResult{
        FolderOpenError::Replaced,
        QStringLiteral("%1 changed since the Desktop listed it").arg(absolutePath), {}};
  }
  return std::nullopt;
}

// The refusal for a canonical folder File Manager could not list and enter.
[[nodiscard]] std::optional<FolderOpenResult> refuseUnusableFolder(const QString &canonical,
                                                                    const QString &absolutePath) {
  if (canonical.isEmpty()) {
    return FolderOpenResult{FolderOpenError::NotFound,
                            QStringLiteral("%1 is a broken link").arg(absolutePath), {}};
  }
  const QFileInfo target(canonical);
  if (!target.isDir()) {
    return FolderOpenResult{FolderOpenError::NotDirectory,
                            QStringLiteral("%1 is not a folder").arg(absolutePath), {}};
  }
  if (!target.isReadable() || !target.isExecutable()) {
    return FolderOpenResult{FolderOpenError::Unreadable,
                            QStringLiteral("%1 cannot be opened").arg(absolutePath), {}};
  }
  return std::nullopt;
}

// Starts the first absolute executable candidate with `arguments`.
[[nodiscard]] FolderOpenResult startFileManager(const QString &absolutePath,
                                                const QString &canonicalFolder,
                                                const QStringList &arguments,
                                                const QStringList &programCandidates,
                                                const ProcessStarter &start) {
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
  // AGENT-GUARD: every argument is one literal argv element; never join them
  // into a command line or hand them to a shell.
  const bool started =
      start ? start(program, arguments) : QProcess::startDetached(program, arguments);
  if (!started) {
    return {FolderOpenError::LaunchRefused,
            QStringLiteral("QindaQt File Manager could not open %1").arg(absolutePath), {}};
  }
  return {FolderOpenError::None, {}, canonicalFolder};
}

} // namespace

FolderOpenResult FileBoundary::openLocalFolder(const QString &absolutePath,
                                               const ListedIdentity listed,
                                               const QStringList &programCandidates,
                                               const ProcessStarter &start) {
  if (auto refusal = refuseUnlisted(absolutePath, listed)) {
    return *refusal;
  }
  const QString canonical = QFileInfo(absolutePath).canonicalFilePath();
  if (auto refusal = refuseUnusableFolder(canonical, absolutePath)) {
    return *refusal;
  }
  return startFileManager(absolutePath, canonical, QStringList{canonical}, programCandidates,
                          start);
}

QStringList FileBoundary::revealArguments(const RevealRequest &request) {
  QStringList arguments;
  for (const QString &name : request.names) {
    // The "=" form keeps a name that starts with "-" from reading as an option.
    arguments.append(QStringLiteral("--select=") + name);
  }
  if (request.showProperties) {
    arguments.append(QStringLiteral("--show-properties"));
  }
  arguments.append(request.folder);
  return arguments;
}

FolderOpenResult FileBoundary::revealLocalItem(const QString &absolutePath,
                                               const ListedIdentity listed,
                                               const bool showProperties,
                                               const QStringList &programCandidates,
                                               const ProcessStarter &start) {
  if (auto refusal = refuseUnlisted(absolutePath, listed)) {
    return *refusal;
  }
  const QFileInfo item(absolutePath);
  const QString name = item.fileName();
  if (!isRevealableName(name)) {
    return {FolderOpenError::NotFound,
            QStringLiteral("%1 is not an item in a folder").arg(absolutePath), {}};
  }
  const QString folder = QFileInfo(item.absolutePath()).canonicalFilePath();
  if (auto refusal = refuseUnusableFolder(folder, item.absolutePath())) {
    return *refusal;
  }
  return startFileManager(absolutePath, folder,
                          revealArguments({folder, {name}, showProperties}),
                          programCandidates, start);
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

std::unique_ptr<ClipboardController>
FileBoundary::createLocalClipboardController(MutationController &mutation,
                                             QClipboard &clipboard, QObject *parent) {
  return std::make_unique<ClipboardController>(mutation, clipboard, parent);
}

} // namespace QindaQt::Apps::FileManager::Desktop
