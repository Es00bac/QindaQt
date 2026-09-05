// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_controls/file_manager_folder_opener.h"

#include "launch_spawner.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QStandardPaths>

#include <utility>

namespace QindaQt::Shell::DesktopControls {

QStringList FileManagerFolderOpener::defaultProgramCandidates()
{
  QStringList candidates;
  const QString sibling = QCoreApplication::applicationDirPath()
      + QStringLiteral("/qindaqt-file-manager");
  candidates.append(sibling);
  const QString onPath =
      QStandardPaths::findExecutable(QStringLiteral("qindaqt-file-manager"));
  if (!onPath.isEmpty() && onPath != sibling) {
    candidates.append(onPath);
  }
  return candidates;
}

FileManagerFolderOpener::FileManagerFolderOpener(Launcher::LaunchSpawner &spawner,
                                                 QStringList programCandidates)
    : m_spawner(spawner), m_programCandidates(std::move(programCandidates))
{
}

QString FileManagerFolderOpener::resolvedProgram() const
{
  for (const QString &candidate : m_programCandidates) {
    const QFileInfo info(candidate);
    if (info.isAbsolute() && info.isFile() && info.isExecutable()) {
      return info.absoluteFilePath();
    }
  }
  return {};
}

FolderOpener::Result FileManagerFolderOpener::open(const QString &absoluteDirectory)
{
  const QFileInfo directory(absoluteDirectory);
  if (!directory.isAbsolute() || !directory.isDir()) {
    return {false, QStringLiteral("the folder is not an existing directory")};
  }
  const QString program = resolvedProgram();
  if (program.isEmpty()) {
    return {false, QStringLiteral("the QindaQt File Manager is not installed")};
  }
  const Launcher::SpawnResult spawned = m_spawner.spawn(
      {program, {directory.absoluteFilePath()}, QString{}});
  return {spawned.ok, spawned.diagnostic};
}

} // namespace QindaQt::Shell::DesktopControls
