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
    const auto enabled = [&coordinator, &navigation](const char *id, bool value) {
      const QLatin1String actionId(id);
      const bool folderAction = actionId == QLatin1String("file.trash") || actionId == QLatin1String("file.copy")
          || actionId == QLatin1String("file.move") || actionId == QLatin1String("file.rename") || actionId == QLatin1String("file.new-folder");
      const auto result = coordinator.setActionEnabled(
          actionId, value && (!folderAction || navigation.folderViewActive()));
      Q_UNUSED(result);
    };
    const bool idle = !mutation.busy() && !navigation.remoteActive();
    for (const char *actionId : {"file.trash"}) {
      enabled(actionId, idle);
    }
    // ADR-0155: remote Copy To is available while browsing remote only with
    // a copier injected and no copy in flight; without one, Copy keeps
    // disabling with the other mutations.
    enabled("file.copy", (!mutation.busy() && !navigation.remoteActive()) ||
                             (navigation.remoteCopyAvailable() &&
                              !navigation.remoteCopyBusy()));
    // ADR-0156: remote Move To follows the same contract with a mover
    // injected -- and, because a move deletes its source at the server, the
    // action additionally disables while a move is already in flight so two
    // destructive moves can never overlap.
    enabled("file.move", (!mutation.busy() && !navigation.remoteActive()) ||
                             (navigation.remoteMoveAvailable() &&
                              !navigation.remoteMoveBusy()));
    // ADR-0154: remote New Folder is available while browsing remote only
    // with a folder creator injected and no creation in flight; without
    // one, New Folder keeps disabling with the other mutations.
    enabled("file.new-folder", (!mutation.busy() && !navigation.remoteActive()) ||
                                    (navigation.remoteCreateAvailable() &&
                                     !navigation.remoteCreateBusy()));
    // ADR-0153: same-folder remote Rename is the one current-folder mutation
    // available while browsing remote -- but only with a renamer injected
    // and no rename already in flight. Without a renamer, Rename keeps
    // disabling with the other current-folder mutations.
    enabled("file.rename", (!mutation.busy() && !navigation.remoteActive()) ||
                               (navigation.remoteRenameAvailable() &&
                                !navigation.remoteRenameBusy()));
    // Empty Trash, Undo, and Restore Last act on the local Trash/history and
    // stay available while browsing remote -- see this file's AGENT-GUARD.
    enabled("file.empty-trash", !mutation.busy());
    enabled("edit.undo", mutation.canUndo());
    enabled("file.restore-last", mutation.canRestore());
    // ADR-0155/0156 repair (review P1): Cancel must also be reachable while
    // an in-flight remote copy or move is retiring through the injected
    // collaborator -- MutationDialogs routes it to the remote owner -- not
    // only while the local mutation backend is busy.
    enabled("operation.cancel", mutation.busy() || navigation.remoteCopyBusy() ||
                                    navigation.remoteMoveBusy());
  };
  QObject::connect(&mutation, &MutationController::stateChanged,
                   &coordinator, sync);
  QObject::connect(&navigation, &NavigationController::navigationChanged,
                   &coordinator, sync);
  QObject::connect(&navigation, &NavigationController::presentationChanged,
                   &coordinator, sync);
  QObject::connect(&navigation, &NavigationController::remoteRenameChanged,
                   &coordinator, sync);
  QObject::connect(&navigation, &NavigationController::remoteCreateChanged,
                   &coordinator, sync);
  QObject::connect(&navigation, &NavigationController::remoteCopyChanged,
                   &coordinator, sync);
  QObject::connect(&navigation, &NavigationController::remoteMoveChanged,
                   &coordinator, sync);
  sync();
}

} // namespace QindaQt::Apps::FileManager
