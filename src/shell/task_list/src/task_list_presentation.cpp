// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/task_list_presentation.h"

namespace QindaQt::ShellTaskList {

QString
TaskListPresentationModel::accessibleName(const TaskEntry &entry) {
  QString name = entry.title.isEmpty()
                     ? entry.applicationName
                     : entry.applicationName + QStringLiteral(" — ") +
                           entry.title;
  if (entry.kind == TaskEntryKind::Container && entry.windowCount != 1) {
    name += QStringLiteral(", %1 windows").arg(entry.windowCount);
  }
  if (entry.active) {
    name += QStringLiteral(", active");
  }
  if (entry.minimized) {
    name += QStringLiteral(", minimized");
  }
  if (entry.urgent) {
    name += QStringLiteral(", urgent");
  }
  return name;
}

TaskListPresentation TaskListPresentationModel::project(
    TaskListSourceStatus status, const TaskGeneration &generation,
    const TaskListScope &scope) {
  TaskListPresentation presentation;
  if (status == TaskListSourceStatus::Loading) {
    presentation.state = TaskListState::Loading;
    return presentation;
  }

  presentation.entries = TaskListFilter::filter(generation.entries, scope);
  presentation.state = presentation.entries.isEmpty()
                           ? TaskListState::Empty
                           : TaskListState::Ready;
  if (status == TaskListSourceStatus::Degraded &&
      !presentation.entries.isEmpty()) {
    presentation.state = TaskListState::Degraded;
  }

  presentation.identities.reserve(presentation.entries.size());
  int keyboardIndex = 1;
  for (const TaskEntry &entry : presentation.entries) {
    TaskEntryIdentity identity;
    identity.taskId = entry.taskId;
    identity.keyboardIndex = keyboardIndex++;
    identity.accessibleName = accessibleName(entry);
    presentation.identities.append(identity);
  }
  return presentation;
}

} // namespace QindaQt::ShellTaskList
