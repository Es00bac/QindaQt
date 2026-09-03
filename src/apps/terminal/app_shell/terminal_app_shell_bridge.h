// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/app_shell/application_coordinator.h"

#include <QHash>
#include <QObject>
#include <QString>

namespace QindaQt::AppShell {
class Error;
}

class QAction;

namespace QindaQt::Apps::Terminal {

// AGENT-CONTRACT: The terminal's one collaborator owning an
// ApplicationCoordinator. It never decides command execution itself;
// TerminalWindow keeps that authority and only asks this bridge to publish
// the action projection and route external activations back onto the local
// QActions, so a future global-menu consumer triggers the identical local
// code path as the menu item.
class TerminalAppShellBridge final : public QObject {
  Q_OBJECT

public:
  explicit TerminalAppShellBridge(QObject *parent = nullptr);

  [[nodiscard]] QindaQt::AppShell::ApplicationCoordinator &coordinator();

  // Publishes the documented command catalog as one atomic replacement.
  // Call exactly once, after the local QAction tree exists.
  [[nodiscard]] QindaQt::AppShell::Error publishActionCatalog();
  [[nodiscard]] QindaQt::AppShell::Error
  setActionEnabled(const QString &actionId, bool enabled);

  // Routes a known AppShell action id's activation to the local QAction
  // that owns the real command.
  void bindActivationTargets(const QHash<QString, QAction *> &targets);

private:
  QindaQt::AppShell::ApplicationCoordinator m_coordinator;
};

} // namespace QindaQt::Apps::Terminal
