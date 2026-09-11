// SPDX-License-Identifier: LGPL-3.0-or-later
#include "task_list_grouping_p.h"

#include <QHash>
#include <algorithm>

namespace QindaQt::ShellTaskList::Grouping {
namespace {

// Canonical entry order: grouped by application, standalone windows before
// collapsed containers, then by stable task identity. This comparator is the
// single source of ordering truth for rows, keyboard traversal, and tests.
bool entryLessThan(const TaskEntry &left, const TaskEntry &right) {
  if (left.applicationId != right.applicationId) {
    return left.applicationId < right.applicationId;
  }
  if (left.kind != right.kind) {
    return left.kind == TaskEntryKind::Window;
  }
  return left.taskId < right.taskId;
}

TaskContainerMember memberOf(const TaskWindowFact &fact) {
  TaskContainerMember member;
  member.windowId = fact.windowId;
  member.applicationId = fact.applicationId;
  member.applicationName = fact.applicationName;
  member.title = fact.title;
  member.urgent = fact.urgent;
  return member;
}

} // namespace

QVector<TaskEntry> canonicalEntries(const QVector<TaskWindowFact> &facts) {
  // AGENT-NOTE: Sorting prebuilt vectors keeps the canonical order and
  // grouping independent of the producer's fact order; never replace this
  // with hash-map iteration, which would make keyboard order vary.
  QVector<TaskEntry> standalone;
  QVector<TaskEntry> containers;
  standalone.reserve(facts.size());
  containers.reserve(facts.size());
  QHash<QString, int> containerIndex;

  for (const TaskWindowFact &fact : facts) {
    if (fact.role == TaskWindowRole::ContainerMember) {
      continue;
    }
    TaskEntry entry;
    entry.applicationId = fact.applicationId;
    entry.applicationName = fact.applicationName;
    entry.title = fact.title;
    entry.colorHex = fact.colorHex;
    entry.primaryWindowId = fact.windowId;
    entry.outputId = fact.outputId;
    entry.workspaceIds = fact.workspaceIds;
    entry.onAllWorkspaces = fact.onAllWorkspaces;
    entry.type = fact.type;
    entry.owner = fact.owner;
    entry.active = fact.active;
    entry.minimized = fact.minimized;
    entry.urgent = fact.urgent;
    if (fact.role == TaskWindowRole::ContainerPrimary) {
      entry.taskId = fact.containerId;
      entry.kind = TaskEntryKind::Container;
      // AGENT-GUARD: The representative primary window is part of the collapsed
      // container; adapters receive every member (including the primary) so
      // container close/minimize policy stays deterministic.
      entry.memberWindowIds = {fact.windowId};
      entry.members = {memberOf(fact)};
      containerIndex.insert(entry.taskId,
                            static_cast<int>(containers.size()));
      containers.append(entry);
    } else {
      entry.taskId = fact.windowId;
      entry.kind = TaskEntryKind::Window;
      entry.memberWindowIds = {fact.windowId};
      standalone.append(entry);
    }
  }

  for (const TaskWindowFact &fact : facts) {
    if (fact.role != TaskWindowRole::ContainerMember) {
      continue;
    }
    TaskEntry &entry = containers[containerIndex.value(fact.containerId)];
    entry.memberWindowIds.append(fact.windowId);
    entry.members.append(memberOf(fact));
    entry.urgent = entry.urgent || fact.urgent;
  }

  for (TaskEntry &entry : containers) {
    std::sort(entry.memberWindowIds.begin(), entry.memberWindowIds.end());
    // Same key and order as memberWindowIds, so members[i] describes
    // memberWindowIds[i] regardless of the producer's fact order.
    std::sort(entry.members.begin(), entry.members.end(),
              [](const TaskContainerMember &left,
                 const TaskContainerMember &right) {
                return left.windowId < right.windowId;
              });
    entry.windowCount = quint32(entry.memberWindowIds.size());
  }

  QVector<TaskEntry> entries;
  entries.reserve(standalone.size() + containers.size());
  entries.append(std::move(standalone));
  entries.append(std::move(containers));
  std::sort(entries.begin(), entries.end(), entryLessThan);
  return entries;
}

} // namespace QindaQt::ShellTaskList::Grouping
