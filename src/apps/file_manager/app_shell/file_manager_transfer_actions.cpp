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
                                    MutationController &mutation) {
  const auto sync = [&coordinator, &navigation, &clipboard, &mutation] {
    const auto enabled = [&coordinator](const char *id, bool value) {
      const auto result = coordinator.setActionEnabled(QLatin1String(id), value);
      Q_UNUSED(result);
    };
    // A remote entry carries no local mutation identity; disable every
    // current-selection mutation action while browsing smb/sftp (S5).
    const bool idle = !mutation.busy() && !navigation.remoteActive();
    const bool hasSelection = clipboard.selectionCount() > 0;
    enabled("edit.cut", idle && hasSelection);
    enabled("edit.copy", idle && hasSelection);
    enabled("edit.paste", idle && clipboard.canPaste());
    enabled("file.properties", hasSelection && !navigation.remoteActive());
  };
  QObject::connect(&clipboard, &ClipboardController::stateChanged,
                   &coordinator, sync);
  QObject::connect(&mutation, &MutationController::stateChanged,
                   &coordinator, sync);
  QObject::connect(&navigation, &NavigationController::navigationChanged,
                   &coordinator, sync);
  sync();
}

} // namespace QindaQt::Apps::FileManager
