// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/task_list_types.h"

namespace QindaQt::ShellTaskList {

// A user-facing task-list request. The source records intent only; wiring the
// accepted outcome to real window operations is a separate shell adapter that
// is intentionally outside this module.
enum class TaskIntentKind {
  Activate,
  Minimize,
  Close,
};

struct TaskIntentRequest {
  // The task-list identity the user acted on, plus the generation revision
  // the caller was displaying. Mismatching the source's current revision is
  // rejected as stale so an adapter can never act on a window the user can
  // no longer see in this list.
  QString taskId;
  TaskIntentKind kind = TaskIntentKind::Activate;
  quint64 expectedRevision = 0;

  friend bool operator==(const TaskIntentRequest &,
                         const TaskIntentRequest &) = default;
};

enum class TaskIntentErrorCode {
  None,
  InvalidRequest,
  NoGeneration,
  SourceDegraded,
  StaleRevision,
  UnknownTask,
};

struct TaskIntentOutcome {
  TaskIntentErrorCode code = TaskIntentErrorCode::None;
  // Filled only when code is None: the resolved entry plus the deterministic
  // window targets. Activate targets the primary window; Close/Minimize
  // adapters receive every member so container policy stays with the adapter.
  TaskEntryKind entryKind = TaskEntryKind::Window;
  QString primaryWindowId;
  QStringList memberWindowIds;

  [[nodiscard]] bool ok() const noexcept {
    return code == TaskIntentErrorCode::None;
  }

  friend bool operator==(const TaskIntentOutcome &,
                         const TaskIntentOutcome &) = default;
};

} // namespace QindaQt::ShellTaskList
