// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"

#include "qindaqt/shell/task_list/applet/task_list_applet_projection.h"
#include "qindaqt/shell/task_list/applet/task_list_applet_operation_port.h"
#include "qindaqt/shell/task_list/producer/task_list_operation_authority.h"

namespace QindaQt::ShellTaskListApplet {

TaskListAppletController::TaskListAppletController(
    ShellTaskList::TaskListSource &source,
    ShellTaskList::Producer::TaskListOperationAuthority &authority,
    TaskListAppletOperationPort &operations, TaskListAppletGrants grants,
    QObject *parent)
    : QObject(parent),
      m_source(source),
      m_authority(authority),
      m_operations(operations),
      m_grants(grants) {
  connect(&m_authority,
          &ShellTaskList::Producer::TaskListOperationAuthority::stateChanged,
          this, &TaskListAppletController::handleAuthorityStateChanged);
  connect(&m_operations,
          &TaskListAppletOperationPort::operationFinished, this,
          &TaskListAppletController::handleOperationFinished);
  reproject();
}

QString TaskListAppletController::phaseText() const {
  return taskListAppletPhaseText(m_projection.phase);
}

QString TaskListAppletController::phaseReasonText() const {
  return m_projection.phaseReason;
}

QVariantList TaskListAppletController::entryRows() const {
  QVariantList rows;
  rows.reserve(m_projection.rows.size());
  for (const TaskListAppletRow &row : m_projection.rows) {
    QVariantMap map;
    map.insert(QStringLiteral("taskId"), row.taskId);
    map.insert(QStringLiteral("kind"),
               row.kind == ShellTaskList::TaskEntryKind::Container
                   ? QStringLiteral("container")
                   : QStringLiteral("window"));
    map.insert(QStringLiteral("title"), row.title);
    map.insert(QStringLiteral("applicationId"), row.applicationId);
    map.insert(QStringLiteral("applicationName"), row.applicationName);
    map.insert(QStringLiteral("iconText"), row.iconText);
    map.insert(QStringLiteral("windowCount"), row.windowCount);
    map.insert(QStringLiteral("active"), row.active);
    map.insert(QStringLiteral("minimized"), row.minimized);
    map.insert(QStringLiteral("urgent"), row.urgent);
    map.insert(QStringLiteral("keyboardIndex"), row.keyboardIndex);
    map.insert(QStringLiteral("accessibleName"), row.accessibleName);
    map.insert(QStringLiteral("generationRevision"), row.generationRevision);
    map.insert(QStringLiteral("pending"), row.pending);
    map.insert(QStringLiteral("memberWindowIds"), row.memberWindowIds);
    rows.append(std::move(map));
  }
  return rows;
}

int TaskListAppletController::entryCount() const noexcept {
  return static_cast<int>(m_projection.rows.size());
}

int TaskListAppletController::totalEntryCount() const noexcept {
  return m_projection.totalCount;
}

int TaskListAppletController::overflowCount() const noexcept {
  return m_projection.overflowCount;
}

bool TaskListAppletController::windowsReadGranted() const noexcept {
  return m_grants.windowsRead;
}

bool TaskListAppletController::windowsActivateGranted() const noexcept {
  return m_grants.windowsActivate;
}

bool TaskListAppletController::windowsManageGranted() const noexcept {
  return m_grants.windowsManage;
}

bool TaskListAppletController::canActivate() const noexcept {
  return m_projection.phase == TaskListAppletPhase::Ready &&
         m_grants.windowsRead && m_grants.windowsActivate;
}

bool TaskListAppletController::canManage() const noexcept {
  return m_projection.phase == TaskListAppletPhase::Ready &&
         m_grants.windowsRead && m_grants.windowsManage;
}

int TaskListAppletController::pendingOperationCount() const noexcept {
  return static_cast<int>(m_pendingByToken.size());
}

QString TaskListAppletController::scopeOutputId() const {
  return m_scope.outputId;
}

QString TaskListAppletController::scopeWorkspaceId() const {
  return m_scope.workspaceId;
}

void TaskListAppletController::setScopeOutputId(const QString &outputId) {
  if (m_scope.outputId == outputId) {
    return;
  }
  m_scope.outputId = outputId;
  reproject();
}

void TaskListAppletController::setScopeWorkspaceId(const QString &workspaceId) {
  if (m_scope.workspaceId == workspaceId) {
    return;
  }
  m_scope.workspaceId = workspaceId;
  reproject();
}

bool TaskListAppletController::feedbackPresent() const noexcept {
  return m_feedbackPresent;
}

QString TaskListAppletController::feedback() const {
  return m_feedback;
}

QString TaskListAppletController::feedbackStatus() const noexcept {
  return m_feedbackStatus;
}

const TaskListAppletProjection &TaskListAppletController::projection()
    const noexcept {
  return m_projection;
}

void TaskListAppletController::clearFeedback() {
  if (!m_feedbackPresent) {
    return;
  }
  m_feedbackPresent = false;
  m_feedback.clear();
  Q_EMIT feedbackChanged();
}

void TaskListAppletController::handleAuthorityStateChanged() {
  // AGENT-GUARD: pending operations are NOT cleared here. The operation port
  // guarantees exactly one terminal result per admitted request (owner loss in
  // flight finishes Uncertain), so the pending marker waits for that result;
  // force-clearing would orphan the marker and admit a duplicate dispatch for
  // a request whose outcome is still unknown.
  reproject();
}

void TaskListAppletController::reproject() {
  const ShellTaskList::TaskListPresentation presentation =
      ShellTaskList::TaskListPresentationModel::project(
          m_source.status(), m_source.generation(), m_scope);
  m_projection = TaskListAppletProjectionModel::project(
      presentation, m_pendingTasks, m_grants.windowsRead, m_source.revision());
  // AGENT-NOTE: the T0 presentation flattens any degraded projection with no
  // visible entries to Empty. The accepted contract (task-list.md,
  // "Presentation") distinguishes a failed FIRST refresh — Degraded with no
  // accepted generation — from an accepted generation whose scope filters to
  // nothing (Empty even while degraded). revision()==0 means no generation was
  // ever accepted, so the applet restores the Degraded truth here rather than
  // presenting "empty" for a source that never published.
  if (m_grants.windowsRead &&
      m_source.status() == ShellTaskList::TaskListSourceStatus::Degraded &&
      m_source.revision() == 0) {
    m_projection.phase = TaskListAppletPhase::Degraded;
    m_projection.phaseReason =
        QStringLiteral("compositor-task-list-unavailable");
    m_projection.rows.clear();
    m_projection.totalCount = 0;
    m_projection.overflowCount = 0;
  }
  Q_EMIT stateReprojected();
}

void TaskListAppletController::setFeedback(const QString &message,
                                           const QString &status) {
  m_feedbackPresent = true;
  m_feedback = message;
  m_feedbackStatus = status;
  Q_EMIT feedbackChanged();
}

} // namespace QindaQt::ShellTaskListApplet
