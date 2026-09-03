// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/terminal_app_shell_bridge.h"

#include "app_shell/terminal_action_catalog.h"

#include <QAction>

namespace QindaQt::Apps::Terminal {

TerminalAppShellBridge::TerminalAppShellBridge(QObject *parent)
    : QObject(parent) {}

QindaQt::AppShell::ApplicationCoordinator &
TerminalAppShellBridge::coordinator() {
  return m_coordinator;
}

QindaQt::AppShell::Error TerminalAppShellBridge::publishActionCatalog() {
  return m_coordinator.replaceActions(terminalActionCatalog());
}

QindaQt::AppShell::Error
TerminalAppShellBridge::setActionEnabled(const QString &actionId,
                                         bool enabled) {
  return m_coordinator.setActionEnabled(actionId, enabled);
}

void TerminalAppShellBridge::bindActivationTargets(
    const QHash<QString, QAction *> &targets) {
  connect(&m_coordinator, &QindaQt::AppShell::ApplicationCoordinator::
                             actionRequested,
          this, [targets](const QString &actionId) {
            if (QAction *action = targets.value(actionId)) {
              action->trigger();
            }
          });
}

} // namespace QindaQt::Apps::Terminal
