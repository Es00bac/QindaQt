// SPDX-License-Identifier: GPL-3.0-or-later
#include "file_manager_dock_actions.h"

#include "../runtime/application_dock_pins.h"

#include <qindaqt/app_shell/application_coordinator.h>

namespace QindaQt::Apps::FileManager {

void bindFileManagerDockActions(AppShell::ApplicationCoordinator &coordinator,
                                ApplicationDockPins &dockPins) {
  const auto sync = [&coordinator, &dockPins] {
    const QString id = QStringLiteral("application.keep-in-dock");
    const auto enabled = coordinator.setActionEnabled(id, dockPins.available());
    Q_UNUSED(enabled);
    const auto checked = coordinator.setActionChecked(id, dockPins.pinned());
    Q_UNUSED(checked);
  };
  QObject::connect(&dockPins, &ApplicationDockPins::stateChanged, &coordinator, sync);
  sync();
}

} // namespace QindaQt::Apps::FileManager
