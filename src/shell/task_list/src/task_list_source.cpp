// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/task_list_source.h"

#include "task_list_grouping_p.h"
#include "task_list_validation_p.h"

#include <algorithm>

namespace QindaQt::ShellTaskList {

TaskListEvaluation
TaskListSource::publishGeneration(const QVector<TaskWindowFact> &facts) {
  TaskListEvaluation evaluation;
  const TaskListError error = Validation::validateBatch(facts);
  if (error.hasError()) {
    // AGENT-GUARD: Rejected input must not touch the retained generation;
    // presentation keeps showing the last coherent task list.
    evaluation.error = error;
    return evaluation;
  }

  evaluation.generation.revision = m_generation.revision + 1;
  evaluation.generation.entries = Grouping::canonicalEntries(facts);
  m_generation = evaluation.generation;
  m_status = TaskListSourceStatus::Ready;
  return evaluation;
}

void TaskListSource::markDegraded() {
  m_status = TaskListSourceStatus::Degraded;
}

void TaskListSource::reset() {
  m_generation = TaskGeneration{};
  m_status = TaskListSourceStatus::Loading;
}

TaskIntentOutcome
TaskListSource::requestIntent(const TaskIntentRequest &request) const {
  TaskIntentOutcome outcome;
  if (request.taskId.isEmpty()) {
    outcome.code = TaskIntentErrorCode::InvalidRequest;
    return outcome;
  }
  if (m_status == TaskListSourceStatus::Loading) {
    outcome.code = TaskIntentErrorCode::NoGeneration;
    return outcome;
  }
  if (m_status == TaskListSourceStatus::Degraded) {
    outcome.code = TaskIntentErrorCode::SourceDegraded;
    return outcome;
  }
  if (request.expectedRevision != m_generation.revision) {
    outcome.code = TaskIntentErrorCode::StaleRevision;
    return outcome;
  }

  const auto match = std::find_if(m_generation.entries.cbegin(),
                                  m_generation.entries.cend(),
                                  [&request](const TaskEntry &entry) {
                                    return entry.taskId == request.taskId;
                                  });
  if (match == m_generation.entries.cend() ||
      (!request.windowId.isEmpty() &&
       !match->memberWindowIds.contains(request.windowId))) {
    outcome.code = TaskIntentErrorCode::UnknownTask;
    return outcome;
  }

  outcome.entryKind = match->kind;
  outcome.primaryWindowId = match->primaryWindowId;
  outcome.memberWindowIds = match->memberWindowIds;
  outcome.minimized = match->minimized;
  // AGENT-CONTRACT: a named member narrows only Activate and Close, the two
  // per-window actions of an ungrouped strip; the shell router then acts on
  // exactly that window (task-list.md, "Request intents"). Minimize and Raise
  // keep the whole-entry targets because a container is one outer window.
  if (!request.windowId.isEmpty() &&
      (request.kind == TaskIntentKind::Activate ||
       request.kind == TaskIntentKind::Close)) {
    outcome.primaryWindowId = request.windowId;
    if (request.kind == TaskIntentKind::Close) {
      outcome.memberWindowIds = {request.windowId};
    }
  }
  return outcome;
}

} // namespace QindaQt::ShellTaskList
