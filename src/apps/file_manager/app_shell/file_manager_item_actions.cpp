// SPDX-License-Identifier: GPL-3.0-or-later
#include "file_manager_item_actions.h"

#include "../model/clipboard_controller.h"
#include "../model/navigation_controller.h"
#include "../mutation/mutation_controller.h"

#include <qindaqt/app_shell/application_coordinator.h>

#include <QDir>

namespace QindaQt::Apps::FileManager {

void bindFileManagerItemActions(AppShell::ApplicationCoordinator &coordinator,
                                NavigationController &navigation,
                                ClipboardController &clipboard,
                                MutationController &mutation,
                                const QString &trashFilesDirectory,
                                QObject *context) {
  QObject *const receiver = context != nullptr ? context : &coordinator;
  const QString trash =
      trashFilesDirectory.isEmpty() ? QString() : QDir::cleanPath(trashFilesDirectory);
  const auto sync = [&coordinator, &navigation, &clipboard, &mutation, trash] {
    const auto enabled = [&coordinator](const char *id, bool value) {
      const auto result = coordinator.setActionEnabled(QLatin1String(id), value);
      Q_UNUSED(result);
    };
    const bool folderView = navigation.folderViewActive();
    const bool selected = folderView && clipboard.selectionCount() > 0;
    const bool local = folderView && !navigation.remoteActive() && !navigation.applicationsPlace();
    const bool inTrash = local && !trash.isEmpty() &&
        QDir::cleanPath(navigation.currentPath()) == trash;
    const bool writable = local && !inTrash && !mutation.busy();
    // ADR-0272: Quick Look previews whatever is selected, in any place.
    for (const char *id : {"file.open", "file.quick-look"}) {
      enabled(id, selected);
    }
    // ADR-0272: Recents lists local files but is no folder to create in.
    const bool folder = !navigation.recentsPlace();
    for (const char *id : {"file.open-with", "file.open-new-window", "edit.copy-path",
                           "file.add-to-sidebar"}) {
      enabled(id, local && selected);
    }
    for (const char *id : {"file.duplicate", "file.make-link", "file.compress",
                           "file.extract"}) {
      enabled(id, writable && selected);
    }
    enabled("file.delete", local && selected && !mutation.busy());
    enabled("file.put-back", inTrash && selected && !mutation.busy());
    enabled("file.new-file", writable && folder);
    enabled("file.open-terminal", local && folder);
  };
  QObject::connect(&clipboard, &ClipboardController::stateChanged, receiver, sync);
  QObject::connect(&mutation, &MutationController::stateChanged, receiver, sync);
  QObject::connect(&navigation, &NavigationController::navigationChanged, receiver, sync);
  QObject::connect(&navigation, &NavigationController::presentationChanged, receiver, sync);
  sync();
}

} // namespace QindaQt::Apps::FileManager
