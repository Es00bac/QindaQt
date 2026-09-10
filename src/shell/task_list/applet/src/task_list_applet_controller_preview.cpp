// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"

#include "qindaqt/shell/task_list/applet/task_list_applet_preview_port.h"
#include "qindaqt/shell/task_list/applet/task_list_preview_provider.h"

#include <QQmlEngine>

namespace QindaQt::ShellTaskListApplet {

void TaskListAppletController::installPreviewProvider(QQmlEngine *engine) {
  if (engine == nullptr || m_previewProvider != nullptr) {
    return;
  }
  auto *provider = new TaskListPreviewProvider();
  engine->addImageProvider(
      QString::fromLatin1(TaskListPreviewProvider::kProviderName), provider);
  m_previewProvider = provider;
}

bool TaskListAppletController::previewsEnabled() const noexcept {
  return m_previewPort != nullptr;
}

void TaskListAppletController::setPreviewPort(TaskListAppletPreviewPort *port) {
  if (port == m_previewPort) {
    return;
  }
  m_previewPort = port;
  // AGENT-GUARD: a swapped port must not leave the previous in-flight
  // request attributed to the new seam; the generation counter drops any
  // late result of the old port.
  ++m_previewGeneration;
  m_previewTaskId.clear();
  m_previewWindowId.clear();
  m_previewRevision = 0;
  if (m_previewPort != nullptr) {
    connect(m_previewPort, &TaskListAppletPreviewPort::previewFinished, this,
            [this](const TaskListPreviewResult &result) {
              if (result.windowId != m_previewWindowId
                  || result.revision != m_previewRevision
                  || m_previewWindowId.isEmpty()) {
                return;
              }
              const QString taskId = m_previewTaskId;
              const quint64 revision = result.revision;
              const int imageToken =
                  result.ok && m_previewProvider != nullptr
                      ? m_previewProvider->insert(result.image)
                      : 0;
              ++m_previewGeneration;
              m_previewTaskId.clear();
              m_previewWindowId.clear();
              m_previewRevision = 0;
              Q_EMIT previewArrived(taskId, revision, imageToken);
            });
  }
  Q_EMIT previewPortChanged();
}

void TaskListAppletController::requestTaskPreview(const QString &taskId,
                                                  quint64 revision,
                                                  int maxWidth, int maxHeight) {
  if (m_previewPort == nullptr || !m_grants.windowsRead ||
      m_projection.phase != TaskListAppletPhase::Ready ||
      revision != m_source.revision()) {
    return;
  }
  const ShellTaskList::TaskEntry *entry = findEntry(taskId);
  if (entry == nullptr || entry->primaryWindowId.isEmpty()) {
    return;
  }
  // Supersede: exactly one capture in flight; the port result handler
  // matches (windowId, revision) so a superseded capture is dropped.
  ++m_previewGeneration;
  m_previewTaskId = taskId;
  m_previewWindowId = entry->primaryWindowId;
  m_previewRevision = revision;
  TaskListPreviewRequest request;
  request.windowId = m_previewWindowId;
  request.revision = revision;
  request.maxSize = QSize(qBound(16, maxWidth, 1024), qBound(16, maxHeight, 1024));
  m_previewPort->requestPreview(request);
}

void TaskListAppletController::cancelTaskPreview() {
  ++m_previewGeneration;
  m_previewTaskId.clear();
  m_previewWindowId.clear();
  m_previewRevision = 0;
}

} // namespace QindaQt::ShellTaskListApplet
