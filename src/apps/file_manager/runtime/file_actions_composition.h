// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "model/file_templates.h"
#include "model/folder_launch_controller.h"
#include "model/open_with_controller.h"

#include <QStringList>
#include <QVariantMap>

#include <memory>

namespace QindaQt::Apps::FileManager {

class ApplicationsController;

// ADR-0269: the three owners behind the right-click set that Main.qml reads
// (Open With, Open Terminal Here / Open in New Window, and New File's
// templates), composed together so main() stays within its size budget.
struct FileActionsComposition final {
  std::unique_ptr<OpenWithController> openWith;
  std::unique_ptr<FolderLaunchController> folders;
  std::unique_ptr<FileTemplates> templates;

  // Adds openWithController, folderLaunchController and fileTemplates.
  void insertInto(QVariantMap &initialProperties) const;
};

// Production wiring: the IncludeNoDisplay catalog scan of `dataRoots`, the
// Settings route's own mimeapps.list store, QProcess starts, QQ_Term, this
// File Manager's program candidates and the XDG templates folder.
// `applications` (the Applications place) must outlive the result.
[[nodiscard]] FileActionsComposition composeFileActions(const QStringList &dataRoots,
                                                        ApplicationsController &applications);

} // namespace QindaQt::Apps::FileManager
