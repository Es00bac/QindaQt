// SPDX-License-Identifier: GPL-3.0-or-later
#include "file_manager_application_actions.h"

#include "../model/clipboard_controller.h"
#include "../model/navigation_controller.h"

#include <qindaqt/app_shell/application_coordinator.h>

namespace QindaQt::Apps::FileManager {

void bindFileManagerApplicationActions(AppShell::ApplicationCoordinator &coordinator,
                                       NavigationController &navigation,
                                       ClipboardController &clipboard) {
  const auto sync = [&coordinator, &navigation, &clipboard] {
    const bool place = navigation.applicationsPlace() && navigation.folderViewActive();
    const int selected = place ? clipboard.selectionCount() : 0;
    const auto apply = [&coordinator](const char *id, bool enabled) {
      const auto result = coordinator.setActionEnabled(QLatin1String(id), enabled);
      Q_UNUSED(result);
    };
    // ADR-0269: Open is file.open now, one action everywhere, bound with the
    // other item actions (file_manager_item_actions.cpp).
    apply("application.show-entry-file", selected == 1);
    apply("view.group-by-category", place);
    const auto checked = coordinator.setActionChecked(
        QStringLiteral("view.group-by-category"),
        place && navigation.sortColumn() == QLatin1String("kind"));
    Q_UNUSED(checked);
  };
  QObject::connect(&clipboard, &ClipboardController::stateChanged, &coordinator, sync);
  QObject::connect(&navigation, &NavigationController::navigationChanged,
                   &coordinator, sync);
  QObject::connect(&navigation, &NavigationController::presentationChanged,
                   &coordinator, sync);
  sync();
}

} // namespace QindaQt::Apps::FileManager
