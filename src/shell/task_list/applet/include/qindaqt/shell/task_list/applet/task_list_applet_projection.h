// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/applet/task_list_applet_types.h"
#include "qindaqt/shell/task_list/task_list_presentation.h"

#include <QSet>

namespace QindaQt::ShellTaskListApplet {

// Pure, reentrant mapping of one T0 presentation (status + generation +
// scope already resolved) plus the controller's pending-intent set into the
// bounded strip projection. Degraded presentations keep their retained rows
// visible but arrive with an empty pending set and disabled actions; the
// controller enforces that no intent dispatches while the source is degraded.
//
// AGENT-CONTRACT: row order is exactly the T0 canonical order (also the
// keyboard traversal order). The cap truncates the tail only; overflowCount
// must always equal totalCount - rows.size() so the strip's overflow truth
// cannot drift from the actual projection.
class TaskListAppletProjectionModel final {
public:
  TaskListAppletProjectionModel() = delete;

  [[nodiscard]] static TaskListAppletProjection
  project(const ShellTaskList::TaskListPresentation &presentation,
          const QSet<QString> &pendingTaskIds, bool windowsReadGranted,
          quint64 generationRevision);

  // Deterministic icon placeholder: the first alphanumeric character of the
  // application name (then id) uppercased, or "?". Never derived from the
  // window title, which is hostile-controlled presentation text.
  [[nodiscard]] static QString iconPlaceholder(const QString &applicationName,
                                               const QString &applicationId);
};

} // namespace QindaQt::ShellTaskListApplet
