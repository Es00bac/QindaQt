// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/operations/task_list_operations.h"
#include "qindaqt/shell/task_list/task_list_intent.h"

#include <QObject>
#include <QTimer>

#include <optional>

namespace QindaQt::ShellTaskList::Producer {
class TaskListFactsProducer;
}

namespace QindaQt::ShellTaskList::Operations {

class TaskListOperationTransport;

// Executes task-list intent through the public Compositor1 mutation surface.
// The producer supplies the exact unique owner, the accepted generation
// revision, and the per-container lineage; every admission is fenced against
// all three atomically (single-threaded, before any bus traffic).
//
// AGENT-CONTRACT: Requests are serialized — one in flight, no queue — so a
// queued intent can never act on a generation the user no longer sees. A
// transaction is submitted exactly once; timeouts, malformed replies, and
// owner loss in flight report Uncertain and are never retried by the adapter.
// Window-level activate/minimize/close have no Compositor1 1.1 operation and
// finish as Unavailable with the exact extension code; do not add a private
// path around that (docs/wiki/shell/task-list.md).
class TaskListOperationAdapter final : public QObject {
  Q_OBJECT

public:
  explicit TaskListOperationAdapter(
      Producer::TaskListFactsProducer &producer,
      TaskListOperationTransport &transport, int replyTimeoutMilliseconds = 5000,
      QObject *parent = nullptr);

  // Maps an accepted T0 intent to the protocol surface. Window-level
  // operations finish as Unavailable; container close/minimize policy
  // (Close All / Ungroup / Cancel) stays with the later shell composition
  // lane, which may call releaseContainer for the Ungroup arm. The request
  // supplies the kind and the generation revision the caller displayed.
  quint64 executeTaskIntent(const TaskIntentRequest &request,
                            const TaskIntentOutcome &outcome);

  // Compositor1 Submit operations (control-bridge authority containers only).
  quint64 activateContainerPage(const QString &containerId,
                                const QString &pageId,
                                quint64 expectedRevision);
  quint64 detachWindow(const QString &containerId, const QString &windowId,
                       quint64 expectedRevision);
  // Compositor1 ReleaseContainer (control-bridge authority only).
  quint64 releaseContainer(const QString &containerId,
                           quint64 expectedRevision);
  // Compositor1 DockWindows for two independent windows.
  quint64 dockWindows(const QString &targetWindowId,
                      const QString &incomingWindowId,
                      const QString &orientation, const QString &position,
                      double ratio, quint64 expectedRevision);

  [[nodiscard]] bool operationInFlight() const noexcept {
    return m_inFlight.has_value();
  }

Q_SIGNALS:
  void operationFinished(const TaskListOperationResult &result);

private Q_SLOTS:
  void handleReply(quint64 token, const QString &uniqueOwner,
                   const QByteArray &payload);
  void handleFailure(quint64 token, const QString &uniqueOwner,
                     const QString &message);
  void handleReplyTimeout();
  void handleProducerStateChanged();

private:
  enum class PendingKind {
    Submit,
    Release,
    Dock,
  };

  struct InFlightOperation {
    quint64 token = 0;
    QString owner;
    PendingKind kind = PendingKind::Submit;
  };

  quint64 admit(PendingKind kind, const QString &firstIdentifier,
                const QString &secondIdentifier, quint64 expectedRevision,
                QString *admittedOwner);
  quint64 finishImmediately(TaskListOperationStatus status, QString code,
                            QString message);
  void finishInFlight(TaskListOperationResult result);
  quint64 nextToken();

  Producer::TaskListFactsProducer &m_producer;
  TaskListOperationTransport &m_transport;
  int m_replyTimeoutMilliseconds;
  QTimer m_replyTimeout;
  std::optional<InFlightOperation> m_inFlight;
  quint64 m_nextToken = 1;
};

} // namespace QindaQt::ShellTaskList::Operations
