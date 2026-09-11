// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"

#include "qindaqt/shell/task_list/applet/task_list_applet_operation_port.h"
#include "qindaqt/shell/task_list/producer/task_list_operation_authority.h"

namespace QindaQt::ShellTaskListApplet {

namespace {

using ShellTaskList::TaskIntentKind;
using ShellTaskList::TaskIntentErrorCode;
using ShellTaskList::Operations::TaskListOperationStatus;

QString intentRefusalText(ShellTaskList::TaskIntentErrorCode code) {
  switch (code) {
  case TaskIntentErrorCode::None:
    break;
  case TaskIntentErrorCode::InvalidRequest:
    return QStringLiteral("the task-list request was malformed");
  case TaskIntentErrorCode::NoGeneration:
    return QStringLiteral("no compositor generation is available yet");
  case TaskIntentErrorCode::SourceDegraded:
    return QStringLiteral(
        "the task-list source is unavailable; actions stay refused until a "
        "fresh generation arrives");
  case TaskIntentErrorCode::StaleRevision:
    return QStringLiteral(
        "the task list changed before the action reached the shell");
  case TaskIntentErrorCode::UnknownTask:
    return QStringLiteral("the window is no longer listed");
  }
  return QStringLiteral("the task-list request was refused");
}

QString operationFeedbackStatus(TaskListOperationStatus status) {
  switch (status) {
  case TaskListOperationStatus::Committed:
    return QStringLiteral("info");
  case TaskListOperationStatus::Unavailable:
    return QStringLiteral("info");
  case TaskListOperationStatus::StaleGeneration:
  case TaskListOperationStatus::SourceNotReady:
  case TaskListOperationStatus::UnknownContainer:
  case TaskListOperationStatus::UnsupportedAuthority:
  case TaskListOperationStatus::Busy:
  case TaskListOperationStatus::Uncertain:
    return QStringLiteral("warning");
  case TaskListOperationStatus::InvalidRequest:
  case TaskListOperationStatus::Rejected:
  case TaskListOperationStatus::Conflict:
  case TaskListOperationStatus::TransportFailure:
    return QStringLiteral("error");
  }
  return QStringLiteral("error");
}

} // namespace

bool TaskListAppletController::activateTask(const QString &taskId,
                                            quint64 revision) {
  return dispatchTaskIntent(TaskIntentKind::Activate, taskId, revision,
                            QStringLiteral("Activate"));
}

bool TaskListAppletController::minimizeTask(const QString &taskId,
                                            quint64 revision) {
  return dispatchTaskIntent(TaskIntentKind::Minimize, taskId, revision,
                            QStringLiteral("Minimize"));
}

bool TaskListAppletController::closeTask(const QString &taskId,
                                         quint64 revision) {
  return dispatchTaskIntent(TaskIntentKind::Close, taskId, revision,
                            QStringLiteral("Close"));
}

bool TaskListAppletController::raiseTask(const QString &taskId,
                                         quint64 revision) {
  return dispatchTaskIntent(TaskIntentKind::Raise, taskId, revision,
                            QStringLiteral("Raise"));
}

bool TaskListAppletController::activateTaskWindow(const QString &taskId,
                                                  const QString &windowId,
                                                  quint64 revision) {
  if (windowId.isEmpty()) {
    return refuse(QStringLiteral("Activate: ") +
                  intentRefusalText(TaskIntentErrorCode::InvalidRequest));
  }
  return dispatchTaskIntent(TaskIntentKind::Activate, taskId, revision,
                            QStringLiteral("Activate"), windowId);
}

bool TaskListAppletController::closeTaskWindow(const QString &taskId,
                                               const QString &windowId,
                                               quint64 revision) {
  if (windowId.isEmpty()) {
    return refuse(QStringLiteral("Close: ") +
                  intentRefusalText(TaskIntentErrorCode::InvalidRequest));
  }
  return dispatchTaskIntent(TaskIntentKind::Close, taskId, revision,
                            QStringLiteral("Close"), windowId);
}

bool TaskListAppletController::activateContainerPage(const QString &taskId,
                                                     const QString &pageId,
                                                     quint64 revision) {
  return dispatchContainerOperation(taskId, revision, pageId,
                                    QStringLiteral("Activate page"),
                                    ContainerOperation::ActivatePage);
}

bool TaskListAppletController::detachContainerWindow(const QString &taskId,
                                                     const QString &windowId,
                                                     quint64 revision) {
  return dispatchContainerOperation(taskId, revision, windowId,
                                    QStringLiteral("Detach window"),
                                    ContainerOperation::DetachWindow);
}

bool TaskListAppletController::ungroupContainer(const QString &taskId,
                                                quint64 revision) {
  return dispatchContainerOperation(taskId, revision, {},
                                    QStringLiteral("Ungroup"),
                                    ContainerOperation::Release);
}

bool TaskListAppletController::dockWindows(
    const QString &targetTaskId, const QString &incomingTaskId,
    const QString &orientation, const QString &position, double ratio,
    quint64 revision) {
  if (!m_grants.windowsRead) {
    return refuse(QStringLiteral(
        "Dock: the windows.read capability was not granted to this applet"));
  }
  if (!m_grants.windowsManage) {
    return refuse(QStringLiteral(
        "Dock: the windows.manage capability was not granted to this applet"));
  }
  if (m_pendingTasks.contains(targetTaskId) ||
      m_pendingTasks.contains(incomingTaskId)) {
    return refuse(
        QStringLiteral("Dock: an operation is already pending for the task"));
  }
  if (m_source.status() != ShellTaskList::TaskListSourceStatus::Ready) {
    return refuse(QStringLiteral(
        "Dock: the task-list source is not ready; no generation may be acted "
        "on"));
  }
  if (revision != m_source.revision()) {
    return refuse(QStringLiteral(
        "Dock: the task list changed before the action reached the shell"));
  }
  const ShellTaskList::TaskEntry *target = findEntry(targetTaskId);
  const ShellTaskList::TaskEntry *incoming = findEntry(incomingTaskId);
  if (target == nullptr || incoming == nullptr) {
    return refuse(QStringLiteral("Dock: the window is no longer listed"));
  }
  m_insidePortCall = true;
  const quint64 token = m_operations.dockWindows(
      target->primaryWindowId, incoming->primaryWindowId, orientation,
      position, ratio, revision);
  m_insidePortCall = false;
  // AGENT-GUARD: the single dock dispatch fences BOTH participants; the
  // pre-dispatch check above already refused when either task was pending,
  // so recording only one side would let a second mutation reach the other
  // participant while the dock is in flight.
  recordDispatch(token, {targetTaskId, incomingTaskId}, QStringLiteral("Dock"));
  drainDeferredResults();
  reproject();
  return true;
}

bool TaskListAppletController::refuse(const QString &message) {
  setFeedback(message, QStringLiteral("warning"));
  return false;
}

bool TaskListAppletController::dispatchTaskIntent(
    ShellTaskList::TaskIntentKind kind, const QString &taskId,
    quint64 revision, const QString &actionText, const QString &windowId) {
  if (!m_grants.windowsRead) {
    return refuse(actionText +
                  QStringLiteral(": the windows.read capability was not "
                                 "granted to this applet"));
  }
  if (kind == TaskIntentKind::Activate && !m_grants.windowsActivate) {
    return refuse(actionText +
                  QStringLiteral(": the windows.activate capability was not "
                                 "granted to this applet"));
  }
  if (kind != TaskIntentKind::Activate && !m_grants.windowsManage) {
    return refuse(actionText +
                  QStringLiteral(": the windows.manage capability was not "
                                 "granted to this applet"));
  }
  if (m_pendingTasks.contains(taskId)) {
    return refuse(actionText + QStringLiteral(": an operation is already "
                                              "pending for the task"));
  }
  ShellTaskList::TaskIntentRequest request;
  request.taskId = taskId;
  request.kind = kind;
  request.expectedRevision = revision;
  request.windowId = windowId;
  const ShellTaskList::TaskIntentOutcome outcome =
      m_source.requestIntent(request);
  if (!outcome.ok()) {
    return refuse(actionText + QStringLiteral(": ") +
                  intentRefusalText(outcome.code));
  }
  m_insidePortCall = true;
  const quint64 token = m_operations.executeTaskIntent(request, outcome);
  m_insidePortCall = false;
  recordDispatch(token, {taskId}, actionText);
  drainDeferredResults();
  reproject();
  return true;
}

bool TaskListAppletController::dispatchContainerOperation(
    const QString &taskId, quint64 revision, const QString &memberWindowId,
    const QString &actionText, ContainerOperation operation) {
  if (!m_grants.windowsRead) {
    return refuse(actionText +
                  QStringLiteral(": the windows.read capability was not "
                                 "granted to this applet"));
  }
  if (!m_grants.windowsManage) {
    return refuse(actionText +
                  QStringLiteral(": the windows.manage capability was not "
                                 "granted to this applet"));
  }
  if (m_pendingTasks.contains(taskId)) {
    return refuse(actionText + QStringLiteral(": an operation is already "
                                              "pending for the task"));
  }
  if (m_source.status() != ShellTaskList::TaskListSourceStatus::Ready) {
    return refuse(actionText +
                  QStringLiteral(": the task-list source is not ready; no "
                                 "generation may be acted on"));
  }
  if (revision != m_source.revision()) {
    return refuse(actionText +
                  QStringLiteral(": the task list changed before the action "
                                 "reached the shell"));
  }
  const ShellTaskList::TaskEntry *entry = findEntry(taskId);
  if (entry == nullptr ||
      entry->kind != ShellTaskList::TaskEntryKind::Container) {
    return refuse(actionText +
                  QStringLiteral(": the container is no longer listed"));
  }
  if (operation != ContainerOperation::Release &&
      !entry->memberWindowIds.contains(memberWindowId)) {
    // AGENT-GUARD: page/detach targets must be members of the exact accepted
    // generation's container entry; memberWindowIds includes the primary, so
    // activating the visible page is admitted while foreign window ids are
    // refused before any dispatch.
    return refuse(actionText + QStringLiteral(": the window is not a member "
                                              "of this container"));
  }
  quint64 token = 0;
  m_insidePortCall = true;
  switch (operation) {
  case ContainerOperation::ActivatePage:
    token = m_operations.activateContainerPage(taskId, memberWindowId,
                                               revision);
    break;
  case ContainerOperation::DetachWindow:
    token = m_operations.detachWindow(taskId, memberWindowId, revision);
    break;
  case ContainerOperation::Release:
    token = m_operations.releaseContainer(taskId, revision);
    break;
  }
  m_insidePortCall = false;
  recordDispatch(token, {taskId}, actionText);
  drainDeferredResults();
  reproject();
  return true;
}

quint64 TaskListAppletController::recordDispatch(quint64 token,
                                                 const QStringList &taskIds,
                                                 const QString &actionText) {
  if (token == 0) {
    // The port exhausted its token lineage without emitting a result; treat
    // the request as refused rather than pretending it is trackable.
    refuse(actionText + QStringLiteral(": the operation transport is "
                                       "exhausted"));
    return 0;
  }
  PendingOperation pending;
  pending.token = token;
  pending.taskIds = taskIds;
  pending.actionText = actionText;
  m_pendingByToken.insert(token, pending);
  for (const QString &taskId : taskIds) {
    m_pendingTasks.insert(taskId);
  }
  return token;
}

void TaskListAppletController::handleOperationFinished(
    const ShellTaskList::Operations::TaskListOperationResult &result) {
  if (m_insidePortCall) {
    m_deferredResults.append(result);
    return;
  }
  resolveResult(result);
  reproject();
}

void TaskListAppletController::drainDeferredResults() {
  const QList<ShellTaskList::Operations::TaskListOperationResult> drained =
      std::exchange(m_deferredResults, {});
  for (const auto &result : drained) {
    resolveResult(result);
  }
}

void TaskListAppletController::resolveResult(
    const ShellTaskList::Operations::TaskListOperationResult &result) {
  const auto it = m_pendingByToken.constFind(result.token);
  if (it == m_pendingByToken.constEnd()) {
    // AGENT-GUARD: results for unknown, superseded, or replayed tokens are
    // dropped whole — they can neither clear another task's pending marker
    // nor forge feedback.
    return;
  }
  const PendingOperation pending = it.value();
  m_pendingByToken.erase(it);
  for (const QString &taskId : pending.taskIds) {
    m_pendingTasks.remove(taskId);
  }
  if (result.status != TaskListOperationStatus::Committed) {
    setFeedback(pending.actionText + QStringLiteral(": ") + result.message,
                operationFeedbackStatus(result.status));
  }
}

const ShellTaskList::TaskEntry *TaskListAppletController::findEntry(
    const QString &taskId) const {
  for (const ShellTaskList::TaskEntry &entry : m_source.generation().entries) {
    if (entry.taskId == taskId) {
      return &entry;
    }
  }
  return nullptr;
}

} // namespace QindaQt::ShellTaskListApplet
