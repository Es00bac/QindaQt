// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/producer/task_list_fact_joiner.h"

#include <QHash>

namespace QindaQt::ShellTaskList::Producer {
namespace {

TaskListJoinResult failure(TaskListJoinError code, QString windowId,
                           QString containerId, QString message) {
  TaskListJoinResult result;
  result.error.code = code;
  result.error.windowId = std::move(windowId);
  result.error.containerId = std::move(containerId);
  result.error.message = std::move(message);
  return result;
}

void applyScope(const TaskListWireScope &scope, TaskWindowFact *fact) {
  fact->outputId = scope.outputId;
  fact->workspaceIds = scope.workspaceIds;
  fact->onAllWorkspaces = scope.onAllWorkspaces;
}

} // namespace

TaskListJoinResult TaskListFactJoiner::join(
    const QVector<TaskListWireWindow> &windows,
    const QVector<TaskListWireContainer> &containers,
    const TaskListWireScopeSnapshot &scope) {
  QHash<QString, const TaskListWireScope *> scopeByWindow;
  scopeByWindow.reserve(scope.scopes.size());
  for (const TaskListWireScope &entry : scope.scopes) {
    scopeByWindow.insert(entry.windowId, &entry);
  }
  QHash<QString, const TaskListWireContainer *> containerById;
  containerById.reserve(containers.size());
  for (const TaskListWireContainer &entry : containers) {
    containerById.insert(entry.containerId, &entry);
  }

  // Group member indices per container and classify each container's primary
  // once. The primary is the single member carrying the compositor's
  // collapsed native identity (skipTaskbar == false).
  QHash<QString, QVector<qsizetype>> membersByContainer;
  QHash<QString, qsizetype> primaryByContainer;
  for (qsizetype index = 0; index < windows.size(); ++index) {
    const TaskListWireWindow &window = windows.at(index);
    if (window.containerId.isEmpty()) {
      continue;
    }
    if (!containerById.contains(window.containerId)) {
      return failure(TaskListJoinError::UnknownContainerOwner, window.windowId,
                     window.containerId,
                     QStringLiteral("window names an unknown container"));
    }
    membersByContainer[window.containerId].append(index);
    if (!window.skipTaskbar) {
      if (primaryByContainer.contains(window.containerId)) {
        return failure(TaskListJoinError::TaskIdentityAmbiguous,
                       window.windowId, window.containerId,
                       QStringLiteral("container has two native identities"));
      }
      primaryByContainer.insert(window.containerId, index);
    }
  }
  for (const TaskListWireContainer &container : containers) {
    if (!membersByContainer.contains(container.containerId)) {
      return failure(TaskListJoinError::EmptyContainerOnWire, {},
                     container.containerId,
                     QStringLiteral("container has no published member window"));
    }
    if (!primaryByContainer.contains(container.containerId)) {
      return failure(TaskListJoinError::TaskIdentityMissing, {},
                     container.containerId,
                     QStringLiteral("container has no native identity member"));
    }
  }

  TaskListJoinResult result;
  result.containers.reserve(containers.size());
  for (const TaskListWireContainer &container : containers) {
    result.containers.append({container.containerId, container.revision,
                              container.authority});
  }
  result.facts.reserve(windows.size());

  for (qsizetype index = 0; index < windows.size(); ++index) {
    const TaskListWireWindow &window = windows.at(index);
    if (window.containerId.isEmpty() && window.skipTaskbar) {
      // A standalone skip-taskbar window opted out of task lists; it is not a
      // task row and produces no fact at all.
      continue;
    }
    const TaskListWireScope *const windowScope =
        scopeByWindow.value(window.windowId, nullptr);
    if (windowScope == nullptr) {
      return failure(TaskListJoinError::ScopeUnavailable, window.windowId, {},
                     QStringLiteral("window has no scope snapshot entry"));
    }

    TaskWindowFact fact;
    fact.windowId = window.windowId;
    fact.applicationId = window.applicationId;
    // AGENT-NOTE: Compositor1 1.1 exposes no application display name; the
    // application id is the truthful fallback until a desktop-entry lookup
    // lands in a later composition lane.
    fact.applicationName = window.applicationId;
    fact.title = window.title;
    applyScope(*windowScope, &fact);

    if (window.containerId.isEmpty()) {
      fact.role = TaskWindowRole::Standalone;
      fact.active = window.active;
      fact.minimized = window.minimized;
      result.facts.append(std::move(fact));
      continue;
    }

    // The primary carries activation/minimized truth; suppressed members carry
    // nothing (urgent is not exposed by Compositor1 1.1). Member scope follows
    // the primary's placement, which describes the whole container.
    fact.containerId = window.containerId;
    if (primaryByContainer.value(window.containerId) == index) {
      fact.role = TaskWindowRole::ContainerPrimary;
      fact.active = window.active;
      fact.minimized = window.minimized;
    } else {
      fact.role = TaskWindowRole::ContainerMember;
      const TaskListWireScope *const primaryScope = scopeByWindow.value(
          windows.at(primaryByContainer.value(window.containerId)).windowId,
          nullptr);
      if (primaryScope != nullptr) {
        applyScope(*primaryScope, &fact);
      }
    }
    result.facts.append(std::move(fact));
  }
  return result;
}

} // namespace QindaQt::ShellTaskList::Producer
