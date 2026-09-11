// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/task_list_applet_projection.h"

#include <algorithm>

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

TaskListAppletRow entryRow(const ShellTaskList::TaskEntry &entry,
                           const QSet<QString> &pendingTaskIds,
                           quint64 generationRevision) {
  TaskListAppletRow row;
  row.taskId = entry.taskId;
  row.kind = entry.kind;
  row.title = entry.title;
  row.applicationId = entry.applicationId;
  row.applicationName = entry.applicationName;
  row.iconText = TaskListAppletProjectionModel::iconPlaceholder(
      entry.applicationName, entry.applicationId);
  row.colorHex = entry.colorHex;
  row.windowCount = entry.windowCount;
  row.active = entry.active;
  row.minimized = entry.minimized;
  row.urgent = entry.urgent;
  row.memberWindowIds = entry.memberWindowIds;
  row.generationRevision = generationRevision;
  row.pending = pendingTaskIds.contains(entry.taskId);
  return row;
}

// One ungrouped row per container member. The container keeps the
// arbitration identity (taskId, pending marker, minimized state); the member
// supplies its own application identity, title, and urgency, and is active
// only while it is the container's visible primary.
TaskListAppletRow memberRow(const ShellTaskList::TaskEntry &container,
                            const ShellTaskList::TaskContainerMember &member,
                            const QSet<QString> &pendingTaskIds,
                            quint64 generationRevision) {
  ShellTaskList::TaskEntry window;
  window.taskId = container.taskId;
  window.kind = ShellTaskList::TaskEntryKind::Window;
  window.applicationId = member.applicationId;
  window.applicationName = member.applicationName;
  window.title = member.title;
  window.primaryWindowId = member.windowId;
  window.memberWindowIds = {member.windowId};
  window.active =
      container.active && member.windowId == container.primaryWindowId;
  window.minimized = container.minimized;
  window.urgent = member.urgent;
  TaskListAppletRow row = entryRow(window, pendingTaskIds, generationRevision);
  row.windowId = member.windowId;
  row.accessibleName =
      ShellTaskList::TaskListPresentationModel::accessibleName(window);
  return row;
}

} // namespace

TaskListAppletProjection TaskListAppletProjectionModel::project(
    const ShellTaskList::TaskListPresentation &presentation,
    const QSet<QString> &pendingTaskIds, bool windowsReadGranted,
    quint64 generationRevision, int maxPresentedEntries) {
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
  // AGENT-GUARD: the bound must stay within the fact ceiling even for a
  // hostile caller; 1 is the floor so overflow truth can never go negative.
  const int presentedLimit =
      std::clamp(maxPresentedEntries, 1, kMaxPresentedDockEntries);
  const int presented = qMin(static_cast<int>(entries.size()), presentedLimit);
  projection.rows.reserve(presented);
  for (int index = 0; index < presented; ++index) {
    const ShellTaskList::TaskEntryIdentity &identity =
        presentation.identities.at(index);
    TaskListAppletRow row =
        entryRow(entries.at(index), pendingTaskIds, generationRevision);
    row.keyboardIndex = identity.keyboardIndex;
    row.accessibleName = identity.accessibleName;
    projection.rows.append(std::move(row));
  }
  projection.overflowCount = projection.totalCount - presented;

  // AGENT-CONTRACT: the ungrouped rows expand every entry in scope, not only
  // the presented head, so the window bound and windowOverflowCount stay
  // exact on their own. Keyboard indices follow the expanded order. A
  // container without member identities (canonicalEntries never builds one)
  // keeps its collapsed row instead of silently vanishing.
  for (int index = 0; index < entries.size(); ++index) {
    const ShellTaskList::TaskEntry &entry = entries.at(index);
    if (entry.kind != ShellTaskList::TaskEntryKind::Container ||
        entry.members.isEmpty()) {
      ++projection.totalWindowCount;
      if (projection.windowRows.size() < presentedLimit) {
        TaskListAppletRow row =
            entryRow(entry, pendingTaskIds, generationRevision);
        row.accessibleName = presentation.identities.at(index).accessibleName;
        row.keyboardIndex = static_cast<int>(projection.windowRows.size()) + 1;
        projection.windowRows.append(std::move(row));
      }
      continue;
    }
    for (const ShellTaskList::TaskContainerMember &member : entry.members) {
      ++projection.totalWindowCount;
      if (projection.windowRows.size() < presentedLimit) {
        TaskListAppletRow row =
            memberRow(entry, member, pendingTaskIds, generationRevision);
        row.keyboardIndex = static_cast<int>(projection.windowRows.size()) + 1;
        projection.windowRows.append(std::move(row));
      }
    }
  }
  projection.windowOverflowCount =
      projection.totalWindowCount -
      static_cast<int>(projection.windowRows.size());
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
