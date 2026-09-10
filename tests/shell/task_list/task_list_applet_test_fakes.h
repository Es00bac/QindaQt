// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/applet/task_list_applet_operation_port.h"
#include "qindaqt/shell/task_list/applet/task_list_applet_preview_port.h"

#include <QVector>

namespace TaskListAppletTest {

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskList::Operations;
using namespace QindaQt::ShellTaskListApplet;

// One recorded port invocation. method names the virtual; firstId/secondId
// carry the container/task/window identifiers in dispatch order (target then
// incoming for dockWindows); request/outcome echo the exact T0 arbitration
// the controller forwarded.
struct RecordedPortCall {
  QString method;
  quint64 token = 0;
  QString firstId;
  QString secondId;
  QString orientation;
  QString position;
  double ratio = 0.0;
  quint64 revision = 0;
  TaskIntentRequest request;
  TaskIntentOutcome outcome;
};

// AGENT-CONTRACT: recording fake for the applet's injected operation seam.
// Manual completion leaves the test driving operationFinished; Synchronous
// completion emits the result INSIDE the dispatch call, before the returned
// token is knowable to the caller — the hostile buffered-attribution case the
// controller's m_insidePortCall deferral exists for (the T1 adapter rejects
// fenced requests synchronously before any bus traffic).
class FakeTaskListOperationPort final : public TaskListAppletOperationPort {
  Q_OBJECT

public:
  enum class Completion {
    Manual,
    Synchronous,
  };

  using TaskListAppletOperationPort::TaskListAppletOperationPort;

  quint64 executeTaskIntent(const TaskIntentRequest &request,
                            const TaskIntentOutcome &outcome) override {
    return record(QStringLiteral("executeTaskIntent"), request.taskId, {},
                  {}, {}, 0.0, request.expectedRevision, request, outcome);
  }
  quint64 activateContainerPage(const QString &containerId,
                                const QString &pageId,
                                quint64 expectedRevision) override {
    return record(QStringLiteral("activateContainerPage"), containerId, pageId,
                  {}, {}, 0.0, expectedRevision, {}, {});
  }
  quint64 detachWindow(const QString &containerId, const QString &windowId,
                       quint64 expectedRevision) override {
    return record(QStringLiteral("detachWindow"), containerId, windowId, {},
                  {}, 0.0, expectedRevision, {}, {});
  }
  quint64 releaseContainer(const QString &containerId,
                           quint64 expectedRevision) override {
    return record(QStringLiteral("releaseContainer"), containerId, {}, {}, {},
                  0.0, expectedRevision, {}, {});
  }
  quint64 dockWindows(const QString &targetWindowId,
                      const QString &incomingWindowId,
                      const QString &orientation, const QString &position,
                      double ratio, quint64 expectedRevision) override {
    return record(QStringLiteral("dockWindows"), targetWindowId,
                  incomingWindowId, orientation, position, ratio,
                  expectedRevision, {}, {});
  }

  // Test-driven terminal result for one recorded call.
  void complete(const RecordedPortCall &call, TaskListOperationStatus status,
                QString code, QString message) {
    TaskListOperationResult result;
    result.token = call.token;
    result.status = status;
    result.code = std::move(code);
    result.message = std::move(message);
    Q_EMIT operationFinished(result);
  }

  // Hostile injection: a result whose token this port never issued must be
  // dropped whole by the controller (no pending change, no feedback).
  void injectResult(TaskListOperationResult result) {
    Q_EMIT operationFinished(result);
  }

  [[nodiscard]] const RecordedPortCall &lastCall() const {
    return calls.constLast();
  }

  QVector<RecordedPortCall> calls;
  quint64 nextToken = 1;
  Completion completion = Completion::Manual;
  TaskListOperationStatus synchronousStatus =
      TaskListOperationStatus::Rejected;
  QString synchronousCode = QStringLiteral("sync-reject");
  QString synchronousMessage = QStringLiteral("rejected synchronously");

private:
  quint64 record(const QString &method, const QString &firstId,
                 const QString &secondId, const QString &orientation,
                 const QString &position, double ratio, quint64 revision,
                 const TaskIntentRequest &request,
                 const TaskIntentOutcome &outcome) {
    RecordedPortCall call;
    call.method = method;
    call.token = nextToken;
    call.firstId = firstId;
    call.secondId = secondId;
    call.orientation = orientation;
    call.position = position;
    call.ratio = ratio;
    call.revision = revision;
    call.request = request;
    call.outcome = outcome;
    calls.append(call);
    const quint64 token = nextToken++;
    if (completion == Completion::Synchronous) {
      TaskListOperationResult result;
      result.token = token;
      result.status = synchronousStatus;
      result.code = synchronousCode;
      result.message = synchronousMessage;
      Q_EMIT operationFinished(result);
    }
    return token;
  }
};

class FakePreviewPort final : public TaskListAppletPreviewPort {
  Q_OBJECT
public:
  struct Call {
    QString windowId;
    quint64 revision = 0;
    QSize maxSize;
  };
  QList<Call> calls;

  void requestPreview(const TaskListPreviewRequest &request) override {
    Call call;
    call.windowId = request.windowId;
    call.revision = request.revision;
    call.maxSize = request.maxSize;
    calls.append(call);
    if (autoReply) {
      Q_EMIT previewFinished(makeResult(request.windowId, request.revision,
                                       replyOk, replyImage));
    }
  }

  static TaskListPreviewResult makeResult(const QString &windowId,
                                          quint64 revision, bool ok,
                                          const QImage &image = {}) {
    TaskListPreviewResult result;
    result.windowId = windowId;
    result.revision = revision;
    result.ok = ok;
    result.image = image;
    if (!ok) {
      result.reason = QStringLiteral("preview-unavailable");
    }
    return result;
  }

  bool autoReply = false;
  bool replyOk = true;
  QImage replyImage;
};

} // namespace TaskListAppletTest
