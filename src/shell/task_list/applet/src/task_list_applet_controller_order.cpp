// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"

#include "qindaqt/shell/task_list/task_list_order.h"

namespace QindaQt::ShellTaskListApplet {

namespace {

// AGENT-CONTRACT: the displayed order — the one the strip presents and
// keyboard traversal follows — is the canonical order with the stored user
// overlay applied. Every reorder operation is computed against this displayed
// order, so what the user sees is exactly what gets reordered.
QStringList displayedOrder(const TaskListAppletProjection &projection) {
  QStringList ids;
  ids.reserve(projection.rows.size());
  for (const TaskListAppletRow &row : projection.rows) {
    ids.append(row.taskId);
  }
  return ids;
}

} // namespace

QStringList TaskListAppletController::userTaskOrder() const {
  return m_userTaskOrder;
}

void TaskListAppletController::setUserTaskOrder(const QStringList &order) {
  const QStringList normalized = ShellTaskList::TaskListOrder::normalize(order);
  if (normalized == m_userTaskOrder) {
    // AGENT-GUARD: settings echoes must be exact no-ops. The runtime writes
    // this value after every taskOrderCommitted and observes its own snapshot
    // again; treating an equal order as fresh state would loop the write path.
    return;
  }
  m_userTaskOrder = normalized;
  reproject();
  Q_EMIT userTaskOrderChanged();
}

bool TaskListAppletController::reorderTask(const QString &movedTaskId,
                                           const QString &beforeTaskId,
                                           quint64 revision) {
  if (!m_grants.windowsRead) {
    return refuse(QStringLiteral("Reorder: the windows.read capability was "
                                 "not granted to this applet"));
  }
  if (m_projection.phase != TaskListAppletPhase::Ready) {
    return refuse(QStringLiteral(
        "Reorder: the task list is not ready; actions stay refused until a "
        "fresh generation arrives"));
  }
  if (revision != m_source.revision()) {
    return refuse(QStringLiteral(
        "Reorder: the task list changed before the gesture was applied"));
  }
  const QStringList displayed = displayedOrder(m_projection);
  if (!displayed.contains(movedTaskId)) {
    return refuse(QStringLiteral("Reorder: the window is no longer listed"));
  }
  if (movedTaskId == beforeTaskId) {
    return refuse(QStringLiteral("Reorder: a task cannot precede itself"));
  }
  if (!beforeTaskId.isEmpty() && !displayed.contains(beforeTaskId)) {
    return refuse(QStringLiteral("Reorder: the drop target is no longer "
                                 "listed"));
  }

  // Compute the persisted order against the displayed slice, preserving
  // stored ids that are filtered out of the current scope (other
  // output/workspace): they ride at the tail in their stored order, so a
  // returned window keeps a deterministic position.
  QStringList hiddenIds;
  hiddenIds.reserve(m_userTaskOrder.size());
  for (const QString &id : m_userTaskOrder) {
    if (id != movedTaskId && !displayed.contains(id)) {
      hiddenIds.append(id);
    }
  }
  QStringList updated = displayed;
  updated.removeAll(movedTaskId);
  if (beforeTaskId.isEmpty()) {
    updated.append(movedTaskId);
  } else {
    updated.insert(updated.indexOf(beforeTaskId), movedTaskId);
  }
  updated.append(hiddenIds);

  m_userTaskOrder = ShellTaskList::TaskListOrder::normalize(updated);
  reproject();
  Q_EMIT userTaskOrderChanged();
  Q_EMIT taskOrderCommitted(m_userTaskOrder);
  return true;
}

} // namespace QindaQt::ShellTaskListApplet
