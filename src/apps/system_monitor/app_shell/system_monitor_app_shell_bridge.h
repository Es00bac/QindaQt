// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/app_shell/application_coordinator.h"

#include <QHash>
#include <QObject>

class QAction;

namespace QindaQt::Apps::SystemMonitor {

// Owns one window's public AppShell projection; local QActions remain the
// command authority, including when a global menu activates a catalog entry.
class SystemMonitorAppShellBridge final : public QObject {
  Q_OBJECT

public:
  explicit SystemMonitorAppShellBridge(QObject *parent = nullptr);
  [[nodiscard]] QindaQt::AppShell::ApplicationCoordinator &coordinator();
  [[nodiscard]] QindaQt::AppShell::Error publishActionCatalog();
  void bindActivationTargets(const QHash<QString, QAction *> &targets);

private:
  QindaQt::AppShell::ApplicationCoordinator m_coordinator;
};

} // namespace QindaQt::Apps::SystemMonitor
