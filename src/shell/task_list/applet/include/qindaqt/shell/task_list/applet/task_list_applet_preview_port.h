// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>

namespace QindaQt::ShellTaskListApplet {

// One hover-preview request. windowId is the compositor window id to capture
// (for a container row: its primary active-page member), revision is the
// displayed task generation the request was made against, and maxSize bounds
// the returned image on both axes.
struct TaskListPreviewRequest {
  QString windowId;
  quint64 revision = 0;
  QSize maxSize{320, 200};
};

// Exactly one result per accepted request. ok == false carries a stable
// reason and an empty image; presentation falls back to the text tooltip.
struct TaskListPreviewResult {
  QString windowId;
  quint64 revision = 0;
  bool ok = false;
  QImage image;
  QString reason;
};

// Injected least-authority preview seam consumed by the applet controller.
// The production implementation (producer-side D-Bus client over the
// authenticated CompositorShell1 preview method, ADR-0119) may be absent —
// the controller treats a null port as "previews unavailable" and
// presentation shows the text tooltip. The seam is GUI-thread confined, the
// controller borrows it and never deletes it, and the caller must outlive
// the controller.
//
// AGENT-CONTRACT: implementations deliver at most one previewFinished per
// request and never deliver for an unknown windowId. Results may arrive
// asynchronously; the controller arbitrates staleness by (windowId,
// revision) against its in-flight request.
class TaskListAppletPreviewPort : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~TaskListAppletPreviewPort() override = default;

  virtual void requestPreview(const TaskListPreviewRequest &request) = 0;

Q_SIGNALS:
  void previewFinished(const QindaQt::ShellTaskListApplet::TaskListPreviewResult &result);
};

} // namespace QindaQt::ShellTaskListApplet

Q_DECLARE_METATYPE(QindaQt::ShellTaskListApplet::TaskListPreviewResult)
