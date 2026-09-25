// SPDX-License-Identifier: GPL-3.0-or-later
#include "file_manager_transfer_actions.h"

#include "../model/clipboard_controller.h"
#include "../model/navigation_controller.h"
#include "../mutation/mutation_controller.h"

#include <qindaqt/app_shell/application_coordinator.h>

namespace QindaQt::Apps::FileManager {

void bindFileManagerTransferActions(AppShell::ApplicationCoordinator &coordinator,
                                    NavigationController &navigation,
                                    ClipboardController &clipboard,
                                    MutationController &mutation,
                                    QObject *context) {
  QObject *const receiver = context != nullptr ? context : &coordinator;
  const auto sync = [&coordinator, &navigation, &clipboard, &mutation] {
    const auto enabled = [&coordinator](const char *id, bool value) {
      const auto result = coordinator.setActionEnabled(QLatin1String(id), value);
      Q_UNUSED(result);
    };
    // A remote entry carries no local mutation identity; disable every
    // current-selection mutation action while browsing smb/sftp (S5).
    // ADR-0262: application rows cannot be cut, copied or pasted into; Get
    // Info (file.properties) stays available for them.
    const bool idle = navigation.folderViewActive() && !mutation.busy() && !navigation.remoteActive()
        && !navigation.applicationsPlace();
    const bool hasSelection = navigation.folderViewActive() && clipboard.selectionCount() > 0;
    enabled("edit.cut", idle && hasSelection);
    enabled("edit.copy", idle && hasSelection);
    // ADR-0272: Recents rows are ordinary files, but Recents is no folder to
    // paste into or to describe.
    const bool folder = !navigation.recentsPlace();
    enabled("edit.paste", idle && folder && clipboard.canPaste());
    // ADR-0269: Get Info with nothing selected describes the browsed folder,
    // which the Applications place does not have.
    enabled("file.properties", navigation.folderViewActive() && !navigation.remoteActive()
                                   && (hasSelection
                                       || (!navigation.applicationsPlace() && folder)));
  };
  QObject::connect(&clipboard, &ClipboardController::stateChanged,
                   receiver, sync);
  QObject::connect(&mutation, &MutationController::stateChanged,
                   receiver, sync);
  QObject::connect(&navigation, &NavigationController::navigationChanged,
                   receiver, sync);
  QObject::connect(&navigation, &NavigationController::presentationChanged,
                   receiver, sync);
  sync();
}

} // namespace QindaQt::Apps::FileManager
