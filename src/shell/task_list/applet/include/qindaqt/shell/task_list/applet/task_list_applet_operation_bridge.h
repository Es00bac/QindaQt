// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/applet/task_list_applet_operation_port.h"

namespace QindaQt::ShellTaskList::Operations {
class TaskListOperationAdapter;
}

namespace QindaQt::ShellTaskListApplet {

// Thin forwarding seam over the T1 Compositor1 operation adapter so the
// applet controller depends on a narrow port instead of the concrete
// transport-owning adapter. All admission fencing, serialization, and
// exactly-once reply lineage stay in the adapter (docs/wiki/shell/task-list.md,
// "Operation adapter"); this bridge adds no policy of its own.
//
// AGENT-CONTRACT: the borrowed adapter must outlive the bridge, and both are
// confined to the composing (GUI) thread. The bridge never creates a bus
// connection; transport wiring belongs to the shell composition lane.
class TaskListAppletOperationBridge final : public TaskListAppletOperationPort {
  Q_OBJECT

public:
  explicit TaskListAppletOperationBridge(
      ShellTaskList::Operations::TaskListOperationAdapter &adapter,
      QObject *parent = nullptr);

  quint64 executeTaskIntent(
      const ShellTaskList::TaskIntentRequest &request,
      const ShellTaskList::TaskIntentOutcome &outcome) override;
  quint64 activateContainerPage(const QString &containerId,
                                const QString &pageId,
                                quint64 expectedRevision) override;
  quint64 detachWindow(const QString &containerId, const QString &windowId,
                       quint64 expectedRevision) override;
  quint64 releaseContainer(const QString &containerId,
                           quint64 expectedRevision) override;
  quint64 dockWindows(const QString &targetWindowId,
                      const QString &incomingWindowId,
                      const QString &orientation, const QString &position,
                      double ratio, quint64 expectedRevision) override;

private:
  ShellTaskList::Operations::TaskListOperationAdapter &m_adapter;
};

} // namespace QindaQt::ShellTaskListApplet
