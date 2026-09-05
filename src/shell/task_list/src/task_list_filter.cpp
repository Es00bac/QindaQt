// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/task_list_filter.h"

namespace QindaQt::ShellTaskList {

bool TaskListFilter::isVisible(const TaskEntry &entry,
                               const TaskListScope &scope) {
  // AGENT-CONTRACT: the compositor publishes role/owner provenance for every
  // managed window. T0 is the final scope authority and must never project
  // shell-owned surfaces or non-normal roles as user tasks.
  if (entry.type != TaskWindowType::Normal ||
      entry.owner != TaskWindowOwner::Application) {
    return false;
  }
  if (!scope.outputId.isEmpty() && entry.outputId != scope.outputId) {
    return false;
  }
  if (!scope.workspaceId.isEmpty() && !entry.onAllWorkspaces &&
      !entry.workspaceIds.contains(scope.workspaceId)) {
    return false;
  }
  return true;
}

QVector<TaskEntry> TaskListFilter::filter(const QVector<TaskEntry> &entries,
                                          const TaskListScope &scope) {
  QVector<TaskEntry> visible;
  visible.reserve(entries.size());
  for (const TaskEntry &entry : entries) {
    if (isVisible(entry, scope)) {
      visible.append(entry);
    }
  }
  return visible;
}

} // namespace QindaQt::ShellTaskList
