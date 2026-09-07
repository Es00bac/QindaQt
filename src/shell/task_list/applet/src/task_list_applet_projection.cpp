// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/task_list_applet_projection.h"

namespace QindaQt::ShellTaskListApplet {

namespace {

using ShellTaskList::TaskListState;

QString placeholderFrom(const QString &identity) {
  for (const QChar &character : identity) {
    if (character.isLetterOrNumber()) {
      return character.toUpper();
    }
  }
  return {};
}

} // namespace

TaskListAppletProjection TaskListAppletProjectionModel::project(
    const ShellTaskList::TaskListPresentation &presentation,
    const QSet<QString> &pendingTaskIds, bool windowsReadGranted,
    quint64 generationRevision) {
  TaskListAppletProjection projection;
  if (!windowsReadGranted) {
    // AGENT-GUARD: read denial withholds observation entirely — no row data,
    // not even the retained generation, may reach presentation state.
    projection.phase = TaskListAppletPhase::Unavailable;
    projection.phaseReason = QStringLiteral("windows-read-not-granted");
    return projection;
  }

  switch (presentation.state) {
  case TaskListState::Loading:
    projection.phase = TaskListAppletPhase::Loading;
    projection.phaseReason = QStringLiteral("compositor-task-list-loading");
    return projection;
  case TaskListState::Empty:
    projection.phase = TaskListAppletPhase::Empty;
    projection.phaseReason = QStringLiteral("no-windows-in-scope");
    return projection;
  case TaskListState::Degraded:
    projection.phase = TaskListAppletPhase::Degraded;
    projection.phaseReason =
        QStringLiteral("compositor-task-list-unavailable");
    break;
  case TaskListState::Ready:
    projection.phase = TaskListAppletPhase::Ready;
    break;
  }

  const auto &entries = presentation.entries;
  projection.totalCount = static_cast<int>(entries.size());
  const int presented =
      qMin(static_cast<int>(entries.size()), kMaxPresentedTaskEntries);
  projection.rows.reserve(presented);
  for (int index = 0; index < presented; ++index) {
    const ShellTaskList::TaskEntry &entry = entries.at(index);
    const ShellTaskList::TaskEntryIdentity &identity =
        presentation.identities.at(index);
    TaskListAppletRow row;
    row.taskId = entry.taskId;
    row.kind = entry.kind;
    row.title = entry.title;
    row.applicationId = entry.applicationId;
    row.applicationName = entry.applicationName;
    row.iconText = iconPlaceholder(entry.applicationName, entry.applicationId);
    row.colorHex = entry.colorHex;
    row.windowCount = entry.windowCount;
    row.active = entry.active;
    row.minimized = entry.minimized;
    row.urgent = entry.urgent;
    row.keyboardIndex = identity.keyboardIndex;
    row.accessibleName = identity.accessibleName;
    row.memberWindowIds = entry.memberWindowIds;
    row.generationRevision = generationRevision;
    row.pending = pendingTaskIds.contains(entry.taskId);
    projection.rows.append(std::move(row));
  }
  projection.overflowCount = projection.totalCount - presented;
  return projection;
}

QString TaskListAppletProjectionModel::iconPlaceholder(
    const QString &applicationName, const QString &applicationId) {
  const QString fromName = placeholderFrom(applicationName);
  if (!fromName.isEmpty()) {
    return fromName;
  }
  const QString fromId = placeholderFrom(applicationId);
  return fromId.isEmpty() ? QStringLiteral("?") : fromId;
}

} // namespace QindaQt::ShellTaskListApplet
