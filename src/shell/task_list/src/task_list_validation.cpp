// SPDX-License-Identifier: LGPL-3.0-or-later
#include "task_list_validation_p.h"

namespace QindaQt::ShellTaskList::Validation {
namespace {

TaskListError makeError(TaskListErrorCode code, const TaskWindowFact &fact,
                        const QString &message) {
  TaskListError error;
  error.code = code;
  error.windowId = fact.windowId;
  error.containerId = fact.containerId;
  error.message = message;
  return error;
}

bool isGrouped(TaskWindowRole role) {
  return role == TaskWindowRole::ContainerPrimary ||
         role == TaskWindowRole::ContainerMember;
}

TaskListError checkShape(const TaskWindowFact &fact) {
  if (!isValidIdentifier(fact.windowId) || !isValidIdentifier(fact.applicationId) ||
      !isValidIdentifier(fact.applicationName)) {
    return makeError(TaskListErrorCode::InvalidIdentity, fact,
                     QStringLiteral("window, application id, and application "
                                    "name must be non-empty"));
  }
  if (fact.title.size() > kMaxIdLength) {
    return makeError(TaskListErrorCode::LimitExceeded, fact,
                     QStringLiteral("title exceeds the identity length bound"));
  }
  if (fact.outputId.isEmpty()) {
    return makeError(TaskListErrorCode::InvalidIdentity, fact,
                     QStringLiteral("compositor output id is required"));
  }

  if (fact.onAllWorkspaces) {
    if (!fact.workspaceIds.isEmpty()) {
      return makeError(TaskListErrorCode::InvalidWorkspaceScope, fact,
                       QStringLiteral("an all-workspaces window carries no "
                                      "workspace id list"));
    }
  } else if (fact.workspaceIds.isEmpty()) {
    return makeError(TaskListErrorCode::InvalidWorkspaceScope, fact,
                     QStringLiteral("a window needs a workspace or "
                                    "all-workspaces scope"));
  } else {
    if (fact.workspaceIds.size() > kMaxWorkspacesPerWindow) {
      return makeError(TaskListErrorCode::LimitExceeded, fact,
                       QStringLiteral("workspace list exceeds the bound"));
    }
    QSet<QString> seenWorkspaces;
    for (const QString &workspaceId : fact.workspaceIds) {
      if (!isValidIdentifier(workspaceId)) {
        return makeError(TaskListErrorCode::InvalidWorkspaceScope, fact,
                         QStringLiteral("workspace ids must be non-empty"));
      }
      if (seenWorkspaces.contains(workspaceId)) {
        return makeError(TaskListErrorCode::InvalidWorkspaceScope, fact,
                         QStringLiteral("duplicate workspace id"));
      }
      seenWorkspaces.insert(workspaceId);
    }
  }

  if (isGrouped(fact.role) != !fact.containerId.isEmpty()) {
    return makeError(TaskListErrorCode::ConflictingContainerRole, fact,
                     QStringLiteral("container id presence must match the "
                                    "grouped role"));
  }
  if (isGrouped(fact.role) && !isValidIdentifier(fact.containerId)) {
    return makeError(TaskListErrorCode::InvalidIdentity, fact,
                     QStringLiteral("container id must be non-empty"));
  }

  if (fact.active && fact.minimized) {
    return makeError(TaskListErrorCode::ActiveMinimizedConflict, fact,
                     QStringLiteral("an active window cannot be minimized"));
  }
  if (fact.role == TaskWindowRole::ContainerMember &&
      (fact.active || fact.minimized)) {
    return makeError(TaskListErrorCode::InvalidMemberState, fact,
                     QStringLiteral("suppressed members carry state only "
                                    "through urgent"));
  }
  return TaskListError{};
}

} // namespace

bool isValidIdentifier(const QString &value) {
  return !value.isEmpty() && value.size() <= kMaxIdLength;
}

TaskListError validateBatch(const QVector<TaskWindowFact> &facts) {
  if (facts.size() > int(kMaxWindowFacts)) {
    TaskWindowFact none;
    return makeError(TaskListErrorCode::LimitExceeded, none,
                     QStringLiteral("generation exceeds the window-fact "
                                    "bound"));
  }

  QSet<QString> windowIds;
  QSet<QString> containerPrimaries;
  int activeCount = 0;

  for (const TaskWindowFact &fact : facts) {
    const TaskListError shapeError = checkShape(fact);
    if (shapeError.hasError()) {
      return shapeError;
    }

    if (windowIds.contains(fact.windowId)) {
      return makeError(TaskListErrorCode::DuplicateWindowId, fact,
                       QStringLiteral("window id already present in this "
                                      "generation"));
    }
    windowIds.insert(fact.windowId);
    if (fact.active) {
      ++activeCount;
      if (activeCount > 1) {
        return makeError(TaskListErrorCode::MultipleActiveWindows, fact,
                         QStringLiteral("at most one window may be active"));
      }
    }

    // AGENT-GUARD: Cross-fact container bookkeeping must stay in this single
    // pass; a second lookup pass over QSets would silently accept an orphan
    // member whose primary appears only later in the scan order.
    if (fact.role == TaskWindowRole::ContainerPrimary) {
      if (containerPrimaries.contains(fact.containerId)) {
        return makeError(TaskListErrorCode::DuplicateContainerPrimary, fact,
                         QStringLiteral("container already has a primary"));
      }
      containerPrimaries.insert(fact.containerId);
    }
  }

  for (const TaskWindowFact &fact : facts) {
    if (fact.role == TaskWindowRole::ContainerMember &&
        !containerPrimaries.contains(fact.containerId)) {
      return makeError(TaskListErrorCode::OrphanContainerMember, fact,
                       QStringLiteral("member references a container with "
                                      "no primary in this generation"));
    }
  }

  // Task-list identities are window ids for standalone windows and container
  // ids for collapsed groups; a collision across those spaces would let one
  // intent resolve to two rows.
  QSet<QString> containerIds;
  for (const TaskWindowFact &fact : facts) {
    if (isGrouped(fact.role)) {
      containerIds.insert(fact.containerId);
    }
  }
  for (const TaskWindowFact &fact : facts) {
    if (fact.role == TaskWindowRole::Standalone &&
        containerIds.contains(fact.windowId)) {
      return makeError(TaskListErrorCode::EntryIdentityCollision, fact,
                       QStringLiteral("standalone window id collides with a "
                                      "container id"));
    }
  }

  return TaskListError{};
}

} // namespace QindaQt::ShellTaskList::Validation
