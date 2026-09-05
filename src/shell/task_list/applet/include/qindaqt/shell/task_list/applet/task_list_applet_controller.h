// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/applet/task_list_applet_types.h"
#include "qindaqt/shell/task_list/operations/task_list_operations.h"
#include "qindaqt/shell/task_list/task_list_filter.h"
#include "qindaqt/shell/task_list/task_list_source.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QVariantList>

#include <functional>

namespace QindaQt::ShellTaskList::Producer {
class TaskListOperationAuthority;
}

namespace QindaQt::ShellTaskListApplet {

class TaskListAppletOperationPort;

// AGENT-CONTRACT: shell-composed facade exposed to the compiled
// QindaQt.Shell.TaskList module as `access`. It borrows the T0 source (owned
// by shell composition, fed by the T1 facts producer), the producer's
// read-only operation authority, and the injected operation port; none of the
// three is owned or deleted here, and all are GUI-thread confined. The
// controller owns no bus connection, no compositor object, and no window
// mutation path of its own: intents are arbitrated by the T0 source against
// the exact displayed revision and dispatched through the port exactly once.
//
// Least authority: TaskListAppletGrants is immutable after construction.
// windowsRead == false withholds all observation (phase unavailable, no rows,
// no dispatch); windowsActivate/windowsManage == false refuse the matching
// intents with feedback before any dispatch.
class TaskListAppletController : public QObject {
  Q_OBJECT

  Q_PROPERTY(QString phaseText READ phaseText NOTIFY stateReprojected)
  Q_PROPERTY(QString phaseReasonText READ phaseReasonText NOTIFY stateReprojected)
  Q_PROPERTY(QVariantList entryRows READ entryRows NOTIFY stateReprojected)
  Q_PROPERTY(int entryCount READ entryCount NOTIFY stateReprojected)
  Q_PROPERTY(int totalEntryCount READ totalEntryCount NOTIFY stateReprojected)
  Q_PROPERTY(int overflowCount READ overflowCount NOTIFY stateReprojected)
  Q_PROPERTY(bool windowsReadGranted READ windowsReadGranted CONSTANT)
  Q_PROPERTY(bool windowsActivateGranted READ windowsActivateGranted CONSTANT)
  Q_PROPERTY(bool windowsManageGranted READ windowsManageGranted CONSTANT)
  Q_PROPERTY(bool canActivate READ canActivate NOTIFY stateReprojected)
  Q_PROPERTY(bool canManage READ canManage NOTIFY stateReprojected)
  Q_PROPERTY(int pendingOperationCount READ pendingOperationCount NOTIFY stateReprojected)
  Q_PROPERTY(QString scopeOutputId READ scopeOutputId WRITE setScopeOutputId NOTIFY stateReprojected)
  Q_PROPERTY(QString scopeWorkspaceId READ scopeWorkspaceId WRITE setScopeWorkspaceId NOTIFY stateReprojected)
  Q_PROPERTY(bool feedbackPresent READ feedbackPresent NOTIFY feedbackChanged)
  Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)
  Q_PROPERTY(QString feedbackStatus READ feedbackStatus NOTIFY feedbackChanged)

public:
  using IconNameResolver = std::function<QString(const QString &applicationId)>;
  using IconResolvedResolver = std::function<bool(const QString &iconName)>;

  TaskListAppletController(
      ShellTaskList::TaskListSource &source,
      ShellTaskList::Producer::TaskListOperationAuthority &authority,
      TaskListAppletOperationPort &operations, TaskListAppletGrants grants,
      QObject *parent = nullptr);
  TaskListAppletController(
      ShellTaskList::TaskListSource &source,
      ShellTaskList::Producer::TaskListOperationAuthority &authority,
      TaskListAppletOperationPort &operations, TaskListAppletGrants grants,
      IconNameResolver iconNameResolver, QObject *parent = nullptr);
  TaskListAppletController(
      ShellTaskList::TaskListSource &source,
      ShellTaskList::Producer::TaskListOperationAuthority &authority,
      TaskListAppletOperationPort &operations, TaskListAppletGrants grants,
      IconNameResolver iconNameResolver,
      IconResolvedResolver iconResolvedResolver, QObject *parent = nullptr);
  ~TaskListAppletController() override = default;

  [[nodiscard]] QString phaseText() const;
  [[nodiscard]] QString phaseReasonText() const;
  [[nodiscard]] QVariantList entryRows() const;
  [[nodiscard]] int entryCount() const noexcept;
  [[nodiscard]] int totalEntryCount() const noexcept;
  // AGENT-CONTRACT: Includes members represented by collapsed container rows;
  // ShellDevelopment1 uses this count to prove compositor windows reached T1.
  [[nodiscard]] int totalWindowCount() const noexcept;
  [[nodiscard]] int overflowCount() const noexcept;
  [[nodiscard]] bool windowsReadGranted() const noexcept;
  [[nodiscard]] bool windowsActivateGranted() const noexcept;
  [[nodiscard]] bool windowsManageGranted() const noexcept;
  [[nodiscard]] bool canActivate() const noexcept;
  [[nodiscard]] bool canManage() const noexcept;
  [[nodiscard]] int pendingOperationCount() const noexcept;
  [[nodiscard]] QString scopeOutputId() const;
  [[nodiscard]] QString scopeWorkspaceId() const;
  void setScopeOutputId(const QString &outputId);
  void setScopeWorkspaceId(const QString &workspaceId);
  [[nodiscard]] bool feedbackPresent() const noexcept;
  [[nodiscard]] QString feedback() const;
  [[nodiscard]] QString feedbackStatus() const noexcept;

  [[nodiscard]] const TaskListAppletProjection &projection() const noexcept;

  // Task intents. The revision argument must be the generationRevision the
  // caller displayed; a mismatch is refused as stale before any dispatch.
  Q_INVOKABLE bool activateTask(const QString &taskId, quint64 revision);
  Q_INVOKABLE bool minimizeTask(const QString &taskId, quint64 revision);
  Q_INVOKABLE bool closeTask(const QString &taskId, quint64 revision);
  Q_INVOKABLE bool raiseTask(const QString &taskId, quint64 revision);

  // Container/dock operations admitted by T1. taskId names a container entry
  // (its taskId is the container id); pageId/windowId must be members of that
  // entry. ungroupContainer maps to the T1 releaseContainer (Ungroup arm of
  // the shell-owned container close policy).
  Q_INVOKABLE bool activateContainerPage(const QString &taskId,
                                         const QString &pageId,
                                         quint64 revision);
  Q_INVOKABLE bool detachContainerWindow(const QString &taskId,
                                         const QString &windowId,
                                         quint64 revision);
  Q_INVOKABLE bool ungroupContainer(const QString &taskId, quint64 revision);
  Q_INVOKABLE bool dockWindows(const QString &targetTaskId,
                               const QString &incomingTaskId,
                               const QString &orientation,
                               const QString &position, double ratio,
                               quint64 revision);

  Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
  void stateReprojected();
  void feedbackChanged();

private Q_SLOTS:
  void handleAuthorityStateChanged();
  void handleOperationFinished(
      const ShellTaskList::Operations::TaskListOperationResult &result);

private:
  struct PendingOperation {
    quint64 token = 0;
    // AGENT-GUARD: one dispatch may fence more than one task — dockWindows
    // marks BOTH participants pending under its single token so neither side
    // can attract a second mutation while the dock is in flight (wiki
    // task-list contract: one pending marker per task). resolveResult must
    // release every listed id when the exactly-one terminal result arrives.
    QStringList taskIds;
    QString actionText;
  };

  enum class ContainerOperation { ActivatePage, DetachWindow, Release };

  void reproject();
  void setFeedback(const QString &message, const QString &status);
  bool refuse(const QString &message);
  bool dispatchTaskIntent(ShellTaskList::TaskIntentKind kind,
                          const QString &taskId, quint64 revision,
                          const QString &actionText);
  bool dispatchContainerOperation(const QString &taskId, quint64 revision,
                                  const QString &memberWindowId,
                                  const QString &actionText,
                                  ContainerOperation operation);
  quint64 recordDispatch(quint64 token, const QStringList &taskIds,
                         const QString &actionText);
  void resolveResult(
      const ShellTaskList::Operations::TaskListOperationResult &result);
  void drainDeferredResults();
  [[nodiscard]] const ShellTaskList::TaskEntry *
  findEntry(const QString &taskId) const;

  ShellTaskList::TaskListSource &m_source;
  ShellTaskList::Producer::TaskListOperationAuthority &m_authority;
  TaskListAppletOperationPort &m_operations;
  TaskListAppletGrants m_grants;
  IconNameResolver m_iconNameResolver;
  IconResolvedResolver m_iconResolvedResolver;
  ShellTaskList::TaskListScope m_scope;
  TaskListAppletProjection m_projection;

  QHash<quint64, PendingOperation> m_pendingByToken;
  QSet<QString> m_pendingTasks;

  // AGENT-GUARD: the operation port may emit operationFinished synchronously
  // INSIDE a dispatch call (the T1 adapter rejects fenced requests before any
  // bus traffic), before the returned token is knowable. Such results are
  // buffered while m_insidePortCall is set and attributed by token after the
  // pending record exists; a flushed result can never impersonate or strand
  // the live request.
  bool m_insidePortCall = false;
  QList<ShellTaskList::Operations::TaskListOperationResult> m_deferredResults;

  bool m_feedbackPresent = false;
  QString m_feedback;
  QString m_feedbackStatus;
};

} // namespace QindaQt::ShellTaskListApplet
