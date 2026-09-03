// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/task_list_applet_operation_bridge.h"

#include "qindaqt/shell/task_list/operations/task_list_operation_adapter.h"

namespace QindaQt::ShellTaskListApplet {

TaskListAppletOperationBridge::TaskListAppletOperationBridge(
    ShellTaskList::Operations::TaskListOperationAdapter &adapter,
    QObject *parent)
    : TaskListAppletOperationPort(parent), m_adapter(adapter) {
  connect(&m_adapter,
          &ShellTaskList::Operations::TaskListOperationAdapter::operationFinished,
          this, &TaskListAppletOperationBridge::operationFinished);
}

quint64 TaskListAppletOperationBridge::executeTaskIntent(
    const ShellTaskList::TaskIntentRequest &request,
    const ShellTaskList::TaskIntentOutcome &outcome) {
  return m_adapter.executeTaskIntent(request, outcome);
}

quint64 TaskListAppletOperationBridge::activateContainerPage(
    const QString &containerId, const QString &pageId,
    quint64 expectedRevision) {
  return m_adapter.activateContainerPage(containerId, pageId, expectedRevision);
}

quint64 TaskListAppletOperationBridge::detachWindow(const QString &containerId,
                                                    const QString &windowId,
                                                    quint64 expectedRevision) {
  return m_adapter.detachWindow(containerId, windowId, expectedRevision);
}

quint64 TaskListAppletOperationBridge::releaseContainer(
    const QString &containerId, quint64 expectedRevision) {
  return m_adapter.releaseContainer(containerId, expectedRevision);
}

quint64 TaskListAppletOperationBridge::dockWindows(
    const QString &targetWindowId, const QString &incomingWindowId,
    const QString &orientation, const QString &position, double ratio,
    quint64 expectedRevision) {
  return m_adapter.dockWindows(targetWindowId, incomingWindowId, orientation,
                               position, ratio, expectedRevision);
}

} // namespace QindaQt::ShellTaskListApplet
