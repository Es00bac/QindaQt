// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/applet/task_list_applet_types.h"
#include "qindaqt/shell/task_list/operations/task_list_operations.h"

#include <QImage>

class QQmlEngine;
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
class TaskListAppletPreviewPort;

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
  // Ungrouped strip rows (`grouping: "never"`) and their exact overflow
  // truth; see TaskListAppletProjection::windowRows. entryRows stays the
  // grouped view, so hosts with different grouping share one controller.
  Q_PROPERTY(QVariantList windowRows READ windowRows NOTIFY stateReprojected)
  Q_PROPERTY(int windowOverflowCount READ windowOverflowCount
                 NOTIFY stateReprojected)
  // Presentation bound for the strip projection (default
  // kMaxPresentedTaskEntries). A scrolling dock host raises it (up to
  // kMaxPresentedDockEntries) so overflow scrolls instead of truncating;
  // overflow truth and the cap contract are otherwise unchanged.
  Q_PROPERTY(int presentationLimit READ presentationLimit
                 WRITE setPresentationLimit NOTIFY stateReprojected)
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
  // Hover preview seam (ADR-0119). Null port = previews unavailable and
  // presentation keeps the text tooltip.
  Q_PROPERTY(bool previewsEnabled READ previewsEnabled NOTIFY previewPortChanged)
  // Persisted user-order overlay over the canonical order (ADR-0118).
  // Settings feeds it at startup and on change; drags go through reorderTask.
  Q_PROPERTY(QStringList userTaskOrder READ userTaskOrder WRITE
                 setUserTaskOrder NOTIFY userTaskOrderChanged)

public:
  using IconNameResolver = std::function<QString(const QString &applicationId)>;
  using IconResolvedResolver = std::function<bool(const QString &iconName)>;
  // Presentation name for a row from (applicationId, compositor-reported
  // applicationName). Absent, or answering empty, keeps the reported name.
  // Rows publish the result as both applicationName and the leading part of
  // accessibleName, so every consumer (buttons, tooltips, the active
  // application indicator, command search) shows one name.
  using ApplicationNameResolver = std::function<QString(
      const QString &applicationId, const QString &reportedName)>;

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
  TaskListAppletController(
      ShellTaskList::TaskListSource &source,
      ShellTaskList::Producer::TaskListOperationAuthority &authority,
      TaskListAppletOperationPort &operations, TaskListAppletGrants grants,
      IconNameResolver iconNameResolver,
      IconResolvedResolver iconResolvedResolver,
      ApplicationNameResolver applicationNameResolver,
      QObject *parent = nullptr);
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
  [[nodiscard]] QVariantList windowRows() const;
  [[nodiscard]] int windowOverflowCount() const noexcept;
  [[nodiscard]] int presentationLimit() const noexcept;
  void setPresentationLimit(int limit);
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
  // Ungrouped-row intents: taskId is the entry the row belongs to and
  // windowId the member it shows (a non-empty id is required). The T0 source
  // refuses a window outside that entry; activation then targets the member
  // itself, so the compositor switches its container page, and Close closes
  // only that window.
  Q_INVOKABLE bool activateTaskWindow(const QString &taskId,
                                      const QString &windowId,
                                      quint64 revision);
  Q_INVOKABLE bool closeTaskWindow(const QString &taskId,
                                   const QString &windowId, quint64 revision);

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

  // User reorder gesture (drag-and-drop or the keyboard "move" actions).
  // movedTaskId must be a displayed row and the revision must be the
  // displayed generationRevision; beforeTaskId is the row the moved task is
  // inserted before, or empty to append. This is presentation preference
  // only — no compositor operation is dispatched and no pending marker is
  // set. On success the committed order is emitted for persistence.
  Q_INVOKABLE bool reorderTask(const QString &movedTaskId,
                               const QString &beforeTaskId, quint64 revision);

  [[nodiscard]] QStringList userTaskOrder() const;
  void setUserTaskOrder(const QStringList &order);

  // Hover-preview observation (ADR-0119). Requests are bounded and
  // superseded: at most one capture is in flight, a newer request drops the
  // older one, and results are matched against the requested (taskId,
  // revision) so a stale capture can never decorate a newer generation.
  // Emitted with an empty image when the compositor cannot provide a
  // preview; presentation falls back to the text tooltip.
  Q_INVOKABLE void requestTaskPreview(const QString &taskId, quint64 revision,
                                      int maxWidth, int maxHeight);
  Q_INVOKABLE void cancelTaskPreview();
  void setPreviewPort(TaskListAppletPreviewPort *port);
  [[nodiscard]] bool previewsEnabled() const noexcept;
  // Registers the QML image provider backing preview tokens. The engine
  // owns the provider; the controller only feeds it. One engine, once.
  void installPreviewProvider(QQmlEngine *engine);

Q_SIGNALS:
  // imageToken addresses the provider image ("image://qindaqt-task-preview/
  // <token>"); 0 means the compositor could not provide a preview and
  // presentation falls back to the text tooltip.
  void previewArrived(const QString &taskId, quint64 revision, int imageToken);
  void previewPortChanged();

Q_SIGNALS:
  void stateReprojected();
  void feedbackChanged();
  // Emitted only for a user-initiated order change (reorderTask), carrying
  // the exact order to persist. Settings-driven setUserTaskOrder echoes do
  // not emit it, so the runtime write path cannot loop.
  void taskOrderCommitted(const QStringList &orderedTaskIds);
  void userTaskOrderChanged();

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
                          const QString &actionText,
                          const QString &windowId = QString());
  [[nodiscard]] QVariantList
  rowsToVariant(const QVector<TaskListAppletRow> &rows) const;
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
  TaskListAppletPreviewPort *m_previewPort = nullptr;
  class TaskListPreviewProvider *m_previewProvider = nullptr;
  QString m_previewTaskId;
  QString m_previewWindowId;
  quint64 m_previewRevision = 0;
  int m_previewGeneration = 0;
  TaskListAppletGrants m_grants;
  IconNameResolver m_iconNameResolver;
  IconResolvedResolver m_iconResolvedResolver;
  ApplicationNameResolver m_applicationNameResolver;
  ShellTaskList::TaskListScope m_scope;
  TaskListAppletProjection m_projection;
  int m_presentationLimit = kMaxPresentedTaskEntries;
  QStringList m_userTaskOrder;

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
