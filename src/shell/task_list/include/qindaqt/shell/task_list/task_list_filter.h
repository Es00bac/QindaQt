// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/task_list_types.h"

namespace QindaQt::ShellTaskList {

// Pure per-output/workspace filtering policy. An empty scope field means "no
// restriction on that axis", so a default scope admits every entry. Container
// entries are placed by their primary member's output and workspace scope.
struct TaskListScope {
  QString outputId;
  QString workspaceId;

  friend bool operator==(const TaskListScope &, const TaskListScope &) = default;
};

class TaskListFilter final {
public:
  TaskListFilter() = delete;

  // AGENT-CONTRACT: Evaluation is pure and total; it never consults platform
  // state, so producers and tests may rely on identical results per value.
  [[nodiscard]] static bool isVisible(const TaskEntry &entry,
                                      const TaskListScope &scope);
  [[nodiscard]] static QVector<TaskEntry>
  filter(const QVector<TaskEntry> &entries, const TaskListScope &scope);
};

} // namespace QindaQt::ShellTaskList
