// SPDX-License-Identifier: GPL-3.0-or-later
#include "system_monitor_app_shell_bridge.h"

#include "system_monitor_action_catalog.h"

#include <QAction>

namespace QindaQt::Apps::SystemMonitor {

SystemMonitorAppShellBridge::SystemMonitorAppShellBridge(QObject *parent)
    : QObject(parent) {}

QindaQt::AppShell::ApplicationCoordinator &
SystemMonitorAppShellBridge::coordinator() {
  return m_coordinator;
}

QindaQt::AppShell::Error SystemMonitorAppShellBridge::publishActionCatalog() {
  return m_coordinator.replaceActions(systemMonitorActionCatalog());
}

void SystemMonitorAppShellBridge::bindActivationTargets(
    const QHash<QString, QAction *> &targets) {
  connect(&m_coordinator,
          &QindaQt::AppShell::ApplicationCoordinator::actionRequested, this,
          [targets](const QString &actionId) {
            if (QAction *action = targets.value(actionId)) {
              action->trigger();
            }
          });
  for (auto it = targets.cbegin(); it != targets.cend(); ++it) {
    const QString id = it.key();
    QAction *const action = it.value();
    if (!action) {
      continue;
    }
    const auto synchronize = [this, id, action] {
      static_cast<void>(
          m_coordinator.setActionEnabled(id, action->isEnabled()));
      static_cast<void>(
          m_coordinator.setActionChecked(id, action->isChecked()));
    };
    connect(action, &QAction::changed, this, synchronize);
    synchronize();
  }
}

} // namespace QindaQt::Apps::SystemMonitor
