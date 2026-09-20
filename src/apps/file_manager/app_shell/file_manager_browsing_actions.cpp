// SPDX-License-Identifier: GPL-3.0-or-later
#include "file_manager_browsing_actions.h"
#include "model/navigation_controller.h"
#include <qindaqt/app_shell/application_coordinator.h>

namespace QindaQt::Apps::FileManager {
void bindFileManagerBrowsingActions(AppShell::ApplicationCoordinator &coordinator,
                                   NavigationController &navigation) {
  const auto sync = [&coordinator, &navigation] {
    coordinator.setWindowTitle(
        QStringLiteral("QindaQt File Manager — %1").arg(navigation.currentPath()));
    const auto enabled = [&coordinator, &navigation](const char *id, bool value) {
      const QLatin1String actionId(id);
      const bool folderAction = actionId.startsWith(QLatin1String("view."))
          || actionId == QLatin1String("edit.select-all") || actionId == QLatin1String("bookmark.add");
      const auto result = coordinator.setActionEnabled(
          actionId, value && (!folderAction || navigation.folderViewActive()));
      Q_UNUSED(result);
    };
    const auto checked = [&coordinator](const char *id, bool value) {
      const auto result = coordinator.setActionChecked(QLatin1String(id), value);
      Q_UNUSED(result);
    };
    enabled("go.back", navigation.canGoBack());
    enabled("go.forward", navigation.canGoForward());
    enabled("go.up", navigation.canGoUp());
    enabled("view.zoom-in", navigation.canZoomIn());
    enabled("view.zoom-out", navigation.canZoomOut());
    // Recursive search never resolves a remote URL through QFileInfo (see
    // SearchController::startSearch), so the action that opens it is
    // disabled instead of presenting a dead end (ADR-0137).
    enabled("view.filter", !navigation.remoteActive());
    for (const char *id : {"view.refresh", "view.show-hidden", "view.grid-mode",
                           "view.details-mode", "view.zoom-reset", "edit.select-all", "bookmark.add"})
      enabled(id, true);
    checked("view.show-hidden", navigation.showHidden());
  };
  QObject::connect(&navigation, &NavigationController::navigationChanged,
                   &coordinator, sync);
  QObject::connect(&navigation, &NavigationController::presentationChanged,
                   &coordinator, sync);
  sync();
}
} // namespace QindaQt::Apps::FileManager
