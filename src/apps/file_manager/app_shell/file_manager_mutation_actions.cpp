// SPDX-License-Identifier: GPL-3.0-or-later
#include "file_manager_mutation_actions.h"

#include "../model/navigation_controller.h"
#include "../mutation/mutation_controller.h"

#include <qindaqt/app_shell/application_coordinator.h>

namespace QindaQt::Apps::FileManager {

void bindFileManagerMutationActions(AppShell::ApplicationCoordinator &coordinator,
                                    NavigationController &navigation,
                                    MutationController &mutation) {
  const auto sync = [&coordinator, &navigation, &mutation] {
    const auto enabled = [&coordinator](const char *id, bool value) {
      const auto result = coordinator.setActionEnabled(QLatin1String(id), value);
      Q_UNUSED(result);
    };
    const bool idle = !mutation.busy() && !navigation.remoteActive();
    for (const char *actionId :
         {"file.new-folder", "file.rename", "file.copy", "file.move",
          "file.trash"}) {
      enabled(actionId, idle);
    }
    // Empty Trash, Undo, and Restore Last act on the local Trash/history and
    // stay available while browsing remote -- see this file's AGENT-GUARD.
    enabled("file.empty-trash", !mutation.busy());
    enabled("edit.undo", mutation.canUndo());
    enabled("file.restore-last", mutation.canRestore());
    enabled("operation.cancel", mutation.busy());
  };
  QObject::connect(&mutation, &MutationController::stateChanged,
                   &coordinator, sync);
  QObject::connect(&navigation, &NavigationController::navigationChanged,
                   &coordinator, sync);
  sync();
}

} // namespace QindaQt::Apps::FileManager
