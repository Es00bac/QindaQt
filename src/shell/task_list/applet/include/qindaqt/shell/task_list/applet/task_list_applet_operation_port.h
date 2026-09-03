// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/operations/task_list_operations.h"
#include "qindaqt/shell/task_list/task_list_intent.h"

#include <QObject>
#include <QString>

namespace QindaQt::ShellTaskListApplet {

// Injected least-authority operation seam consumed by the applet controller.
// The production implementation is TaskListAppletOperationBridge over the T1
// Compositor1 operation adapter; tests inject fakes. The seam is GUI-thread
// confined, the controller borrows it and never deletes it, and the caller
// must outlive the controller.
//
// AGENT-CONTRACT: every method returns the operation token and guarantees
// exactly one operationFinished per call — including fenced rejections, which
// MAY be emitted synchronously before the method returns (the T1 adapter
// rejects atomically before bus traffic). Implementations never resubmit and
// never deliver a result twice; results for unknown tokens are invalid and
// the controller drops them.
class TaskListAppletOperationPort : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~TaskListAppletOperationPort() override = default;

  virtual quint64
  executeTaskIntent(const ShellTaskList::TaskIntentRequest &request,
                    const ShellTaskList::TaskIntentOutcome &outcome) = 0;
  virtual quint64 activateContainerPage(const QString &containerId,
                                        const QString &pageId,
                                        quint64 expectedRevision) = 0;
  virtual quint64 detachWindow(const QString &containerId,
                               const QString &windowId,
                               quint64 expectedRevision) = 0;
  virtual quint64 releaseContainer(const QString &containerId,
                                   quint64 expectedRevision) = 0;
  virtual quint64 dockWindows(const QString &targetWindowId,
                              const QString &incomingWindowId,
                              const QString &orientation,
                              const QString &position, double ratio,
                              quint64 expectedRevision) = 0;

Q_SIGNALS:
  void operationFinished(
      const ShellTaskList::Operations::TaskListOperationResult &result);
};

} // namespace QindaQt::ShellTaskListApplet
