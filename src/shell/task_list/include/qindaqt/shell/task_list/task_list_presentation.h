// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/task_list_filter.h"
#include "qindaqt/shell/task_list/task_list_types.h"

namespace QindaQt::ShellTaskList {

enum class TaskListState {
  // No generation has been accepted yet; presentation shows its loading row.
  Loading,
  // A generation is present but the filtered selection is empty.
  Empty,
  Ready,
  Degraded,
};

// Deterministic keyboard and accessibility identity for one presented entry.
// keyboardIndex is 1-based and follows the canonical filtered order, so a
// keyboard shortcut scheme can stay stable across identical generations.
struct TaskEntryIdentity {
  QString taskId;
  int keyboardIndex = 0;
  QString accessibleName;

  friend bool operator==(const TaskEntryIdentity &,
                         const TaskEntryIdentity &) = default;
};

struct TaskListPresentation {
  TaskListState state = TaskListState::Loading;
  QVector<TaskEntry> entries;
  // Parallel to entries; identities[i] describes entries[i].
  QVector<TaskEntryIdentity> identities;
};

class TaskListPresentationModel final {
public:
  TaskListPresentationModel() = delete;

  // Projects one source status/generation through the scope filter. Loading
  // ignores the generation; Degraded keeps the retained generation visible.
  [[nodiscard]] static TaskListPresentation
  project(TaskListSourceStatus status, const TaskGeneration &generation,
          const TaskListScope &scope);

  // AGENT-NOTE: The composed name is deliberately deterministic English
  // ("App — Title, 3 windows, minimized") so tests and keyboard users get a
  // stable announcement. A future QML layer may re-render it, but must not
  // reorder the state suffixes or drop the window count.
  [[nodiscard]] static QString accessibleName(const TaskEntry &entry);
};

} // namespace QindaQt::ShellTaskList
