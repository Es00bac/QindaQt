// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/task_list_types.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace QindaQt::ShellTaskListApplet {

// AGENT-CONTRACT: the panel strip presents at most this many rows. The
// compositor-bound fact ceiling (kMaxWindowFacts) is far larger than a usable
// strip, so the controller caps presentation here and reports the exact
// overflow count; the cap must never silently drop a row without that truth.
inline constexpr int kMaxPresentedTaskEntries = 64;

// Presentation phases. Degraded keeps the retained generation visible but
// refuses every intent; Unavailable means observation itself is withheld
// (read capability denied). Cold start is Loading; the T1 producer degrades
// explicitly once owner discovery resolves, so Loading cannot persist
// silently (docs/wiki/shell/task-list.md).
enum class TaskListAppletPhase {
  Loading,
  Ready,
  Empty,
  Degraded,
  Unavailable,
};

// Least-authority manifest/policy grants evaluated by the shell and injected
// at construction; immutable afterwards. Read gates observation, activate
// gates activation intents, manage gates minimize/close and every
// container/dock operation.
struct TaskListAppletGrants {
  bool windowsRead = false;
  bool windowsActivate = false;
  bool windowsManage = false;

  friend bool operator==(const TaskListAppletGrants &,
                         const TaskListAppletGrants &) = default;
};

// One presented strip row. generationRevision is the source revision the row
// was projected from; every intent echoes it so the T0 stale-id arbitration
// can refuse actions against a generation the user no longer sees.
struct TaskListAppletRow {
  QString taskId;
  ShellTaskList::TaskEntryKind kind = ShellTaskList::TaskEntryKind::Window;
  QString title;
  QString applicationId;
  QString applicationName;
  // Typed icon placeholder: a deterministic one-letter badge derived from the
  // application identity. No freedesktop/QIcon seam exists in this tree yet;
  // the placeholder keeps the row shape stable until one lands.
  QString iconText;
  quint32 windowCount = 1;
  bool active = false;
  bool minimized = false;
  bool urgent = false;
  int keyboardIndex = 0;
  QString accessibleName;
  quint64 generationRevision = 0;
  bool pending = false;
  // Sorted member windows for container rows; empty for standalone windows.
  QStringList memberWindowIds;

  friend bool operator==(const TaskListAppletRow &,
                         const TaskListAppletRow &) = default;
};

struct TaskListAppletProjection {
  TaskListAppletPhase phase = TaskListAppletPhase::Loading;
  // Stable machine reason for non-ready phases; empty while ready.
  QString phaseReason;
  QVector<TaskListAppletRow> rows;
  // Entries visible in scope before the presentation cap.
  int totalCount = 0;
  // totalCount minus presented rows; zero means no hidden entries.
  int overflowCount = 0;
};

[[nodiscard]] QString taskListAppletPhaseText(TaskListAppletPhase phase);

} // namespace QindaQt::ShellTaskListApplet
