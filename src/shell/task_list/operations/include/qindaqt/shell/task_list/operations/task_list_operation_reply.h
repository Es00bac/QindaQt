// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QByteArrayView>
#include <QString>
#include <QtTypes>

namespace QindaQt::ShellTaskList::Operations {

// The Compositor1 mutation method one in-flight request issued.
enum class TaskListOperationKind {
  Submit,
  Release,
  Dock,
};

// The lineage a reply must echo to settle the in-flight request.
// transactionId/containerId/expectedContainerRevision are populated for
// Submit; Dock validates only their presence and shape (the compositor
// generates those ids); the ReleaseContainer wire reply carries no echo.
struct TaskListReplyExpectation {
  TaskListOperationKind kind = TaskListOperationKind::Submit;
  QString transactionId;
  QString containerId;
  quint64 expectedContainerRevision = 0;
};

enum class TaskListReplyVerdict {
  Committed,
  Conflict,
  Rejected,
  // Not a JSON object at all; the transaction may have committed.
  UncertainMalformed,
  // The canonical echo (protocol, transactionId, containerId, status,
  // revision) is missing or mismatched; the reply cannot be this
  // transaction's outcome, but the transaction may have committed.
  UncertainLineage,
  // A well-formed reply whose status names no known outcome.
  UncertainUnknownStatus,
};

struct TaskListReplyClassification {
  TaskListReplyVerdict verdict = TaskListReplyVerdict::UncertainMalformed;
  QString code;
  QString message;
  quint64 currentContainerRevision = 0;
  bool hasCurrentContainerRevision = false;
};

// Stateless hostile-input classifier for Compositor1 operation replies.
// AGENT-CONTRACT: A Submit reply settles as Committed only when the full
// canonical lineage echoes the submitted transaction: protocol 1.x with
// minor <= 1, the exact transactionId and containerId, and a committed
// revision exactly one above the fenced container revision (the bridge
// increments exactly once per commit — docs/wiki/reference/compositor-control-v1.md).
class TaskListOperationReplyCodec final {
public:
  [[nodiscard]] static TaskListReplyClassification
  classify(const TaskListReplyExpectation &expectation, QByteArrayView payload);
};

} // namespace QindaQt::ShellTaskList::Operations
