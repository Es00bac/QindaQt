// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QMetaType>
#include <QString>
#include <QtTypes>

namespace QindaQt::ShellTaskList::Operations {

// Outcome of one operation request. Exactly one result is ever published per
// admitted request, delivered through TaskListOperationAdapter::operationFinished.
enum class TaskListOperationStatus {
  // The compositor committed the transaction (Submit "committed",
  // ReleaseContainer "released", DockWindows "docked").
  Committed,
  // The public Compositor1 protocol does not expose this window-level
  // operation; the exact extension needed is named in code (see
  // docs/wiki/shell/task-list.md, "Protocol extension requests").
  Unavailable,
  // The caller's task-list generation no longer matches the producer's last
  // accepted generation; rejected atomically before any bus traffic.
  StaleGeneration,
  // The facts producer is Loading or Degraded; no generation may be acted on.
  SourceNotReady,
  // The named container is not part of the last accepted generation.
  UnknownContainer,
  // The container's authority does not admit this operation (Submit and
  // ReleaseContainer mutate control-bridge containers only).
  UnsupportedAuthority,
  // Another operation is in flight; requests are serialized so exactly-once
  // lineage never requires client-side queuing of stale intent.
  Busy,
  // The request itself is malformed (empty or over-limit identifier, bad
  // dock geometry); rejected before any bus traffic.
  InvalidRequest,
  // The compositor rejected the request; code/message are the wire failure.
  Rejected,
  // The compositor reports a revision conflict; currentContainerRevision
  // carries the server's current revision.
  Conflict,
  // The request never reached the bus (transport stopped or owner lost
  // before sending).
  TransportFailure,
  // The request was sent but its outcome is unknown (timeout, malformed
  // reply, or owner loss in flight). Exactly-once: the adapter never
  // resubmits; the caller must re-read state before deciding.
  Uncertain,
};

struct TaskListOperationResult {
  quint64 token = 0;
  TaskListOperationStatus status = TaskListOperationStatus::Unavailable;
  // Stable machine code: the compositor's failure code for Rejected/Conflict,
  // or an adapter code such as "compositor-window-activate-unavailable".
  QString code;
  QString message;
  quint64 currentContainerRevision = 0;

  [[nodiscard]] bool ok() const noexcept {
    return status == TaskListOperationStatus::Committed;
  }

  friend bool operator==(const TaskListOperationResult &,
                         const TaskListOperationResult &) = default;
};

} // namespace QindaQt::ShellTaskList::Operations

Q_DECLARE_METATYPE(QindaQt::ShellTaskList::Operations::TaskListOperationResult)
