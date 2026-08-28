// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/task_list_intent.h"
#include "qindaqt/shell/task_list/task_list_presentation.h"
#include "qindaqt/shell/task_list/task_list_source.h"
#include "qindaqt/shell/task_list/task_list_types.h"

namespace TaskListTest {

// Deterministic fact builders. Defaults satisfy validation so each test only
// overrides the single field under examination.
inline QindaQt::ShellTaskList::TaskWindowFact
makeFact(const QString &windowId, const QString &applicationId) {
  QindaQt::ShellTaskList::TaskWindowFact fact;
  fact.windowId = windowId;
  fact.applicationId = applicationId;
  fact.applicationName = QStringLiteral("App %1").arg(applicationId);
  fact.title = QStringLiteral("Title %1").arg(windowId);
  fact.outputId = QStringLiteral("output-1");
  fact.workspaceIds = {QStringLiteral("ws-1")};
  return fact;
}

inline QindaQt::ShellTaskList::TaskWindowFact
standalone(const QString &windowId, const QString &applicationId) {
  return makeFact(windowId, applicationId);
}

inline QindaQt::ShellTaskList::TaskWindowFact
primary(const QString &windowId, const QString &applicationId,
        const QString &containerId) {
  auto fact = makeFact(windowId, applicationId);
  fact.role = QindaQt::ShellTaskList::TaskWindowRole::ContainerPrimary;
  fact.containerId = containerId;
  return fact;
}

inline QindaQt::ShellTaskList::TaskWindowFact
member(const QString &windowId, const QString &containerId) {
  auto fact = makeFact(windowId, QStringLiteral("app.member"));
  fact.role = QindaQt::ShellTaskList::TaskWindowRole::ContainerMember;
  fact.containerId = containerId;
  return fact;
}

inline QindaQt::ShellTaskList::TaskWindowFact
withOutput(QindaQt::ShellTaskList::TaskWindowFact fact,
           const QString &outputId) {
  fact.outputId = outputId;
  return fact;
}

inline QindaQt::ShellTaskList::TaskWindowFact
withWorkspaces(QindaQt::ShellTaskList::TaskWindowFact fact,
               const QStringList &workspaceIds) {
  fact.workspaceIds = workspaceIds;
  fact.onAllWorkspaces = false;
  return fact;
}

inline QindaQt::ShellTaskList::TaskWindowFact
allWorkspaces(QindaQt::ShellTaskList::TaskWindowFact fact) {
  fact.workspaceIds.clear();
  fact.onAllWorkspaces = true;
  return fact;
}

inline QindaQt::ShellTaskList::TaskListEvaluation
publish(QindaQt::ShellTaskList::TaskListSource &source,
        const QVector<QindaQt::ShellTaskList::TaskWindowFact> &facts) {
  return source.publishGeneration(facts);
}

} // namespace TaskListTest
