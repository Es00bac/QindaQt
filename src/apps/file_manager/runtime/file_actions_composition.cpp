// SPDX-License-Identifier: GPL-3.0-or-later
#include "file_actions_composition.h"

#include "model/applications_controller.h"
#include "public/desktop_file_boundary.h"

#include <qindaqt/application_catalog/application_directory_scan.h>
#include <qindaqt/apps/settings_default_apps/default_applications_store.h>

#include <QVariant>

namespace QindaQt::Apps::FileManager {

void FileActionsComposition::insertInto(QVariantMap &initialProperties) const {
  initialProperties.insert(QStringLiteral("openWithController"),
                           QVariant::fromValue(static_cast<QObject *>(openWith.get())));
  initialProperties.insert(QStringLiteral("folderLaunchController"),
                           QVariant::fromValue(static_cast<QObject *>(folders.get())));
  initialProperties.insert(QStringLiteral("fileTemplates"),
                           QVariant::fromValue(static_cast<QObject *>(templates.get())));
}

FileActionsComposition composeFileActions(const QStringList &dataRoots,
                                          ApplicationsController &applications) {
  using QindaQt::ApplicationCatalog::ApplicationVisibility;
  FileActionsComposition composed;
  // The store starts with an empty scan; OpenWithController rescans (and
  // hands the store the fresh scan) before every query, so startup scans
  // nothing extra.
  composed.openWith = std::make_unique<OpenWithController>(
      [dataRoots] {
        return QindaQt::ApplicationCatalog::scanApplicationDirectories(
            dataRoots, ApplicationVisibility::IncludeNoDisplay);
      },
      [&applications] {
        applications.refresh();
        return applications.listing();
      },
      SettingsDefaultApps::createSessionDefaultApplicationsStore(dataRoots, {}),
      OpenWithLauncher(OpenWithLauncher::processStarter(),
                       OpenWithLauncher::desktopTerminalPrefix()));
  composed.folders = std::make_unique<FolderLaunchController>(
      OpenWithLauncher::processStarter(), FolderLaunchController::terminalProgramCandidates(),
      Desktop::FileBoundary::fileManagerProgramCandidates());
  composed.templates = std::make_unique<FileTemplates>(FileTemplates::userTemplatesDirectory());
  return composed;
}

} // namespace QindaQt::Apps::FileManager
