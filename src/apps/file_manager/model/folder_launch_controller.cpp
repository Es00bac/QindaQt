// SPDX-License-Identifier: GPL-3.0-or-later
#include "folder_launch_controller.h"

#include "public/desktop_file_boundary.h"

#include <QFileInfo>
#include <QStandardPaths>

#include <utility>

namespace QindaQt::Apps::FileManager {
namespace {

[[nodiscard]] QString firstExecutable(const QStringList &candidates) {
  for (const QString &candidate : candidates) {
    const QFileInfo info(candidate);
    if (info.isAbsolute() && info.isFile() && info.isExecutable()) {
      return info.absoluteFilePath();
    }
  }
  return {};
}

} // namespace

FolderLaunchController::FolderLaunchController(DetachedStarter start,
                                               QStringList terminalPrograms,
                                               QStringList fileManagerPrograms,
                                               QObject *parent)
    : QObject(parent), m_start(std::move(start)),
      m_terminalPrograms(std::move(terminalPrograms)),
      m_fileManagerPrograms(std::move(fileManagerPrograms)) {}

QStringList FolderLaunchController::terminalProgramCandidates() {
  const QString onPath = QStandardPaths::findExecutable(QStringLiteral("qqterm"));
  return onPath.isEmpty() ? QStringList{} : QStringList{onPath};
}

bool FolderLaunchController::openTerminal(const QString &folderPath) {
  const QFileInfo requested(folderPath);
  const QString canonical = requested.isAbsolute() ? requested.canonicalFilePath() : QString();
  const QFileInfo target(canonical);
  if (canonical.isEmpty() || !target.isDir() || !target.isReadable() || !target.isExecutable()) {
    setLastError(QStringLiteral("“%1” cannot be opened in a terminal").arg(folderPath));
    return false;
  }
  const QString program = firstExecutable(m_terminalPrograms);
  if (program.isEmpty()) {
    setLastError(QStringLiteral("The terminal, QQ_Term, is not installed"));
    return false;
  }
  // AGENT-GUARD: the folder is one literal argv element, never a command line.
  if (!m_start || !m_start(program, {QStringLiteral("--working-directory"), canonical})) {
    setLastError(QStringLiteral("The terminal could not be started"));
    return false;
  }
  setLastError({});
  return true;
}

bool FolderLaunchController::openInNewWindow(const QVariantMap &entry) {
  bool deviceOk = false;
  bool inodeOk = false;
  const Desktop::ListedIdentity listed{
      entry.value(QStringLiteral("device")).toString().toULongLong(&deviceOk, 10),
      entry.value(QStringLiteral("inode")).toString().toULongLong(&inodeOk, 10)};
  if (!deviceOk || !inodeOk) {
    setLastError(QStringLiteral("The selection is stale; refresh and try again"));
    return false;
  }
  const Desktop::FolderOpenResult result = Desktop::FileBoundary::openLocalFolder(
      entry.value(QStringLiteral("path")).toString(), listed, m_fileManagerPrograms, m_start);
  setLastError(result.ok() ? QString() : result.diagnostic);
  return result.ok();
}

void FolderLaunchController::clearLastError() {
  setLastError({});
}

void FolderLaunchController::setLastError(const QString &message) {
  if (m_lastError == message) {
    return;
  }
  m_lastError = message;
  emit lastErrorChanged();
}

} // namespace QindaQt::Apps::FileManager
