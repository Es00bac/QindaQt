// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell/desktop_controls/dock_path_port.h"

#include <functional>
#include <memory>

namespace QindaQt::Apps::FileManager {
class MutationController;
}

namespace QindaQt::Shell::DesktopControls {

// Production DockPathPort (ADR-0265). Folders and the Trash open through the
// borrowed FolderOpener (the launcher's bounded process seam, shared with the
// Places menu); files, folder listings, listed children, and Empty Trash go
// through the File Manager's public FileBoundary, the same boundary the
// desktop surface uses.
//
// AGENT-CONTRACT: the borrowed opener must outlive this object; GUI thread
// only. The Empty Trash mutation controller is created on first use, owned
// here, and runs at most one operation at a time.
class FileManagerDockPaths final : public DockPathPort {
public:
  explicit FileManagerDockPaths(FolderOpener &opener);
  ~FileManagerDockPaths() override;

  FileManagerDockPaths(const FileManagerDockPaths &) = delete;
  FileManagerDockPaths &operator=(const FileManagerDockPaths &) = delete;

  [[nodiscard]] PathKind classify(const QString &absolutePath) const override;
  [[nodiscard]] FolderOpener::Result openFolder(const QString &absolutePath) override;
  [[nodiscard]] FolderOpener::Result openFile(const QString &absolutePath) override;
  [[nodiscard]] QVariantList listFolder(const QString &absolutePath, int limit,
                                        QString *diagnostic) const override;
  [[nodiscard]] FolderOpener::Result openListedEntry(const QVariantMap &entry) override;
  [[nodiscard]] QString trashFilesDirectory() const override;
  [[nodiscard]] bool emptyTrash(std::function<void(bool, const QString &)> finished,
                                QString *diagnostic) override;

private:
  void settleTrash();

  FolderOpener &m_opener;
  std::unique_ptr<Apps::FileManager::MutationController> m_mutation;
  std::function<void(bool, const QString &)> m_trashFinished;
};

} // namespace QindaQt::Shell::DesktopControls
